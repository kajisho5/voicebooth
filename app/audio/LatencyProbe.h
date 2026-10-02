#pragma once

#include <juce_core/juce_core.h>
#include <atomic>
#include <vector>

/*  往復の遅れ（レイテンシ）の実測（DESIGN 5 Step 3 / B6）

    出力に短いチャープ（500 Hz → 8 kHz、40 ms、-12 dBFS）を一定の間隔で 5 回出し、同時に入力を録る。
    録った入力とチャープの相互相関の山から「出力に書いたサンプル → 入力に返ってきたサンプル」の差を 1 回ずつ求め、
    そろった回の中央値を往復の遅れ（サンプル）とする。録音（TakeRecorder）と同じ数え方（同じコールバックの
    出力の位置と入力の位置）なので、テイクの頭をこの分だけ前にずらせば、歌い手が聞いた伴奏の位置にそろう。

    - 1 回の間隔（0.65 秒）は測れる最大の遅れ（0.6 秒）より長い（次の回の音を取り違えない）
    - そろわない（反響・Bluetooth の揺れ）、小さすぎる、届かない時は失敗にして、理由を返す（手入力に回す）
    - オーディオスレッドではメモリを確保しない（start でまとめて確保）。解析はメッセージスレッドか裏のスレッドで */

namespace vb::audio::latency
{
using int64 = juce::int64;

/** 測定音の段取り（サンプル数は SR から決まる） */
struct Plan
{
    double sampleRate = 0.0;
    int chirpLength = 0;      // 1 回のチャープ
    int lead = 0;             // 最初の無音
    int period = 0;           // 回の間隔
    int bursts = 5;
    int maxLatency = 0;       // 測れる最大の遅れ
    int64 totalLength = 0;    // 出す・録る長さ（最後の回 + 最大の遅れ + チャープ）

    int64 burstStart (int k) const { return (int64) lead + (int64) k * period; }
};

Plan planFor (double sampleRate);

/** 1 回分のチャープ（両端は 2 ms の窓で上げ下げ。振幅 0.25 = -12 dBFS） */
std::vector<float> makeChirp (double sampleRate);

/** 出力に流す全体（無音 + 5 回のチャープ）。長さは plan.totalLength */
std::vector<float> makeSignal (const Plan&);

/** 測った結果 */
struct Result
{
    enum class Status { ok, silent, weak, unstable, failed };

    Status status = Status::failed;
    int64 samples = 0;        // 往復の遅れ（ok の時）
    int agreeing = 0;         // そろった回数
    int bursts = 0;
    double spreadMs = 0.0;    // そろった回の幅
    float snrDb = 0.0f;       // 山の高さ（相関の山 / 山以外の RMS）の中央値
    float inputPeak = 0.0f;   // 録った入力の最大振幅
    bool clipped = false;     // 入力が割れた（結果は使えるが、音量を下げる案内を出す）

    bool ok() const { return status == Status::ok; }
};

/** 録った入力（測定音を出し始めた時から plan.totalLength）を解析する */
Result analyse (const float* captured, int64 length, const Plan&);

/** 相互相関に使う FFT（2 のべき乗。re / im をその場で変換。inverse は 1/n を掛けない） */
void fft (std::vector<double>& re, std::vector<double>& im, bool inverse);

//==============================================================================
/** オーディオスレッド側：測定音を出して入力を録る */
class Probe
{
public:
    /** メッセージスレッド：SR に合わせて確保し、始める（走っている間は確保し直さない） */
    void start (double sampleRate);
    void cancel() noexcept                 { running.store (false); }
    bool isRunning() const noexcept        { return running.load(); }
    bool isFinished() const noexcept       { return finished.load(); }

    /** オーディオスレッド：走っていれば出力を測定音で置き換え（全チャンネル同じ）、入力を録る。置き換えたら true */
    bool process (const float* input, float* const* outputs, int numOutputs, int numSamples) noexcept;

    /** 終わった後（メッセージスレッド）：録った入力と段取り */
    const std::vector<float>& captured() const { return capture; }
    const Plan& plan() const               { return currentPlan; }

private:
    juce::SpinLock lock;            // start の確保とオーディオスレッドが重ならないように（オーディオ側は try-lock）
    Plan currentPlan;
    std::vector<float> signal, capture;
    int64 position = 0;
    std::atomic<bool> running { false }, finished { false };
};
} // namespace vb::audio::latency
