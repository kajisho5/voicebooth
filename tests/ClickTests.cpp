#include "audio/PlaybackCore.h"
#include "audio/Metronome.h"
#include "audio/MonitorLevel.h"
#include "song/SongInfo.h"

/*  クリック（メトロノーム）・カウントイン・モニターの帯のメーター（2026-10-02）
    クリックは出力の側で作るので、出力のどのサンプルで鳴ったかを直接数えて確かめる */

namespace vb::audio
{
namespace
{
    constexpr double rate = 48000.0;

    /** 同じ値がずっと続く曲（0 なら無音。クリックだけが出力に残る） */
    std::shared_ptr<const SongAudio> constantSong (float value, double seconds, double sampleRate = rate)
    {
        auto s = std::make_shared<SongAudio>();
        s->sampleRate = sampleRate;
        s->buffer.setSize (2, (int) (seconds * sampleRate));
        for (int c = 0; c < 2; ++c)
            juce::FloatVectorOperations::fill (s->buffer.getWritePointer (c), value, s->buffer.getNumSamples());
        return s;
    }

    /** 無音の曲で、at 秒から 0.3 秒だけ 440 Hz（頭の位置を見る） */
    std::shared_ptr<const SongAudio> burstSong (double seconds, double at)
    {
        auto s = std::make_shared<SongAudio>();
        s->sampleRate = rate;
        const auto n = (int) (seconds * rate);
        s->buffer.setSize (2, n);
        s->buffer.clear();
        for (int i = (int) (at * rate); i < juce::jmin (n, (int) ((at + 0.3) * rate)); ++i)
        {
            const auto v = 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * 440.0 * (i / rate - at) + 0.5);
            s->buffer.setSample (0, i, v);
            s->buffer.setSample (1, i, v);
        }
        return s;
    }

    struct Output
    {
        std::vector<float> left;
        std::vector<PlaybackCore::Rendered> blocks;
    };

    /** ブロックごとに鳴らして左 ch をつなげる（各ブロックの Rendered も残す） */
    Output play (PlaybackCore& core, int total, int block = 512)
    {
        Output o;
        juce::AudioBuffer<float> b (2, block);
        for (int done = 0; done < total; done += block)
        {
            const auto n = juce::jmin (block, total - done);
            o.blocks.push_back (core.render (b.getArrayOfWritePointers(), 2, n));
            for (int i = 0; i < n; ++i)
                o.left.push_back (b.getSample (0, i));
        }
        return o;
    }

    /** 音の頭：大きさが threshold を越えたサンプルのうち、直前 quiet サンプルの間は threshold を越えていない所 */
    std::vector<int> onsets (const std::vector<float>& x, float threshold = 0.05f, int quiet = 200, float base = 0.0f)
    {
        std::vector<int> found;
        int lastLoud = -1000000;
        for (int i = 0; i < (int) x.size(); ++i)
        {
            const auto a = std::abs (x[(size_t) i] - base);
            if (a > threshold && i - lastLoud > quiet)
                found.push_back (i);
            if (a > threshold)
                lastLoud = i;
        }
        return found;
    }

    /** 上向きのゼロ交差の間隔から周波数（[from, to)） */
    double frequency (const std::vector<float>& x, int from, int to)
    {
        double first = -1.0, last = -1.0;
        int crossings = 0;
        for (int i = from + 1; i < to; ++i)
            if (x[(size_t) i - 1] < 0.0f && x[(size_t) i] >= 0.0f)
            {
                const auto at = (double) (i - 1) + x[(size_t) i - 1] / (x[(size_t) i - 1] - x[(size_t) i]);
                if (first < 0.0) first = at;
                last = at;
                ++crossings;
            }
        return crossings > 1 ? (crossings - 1) * rate / (last - first) : 0.0;
    }

    juce::String list (const std::vector<int>& v)
    {
        juce::StringArray a;
        for (auto x : v) a.add (juce::String (x));
        return a.joinIntoString (" ");
    }
}

class ClickTests : public juce::UnitTest
{
public:
    ClickTests() : juce::UnitTest ("Click / count-in / monitor meters", "VoiceBooth") {}

