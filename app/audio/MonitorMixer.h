#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

/*  自分の声のモニター（DESIGN 7.3 / Phase B4）。デバイスに依存しない中身
    入力（1 ch・モノラル）を出力の L / R に同じ量で足す。録音はしない（B5）。
    モニターリバーブはここだけで掛ける（録音バスには絶対に入らない。DESIGN 4.7 / 14）。
    リバーブへの送りはフェーダーの後（自分をミュートすればリバーブも止まる。残響の尾だけは自然に消える）

    ルール（DESIGN 17）
      - process() ではメモリ確保しない。prepare() で最大ブロック分を確保し、それより長いブロックは分けて処理する
      - 音量・ミュート・リバーブ量は atomic で受け、数 ms でなめらかに変える（切り替えでプチッといわない）
      - 足す自分の声（リバーブ込み）は ±1 に収める。ハウリングや大声で耳を傷めないための最後の砦。オフボには掛けない */

namespace vb::audio
{
class MonitorMixer
{
public:
    /** デバイスが始まる時（オーディオスレッドが止まっている間）に呼ぶ */
    void prepare (double sampleRate, int maxBlockSize);

    /** 自分の声の音量（直線の倍率）とミュート。メッセージスレッドから */
    void setGain (float linearGain)     { gain.store (linearGain); }
    void setMuted (bool m)              { muted.store (m); }
    /** モニターリバーブの量（0..1。0 で掛けない） */
    void setReverb (float amount)       { reverbAmount.store (juce::jlimit (0.0f, 1.0f, amount)); }

    /** オーディオスレッド。input が nullptr なら無音として扱う（リバーブの尾は鳴り続ける）。
        out の先頭 2 ch（1 ch なら 1 ch）に足す。3 ch 目以降は触らない */
    void process (const float* input, float* const* out, int numChannels, int numSamples) noexcept;

    /** リバーブの尾を消す（デバイスを開き直した・曲を止めた時など。オーディオスレッドが止まっている間） */
    void reset();

    /** リバーブが鳴りきるまで処理を続ける長さ（秒） */
    static constexpr double tailSeconds = 4.0;

private:
    void processChunk (const float* input, float* const* out, int numChannels, int numSamples) noexcept;

    std::atomic<float> gain { 1.0f }, reverbAmount { 0.0f };
    std::atomic<bool> muted { false };

    // オーディオスレッドだけが触る
    juce::SmoothedValue<float> smoothedGain { 0.0f }, smoothedSend { 0.0f };
    juce::Reverb reverb;
    juce::AudioBuffer<float> scratch;   // 0：自分（フェーダー後）、1 / 2：リバーブの L / R
    int maxBlock = 0;
    juce::int64 tailLeft = 0, tailLength = 0;   // 送りが 0 になってからリバーブを回し続ける残り（サンプル）
    bool prepared = false;
};
} // namespace vb::audio
