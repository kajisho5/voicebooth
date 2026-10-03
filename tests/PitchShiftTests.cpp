#include "audio/PitchShift.h"
#include <cmath>

namespace vb::audio
{
class PitchShiftTests : public juce::UnitTest
{
public:
    PitchShiftTests() : juce::UnitTest ("PitchShift", "VoiceBooth") {}

    static double frequency (const std::vector<float>& x, size_t from, size_t to, double rate)
    {
        int crossings = 0;
        for (auto i = from + 1; i < to; ++i)
            crossings += (x[i - 1] < 0.0f) != (x[i] < 0.0f) ? 1 : 0;
        return crossings / 2.0 / ((double) (to - from) / rate);
    }

    static size_t onset (const std::vector<float>& x)
    {
        for (size_t i = 0; i < x.size(); ++i)
            if (std::abs (x[i]) > 0.1f)
                return i;
        return x.size();
    }

    void runTest() override
    {
        constexpr double rate = 44100.0;
        // 0.5 秒の無音 → 440 Hz を 1.5 秒 → 0.5 秒の無音
        std::vector<float> x ((size_t) (2.5 * rate), 0.0f);
        for (size_t i = (size_t) (0.5 * rate); i < (size_t) (2.0 * rate); ++i)
            x[i] = 0.5f * (float) std::sin (2.0 * 3.14159265358979323846 * 440.0 * (double) i / rate);

        beginTest ("0 semitones returns the input unchanged");
        {
            const auto y = shiftPitch (x.data(), (juce::int64) x.size(), rate, 0);
            expect (y == x);
        }

        beginTest ("+2 semitones: same length, tone moves to 493.9 Hz, onset stays in place");
        {
            const auto y = shiftPitch (x.data(), (juce::int64) x.size(), rate, 2);
            expectEquals ((int) y.size(), (int) x.size());
            expectWithinAbsoluteError (frequency (y, (size_t) (0.8 * rate), (size_t) (1.8 * rate), rate), 440.0 * std::pow (2.0, 2.0 / 12.0), 3.0);
            expectWithinAbsoluteError ((double) onset (y) / rate, (double) onset (x) / rate, 0.02);
        }

        beginTest ("-3 semitones moves the tone down");
        {
            const auto y = shiftPitch (x.data(), (juce::int64) x.size(), rate, -3);
            expectWithinAbsoluteError (frequency (y, (size_t) (0.8 * rate), (size_t) (1.8 * rate), rate), 440.0 * std::pow (2.0, -3.0 / 12.0), 3.0);
        }
    }
};

static PitchShiftTests pitchShiftTests;
} // namespace vb::audio
