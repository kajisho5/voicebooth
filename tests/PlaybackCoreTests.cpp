#include "audio/PlaybackCore.h"

namespace vb::audio
{
namespace
{
    /** 0, 1, 2, ... と値がサンプル番号になっている曲（どの位置を鳴らしたかが出力から分かる） */
    std::shared_ptr<const SongAudio> rampSong (int channels, int length, double rate)
    {
        auto s = std::make_shared<SongAudio>();
        s->sampleRate = rate;
        s->buffer.setSize (channels, length);
        for (int c = 0; c < channels; ++c)
            for (int i = 0; i < length; ++i)
                s->buffer.setSample (c, i, (float) i + (float) c * 0.25f);   // 右は +0.25（左右の取り違えが分かる）
        return s;
    }

    struct Block
    {
        juce::AudioBuffer<float> b;
        explicit Block (int n) : b (2, n) {}
        float l (int i) const { return b.getSample (0, i); }
        float r (int i) const { return b.getSample (1, i); }
    };

    Block render (PlaybackCore& core, int n)
    {
        Block out (n);
        core.render (out.b.getArrayOfWritePointers(), 2, n);
        return out;
    }
}

class PlaybackCoreTests : public juce::UnitTest
{
public:
    PlaybackCoreTests() : juce::UnitTest ("PlaybackCore", "VoiceBooth") {}

    void runTest() override
    {
        beginTest ("stopped renders silence, play renders the song sample-exact");
        {
            PlaybackCore core;
            core.setSong (rampSong (2, 1000, 48000.0));
            core.prepare (48000.0);
            core.setGain (1.0f);

            auto a = render (core, 64);
            expectEquals (a.b.getMagnitude (0, 64), 0.0f);
            expectEquals (core.getPosition(), (juce::int64) 0);

            core.play();
            auto b = render (core, 64);
            expectEquals (b.l (0), 0.0f);
            expectEquals (b.l (63), 63.0f);
            expectEquals (b.r (10), 10.25f);
            expectEquals (core.getPosition(), (juce::int64) 64);

            auto c = render (core, 10);   // 続きから
            expectEquals (c.l (0), 64.0f);
        }

        beginTest ("seek is applied at the next block");
        {
            PlaybackCore core;
            core.setSong (rampSong (2, 1000, 48000.0));
            core.prepare (48000.0);
            core.play();
            render (core, 32);
            core.seek (500);
            expectEquals (core.getPosition(), (juce::int64) 500);   // 画面にはすぐ
            auto b = render (core, 8);
            expectEquals (b.l (0), 500.0f);
            expectEquals (core.getPosition(), (juce::int64) 508);
        }

        beginTest ("loop wraps from out to in without a gap");
        {
            PlaybackCore core;
            core.setSong (rampSong (1, 1000, 48000.0));
            core.prepare (48000.0);
            core.setLoop (100, 110, true);
            core.seek (105);
            core.play();
            auto b = render (core, 12);
            // 105..109 の次は 100（110 は鳴らない）
            expectEquals (b.l (4), 109.0f);
            expectEquals (b.l (5), 100.0f);
            expectEquals (b.l (11), 106.0f);
            expectEquals (b.r (5), 100.0f);   // モノラルの曲は両耳へ

            core.setLoop (100, 110, false);   // ループを外すとそのまま進む
            core.seek (108);
            auto c = render (core, 4);
            expectEquals (c.l (3), 111.0f);
        }

        beginTest ("stops at the end and reports it once");
        {
            PlaybackCore core;
            core.setSong (rampSong (2, 100, 48000.0));
            core.prepare (48000.0);
            core.seek (95);
            core.play();
            auto b = render (core, 10);
            expectEquals (b.l (4), 99.0f);
            expectEquals (b.l (5), 0.0f);                 // 終わりの後は無音
            expect (! core.isPlaying());
            expectEquals (core.getPosition(), (juce::int64) 100);
            expect (core.consumeReachedEnd());
            expect (! core.consumeReachedEnd());          // 1 度だけ
        }

        beginTest ("gain and mute ramp smoothly");
        {
            PlaybackCore core;
            auto s = std::make_shared<SongAudio>();
            s->sampleRate = 48000.0;
            s->buffer.setSize (1, 48000);
            for (int i = 0; i < 48000; ++i) s->buffer.setSample (0, i, 1.0f);
            core.setSong (s);
            core.setGain (1.0f);
            core.prepare (48000.0);
            core.play();
            render (core, 256);

            core.setMuted (true);
            auto b = render (core, 2048);                 // 20 ms（960 サンプル）で 0 へ
            expectGreaterThan (b.l (0), 0.9f);
            expectWithinAbsoluteError (b.l (1500), 0.0f, 1.0e-6f);
            for (int i = 1; i < 1000; ++i)
                expect (b.l (i) <= b.l (i - 1) + 1.0e-6f);   // 段差なく下がる
        }

        beginTest ("fader taper");
        {
            expectEquals (PlaybackCore::faderToGain (0.0f), 0.0f);
            expectWithinAbsoluteError (PlaybackCore::faderToGain (0.75f), 1.0f, 1.0e-5f);                                  // 0 dB
            expectWithinAbsoluteError (PlaybackCore::faderToGain (1.0f), juce::Decibels::decibelsToGain (6.0f), 1.0e-4f);  // +6 dB
            expectWithinAbsoluteError (juce::Decibels::gainToDecibels (PlaybackCore::faderToGain (0.375f)), -12.04f, 0.05f);
            expect (PlaybackCore::faderToGain (0.5f) < PlaybackCore::faderToGain (0.6f));
        }

        beginTest ("different device rate: position advances in song samples");
        {
            PlaybackCore core;
            core.setSong (rampSong (1, 100000, 44100.0));
            core.prepare (48000.0);                        // 曲 44.1 kHz をデバイス 48 kHz で
            core.play();
            auto b = render (core, 48000);                 // 1 秒ぶん
            expect (std::abs (core.getPosition() - 44100) <= 1, juce::String (core.getPosition()));   // 小数の丸めで ±1
            // 補間した値は位置の前後に収まる（線形補間）
            expectWithinAbsoluteError (b.l (48000 - 1), 44100.0f - 44100.0f / 48000.0f, 1.0f);
        }

        beginTest ("new song stops and rewinds");
        {
            PlaybackCore core;
            core.setSong (rampSong (2, 1000, 48000.0));
            core.prepare (48000.0);
            core.play();
            render (core, 100);
            core.setSong (rampSong (2, 500, 48000.0));
            expect (! core.isPlaying());
            expectEquals (core.getPosition(), (juce::int64) 0);
            core.setSong (nullptr);
            auto b = render (core, 16);
            expectEquals (b.b.getMagnitude (0, 16), 0.0f);
            expect (! core.hasSong());
        }
    }
};

static PlaybackCoreTests playbackCoreTests;
} // namespace vb::audio
