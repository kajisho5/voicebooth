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
    }
};

static TakeStatsTests takeStatsTests;
} // namespace vb::analysis
