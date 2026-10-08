#include "analysis/TakeStats.h"
#include <cmath>

namespace vb::analysis
{
class TakeStatsTests : public juce::UnitTest
{
public:
    TakeStatsTests() : juce::UnitTest ("TakeStats", "VoiceBooth") {}

    static constexpr double rate = 48000.0;
    static juce::int64 ms (double v) { return (juce::int64) std::llround (v * rate / 1000.0); }

    /** 音符（開始 ms、長さ ms、midi）の並び → 10 ms ごとの点。休みの所は点を出さない（無声） */
    struct Note { double startMs, lengthMs; float midi; };
    static std::vector<audio::PitchFrame> frames (const std::vector<Note>& notes, double shiftMs = 0.0, float addMidi = 0.0f,
                                                  float vibHz = 0.0f, float vibCents = 0.0f)
    {
        std::vector<audio::PitchFrame> out;
        for (int t = 0; t < 12000; t += 10)
        {
            audio::PitchFrame f;
            f.songSample = ms (t);
            for (auto& n : notes)
            {
                const auto s = n.startMs + shiftMs;
                if (t >= s && t < s + n.lengthMs)
                {
                    const auto v = vibHz > 0.0f ? vibCents / 100.0f * (float) std::sin (juce::MathConstants<double>::twoPi * vibHz * (t - s) / 1000.0) : 0.0f;
                    f.midi = n.midi + addMidi + v;
                    f.confidence = 0.9f;
                }
            }
            out.push_back (f);
        }
        return out;
    }

