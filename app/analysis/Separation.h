#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <functional>
#include <vector>

/*  ボーカル分離の前後（DESIGN 11.3 / B16）。モデル（Mel-Band RoFormer の ONNX）に入れるのは
    「スペクトログラム → ボーカルのスペクトログラム」だけで、STFT / iSTFT とチャンクの分割・重ね合わせはここ（C++）。
    Python の参照実装（MSST の demix、torch.stft と同じ STFT）と同じ計算にする。

    - 44.1 kHz ステレオ。n_fft 2048・hop 441・周期 Hann・正規化なし・center（両端 1024 サンプルを reflect）
    - スペクトログラムの並び：[ch 2][bin 1025][frame][re, im]（ONNX の [batch, 2, 1025, frames, 2] の 1 個分）
    - 分割：8 秒（352800 サンプル）、step = chunk / overlap、fade = chunk / 10 の直線（最初はフェードインなし、最後はフェードアウトなし）。
      曲が 2 × border（chunk − step）より長ければ両端を border ずつ reflect、最後の半端なチャンクは半分より長ければ reflect・短ければ 0 埋め、
      重み付き和 ÷ 重み和、最後に border を切る
    UI・音声デバイスに依存しない（分離プロセスとテストから使う） */

namespace vb::analysis::separation
{
constexpr int nFft = 2048, hop = 441, bins = nFft / 2 + 1;
constexpr int chunkSamples = 352800;                    // 8 秒
constexpr double sampleRate = 44100.0;

inline int framesFor (int samples) { return samples / hop + 1; }

/** x は 2 ch × n。戻り値は [2][1025][frames][2] */
std::vector<float> stft (const float* const* x, int n, int& framesOut);

/** spec（[2][1025][frames][2]）→ out 2 ch × length */
void istft (const std::vector<float>& spec, int frames, int length, float* const* out);

/** モデル：spec（[2][1025][frames][2]）→ est（同じ形のボーカル）。失敗・中止なら false */
using Model = std::function<bool (const std::vector<float>& spec, int frames, std::vector<float>& est)>;

/** mix（44.1 kHz ステレオ）からボーカルを取り出す。progress は 0..1（false で中止）。失敗・中止なら false */
bool demix (const juce::AudioBuffer<float>& mix, const Model&, int overlap, juce::AudioBuffer<float>& vocals,
            const std::function<bool (float)>& progress = {});

/** チャンクの数（時間の見込み：最初の 1 個の実測 × この数。DESIGN 11.6.1） */
int chunkCount (int samples, int overlap);
} // namespace vb::analysis::separation
