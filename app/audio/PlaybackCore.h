#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "SongLoader.h"

/*  オフボの再生（DESIGN 7.2 / Phase B2）。デバイスに依存しない中身
    render() をオーディオスレッドから呼ぶ。ほかはメッセージスレッドから呼ぶ。

    ルール（DESIGN 17）
      - render() ではメモリ確保・ファイル I/O・待つロックをしない（曲の差し替えは try-lock、取れなければ無音）
      - 位置は曲のサンプル（int64）。出力デバイスの SR が曲と違うときだけ試聴用に変換する（線形補間）
      - テンポ / キー（B11）、クリック・カウントイン、入力（B3〜）はまだ無い */

namespace vb::audio
{
class PlaybackCore
{
public:
    /** 曲を差し替える（停止して頭へ）。古い曲はこのスレッドで解放される */
    void setSong (std::shared_ptr<const SongAudio>);
    bool hasSong() const;

    /** 出力の準備（デバイスが始まる時に呼ぶ） */
    void prepare (double outputSampleRate);
    double getOutputSampleRate() const { return outputRate.load(); }

    void play();
    void stop();
    bool isPlaying() const { return playing.load(); }

    /** 次の render の頭でこの位置へ */
    void seek (juce::int64 sample);
    juce::int64 getPosition() const;

    /** ループ範囲 [in, out)。out に来たら in へ戻る */
    void setLoop (juce::int64 in, juce::int64 out, bool enabled);

    /** オフボの音量（直線の倍率）とミュート。急に変えず数 ms でなめらかに */
    void setGain (float linearGain);
    void setMuted (bool);

    /** 曲の終わりまで行って止まったら、1 度だけ true */
    bool consumeReachedEnd() { return reachedEnd.exchange (false); }

    /** オーディオスレッド。out は numChannels 本 × numSamples（必ず全部書く） */
    void render (float* const* out, int numChannels, int numSamples) noexcept;

    /** フェーダーの位置（0..1）→ 倍率。0.75 で 0 dB、1.0 で +6 dB、0 で無音 */
    static float faderToGain (float position) noexcept;

private:
    juce::SpinLock songLock;
    std::shared_ptr<const SongAudio> song;          // songLock で保護

    std::atomic<double> outputRate { 0.0 };
    std::atomic<bool> playing { false }, reachedEnd { false }, muted { false }, loopOn { false };
    std::atomic<juce::int64> position { 0 }, pendingSeek { -1 }, loopIn { 0 }, loopOut { 0 };
    std::atomic<float> gain { 1.0f };

    // オーディオスレッドだけが触る
    double fraction = 0.0;                          // SR 変換時の小数部
    juce::SmoothedValue<float> smoothedGain { 1.0f };
    bool prepared = false;
};
} // namespace vb::audio
