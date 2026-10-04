#pragma once

#include "audio/AudioEngine.h"
#include "audio/SongLoader.h"
#include <array>

/*  UiSession のテスト用の偽のエンジン（音声の機器なし）。曲を持ち、再生位置を覚えるだけ。
    出力はいつも「開いている・曲の SR のまま鳴らせる」 */

namespace vb::test
{
class FakeEngine final : public audio::AudioEngine
{
public:
    void setSong (std::shared_ptr<const audio::SongAudio> a) override
    {
        song = std::move (a);
        playhead = 0;
        playing = false;
    }
    bool hasSong() const override { return song != nullptr; }

    void  play() override                          { playing = true; }
    void  stop() override                          { playing = false; }
    bool  isPlaying() const override               { return playing; }
    void  seek (audio::int64 sample) override      { playhead = sample; }
    audio::int64 getPlayheadSample() const override { return playhead; }
    bool  consumeReachedEnd() override             { return false; }
    void  setLoop (audio::int64, audio::int64, bool) override {}
    void  setVocalGain (int slot, float g) override { if (slot >= 0 && slot < 4) vocalGains[(size_t) slot] = g; }   // 0 = Main

    void setBackingLevel (float, bool) override {}

    audio::OutputStatus getOutputStatus() const override
    {
        audio::OutputStatus o;
        o.open = true;
        o.deviceName = "Fake";
        o.sampleRate = song != nullptr ? song->sampleRate : 48000.0;
        o.bufferSize = 256;
        return o;
    }

    std::shared_ptr<const audio::SongAudio> song;
    audio::int64 playhead = 0;
    bool playing = false;
    std::array<float, 4> vocalGains { -1.0f, -1.0f, -1.0f, -1.0f };
};
} // namespace vb::test
