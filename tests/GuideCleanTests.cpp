#include "analysis/GuideClean.h"
#include <cmath>

namespace vb::analysis
{
class GuideCleanTests : public juce::UnitTest
{
public:
    GuideCleanTests() : juce::UnitTest ("GuideClean", "VoiceBooth") {}

    static constexpr double rate = 48000.0;
    static constexpr juce::int64 hop = 480;   // 10 ms

    /** 10 ms ごとの点。midi 0 = 無声 */
    static std::vector<audio::PitchFrame> frames (const std::vector<float>& midi)
    {
        std::vector<audio::PitchFrame> out;
        for (size_t i = 0; i < midi.size(); ++i)
        {
            audio::PitchFrame f;
            f.songSample = (juce::int64) i * hop;
            f.midi = midi[i];
            f.confidence = midi[i] > 0.0f ? 0.9f : 0.0f;
            out.push_back (f);
        }
        return out;
    }

    static void add (std::vector<float>& v, float midi, int count) { v.insert (v.end(), (size_t) count, midi); }

    static int countNear (const std::vector<audio::PitchFrame>& f, float midi, float tol = 0.5f)
    {
        int c = 0;
        for (auto& p : f)
            c += p.confidence >= 0.5f && std::abs (p.midi - midi) <= tol ? 1 : 0;
        return c;
    }

    void runTest() override
    {
        beginTest ("vibrato depth and centre survive");
        {
            std::vector<float> v;
            add (v, 0.0f, 10);
            for (int i = 0; i < 100; ++i)
                v.push_back (60.0f + 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * 6.0 * i * 0.01));
            add (v, 0.0f, 10);
            const auto out = cleanGuideContour (frames (v), rate);
            float lo = 99.0f, hi = 0.0f;
            for (int i = 20; i < 100; ++i) { lo = std::min (lo, out[(size_t) i].midi); hi = std::max (hi, out[(size_t) i].midi); }
            expectGreaterThan (hi - lo, 0.9f, "peak-to-peak stays close to 1 semitone");
            expectWithinAbsoluteError ((hi + lo) * 0.5f, 60.0f, 0.05f);
        }

        beginTest ("a 30 ms octave blip inside a note is folded back");
        {
            std::vector<float> v;
            add (v, 60.0f, 30); add (v, 72.0f, 3); add (v, 60.0f, 30);
            const auto out = cleanGuideContour (frames (v), rate);
            expectEquals (countNear (out, 60.0f), 63);
        }

        beginTest ("an octave error at a note onset is folded back");
        {
            std::vector<float> v;
            add (v, 0.0f, 5); add (v, 48.0f, 3); add (v, 60.0f, 40); add (v, 0.0f, 5);
            const auto out = cleanGuideContour (frames (v), rate);
            expectEquals (countNear (out, 60.0f), 43);
        }

        beginTest ("a sustained flip (falsetto break) is kept");
        {
            std::vector<float> v;
            add (v, 60.0f, 30); add (v, 72.0f, 25); add (v, 60.0f, 30);
            const auto out = cleanGuideContour (frames (v), rate);
            expectGreaterOrEqual (countNear (out, 72.0f), 22);
            expectGreaterOrEqual (countNear (out, 60.0f), 55);
        }

        beginTest ("an octave leap between two long notes is kept");
        {
            std::vector<float> v;
            add (v, 60.0f, 30); add (v, 72.0f, 30);
            const auto out = cleanGuideContour (frames (v), rate);
            expectGreaterOrEqual (countNear (out, 60.0f), 28);
            expectGreaterOrEqual (countNear (out, 72.0f), 28);
        }

        beginTest ("a 20 ms non-octave spike is removed and the gap joined");
        {
            std::vector<float> v;
            add (v, 60.0f, 30); add (v, 69.0f, 2); add (v, 60.0f, 30);
            const auto out = cleanGuideContour (frames (v), rate);
            expectEquals (countNear (out, 69.0f, 1.0f), 0);
            expectEquals (countNear (out, 60.0f), 62);
        }

        beginTest ("ornaments (fakes) are kept");
        {
            std::vector<float> v;
            add (v, 60.0f, 20); add (v, 62.0f, 4); add (v, 64.0f, 4); add (v, 62.0f, 4); add (v, 60.0f, 20);
            const auto out = cleanGuideContour (frames (v), rate);
            expectGreaterOrEqual (countNear (out, 62.0f, 0.3f), 4);
            expectGreaterOrEqual (countNear (out, 64.0f, 0.3f), 2);
        }

        beginTest ("a short pickup a big leap away is kept (it doesn't return)");
        {
            std::vector<float> v;
            add (v, 0.0f, 5); add (v, 51.0f, 4); add (v, 60.0f, 40);
            const auto out = cleanGuideContour (frames (v), rate);
            expectGreaterOrEqual (countNear (out, 51.0f), 3);
        }

        beginTest ("isolated blips (consonants, breaths) are removed");
        {
            std::vector<float> v;
            add (v, 0.0f, 10); add (v, 65.0f, 3); add (v, 0.0f, 10); add (v, 60.0f, 20); add (v, 0.0f, 10);
            const auto out = cleanGuideContour (frames (v), rate);
            expectEquals (countNear (out, 65.0f), 0);
            expectEquals (countNear (out, 60.0f), 20);
        }

        beginTest ("small gaps join only between close pitches");
        {
            std::vector<float> v;
            add (v, 60.0f, 20); add (v, 0.0f, 3); add (v, 60.5f, 20); add (v, 0.0f, 3); add (v, 64.0f, 20);
            const auto out = cleanGuideContour (frames (v), rate);
            int voicedCount = 0;
            for (auto& p : out) voicedCount += p.confidence >= 0.5f ? 1 : 0;
            expectEquals (voicedCount, 63, "the first gap is filled, the second (a different note) is not");
        }

        beginTest ("regions separated in time are cleaned independently");
        {
            auto f = frames ({ 60, 60, 60, 60, 60, 60 });
            auto g = frames ({ 72, 72, 72, 72, 72, 72 });
            for (auto& p : g) p.songSample += 100 * hop;
            f.insert (f.end(), g.begin(), g.end());
            const auto out = cleanGuideContour (f, rate);
            expectEquals (countNear (out, 60.0f), 6);
            expectEquals (countNear (out, 72.0f), 6);
        }
    }
};

static GuideCleanTests guideCleanTests;
} // namespace vb::analysis