    void runTest() override
    {
        beginTest ("beat positions use the same rounding as the ruler (song::beatSample)");
        {
            song::TempoInfo t;
            t.bpm = 123.45;
            t.downbeatSample = 1234;
            t.signature = { 3, 4 };
            const auto sr = 44100.0;
            const Metronome::Grid g { t.samplesPerBeat (sr), t.downbeatSample, t.signature.beatsPerBar() };
            for (juce::int64 k = -20; k <= 400; ++k)
            {
                const auto at = song::beatSample (t, k, sr);
                expectEquals (g.beatSample (k), at);
                expectEquals (g.firstBeatFrom ((double) at), k);              // ちょうど拍の上はその拍
                expectEquals (g.firstBeatFrom ((double) at + 0.5), k + 1);    // 少しでも過ぎたら次
                expect (g.isDownbeat (k) == (song::barBeatAt (t, at, sr).beat == 1));
            }
        }

        beginTest ("click on: every beat at its exact output sample, downbeat accented");
        {
            PlaybackCore core;
            core.setSong (constantSong (0.0f, 4.0));
            core.prepare (rate);
            core.setClickGrid ({ 24000.0, 6000, 3 });   // 120 BPM・3/4・1 小節目は 0.125 秒
            core.setClick (true, 1.0f);
            core.play();
            const auto out = play (core, (int) (3.5 * rate));
            const auto found = onsets (out.left);
            const std::vector<int> expected { 6000, 30000, 54000, 78000, 102000, 126000, 150000 };
            expect (found == expected, list (found));
            for (size_t k = 0; k < found.size() && k < expected.size(); ++k)
            {
                const bool down = k % 3 == 0;
                expectWithinAbsoluteError (out.left[(size_t) found[k]], down ? Metronome::accentPeak : Metronome::beatPeak, 1.0e-6f);
            }
            // 拍の間は無音（音は約 40 ms で消える）
            expectEquals (out.left[6000 + 2400], 0.0f);
        }

        beginTest ("click level follows the fader, off is silent");
        {
            PlaybackCore core;
            core.setSong (constantSong (0.0f, 2.0));
            core.prepare (rate);
            core.setClickGrid ({ 24000.0, 0, 4 });
            core.setClick (true, PlaybackCore::faderToGain (0.375f));   // -12 dB
            core.play();
            auto out = play (core, 30000);
            expectWithinAbsoluteError (out.left[0], Metronome::accentPeak * PlaybackCore::faderToGain (0.375f), 1.0e-6f);

            core.setClick (false, 1.0f);
            out = play (core, 48000);
            expect (onsets (out.left).empty());
        }

        beginTest ("loop and seek: clicks restart from the new position");
        {
            PlaybackCore core;
            core.setSong (constantSong (0.0f, 10.0));
            core.prepare (rate);
            core.setClickGrid ({ 24000.0, 0, 4 });
            core.setClick (true, 1.0f);
            core.setLoop (96000, 144000, true);      // 2 小節目の頭から 1 小節（拍の上で戻る）
            core.seek (120000);
            core.play();
            const auto found = onsets (play (core, 48000).left);
            // 120000（拍）・ループの終わり 144000 で 96000 へ（1 拍目）・120000
            expect (found == std::vector<int> { 0, 24000 }, list (found));
        }

        beginTest ("practice tempo: beats follow the stretched song, the click keeps its pitch");
        {
            PlaybackCore core;
            core.setSong (constantSong (0.0f, 8.0));
            core.prepare (rate);
            core.setPractice (0.8, 3);
            core.setClickGrid ({ 24000.0, 0, 4 });
            core.setClick (true, 1.0f);
            core.seek (48000);
            core.play();
            const auto out = play (core, 100000);
            const auto found = onsets (out.left);
            // 曲の拍 48000, 72000, 96000, 120000 → 出力では 0.8 倍の速さなので 30000 ずつ
            expectEquals ((int) found.size(), 4, list (found));
            for (size_t k = 0; k < found.size(); ++k)
                expect (std::abs (found[k] - 30000 * (int) k) <= 1, list (found));
            // キー +3 でもクリックの高さは変わらない（伸ばした後に足しているので）
            if (found.size() >= 2)
                expectWithinAbsoluteError (frequency (out.left, found[1], found[1] + 1800), (double) Metronome::beatHz, 15.0);
        }

        beginTest ("practice tempo: the click lands on the beat of the song that is heard");
        {
            // 1.5 秒（3 拍目）に音の頭がある曲を 0.75 倍で鳴らす。クリックを足した出力と足さない出力の差がクリック
            auto render = [] (bool click)
            {
                PlaybackCore core;
                core.setSong (burstSong (3.0, 1.5));
                core.prepare (rate);
                core.setPractice (0.75, 0);
                core.setClickGrid ({ 24000.0, 0, 4 });
                core.setClick (click, 1.0f);
                core.seek (60000);                // 1.25 秒から（次の拍は 1.5 秒）
                core.play();
                return play (core, 48000).left;
            };
            const auto with = render (true), without = render (false);
            std::vector<float> clickOnly (with.size());
            for (size_t i = 0; i < with.size(); ++i)
                clickOnly[i] = with[i] - without[i];
            const auto clicks = onsets (clickOnly), song = onsets (without, 0.1f);
            expect (! clicks.empty() && ! song.empty(), "clicks " + list (clicks) + " / song " + list (song));
            if (! clicks.empty() && ! song.empty())
            {
                const auto errorMs = (clicks[0] - song[0]) / 48.0;
                logMessage ("    click vs song onset at x0.75: " + juce::String (errorMs, 2) + " ms");
                expectEquals (clicks[0], 16000);            // (72000 - 60000) / 0.75
                expect (std::abs (errorMs) < 12.0, juce::String (errorMs) + " ms");
            }
        }

        beginTest ("count-in: N bars of clicks before the song, then the song from the start position");
        {
            for (int bars : { 1, 2 })
            {
                PlaybackCore core;
                core.setSong (constantSong (0.1f, 4.0));
                core.prepare (rate);
                core.setGain (1.0f);
                core.setClickGrid ({ 12000.0, 0, 4 });   // 240 BPM・4/4：1 小節 48000
                core.setClick (false, 1.0f);              // クリック Off でもカウントインは鳴る
                core.seek (96000);                        // 3 小節目の頭から録る
                song::TempoInfo t;
                t.bpm = 240.0;
                const auto from = song::countInStart (t, 96000, bars, rate);
                expectEquals (from, (juce::int64) (96000 - 48000 * bars));
                core.play (96000 - from, 96000);
                expect (core.isCountingIn());

                const auto lead = (int) (96000 - from);
                const auto out = play (core, lead + 24000);
                expectEquals (core.getPosition(), (juce::int64) 96000 + 24000);

                // 数えている間は曲が鳴らない（クリックだけ）。1 拍目は高い音
                std::vector<float> counted (out.left.begin(), out.left.begin() + lead);
                const auto found = onsets (counted);
                expectEquals ((int) found.size(), 4 * bars, list (found));
                for (size_t k = 0; k < found.size(); ++k)
                {
                    expectEquals (found[k], 12000 * (int) k);
                    expectWithinAbsoluteError (std::abs (counted[(size_t) found[k]]), k % 4 == 0 ? Metronome::accentPeak : Metronome::beatPeak, 1.0e-6f);
                }
                expectEquals (out.left[(size_t) lead - 1], 0.0f);
                // 数え終わったサンプルから曲（クリックは Off なので、次の 1 拍目は鳴らない）
                expectEquals (out.left[(size_t) lead], 0.1f);
                expectEquals (out.left[(size_t) lead + 5], 0.1f);

                // 曲が始まったブロック：lead サンプル目から鳴らした（録音・ピッチは入力をそこから合わせる）
                int seen = 0;
                for (auto& b : out.blocks)
                    if (b.lead > 0 && b.lead < 512)
                    {
                        ++seen;
                        expectEquals (b.lead, lead % 512);
                        expectEquals (b.start, (juce::int64) 96000);
                        expectEquals (b.played, 512 - b.lead);
                    }
                expectEquals (seen, 1);
                expect (! core.isCountingIn());
            }
        }

        beginTest ("count-in at a practice tempo: the bar is stretched like the song");
        {
            PlaybackCore core;
            core.setSong (constantSong (0.0f, 4.0));
            core.prepare (rate);
            core.setPractice (0.8, 0);
            core.setClickGrid ({ 12000.0, 0, 4 });
            core.setClick (true, 1.0f);
            core.play (48000, 0);                       // 曲の頭から、1 小節（曲の頭より前）を数える
            const auto out = play (core, 70000);
            const auto found = onsets (out.left);
            // 拍は 12000 / 0.8 = 15000 ずつ。数え終わる 60000 は曲の 1 拍目（クリック On なので鳴る）
            expect (found.size() >= 5, list (found));
            for (size_t k = 0; k < juce::jmin<size_t> (5, found.size()); ++k)
                expect (std::abs (found[k] - 15000 * (int) k) <= 1, list (found));
            int start = -1;
            for (size_t b = 0, at = 0; b < out.blocks.size(); at += 512, ++b)
                if (out.blocks[b].lead > 0 && out.blocks[b].lead < 512)
                    start = (int) at + out.blocks[b].lead;
            expect (std::abs (start - 60000) <= 1, juce::String (start));
        }

        beginTest ("count-in start: always from beat 1 of a bar");
        {
            song::TempoInfo t;
            t.bpm = 120.0;
            t.downbeatSample = 1000;
            const auto bar = [&] (juce::int64 b) { return song::barSample (t, b, rate); };
            const auto beat = t.samplesPerBeat (rate);
            expectEquals (song::countInStart (t, bar (5), 1, rate), bar (4));                       // 小節線ちょうど：ちょうど 1 小節
            expectEquals (song::countInStart (t, bar (5), 2, rate), bar (3));
            expectEquals (song::countInStart (t, bar (5) + (juce::int64) (2 * beat), 1, rate), bar (4));   // 3 拍目から：その小節の頭の 1 小節前
            expectEquals (song::countInStart (t, bar (6) - (juce::int64) (0.1 * beat), 1, rate), bar (5)); // 小節線の少し手前：次の小節とみなす
            expectEquals (song::countInStart (t, 500, 1, rate), bar (0));                           // 1 小節目より前（弱起）
            expectEquals (song::countInStart (t, bar (5), 0, rate), bar (5));                       // Off
            t.bpm = 0.0;
            expectEquals (song::countInStart (t, 77777, 2, rate), (juce::int64) 77777);              // テンポが分からない
        }

        beginTest ("meter math: dB to LED position, 20 dB/s fall");
        {
            expectEquals (meterFraction (0.0f), 1.0f);
            expectEquals (meterFraction (3.0f), 1.0f);
            expectEquals (meterFraction (-24.0f), 0.5f);
            expectEquals (meterFraction (-48.0f), 0.0f);
            expectEquals (meterFraction (-100.0f), 0.0f);

            LevelFollower f;
            f.prepare (rate);
            expectEquals (f.readDb(), LevelFollower::floorDb);
            f.push (0.5f, 480);
            expectWithinAbsoluteError (f.readDb(), -6.02f, 0.01f);
            for (int i = 0; i < 100; ++i)
                f.push (0.0f, 480);                     // 1 秒
            expectWithinAbsoluteError (f.readDb(), -26.02f, 0.05f);
            f.push (0.25f, 480);                        // 上がる時はすぐ
            expectWithinAbsoluteError (f.readDb(), -12.04f, 0.01f);
            f.push (std::numeric_limits<float>::quiet_NaN(), 480);   // 壊れた値で止まらない
            expect (std::isfinite (f.readDb()));
        }

        beginTest ("monitor meters: backing, guide and click are measured after their faders");
        {
            PlaybackCore core;
            core.setSong (constantSong (0.5f, 2.0));
            auto guide = std::make_shared<juce::AudioBuffer<float>> (1, (int) (2.0 * rate));
            juce::FloatVectorOperations::fill (guide->getWritePointer (0), 0.25f, guide->getNumSamples());
            core.setStem (PlaybackCore::guideSlot, guide);
            core.setGain (PlaybackCore::faderToGain (0.375f));          // -12 dB
            core.setStemGain (PlaybackCore::guideSlot, 0.5f);           // -6 dB
            core.prepare (rate);
            core.setClickGrid ({ 24000.0, 0, 4 });
            core.setClick (true, PlaybackCore::faderToGain (0.75f));    // 0 dB
            core.play();
            play (core, 512);
            auto l = core.getLevels();
            const auto fader12 = juce::Decibels::gainToDecibels (PlaybackCore::faderToGain (0.375f));
            expectWithinAbsoluteError (l.backingDb, -6.02f + fader12, 0.02f);
            expectWithinAbsoluteError (l.guideDb, -12.04f - 6.02f, 0.02f);
            expectWithinAbsoluteError (l.clickDb, -6.02f, 0.02f);   // 1 拍目の頭（0 dB で 0.5）

            // ミュート・音量 0：その帯だけ下がっていく（フェーダーの後で測っている）
            core.setMuted (true);
            core.setStemGain (PlaybackCore::guideSlot, 0.0f);
            play (core, 48000);
            l = core.getLevels();
            expect (l.backingDb < -30.0f, juce::String (l.backingDb));
            expect (l.guideDb < -30.0f, juce::String (l.guideDb));

            // 止まったらどれも下がる
            core.stop();
            play (core, 96000 * 3);
            l = core.getLevels();
            expectEquals (l.backingDb, LevelFollower::floorDb);
            expectEquals (l.clickDb, LevelFollower::floorDb);
        }
    }
};

static ClickTests clickTests;
} // namespace vb::audio
