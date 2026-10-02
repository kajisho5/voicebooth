#include "analysis/Separation.h"
#include "analysis/OffVocal.h"
#include "audio/Resample.h"

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

        // 原曲だけ（B16）：原曲 − 分離した声 = オフボ。「原曲 − オフボ」がちょうど声になる（B9 の引き算がそのまま使える）
        auto tone = [] (int n, double rate, double hz, float amp, int channels)
        {
            audio::SongAudio a;
            a.sampleRate = rate;
            a.buffer.setSize (channels, n);
            for (int ch = 0; ch < channels; ++ch)
                for (int i = 0; i < n; ++i)
                    a.buffer.setSample (ch, i, amp * (float) std::sin (juce::MathConstants<double>::twoPi * hz * i / rate + ch));
            return a;
        };
        auto maxDiff = [] (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b, int from, int to)
        {
            float e = 0.0f;
            for (int ch = 0; ch < a.getNumChannels(); ++ch)
                for (int i = from; i < to; ++i)
                    e = juce::jmax (e, std::abs (a.getSample (ch, i) - b.getSample (ch, i)));
            return e;
        };

        beginTest ("off vocal: original minus vocals at the same rate gives the backing back");
        {
            const auto back = tone (44100, 44100.0, 220.0, 0.3f, 2), voc = tone (44100, 44100.0, 660.0, 0.2f, 2);
            audio::SongAudio orig = back;
            for (int ch = 0; ch < 2; ++ch) orig.buffer.addFrom (ch, 0, voc.buffer, ch, 0, 44100);
            const auto off = offVocalFrom (orig, voc);
            expectEquals (off.getNumSamples(), 44100);
            expectEquals (off.getNumChannels(), 2);
            expect (maxDiff (off, back.buffer, 0, 44100) < 1.0e-6f);
        }

        beginTest ("off vocal: 48 kHz original, vocals at 44.1 kHz: original minus off vocal is exactly the vocals at 48 kHz");
        {
            const int n = 96000;
            const auto back = tone (n, 48000.0, 220.0, 0.3f, 2), voc48 = tone (n, 48000.0, 660.0, 0.2f, 2);
            audio::SongAudio orig = back;
            for (int ch = 0; ch < 2; ++ch) orig.buffer.addFrom (ch, 0, voc48.buffer, ch, 0, n);
            const auto voc44 = audio::resampleSong (voc48, 44100.0);
            const auto off = offVocalFrom (orig, *voc44);
            expectEquals (off.getNumSamples(), n);
            // 端（リサンプルの立ち上がり）を除けば、伴奏がそのまま戻る
            expect (maxDiff (off, back.buffer, 2000, n - 2000) < 2.0e-3f, juce::String (maxDiff (off, back.buffer, 2000, n - 2000)));
            const auto back48 = audio::resampleSong (*voc44, 48000.0);
            juce::AudioBuffer<float> residual (orig.buffer);
            for (int ch = 0; ch < 2; ++ch) residual.addFrom (ch, 0, off, ch, 0, n, -1.0f);
            float e = 0.0f;
            const auto m = (int) juce::jmin ((juce::int64) n, back48->length());
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < m; ++i)
                    e = juce::jmax (e, std::abs (residual.getSample (ch, i) - back48->buffer.getSample (ch, i)));
            expect (e < 1.0e-6f, juce::String (e));
        }

        beginTest ("off vocal: mono original takes the mean of the vocal channels, short vocals leave the tail alone");
        {
            audio::SongAudio orig;
            orig.sampleRate = 44100.0;
            orig.buffer.setSize (1, 1000);
            orig.buffer.clear();
            for (int i = 0; i < 1000; ++i) orig.buffer.setSample (0, i, 0.5f);
            audio::SongAudio voc;
            voc.sampleRate = 44100.0;
            voc.buffer.setSize (2, 600);
            for (int i = 0; i < 600; ++i) { voc.buffer.setSample (0, i, 0.2f); voc.buffer.setSample (1, i, 0.4f); }
            const auto off = offVocalFrom (orig, voc);
            expectEquals (off.getNumChannels(), 1);
            expectEquals (off.getNumSamples(), 1000);
            expectWithinAbsoluteError (off.getSample (0, 10), 0.2f, 1.0e-6f);
            expectWithinAbsoluteError (off.getSample (0, 599), 0.2f, 1.0e-6f);
            expectWithinAbsoluteError (off.getSample (0, 600), 0.5f, 1.0e-6f);
            expectWithinAbsoluteError (off.getSample (0, 999), 0.5f, 1.0e-6f);
        }
    }
};

static SeparationTests separationTests;
} // namespace vb::analysis::separation
