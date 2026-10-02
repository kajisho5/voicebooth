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

    /** 正弦波の曲。burstAt 以降の 0.3 秒だけ 1 kHz を足す（頭の位置がそろっているかを見る） */
    std::shared_ptr<const SongAudio> toneSong (double hz, double seconds, double rate, double burstAt = -1.0)
    {
        auto s = std::make_shared<SongAudio>();
        s->sampleRate = rate;
        const auto n = (int) (seconds * rate);
        s->buffer.setSize (2, n);
        for (int i = 0; i < n; ++i)
        {
            const auto t = i / rate;
            auto v = burstAt < 0.0 ? 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * hz * t) : 0.0f;
            if (burstAt >= 0.0 && t >= burstAt && t < burstAt + 0.3)
                v = 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * 1000.0 * (t - burstAt));
            s->buffer.setSample (0, i, v);
            s->buffer.setSample (1, i, v);
        }
        return s;
    }

    /** ブロックごとに鳴らして左 ch をつなげる */
    std::vector<float> play (PlaybackCore& core, int total, int block = 512)
    {
        std::vector<float> out;
        out.reserve ((size_t) total);
        for (int done = 0; done < total; done += block)
        {
            auto b = render (core, juce::jmin (block, total - done));
            for (int i = 0; i < b.b.getNumSamples(); ++i)
                out.push_back (b.l (i));
        }
        return out;
    }

    /** 上向きのゼロ交差の間隔から周波数（from 以降） */
    double frequency (const std::vector<float>& x, size_t from, double rate)
    {
        double first = -1.0, last = -1.0;
        int crossings = 0;
        for (size_t i = from + 1; i < x.size(); ++i)
            if (x[i - 1] < 0.0f && x[i] >= 0.0f)
            {
                const auto frac = x[i - 1] / (x[i - 1] - x[i]);
                const auto at = (double) (i - 1) + frac;
                if (first < 0.0) first = at;
                last = at;
                ++crossings;
            }
        return crossings > 1 ? (crossings - 1) * rate / (last - first) : 0.0;
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

        beginTest ("practice: speed changes position, not pitch (B11)");
        {
            PlaybackCore core;
            core.setSong (toneSong (440.0, 6.0, 48000.0));
            core.prepare (48000.0);
            core.setPractice (0.8, 0);
            expect (core.isPracticeShifted());
            core.play();
            const auto out = play (core, 96000);            // 2 秒
            expectWithinAbsoluteError ((double) core.getPosition(), 96000.0 * 0.8, 2.0);
            expectWithinAbsoluteError (frequency (out, 24000, 48000.0), 440.0, 2.0);
        }

        beginTest ("practice: key shifts pitch, not position (B11)");
        {
            PlaybackCore core;
            core.setSong (toneSong (440.0, 6.0, 48000.0));
            core.prepare (48000.0);
            core.setPractice (1.0, 2);
            core.play();
            const auto out = play (core, 96000);
            expectWithinAbsoluteError ((double) core.getPosition(), 96000.0, 2.0);
            expectWithinAbsoluteError (frequency (out, 24000, 48000.0), 440.0 * std::pow (2.0, 2.0 / 12.0), 3.0);
        }

        beginTest ("practice: what is heard lines up with the reported position (B11)");
        {
            // 1.5 秒に音の頭がある曲を 0.75 倍・+3 で鳴らす：出力で頭が出るのは 2.0 秒、その時の位置は 1.5 秒
            // 曲 44.1 kHz をデバイス 48 kHz で鳴らす組も（SR の変換をストレッチの比に含める）
            struct Case { double speed; int key; double songRate; };
            for (auto c : { Case { 0.75, 3, 48000.0 }, Case { 1.25, -2, 48000.0 }, Case { 0.9, 0, 44100.0 } })
            {
                PlaybackCore core;
                core.setSong (toneSong (0.0, 4.0, c.songRate, 1.5));
                core.prepare (48000.0);
                core.setPractice (c.speed, c.key);
                core.play();
                const auto out = play (core, (int) (48000 * 1.5 / c.speed + 24000));
                size_t onset = 0;
                for (size_t i = 0; i < out.size(); ++i)
                    if (std::abs (out[i]) > 0.1f) { onset = i; break; }
                const auto expected = 1.5 / c.speed * 48000.0;
                const auto errorMs = ((double) onset - expected) / 48.0;
                logMessage ("    x" + juce::String (c.speed) + " " + juce::String (c.key) + " (" + juce::String (c.songRate / 1000.0) + " kHz): onset error "
                            + juce::String (errorMs, 2) + " ms");
                expect (std::abs (errorMs) < 12.0, juce::String (errorMs) + " ms");
            }
        }

        beginTest ("practice: seek, loop and end (B11)");
        {
            PlaybackCore core;
            core.setSong (toneSong (440.0, 2.0, 48000.0));
            core.prepare (48000.0);
            core.setPractice (1.25, 0);
            core.play();
            core.seek (48000);
            play (core, 4800);
            expectWithinAbsoluteError ((double) core.getPosition(), 48000.0 + 4800 * 1.25, 2.0);

            core.setLoop (48000, 60000, true);
            core.seek (55000);
            play (core, 48000);                               // 何周もする
            expect (core.getPosition() >= 48000 && core.getPosition() < 60000, juce::String (core.getPosition()));
            expect (core.isPlaying());

            core.setLoop (0, 0, false);
            core.seek (90000);
            play (core, 48000);                               // 曲の終わり（96000）で止まる
            expect (! core.isPlaying());
            expect (core.consumeReachedEnd());
            expectEquals (core.getPosition(), (juce::int64) 96000);
        }

        beginTest ("practice: back to original speed and key is sample-exact again (B11)");
        {
            PlaybackCore core;
            core.setSong (rampSong (2, 200000, 48000.0));
            core.prepare (48000.0);
            core.setPractice (0.9, 1);
            core.play();
            play (core, 4800);
            core.setPractice (1.0, 0);
            expect (! core.isPracticeShifted());
            const auto at = core.getPosition();
            render (core, 2048);                               // 切り替えのフェード（20 ms）が終わるまで
            auto b = render (core, 64);
            expectEquals (core.getPosition(), at + 2048 + 64);
            expectEquals (b.l (63), (float) (at + 2048 + 63));  // 素通し：鳴らした値 = 曲の位置
        }

        beginTest ("practice: realtime cost (R3)");
        {
            PlaybackCore core;
            core.setSong (toneSong (440.0, 12.0, 48000.0));
            core.prepare (48000.0);
            core.setPractice (0.8, -3);
            core.play();
            const auto t0 = juce::Time::getMillisecondCounterHiRes();
            play (core, 48000 * 5, 256);
            const auto ms = juce::Time::getMillisecondCounterHiRes() - t0;
            logMessage ("    Rubber Band R3 stereo 48 kHz x0.8 -3: " + juce::String (ms / 5000.0 * 100.0, 1) + " % of real time");
            expect (ms < 5000.0, juce::String (ms) + " ms for 5 s");
        }

        beginTest ("recorded tracks play at the same position as the song (B12)");
        {
            PlaybackCore core;
            core.setSong (rampSong (2, 10000, 48000.0));
            core.prepare (48000.0);
            auto stem = std::make_shared<juce::AudioBuffer<float>> (1, 10000);
            for (int i = 0; i < 10000; ++i)
                stem->setSample (0, i, 1000.0f * (float) i);           // 曲の値と区別できる
            core.setStem (2, stem);
            core.setStemGain (2, 1.0f);
            core.play();
            core.seek (100);
            auto b = render (core, 16);
            expectEquals (b.l (0), 100.0f + 100000.0f);                // 曲 + トラック（同じ位置）
            expectEquals (b.r (5), 105.25f + 105000.0f);               // 両耳に同じトラック

            core.setStemGain (2, 0.0f);                                 // 20 ms でなめらかに消える
            render (core, 1200);
            core.seek (10);
            auto c = render (core, 4);
            expectEquals (c.l (0), 10.0f);

            core.setStem (2, nullptr);
            core.setStemGain (2, 1.0f);
            render (core, 960);
            core.seek (20);
            expectEquals (render (core, 1).l (0), 20.0f);
        }

        beginTest ("recorded tracks follow the practice tempo (B12)");
        {
            PlaybackCore core;
            core.setSong (toneSong (0.0, 4.0, 48000.0, 1.5));          // 伴奏は 1.5 秒の頭だけ
            core.prepare (48000.0);
            auto stem = std::make_shared<juce::AudioBuffer<float>> (1, 4 * 48000);
            stem->clear();
            for (int i = 0; i < 9600; ++i)                              // トラックは 2.5 秒から 0.2 秒の 3 kHz
                stem->setSample (0, 120000 + i, 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * 3000.0 * i / 48000.0));
            core.setStem (0, stem);
            core.setStemGain (0, 1.0f);
            core.setPractice (0.8, 0);
            core.play();
            const auto out = play (core, (int) (48000 * 2.5 / 0.8 + 12000));
            size_t first = 0, second = 0;
            for (size_t i = 0; i < out.size(); ++i)
                if (std::abs (out[i]) > 0.1f) { if (first == 0) first = i; else if (i > first + 24000) { second = i; break; } }
            expectWithinAbsoluteError ((double) first / 48000.0, 1.5 / 0.8, 0.012);
            expectWithinAbsoluteError ((double) second / 48000.0, 2.5 / 0.8, 0.012);
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
