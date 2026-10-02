#include "analysis/Separation.h"

namespace vb::analysis::separation
{
class SeparationTests : public juce::UnitTest
{
public:
    SeparationTests() : juce::UnitTest ("Separation", "VoiceBooth") {}

    static juce::AudioBuffer<float> noise (int n, juce::int64 seed)
    {
        juce::Random r (seed);
        juce::AudioBuffer<float> b (2, n);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < n; ++i)
                b.setSample (ch, i, (r.nextFloat() - 0.5f) * 0.8f);
        return b;
    }

    void runTest() override
    {
        beginTest ("STFT shape and a tone lands in its bin");
        {
            juce::AudioBuffer<float> b (2, chunkSamples);
            for (int i = 0; i < chunkSamples; ++i)
            {
                const auto v = (float) std::sin (2.0 * juce::MathConstants<double>::pi * (100.0 * sampleRate / nFft) * i / sampleRate);
                b.setSample (0, i, v);
                b.setSample (1, i, 0.0f);
            }
            int frames = 0;
            const auto spec = stft (b.getArrayOfReadPointers(), chunkSamples, frames);
            expectEquals (frames, 801);                                   // 8 秒 = 801 フレーム（ONNX と同じ）
            expectEquals ((int) spec.size(), 2 * bins * 801 * 2);
            auto mag = [&] (int ch, int bin, int f) { const auto i = (((size_t) ch * bins + (size_t) bin) * 801 + (size_t) f) * 2; return std::hypot (spec[i], spec[i + 1]); };
            expect (mag (0, 100, 400) > 500.0f);                          // 窓の和 1024 × 振幅 1 / 2
            expect (mag (0, 300, 400) < 1.0f);
            expect (mag (1, 100, 400) < 1.0e-3f);
        }

        beginTest ("STFT then iSTFT gives the signal back");
        {
            const auto b = noise (chunkSamples, 7);
            int frames = 0;
            const auto spec = stft (b.getArrayOfReadPointers(), chunkSamples, frames);
            juce::AudioBuffer<float> out (2, chunkSamples);
            istft (spec, frames, chunkSamples, out.getArrayOfWritePointers());
            float err = 0.0f;
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < chunkSamples; ++i)
                    err = juce::jmax (err, std::abs (out.getSample (ch, i) - b.getSample (ch, i)));
            expect (err < 1.0e-5f, juce::String (err));
        }

        beginTest ("demix with a pass-through model returns the mix (short, long, odd tails)");
        {
            const Model pass = [] (const std::vector<float>& spec, int, std::vector<float>& est) { est = spec; return true; };
            for (int n : { 100000, 352800 + 1000, 882000, 882000 + 200000, 882000 + 100000 })
            {
                const auto mix = noise (n, n);
                juce::AudioBuffer<float> vocals;
                int calls = 0;
                expect (demix (mix, pass, 2, vocals, [&] (float) { ++calls; return true; }));
                expectEquals (vocals.getNumSamples(), n);
                expectEquals (calls, chunkCount (n, 2));
                float err = 0.0f;
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < n; ++i)
                        err = juce::jmax (err, std::abs (vocals.getSample (ch, i) - mix.getSample (ch, i)));
                expect (err < 2.0e-5f, juce::String (n) + ": " + juce::String (err));
            }
        }

        beginTest ("demix stops when the model or progress says so");
        {
            const auto mix = noise (882000, 3);
            juce::AudioBuffer<float> vocals;
            expect (! demix (mix, [] (const std::vector<float>&, int, std::vector<float>&) { return false; }, 2, vocals));
            const Model pass = [] (const std::vector<float>& spec, int, std::vector<float>& est) { est = spec; return true; };
            expect (! demix (mix, pass, 2, vocals, [] (float p) { return p < 0.3f; }));
        }
    }
};

static SeparationTests separationTests;
} // namespace vb::analysis::separation