    void runTest() override
    {
        // 自作の旋律：3 つのフレーズ（休みで区切る）
        const std::vector<Note> melody {
            { 1000, 400, 64 }, { 1400, 400, 67 }, { 1800, 600, 69 },     // フレーズ 1（入りは 1000 ms）
            { 3000, 300, 72 }, { 3300, 300, 71 }, { 3600, 800, 69 },     // フレーズ 2（入りは 3000 ms）
            { 5500, 1200, 67 },                                         // フレーズ 3（入りは 5500 ms、長い音）
        };

        beginTest ("notes are split at pitch changes and rests");
        {
            const auto notes = segmentNotes (frames (melody), rate);
            expectEquals ((int) notes.size(), 7);
            expectWithinAbsoluteError ((double) notes[0].start, (double) ms (1000), (double) ms (10));
            expectWithinAbsoluteError ((double) notes[1].start, (double) ms (1400), (double) ms (10));
            expectWithinAbsoluteError (notes[2].midi, 69.0f, 0.01f);
            expectWithinAbsoluteError ((double) notes[6].start, (double) ms (5500), (double) ms (10));
        }

        beginTest ("entries: a take that comes in 40 ms late everywhere, one phrase an octave down");
        {
            auto take = frames (melody, 40.0);
            for (auto& f : take)   // フレーズ 2 だけオクターブ下で歌った
                if (f.songSample >= ms (3000) && f.songSample < ms (4500) && f.midi > 0.0f) f.midi -= 12.0f;
            const auto s = onsetStats (frames (melody), take, rate, 0, ms (12000));
            expectEquals (s.entries, 3);
            expectEquals ((int) s.items.size(), 3);
            expectWithinAbsoluteError (s.medianMs, 40.0, 10.0);
            for (auto& it : s.items)
                expectWithinAbsoluteError (it.offsetMs, 40.0, 10.0);
        }

        beginTest ("entries: a short blip or the end of the last phrase just before an entry isn't taken as the entry");
        {
            auto take = frames (melody, 40.0);
            for (auto& f : take)   // 入りの 200 ms 前に 30 ms だけ同じ音の息、2 つ目の入りの 150 ms 前に前のフレーズの残り（同じ音で 120 ms）
            {
                if (f.songSample >= ms (800) && f.songSample < ms (830))   { f.midi = 64.0f; f.confidence = 0.9f; }
                if (f.songSample >= ms (2730) && f.songSample < ms (2850)) { f.midi = 72.0f; f.confidence = 0.9f; }
            }
            const auto s = onsetStats (frames (melody), take, rate, 0, ms (12000));
            expectEquals ((int) s.items.size(), 3);
            expectWithinAbsoluteError (s.items[0].offsetMs, 40.0, 10.0);   // 息（30 ms）は入りにしない
            expectWithinAbsoluteError (s.items[1].offsetMs, 40.0, 10.0);   // 近い方（本当の入り）を取る
            expectWithinAbsoluteError (s.medianMs, 40.0, 10.0);
        }

        beginTest ("entries: early, and a phrase that wasn't sung is left out");
        {
            auto take = frames ({ melody[0], melody[1], melody[2], melody[6] }, -60.0);   // フレーズ 2 を歌っていない、60 ms 早い
            const auto s = onsetStats (frames (melody), take, rate, 0, ms (12000));
            expectEquals (s.entries, 3);
            expectEquals ((int) s.items.size(), 2);
            expectWithinAbsoluteError (s.medianMs, -60.0, 10.0);
            const auto onlyFirst = onsetStats (frames (melody), take, rate, 0, ms (2500));   // 範囲
            expectEquals (onlyFirst.entries, 1);
        }

        beginTest ("pitch accuracy: 20 cents sharp is in a 30-cent band, not in a 10-cent band; octave is the same note");
        {
            const auto guide = frames (melody);
            auto a = pitchAccuracy (guide, frames (melody, 0.0, 0.2f), rate, 0, ms (12000), 30.0f);
            expect (a.frames > 300, juce::String (a.frames));
            expectWithinAbsoluteError (a.inBand, 1.0f, 0.001f);
            expectWithinAbsoluteError (a.meanAbsCents, 20.0f, 0.5f);
            a = pitchAccuracy (guide, frames (melody, 0.0, 0.2f), rate, 0, ms (12000), 10.0f);
            expectWithinAbsoluteError (a.inBand, 0.0f, 0.001f);
            a = pitchAccuracy (guide, frames (melody, 0.0, -12.0f), rate, 0, ms (12000), 10.0f);
            expectWithinAbsoluteError (a.inBand, 1.0f, 0.001f);
        }

        beginTest ("pitch accuracy follows octave matching: off, an octave down is wrong (#25)");
        {
            const auto guide = frames (melody);
            auto a = pitchAccuracy (guide, frames (melody, 0.0, -12.0f), rate, 0, ms (12000), 30.0f, false);
            expectWithinAbsoluteError (a.inBand, 0.0f, 0.001f);
            expectWithinAbsoluteError (a.meanAbsCents, 1200.0f, 1.0f);
            a = pitchAccuracy (guide, frames (melody, 0.0, 0.2f), rate, 0, ms (12000), 30.0f, false);   // オクターブが同じなら変わらない
            expectWithinAbsoluteError (a.inBand, 1.0f, 0.001f);
        }

        beginTest ("the yellow band stays wider than the green band (#25)");
        {
            expectEquals (pitchWarnLimitCents (20.0f), 50.0f);
            expectEquals (pitchWarnLimitCents (30.0f), 50.0f);   // 既定（30）は今までどおり ±50 まで黄
            expectEquals (pitchWarnLimitCents (50.0f), 70.0f);   // 50 にしても黄が残る
        }

        beginTest ("vibrato: 6 Hz, 40 cents on the long note only");
        {
            const auto v = vibratos (frames ({ { 1000, 300, 64 }, { 5500, 1200, 67 } }, 0.0, 0.0f, 6.0f, 40.0f), rate);
            expectEquals ((int) v.size(), 1);
            if (! v.empty())
            {
                logMessage ("    vibrato " + juce::String (v[0].rateHz, 2) + " Hz, " + juce::String (v[0].depthCents, 1) + " cents");
                expectWithinAbsoluteError (v[0].rateHz, 6.0f, 0.5f);
                expectWithinAbsoluteError (v[0].depthCents, 40.0f, 8.0f);
            }
            expect (vibratos (frames (melody), rate).empty());   // まっすぐな音には無い
        }

        beginTest ("vibrato depth does not depend on its rate (4, 5 and 7.5 Hz, 40 cents)");
        {
            for (auto hz : { 4.0f, 5.0f, 7.5f })
            {
                const auto v = vibratos (frames ({ { 1000, 2000, 67 } }, 0.0, 0.0f, hz, 40.0f), rate);
                expectEquals ((int) v.size(), 1, juce::String (hz) + " Hz");
                if (! v.empty())
                {
                    expectWithinAbsoluteError (v[0].rateHz, hz, 0.6f);
                    expectWithinAbsoluteError (v[0].depthCents, 40.0f, 8.0f, juce::String (hz) + " Hz: " + juce::String (v[0].depthCents, 1));
                }
            }
        }

        // 苦手な小節（Phase C）：1 小節 1 秒の線で 12 小節。お手本は 0〜12 秒ずっと 67
        std::vector<juce::int64> barLines;
        for (int b = 0; b <= 12; ++b)
            barLines.push_back (ms (b * 1000.0));
        const auto steady = frames ({ { 0, 12000, 67 } });

        beginTest ("weak spots: the two bars sung 60 cents sharp come first, then the next one that doesn't overlap");
        {
            // 4〜6 秒（小節 4・5）と 8〜9 秒（小節 8）を 60 セント高く
            const auto take = frames ({ { 0, 4000, 67 }, { 4000, 2000, 67.6f }, { 6000, 2000, 67 }, { 8000, 1000, 67.6f }, { 9000, 3000, 67 } });
            const auto w = weakSpans (steady, take, rate, barLines, 30.0f);
            expectEquals ((int) w.size(), 2);
            if (w.size() == 2)
            {
                expectEquals (w[0].start, ms (4000));
                expectEquals (w[0].end, ms (6000));
                expectEquals (w[0].firstBar, 4);
                expectWithinAbsoluteError (w[0].inBand, 0.0f, 0.01f);
                // 小節 8 を含む 2 小節（7〜8 と 8〜9 は同じ 50%）：前のほう
                expectEquals (w[1].start, ms (7000));
                expectEquals (w[1].end, ms (9000));
                expectWithinAbsoluteError (w[1].inBand, 0.5f, 0.02f);
            }
        }

        beginTest ("weak spots: at the same share, the span with more singing comes first");
        {
            // 小節 3 は歌わず、小節 4・5 を高く：3〜4（歌は 1 小節）・4〜5（2 小節）・5〜6（1 小節。6 は歌わない）がどれも 0%
            const auto take = frames ({ { 0, 3000, 67 }, { 4000, 2000, 67.6f } });
            const auto w = weakSpans (steady, take, rate, barLines, 30.0f);
            expect (! w.empty());
            if (! w.empty())
            {
                expectEquals (w[0].start, ms (4000), "the two bars that were both sung sharp");
                expectEquals (w[0].frames, 200);
            }
        }

        beginTest ("weak spots: none when every two bars are in band, too little singing, or the bar lines are missing");
        {
            expect (weakSpans (steady, frames ({ { 0, 12000, 67.1f } }), rate, barLines, 30.0f).empty(), "10 cents off is in a 30-cent band");
            expect (weakSpans (steady, frames ({ { 5000, 300, 68 } }), rate, barLines, 30.0f).empty(), "300 ms of singing is too little to judge");
            expect (weakSpans (steady, frames ({ { 0, 12000, 68 } }), rate, { ms (0) }, 30.0f).empty(), "no bars");
            expect (weakSpans ({}, frames ({ { 0, 12000, 68 } }), rate, barLines, 30.0f).empty(), "no guide");
        }

        beginTest ("weak spots follow octave matching (#25)");
        {
            const auto octaveDown = frames ({ { 0, 12000, 55 } });
            expect (weakSpans (steady, octaveDown, rate, barLines, 30.0f, true).empty(), "an octave down is the same note");
            const auto w = weakSpans (steady, octaveDown, rate, barLines, 30.0f, false);
            expectEquals ((int) w.size(), 6, "with matching off, every pair of bars is wrong; six that don't overlap");
        }
    }
};

static TakeStatsTests takeStatsTests;
} // namespace vb::analysis
