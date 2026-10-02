#include "audio/WaveformOverview.h"

namespace vb::audio
{
namespace
{
    constexpr int bin = WaveformOverview::samplesPerBin;

    /** 決まった並びの擬似乱数（-1..1）。毎回同じ値 */
    struct Lcg
    {
        juce::uint32 state = 12345;
        float next()
        {
            state = state * 1664525u + 1013904223u;
            return (float) (state >> 8) / (float) (1u << 24) * 2.0f - 1.0f;
        }
    };

    /** 少しずつ流し込む（読み手のブロック境界がビン境界とずれても正しいこと） */
    WaveformOverview build (const std::vector<std::vector<float>>& chans, int chunk)
    {
        const auto len = (int64) chans[0].size();
        WaveformOverview o (len);
        for (int64 pos = 0; pos < len; pos += chunk)
        {
            const auto n = (int) juce::jmin ((int64) chunk, len - pos);
            std::vector<const float*> ptrs;
            for (auto& c : chans)
                ptrs.push_back (c.data() + pos);
            o.append (ptrs.data(), (int) ptrs.size(), n);
        }
        return o;
    }

    /** 期待値：範囲を含むビン全体の生サンプルの最小・最大 */
    WaveformOverview::Peak bruteForce (const std::vector<std::vector<float>>& chans, int64 start, int64 end)
    {
        const auto len = (int64) chans[0].size();
        start = juce::jmax ((int64) 0, start);
        end = juce::jmin (len, end);
        if (end <= start)
            return {};

        const auto from = (start / bin) * bin;
        const auto to = juce::jmin (len, ((end - 1) / bin + 1) * bin);
        WaveformOverview::Peak p { chans[0][(size_t) from], chans[0][(size_t) from] };
        for (auto& c : chans)
            for (auto i = from; i < to; ++i)
            {
                p.min = juce::jmin (p.min, c[(size_t) i]);
                p.max = juce::jmax (p.max, c[(size_t) i]);
            }
        return p;
    }
}

class WaveformOverviewTests : public juce::UnitTest
{
public:
    WaveformOverviewTests() : juce::UnitTest ("WaveformOverview", "VoiceBooth") {}

