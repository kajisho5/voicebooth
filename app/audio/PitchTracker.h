#pragma once

#include <juce_core/juce_core.h>
#include <atomic>
#include <vector>

/*  自分の声のリアルタイムピッチ（DESIGN 7.3 / 11.3 / B8）

    オーディオスレッドは入力と「そのブロックで鳴らした曲の位置」をリングバッファに入れるだけ。
    検出は専用のスレッドで：約 16 kHz に間引き（窓付き sinc の低域通過）→ YIN（差分関数の累積平均正規化・
    しきい値 0.15・放物線補間）を 10 ms ごと、窓 32 ms、55〜1100 Hz。信頼度 = 1 - d'(τ)。-50 dBFS 未満は無声。
    前後 2 点ずつ（5 点）を見て、両側を別の音にはさまれた 20 ms 以下の飛びは信用しない（音の切り替わりで 2 つの音が窓に
    混ざった時の低い周期・オクターブの飛び）。前後とも声があれば 3 点の中央値でならす（20 ms 遅れる）。

    出す位置は「その窓の真ん中のサンプルと同じコールバックで鳴らした曲の位置」（遅れの補正前。補正は UiSession）。
    曲が止まっている間の入力は位置を持たない（線にしない）。 */

namespace vb::audio
{
using int64 = juce::int64;

struct PitchFrame
{
    int64 songSample = 0;     // 窓の真ん中（遅れの補正前）
    float midi = 0.0f;        // 0 = 無声
    float confidence = 0.0f;  // 0..1
    float levelDb = -100.0f;  // 窓のピーク
};

namespace pitch
{
constexpr double analysisRate = 16000.0;   // 目安。実際は SR / 整数
constexpr double hopSeconds = 0.01;
constexpr double windowSeconds = 0.032;
constexpr double minHz = 55.0, maxHz = 1100.0;
constexpr float threshold = 0.15f;
constexpr float gateDb = -50.0f;

/** 1 窓の YIN。x は window + maxLag サンプル以上。周期（サンプル、補間つき）と信頼度を返す */
struct Estimate { double lag = 0.0; float confidence = 0.0f; };
Estimate yin (const float* x, int window, int minLag, int maxLag, std::vector<float>& scratch);

inline float hzToMidi (double hz) { return hz > 0.0 ? (float) (69.0 + 12.0 * std::log2 (hz / 440.0)) : 0.0f; }

/** 間引きの比（SR / 約 16 kHz の整数） */
inline int decimation (double sampleRate) { return juce::jmax (1, juce::roundToInt (sampleRate / analysisRate)); }
} // namespace pitch

class PitchTracker : private juce::Thread
{
public:
    PitchTracker();
    ~PitchTracker() override;

    /** メッセージスレッド：SR に合わせて準備して、検出のスレッドを動かす（デバイスが開いた時） */
    void prepare (double sampleRate);
    void release();

    /** オーディオスレッド：入力 1 ch と、同じコールバックで鳴らした曲の範囲（played = 0 なら止まっている） */
    void push (const float* input, int numSamples, int64 songStart, int songPlayed) noexcept;

    /** メッセージスレッド：出てきた点を取り出す */
    void pop (std::vector<PitchFrame>& out);

    double getSampleRate() const { return sampleRate.load(); }
    /** 入力のサンプルから点が出るまでの遅れ（窓の半分 + 後ろ 2 点分。秒） */
    static double processingDelaySeconds() { return windowSeconds() * 0.5 + 2.0 * pitch::hopSeconds; }

private:
    static double windowSeconds() { return pitch::windowSeconds; }
    void run() override;
    void analyse (const float* in, const int64* pos, int n);

    // オーディオスレッド → 検出スレッド。prepare / release の作り直しとオーディオスレッドが重ならないよう lock（オーディオ側は try-lock）
    juce::SpinLock pushLock;
    juce::AbstractFifo fifo { 1 };
    std::vector<float> ring;
    std::vector<int64> ringPos;            // そのサンプルの曲の位置（-1 = 止まっている）
    std::atomic<double> sampleRate { 0.0 };
    std::atomic<bool> ready { false };
    std::atomic<int> overflow { 0 };

    // 検出スレッド
    int decim = 3, hop = 160, window = 512, minLag = 14, maxLag = 290;
    std::vector<float> taps;               // 間引きの低域通過
    std::vector<float> history;            // 低域通過の入力の履歴（taps と同じ長さ、循環）
    int historyPos = 0, phase = 0;
    std::vector<float> buf;                // 間引いた後の最近の音（window + maxLag）
    std::vector<int64> bufPos;
    int sinceHop = 0, filled = 0;
    std::vector<float> scratch, frameIn;
    std::vector<int64> frameInPos;
    PitchFrame recent[5];
    int recentCount = 0;

    // 検出スレッド → メッセージスレッド
    juce::CriticalSection outLock;
    std::vector<PitchFrame> output;
};
} // namespace vb::audio
