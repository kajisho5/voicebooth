#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include "audio/PitchTracker.h"

/*  お手本の音程を RMVPE で取る（2026-10-02）
    分離の残りや引き算の残り（伴奏が少し残った声）に、自分の声用の YIN（PitchAnalyzer）は弱い
    （伴奏と声が同じ大きさで正しい音程 20%・オクターブの誤り 30%）。RMVPE は 94%・0.2%（pitchbench、MIR-1K 0 dB）。
    重い（約 100 MB の ONNX）ので本体では動かさず、分離プロセス（VoiceBoothSeparator --pitch）で回す。
    ここは ONNX Runtime に依存しない前後の計算だけ（テストから使う）：
      16 kHz へのリサンプル → log-mel（RVC の rmvpe.py と同じ）→［ONNX］→ 360 bin の読み取り → Viterbi（オクターブの誤りを消す）

    モデル：RVC の rmvpe.onnx（https://huggingface.co/lj1995/VoiceConversionWebUI 、MIT）を int8 に量子化した物。
    int8 と fp32 の差：伴奏 −5 dB でも声の有無の食い違い 1%・50 セント超の差 0.04%（2026-10-02 実測） */

namespace vb::analysis::rmvpe
{
constexpr double sampleRate = 16000.0;
constexpr int nFft = 1024;
constexpr int hop = 160;              // 10 ms
constexpr int mels = 128;
constexpr int bins = 360;             // 20 セントごと
constexpr int chunkFrames = 1000;     // ONNX に一度に渡す長さ（メモリ。前後に context を足す）
constexpr int contextFrames = 128;    // 1.28 秒

/** モノラルを 16 kHz に（窓付き sinc。下げる時は折り返しを切る） */
std::vector<float> resampleTo16k (const float* x, int64_t n, double rate);

/** log-mel [mels × frames]（frames = 1 + n / hop。torch.stft center=True・reflect・周期 Hann、HTK・slaney の mel 30..8000 Hz、ln(max(·, 1e-5))） */
std::vector<float> logMel (const std::vector<float>& x16, int& frames);

/** 1 フレームの 360 bin（sigmoid）から音程（セント、10 Hz 基準）と強さ（最大値） */
void decodeFrame (const float* salience, float& cents, float& strength);

/** 強さ → 声のある確率（pitchbench の dev で合わせた isotonic の表） */
float voicedProbability (float strength);

/** 1 フレームの結果 */
struct Frame
{
    float cents = 0.0f;      // 0 = 無声
    float strength = 0.0f;
};

/** Viterbi：各フレームの候補（RMVPE の音程と ±1 オクターブ）と無声から、つながりの良い道を選ぶ（pitchbench の rmvpe1 の設定）。
    続いたオクターブの誤り・短い声の有無のちらつきが消える。返り値はフレームごとのセント（0 = 無声） */
std::vector<float> smoothPath (const std::vector<Frame>&);

/** セント（10 Hz 基準）→ MIDI */
inline float centsToMidi (float cents) { return cents / 100.0f + 3.4868205f; }   // 69 − 12·log2(440 / 10)

/** 声がほとんど無い所は無声にする：その点の ±10 ms の声のピークが、声の大きい所（99 パーセンタイル）より dbBelow 以上小さい。
    vocals は points と同じ時間（曲のサンプル）。引き算・分離で残ったかすかな音に線を出さない。
    loudFrom を渡すと「声の大きい所」はそちらで測る（ハモリの線：全体の声より小さすぎる残りを消す） */
void gateQuiet (std::vector<audio::PitchFrame>& points, const float* vocals, int64_t n, double rate, float dbBelow = 35.0f,
                const float* loudFrom = nullptr);

/** 解析の結果をアプリの音程の点にする。offsetSamples = 16 kHz の 0 フレーム目が曲のどこか、rate = 曲の SR */
std::vector<audio::PitchFrame> toPitchFrames (const std::vector<Frame>&, double rate, int64_t offsetSamples = 0);
} // namespace vb::analysis::rmvpe
