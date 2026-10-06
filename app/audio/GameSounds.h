#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>

/*  待ち時間のゲーム（起動画面で分離を待つ間）の音（2026-10-04）。デバイスに依存しない中身
    - リズムタップ：一定のテンポのクリック。曲とは別に鳴らす（まだ曲を開いていない）。4 拍ごとに高い音
    - 音程あて：目標の音を正弦波で鳴らす（頭と終わりは 20 ms でなめらかに。プツッと鳴らさない）
    - process() はオーディオスレッドで出力に「足す」。確保しない・止まらない。ほかはどのスレッドからでも
    - 時計：クリックを始めてから出力に書いたサンプル数（samplesRendered）。画面は「いま耳に届いている位置」を
      この数・書いた時刻・出力の遅延から求めて、押した瞬間が拍からどれだけずれたかを測る */

namespace vb::audio
{
class GameSounds
{
public:
    void prepare (double sampleRate);

    /** クリックの速さ（BPM）。0 で止める。変えるたびに 0 拍目から数え直す */
    void setBeat (double bpm);
    /** 目標の音を鳴らす（MIDI の音の高さ・秒）。midi <= 0 で止める */
    void playTone (float midi, double seconds);

    /** 出力に足す（オーディオスレッド）。クリック・音とも鳴っていなければ何もしない */
    void process (float* const* outputs, int numOutputs, int numSamples) noexcept;

    /** クリックを始めてから書いたサンプル数（いまのブロックの終わりまで）。止まっていれば -1 */
    juce::int64 samplesRendered() const noexcept { return rendered.load (std::memory_order_acquire); }
    double getSampleRate() const noexcept { return sampleRate.load(); }

    static constexpr float clickGain = 0.35f;
    static constexpr float toneGain = 0.18f;
    static constexpr double clickSeconds = 0.035;

private:
    std::atomic<double> sampleRate { 48000.0 };
    std::atomic<double> wantBpm { 0.0 };
    std::atomic<int> beatSerial { 0 };     // setBeat のたびに増やす（オーディオスレッドが数え直す）
    std::atomic<float> wantToneMidi { 0.0f };
    std::atomic<double> wantToneSeconds { 0.0 };
    std::atomic<int> toneSerial { 0 };
    std::atomic<juce::int64> rendered { -1 };

    // オーディオスレッドだけが触る
    int seenBeatSerial = 0, seenToneSerial = 0;
    double bpm = 0.0;
    juce::int64 beatCounter = 0;       // クリックを始めてからのサンプル
    juce::int64 clickLeft = 0;         // 鳴っているクリックの残り（サンプル）
    double clickPhase = 0.0, clickFreq = 1000.0;
    juce::int64 toneLeft = 0, toneTotal = 0, nextToneTotal = 0;   // next：鳴っている音を下げきってから鳴らす音
    double tonePhase = 0.0, toneFreq = 0.0, nextToneFreq = 0.0;
};
} // namespace vb::audio
