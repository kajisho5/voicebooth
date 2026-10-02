#include "audio/Resample.h"

namespace vb::audio
{
namespace
{
    constexpr double twoPi = 6.283185307179586;

    SongAudio sineSong (double rate, double seconds, double hz, float amp = 0.5f)
    {
        SongAudio s;
        s.sampleRate = rate;
        const auto n = (int) std::llround (rate * seconds);
        s.buffer.setSize (2, n);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i)
                s.buffer.setSample (c, i, amp * (float) std::sin (twoPi * hz * (double) i / rate));
        return s;
    }

    /** 真ん中あたり（端の窓の影響を除く）で、理論値との最大の差 */
    float worstError (const SongAudio& out, double hz, float amp)
    {
        const auto n = out.buffer.getNumSamples();
        float worst = 0.0f;
        for (int i = n / 4; i < n * 3 / 4; ++i)
            worst = std::max (worst, std::abs (out.buffer.getSample (0, i)
                                               - amp * (float) std::sin (twoPi * hz * (double) i / out.sampleRate)));
        return worst;
    }
}

class ResampleTests : public juce::UnitTest
{
public:
    ResampleTests() : juce::UnitTest ("Resample", "VoiceBooth") {}

    void runTest() override
    {
        beginTest ("length is round(length * to / from)");
        {
            expectEquals (resampledLength (960000, 48000.0, 96000.0), (juce::int64) 1920000);
            expectEquals (resampledLength (441000, 44100.0, 48000.0), (juce::int64) 480000);
            expectEquals (resampledLength (100, 48000.0, 44100.0), (juce::int64) 92);
            expectEquals (resampledLength (100, 0.0, 44100.0), (juce::int64) 100);
        }

        beginTest ("up: 48 -> 96 / 192 / 384 kHz keeps time (a 1 kHz sine lands on the same phase)");
        {
            const auto song = sineSong (48000.0, 0.5, 1000.0);
            for (auto to : { 96000.0, 192000.0, 384000.0 })
            {
                const auto out = resampleSong (song, to);
                expect (out != nullptr);
                expectEquals (out->sampleRate, to);
                expectEquals ((juce::int64) out->buffer.getNumSamples(), resampledLength (song.length(), 48000.0, to));
                expectLessThan (worstError (*out, 1000.0, 0.5f), 1.0e-3f, "48 -> " + juce::String (to));
            }
        }

        beginTest ("between families: 44.1 <-> 48 / 88.2 kHz keeps time");
        {
            const auto a = resampleSong (sineSong (44100.0, 0.5, 1000.0), 48000.0);
            expectLessThan (worstError (*a, 1000.0, 0.5f), 1.0e-3f);
            const auto b = resampleSong (sineSong (48000.0, 0.5, 1000.0), 44100.0);
            expectLessThan (worstError (*b, 1000.0, 0.5f), 1.0e-3f);
            const auto c = resampleSong (sineSong (44100.0, 0.5, 1000.0), 88200.0);
            expectLessThan (worstError (*c, 1000.0, 0.5f), 1.0e-3f);
        }

        beginTest ("down: 96 -> 48 kHz removes what can't exist (30 kHz tone is filtered, not folded to 18 kHz)");
        {
            const auto high = resampleSong (sineSong (96000.0, 0.5, 30000.0), 48000.0);
            float peak = 0.0f;
            const auto n = high->buffer.getNumSamples();
            for (int i = n / 4; i < n * 3 / 4; ++i)
                peak = std::max (peak, std::abs (high->buffer.getSample (0, i)));
            expectLessThan (peak, 0.5f * 0.01f);   // -40 dB 以下

            const auto low = resampleSong (sineSong (96000.0, 0.5, 1000.0), 48000.0);
            expectLessThan (worstError (*low, 1000.0, 0.5f), 2.0e-3f);   // 通す帯域は時間も大きさもそのまま
        }

        beginTest ("same rate is a plain copy; cancelling returns nothing");
        {
            const auto song = sineSong (48000.0, 0.1, 440.0);
            const auto same = resampleSong (song, 48000.0);
            expectEquals (same->buffer.getNumSamples(), song.buffer.getNumSamples());
            expectEquals (same->buffer.getSample (1, 1234), song.buffer.getSample (1, 1234));
            expect (resampleSong (song, 96000.0, [] (float) { return false; }) == nullptr);
            expect (recordingRates().contains (384000.0) && recordingRates().contains (44100.0));
        }
    }
};

static ResampleTests resampleTests;
} // namespace vb::audio
