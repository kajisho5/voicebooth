#include "analysis/Rmvpe.h"
#include <cmath>

namespace vb::analysis::rmvpe
{
class RmvpeTests : public juce::UnitTest
{
public:
    RmvpeTests() : juce::UnitTest ("Rmvpe", "VoiceBooth") {}

    static std::vector<float> sine (double freq, double rate, double seconds, float amp = 0.5f)
    {
        std::vector<float> x ((size_t) (seconds * rate));
        for (size_t i = 0; i < x.size(); ++i)
            x[i] = amp * (float) std::sin (2.0 * 3.14159265358979323846 * freq * (double) i / rate);
        return x;
    }

    static float rms (const std::vector<float>& x, size_t from, size_t to)
    {
        double s = 0.0;
        for (auto i = from; i < to; ++i) s += (double) x[i] * x[i];
        return (float) std::sqrt (s / (double) (to - from));
    }

    /** フレーム t の 360 bin：bin c を中心にした山（幅 sigma bin） */
    static std::vector<float> bump (float c, float peak, float sigma = 1.5f)
    {
        std::vector<float> s ((size_t) bins);
        for (int i = 0; i < bins; ++i)
            s[(size_t) i] = peak * std::exp (-0.5f * ((float) i - c) * ((float) i - c) / (sigma * sigma));
        return s;
    }

    void runTest() override
    {
        beginTest ("resample to 16 kHz keeps the tone and cuts what folds over");
        {
            const auto low = resampleTo16k (sine (1000.0, 44100.0, 1.0).data(), 44100, 44100.0);
            expectEquals ((int) low.size(), 16000);
            expectWithinAbsoluteError (rms (low, 1000, 15000), 0.5f / std::sqrt (2.0f), 0.01f);
            int crossings = 0;
            for (size_t i = 1001; i < 15000; ++i)
                crossings += (low[i - 1] < 0.0f) != (low[i] < 0.0f) ? 1 : 0;
            expectWithinAbsoluteError ((float) crossings / (14000.0f / 16000.0f) / 2.0f, 1000.0f, 3.0f);

            const auto high = resampleTo16k (sine (10000.0, 44100.0, 1.0).data(), 44100, 44100.0);   // 8 kHz より上 → 消える
            expectLessThan (rms (high, 1000, 15000), 0.5f / std::sqrt (2.0f) * 0.01f);
        }

        beginTest ("log-mel: frame count, silence floor and where a tone lands");
        {
            int frames = 0;
            const auto quiet = logMel (std::vector<float> (16000, 0.0f), frames);
            expectEquals (frames, 1 + 16000 / hop);
            expectWithinAbsoluteError (quiet[0], std::log (1e-5f), 1e-4f);

            const auto tone = logMel (sine (440.0, 16000.0, 1.0), frames);
            // 真ん中のフレームで一番大きい mel の帯の中心が 440 Hz の近く（HTK の mel、30..8000 Hz を 129 等分）
            const int f = frames / 2;
            int best = 0;
            for (int m = 1; m < mels; ++m)
                if (tone[(size_t) m * (size_t) frames + (size_t) f] > tone[(size_t) best * (size_t) frames + (size_t) f]) best = m;
            auto toMel = [] (double hz) { return 2595.0 * std::log10 (1.0 + hz / 700.0); };
            const auto step = (toMel (8000.0) - toMel (30.0)) / (mels + 1);
            const auto centre = 700.0 * (std::pow (10.0, (toMel (30.0) + step * (best + 1)) / 2595.0) - 1.0);
            expectWithinAbsoluteError ((float) centre, 440.0f, 30.0f);
        }

        beginTest ("decode: weighted mean of 9 bins around the peak");
        {
            float cents = 0.0f, strength = 0.0f;
            const auto s = bump (100.5f, 0.8f);
            decodeFrame (s.data(), cents, strength);
            expectWithinAbsoluteError (cents, 20.0f * 100.5f + 1997.3794f, 1.0f);
            expectWithinAbsoluteError (strength, s[100], 1e-6f);
            // A4 = 440 Hz = 1200·log2(44) セント
            expectWithinAbsoluteError (centsToMidi (1200.0f * std::log2 (44.0f)), 69.0f, 1e-3f);
        }

        beginTest ("voiced probability rises with strength");
        {
            expectLessThan (voicedProbability (0.0f), 0.01f);
            expectGreaterThan (voicedProbability (0.99f), 0.99f);
            float last = 0.0f;
            for (float p = 0.0f; p <= 1.0f; p += 0.01f)
            {
                const auto v = voicedProbability (p);
                expectGreaterOrEqual (v, last);
                last = v;
            }
        }

        beginTest ("path: short octave blips fold back, a held octave jump stays, silence stays silent");
        {
            std::vector<Frame> f;
            auto add = [&f] (float cents, float strength, int count) { f.insert (f.end(), (size_t) count, Frame { cents, strength }); };
            add (0.0f, 0.001f, 20);
            add (6000.0f, 0.9f, 40);
            add (7200.0f, 0.9f, 3);      // 30 ms の飛び（検出の誤り）
            add (6000.0f, 0.9f, 40);
            add (7200.0f, 0.9f, 40);     // 400 ms 続く 1 オクターブ上（歌い方）
            add (0.0f, 0.001f, 20);
            const auto p = smoothPath (f);
            expectEquals ((int) p.size(), (int) f.size());
            expectEquals (p[5], 0.0f);
            expectWithinAbsoluteError (p[61], 6000.0f, 1.0f);   // 飛びは元の音へ
            expectWithinAbsoluteError (p[100], 6000.0f, 1.0f);
            expectWithinAbsoluteError (p[130], 7200.0f, 1.0f);  // 続いた跳びは残す
            expectEquals (p[f.size() - 1], 0.0f);
        }

        beginTest ("pitch frames sit on the song's sample grid");
        {
            std::vector<Frame> f (200, Frame { 1200.0f * std::log2 (44.0f), 0.9f });
            const auto pts = toPitchFrames (f, 44100.0, 1000);
            expectEquals ((int) pts.size(), 200);
            expectEquals (pts[100].songSample, (juce::int64) (1000 + 44100));
            expectWithinAbsoluteError (pts[100].midi, 69.0f, 1e-3f);
            expectGreaterOrEqual (pts[100].confidence, 0.5f);
        }
    }
};

static RmvpeTests rmvpeTests;
} // namespace vb::analysis::rmvpe