    void runTest() override
    {
        beginTest ("bin boundaries and odd length");
        {
            std::vector<std::vector<float>> ch { std::vector<float> (1000, 0.0f) };
            ch[0][300] = -0.5f;
            ch[0][700] = 0.9f;
            const auto o = build (ch, 37);

            expect (o.isComplete());
            expectEquals (o.getLengthSamples(), (int64) 1000);
            expectEquals (o.getPeak (0, 1000).max, 0.9f);
            expectEquals (o.getPeak (0, 1000).min, -0.5f);
            expectEquals (o.getPeak (0, 256).max, 0.0f);          // 0–255 に山は無い
            expectEquals (o.getPeak (256, 512).min, -0.5f);
            expectEquals (o.getPeak (700, 701).max, 0.9f);
            expectEquals (o.getPeak (768, 1000).max, 0.0f);       // 最後の半端なビン
            expectEquals (o.getOverallMagnitude(), 0.9f);
        }

        beginTest ("out-of-range and empty ranges are zero");
        {
            std::vector<std::vector<float>> ch { std::vector<float> (600, 0.25f) };
            const auto o = build (ch, 600);
            expectEquals (o.getPeak (-100, 0).max, 0.0f);
            expectEquals (o.getPeak (600, 900).max, 0.0f);
            expectEquals (o.getPeak (10, 10).max, 0.0f);
            expectEquals (o.getPeak (-100, 50).max, 0.25f);     // はみ出しは切って数える
        }

        beginTest ("peaks from every channel");
        {
            std::vector<std::vector<float>> ch { std::vector<float> (8000, 0.0f), std::vector<float> (8000, 0.0f) };
            ch[1][5000] = 0.7f;
            ch[0][100] = -0.3f;
            const auto o = build (ch, 1024);
            expectEquals (o.getPeak (4900, 5100).max, 0.7f);
            expectEquals (o.getPeak (0, 200).min, -0.3f);
        }

        beginTest ("appending past the length is ignored");
        {
            WaveformOverview o (300);
            std::vector<float> data (1000, 0.5f);
            const float* p = data.data();
            o.append (&p, 1, 1000);
            expectEquals (o.getSamplesAppended(), (int64) 300);
            expect (o.isComplete());
            expectEquals (o.getPeak (0, 300).max, 0.5f);
        }

        beginTest ("RMS: constant, sine and silence");
        {
            const int64 len = 48000 * 3 + 100;
            std::vector<std::vector<float>> ch (2, std::vector<float> ((size_t) len, 0.0f));
            for (int64 i = 0; i < 48000; ++i)
                ch[0][(size_t) i] = ch[1][(size_t) i] = 0.5f;                                    // 0–1 s：一定 0.5
            for (int64 i = 48000; i < 96000; ++i)
                ch[0][(size_t) i] = (float) std::sin (juce::MathConstants<double>::twoPi * 1000.0 * (double) i / 48000.0);   // 1–2 s：正弦（左だけ）
            const auto o = build (ch, 4000);

            expectWithinAbsoluteError (o.getRms (0, 48000), 0.5f, 1.0e-4f);   // 48000 はビン境界でないので端のビンを含む
            // 左だけ振幅 1 の正弦：チャンネル平均パワー 0.25 → RMS 0.5
            expectWithinAbsoluteError (o.getRms (48000, 96000), 0.5f, 1.0e-3f);
            expectWithinAbsoluteError (o.getRms (96000, len), 0.0f, 1.0e-6f);
            expectWithinAbsoluteError (o.getRms (0, len), std::sqrt ((0.25f * 48000 + 0.25f * 48000) / (float) len), 1.0e-4f);
            expectEquals (o.getRms (len, len + 10), 0.0f);
        }

        beginTest ("RMS: coarse level matches brute force");
        {
            const int64 len = 48000 * 7 + 333;
            std::vector<std::vector<float>> ch (1, std::vector<float> ((size_t) len));
            Lcg rng;
            for (auto& v : ch[0])
                v = rng.next();
            const auto o = build (ch, 65536);

            Lcg pick;
            for (int i = 0; i < 100; ++i)
            {
                const auto a = (int64) ((pick.next() * 0.5f + 0.5f) * (float) len);
                const auto b = juce::jmin (len, a + (int64) ((pick.next() * 0.5f + 0.5f) * (float) len));
                if (b <= a) continue;
                // 期待値：範囲を含むビン全体の生サンプル
                const auto from = (a / bin) * bin, to = juce::jmin (len, ((b - 1) / bin + 1) * bin);
                double sum = 0.0;
                for (auto k = from; k < to; ++k)
                    sum += (double) ch[0][(size_t) k] * ch[0][(size_t) k];
                expectWithinAbsoluteError (o.getRms (a, b), (float) std::sqrt (sum / (double) (to - from)), 1.0e-4f);
            }
        }

        beginTest ("dB display scale");
        {
            using O = WaveformOverview;
            expectEquals (O::toDbScale (1.0f), 1.0f);                                   // 0 dBFS
            expectEquals (O::toDbScale (1.5f), 1.0f);                                   // 超えても 1
            expectWithinAbsoluteError (O::toDbScale (0.1f), 0.5f, 1.0e-5f);             // -20 dB（下限 -40）
            expectWithinAbsoluteError (O::toDbScale (0.01f), 0.0f, 1.0e-5f);            // -40 dB
            expectEquals (O::toDbScale (0.001f), 0.0f);                                 // 下限より小さい
            expectEquals (O::toDbScale (0.0f), 0.0f);
            expectWithinAbsoluteError (O::toDbScale (0.1f, -60.0f), 2.0f / 3.0f, 1.0e-5f);
            expect (O::toDbScale (0.2f) > O::toDbScale (0.1f));                         // 単調増加
        }

        beginTest ("coarse level matches brute force (long-song LOD)");
        {
            const int64 len = 48000 * 10 + 77;
            std::vector<std::vector<float>> ch (2, std::vector<float> ((size_t) len));
            Lcg rng;
            for (auto& c : ch)
                for (auto& v : c)
                    v = rng.next() * 0.8f;

            const auto o = build (ch, 65536);
            Lcg pick;
            for (int i = 0; i < 300; ++i)
            {
                const auto a = (int64) ((pick.next() * 0.5f + 0.5f) * (float) len);
                const auto w = (int64) ((pick.next() * 0.5f + 0.5f) * (float) (i % 3 == 0 ? len : 20000));
                const auto got = o.getPeak (a, a + w);
                const auto want = bruteForce (ch, a, a + w);
                expectEquals (got.min, want.min);
                expectEquals (got.max, want.max);
            }
        }
    }
};

static WaveformOverviewTests waveformOverviewTests;
} // namespace vb::audio
