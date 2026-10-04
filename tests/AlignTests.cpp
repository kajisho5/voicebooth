#include "analysis/Align.h"
#include "audio/Resample.h"

/*  原曲とカラオケの時間合わせ（DESIGN 7.1.1）。自作の合成音で確かめる
    「伴奏」＝ばらばらな間隔の打音（雑音の立ち上がり）＋和音の進行。原曲＝前奏を足した伴奏＋声（ビブラート付きの旋律） */

namespace vb::analysis
{
namespace
{
    constexpr double sr = 44100.0;
    constexpr double twoPi = 6.283185307179586;

    double midiHz (double m) { return 440.0 * std::pow (2.0, (m - 69.0) / 12.0); }

    /** 伴奏。transpose で半音ずらしたバージョンも作れる（同じ乱数の並び） */
    std::vector<float> backing (double seconds, double transpose, int seed)
    {
        juce::Random rnd (seed);
        const auto n = (size_t) (seconds * sr);
        std::vector<float> x (n, 0.0f);

        // 打音：0.18〜0.65 秒のばらばらな間隔
        for (double t = 0.1; t < seconds - 0.3;)
        {
            const auto start = (size_t) (t * sr);
            const auto amp = 0.4f + 0.4f * rnd.nextFloat();
            for (size_t i = 0; i < (size_t) (0.12 * sr) && start + i < n; ++i)
                x[start + i] += amp * (rnd.nextFloat() * 2.0f - 1.0f) * std::exp (-(float) i / (float) (0.025 * sr));
            t += 0.18 + 0.47 * rnd.nextDouble();
        }

        // 和音：1.5〜3 秒ごとに長調の三和音が変わる
        const int scale[] = { 0, 2, 4, 5, 7, 9 };
        for (double t = 0.0; t < seconds;)
        {
            const auto len = 1.5 + 1.5 * rnd.nextDouble();
            const auto root = 48 + scale[rnd.nextInt (6)] + transpose;
            const auto s0 = (size_t) (t * sr), s1 = std::min (n, (size_t) ((t + len) * sr));
            for (const auto iv : { 0.0, 4.0, 7.0 })
            {
                const auto f = midiHz (root + iv);
                for (size_t i = s0; i < s1; ++i)
                {
                    const auto ph = twoPi * f * (double) (i - s0) / sr;
                    x[i] += (float) (0.06 * (std::sin (ph) + 0.5 * std::sin (2 * ph) + 0.25 * std::sin (3 * ph)));
                }
            }
            t += len;
        }
        return x;
    }

    /** 原曲：前奏（lead 秒、別の音）＋伴奏＋声 */
    std::vector<float> original (const std::vector<float>& band, double lead, int seed)
    {
        juce::Random rnd (seed);
        const auto pre = (size_t) (lead * sr);
        std::vector<float> x (pre + band.size(), 0.0f);
        for (size_t i = 0; i < pre; ++i)
            x[i] = 0.05f * (rnd.nextFloat() * 2.0f - 1.0f) * (float) (0.5 + 0.5 * std::sin (twoPi * 0.7 * (double) i / sr));
        for (size_t i = 0; i < band.size(); ++i)
            x[pre + i] = band[i];

        // 声：0.4〜1.2 秒の音符、ビブラート 5.5 Hz
        double phase = 0.0;
        for (double t = lead + 0.5; t < (double) x.size() / sr - 1.0;)
        {
            const auto len = 0.4 + 0.8 * rnd.nextDouble();
            const auto note = 60.0 + rnd.nextInt (12);
            for (size_t i = (size_t) (t * sr); i < (size_t) ((t + len) * sr) && i < x.size(); ++i)
            {
                const auto f = midiHz (note + 0.3 * std::sin (twoPi * 5.5 * (double) i / sr));
                phase += twoPi * f / sr;
                x[i] += (float) (0.25 * std::sin (phase));
            }
            t += len + 0.1;
        }
        return x;
    }

    /** カラオケ：頭に lead 秒の無音＋伴奏（ratio で速さを変えられる）＋ごく小さな雑音 */
    std::vector<float> karaoke (const std::vector<float>& band, double lead, double ratio, int seed)
    {
        juce::Random rnd (seed);
        const auto pre = (size_t) (lead * sr);
        const auto bodyLen = (size_t) ((double) band.size() / ratio) - 2;
        std::vector<float> x (pre + bodyLen);
        for (size_t i = 0; i < bodyLen; ++i)
        {
            const auto pos = (double) i * ratio;   // カラオケの i 番目 = 伴奏の i × ratio
            const auto i0 = (size_t) pos;
            const auto frac = (float) (pos - (double) i0);
            x[pre + i] = band[i0] * (1.0f - frac) + band[std::min (band.size() - 1, i0 + 1)] * frac;
        }
        for (auto& v : x) v += 0.001f * (rnd.nextFloat() * 2.0f - 1.0f);
        return x;
    }

    /** 4 倍の SR（176.4 kHz）にする（高い SR の曲の解析を確かめる。#21） */
    std::vector<float> upsample4 (const std::vector<float>& x)
    {
        audio::SongAudio a;
        a.sampleRate = sr;
        a.buffer.setSize (1, (int) x.size());
        a.buffer.copyFrom (0, 0, x.data(), (int) x.size());
        const auto b = audio::resampleSong (a, sr * 4.0);
        return std::vector<float> (b->buffer.getReadPointer (0), b->buffer.getReadPointer (0) + b->buffer.getNumSamples());
    }
}

class AlignTests : public juce::UnitTest
{
public:
    AlignTests() : juce::UnitTest ("Align", "VoiceBooth") {}

    void runTest() override
    {
        const auto band = backing (40.0, 0.0, 7);
        const auto orig = original (band, 2.0, 11);

        beginTest ("original vs karaoke: different intro lengths, sample-exact offset");
        {
            const auto kar = karaoke (band, 0.5, 1.0, 13);
            const auto r = alignReference (orig.data(), (juce::int64) orig.size(), kar.data(), (juce::int64) kar.size(), sr);
            const auto expected = (juce::int64) (2.0 * sr) - (juce::int64) (0.5 * sr);   // 原曲 = カラオケ + 1.5 秒
            expect (r.quality == AlignResult::Quality::good);
            expectEquals (r.offsetSamples, expected);
            expectWithinAbsoluteError (r.tempoRatio, 1.0, 1e-9);
            expectGreaterOrEqual (r.windowsAgreeing, 3);
            expectEquals ((int) r.covered.size(), 1);
            logMessage ("  offset " + juce::String (r.offsetSamples) + " (expected " + juce::String (expected) + "), confidence "
                        + juce::String (r.confidence, 2) + ", windows " + juce::String (r.windowsAgreeing) + "/" + juce::String (r.windowsUsed));
        }

        beginTest ("at 176.4 kHz: the offset comes back in the original rate, within a few samples (#21)");
        {
            const auto kar = karaoke (band, 0.5, 1.0, 13);
            const auto origHi = upsample4 (orig), karHi = upsample4 (kar);
            const auto r = alignReference (origHi.data(), (juce::int64) origHi.size(), karHi.data(), (juce::int64) karHi.size(), sr * 4.0);
            const auto expected = (juce::int64) (1.5 * sr * 4.0);
            expect (r.quality == AlignResult::Quality::good);
            expect (std::llabs (r.offsetSamples - expected) <= 8, juce::String (r.offsetSamples) + " vs " + juce::String (expected));
            expectEquals ((int) r.covered.size(), 1);
            if (! r.covered.empty())
                expect (r.covered[0].karaokeEnd <= (juce::int64) karHi.size());
            const auto k = estimateKeyShift (origHi.data(), (juce::int64) origHi.size(), karHi.data(), (juce::int64) karHi.size(), sr * 4.0);
            expectEquals (k.semitones, 0);
        }

        beginTest ("local lag (here is the same spot): finds a small residual offset around a position");
        {
            // カラオケ b と、37 サンプル遅れた原曲 a（＋声の代わりの別の音）
            const double rate = 44100.0;
            juce::Random rnd (7);
            std::vector<float> b ((size_t) (rate * 6.0));
            for (auto& v : b) v = rnd.nextFloat() * 2.0f - 1.0f;
            std::vector<float> a (b.size(), 0.0f);
            for (size_t i = 37; i < a.size(); ++i)
                a[i] = b[i - 37] + 0.3f * (float) std::sin (2.0 * 3.14159265358979323846 * 330.0 * (double) i / rate);
            const auto r = localLag (a.data(), (juce::int64) a.size(), b.data(), (juce::int64) b.size(),
                                     (juce::int64) (rate * 3.0), (juce::int64) (rate * 1.0), (juce::int64) (rate * 0.2));
            expect (r.found);
            expectEquals ((int) r.lag, 37);
            expectGreaterThan (r.correlation, 0.8);
            // 音が無い所は見つからない
            std::vector<float> silent (b.size(), 0.0f);
            expect (! localLag (silent.data(), (juce::int64) silent.size(), b.data(), (juce::int64) b.size(),
                                (juce::int64) (rate * 3.0), (juce::int64) rate, 400).found);
        }

        beginTest ("karaoke starts earlier than the original (negative offset)");
        {
            const auto plainOrig = original (band, 0.0, 11);
            const auto kar = karaoke (band, 1.25, 1.0, 13);
            const auto r = alignReference (plainOrig.data(), (juce::int64) plainOrig.size(), kar.data(), (juce::int64) kar.size(), sr);
            expect (r.quality == AlignResult::Quality::good);
            expectEquals (r.offsetSamples, -(juce::int64) (1.25 * sr));
        }

        beginTest ("slightly different speed: ratio is found, position error stays under 3 samples");
        {
            const double ratio = 1.0005;
            const auto kar = karaoke (band, 0.5, ratio, 13);
            const auto r = alignReference (orig.data(), (juce::int64) orig.size(), kar.data(), (juce::int64) kar.size(), sr);
            expect (r.quality == AlignResult::Quality::good);
            expectWithinAbsoluteError (r.tempoRatio, ratio, 2e-5);
            // 原曲の位置 = 2.0 s + (k - 0.5 s) × ratio
            for (const double kt : { 1.0, 20.0, 38.0 })
            {
                const auto k = (juce::int64) (kt * sr);
                const auto want = 2.0 * sr + ((double) k - 0.5 * sr) * ratio;
                expectWithinAbsoluteError (r.referencePosition (k), want, 3.0);
            }
            logMessage ("  ratio " + juce::String (r.tempoRatio, 6));
        }

        beginTest ("cut version (TV size): each part gets its own offset, nothing is bridged");
        {
            // カラオケ = 伴奏の 0〜18 秒 ＋ 28〜40 秒（10 秒を切ったバージョン）
            std::vector<float> cut (band.begin(), band.begin() + (long) (18.0 * sr));
            cut.insert (cut.end(), band.begin() + (long) (28.0 * sr), band.end());
            const auto kar = karaoke (cut, 0.5, 1.0, 13);
            const auto r = alignReference (orig.data(), (juce::int64) orig.size(), kar.data(), (juce::int64) kar.size(), sr);

            const auto first = (juce::int64) (1.5 * sr), second = first + (juce::int64) (10.0 * sr);
            bool sawFirst = false, sawSecond = false;
            juce::int64 coveredLen = 0;
            for (auto& c : r.covered)
            {
                coveredLen += c.karaokeEnd - c.karaokeStart;
                sawFirst  |= std::llabs (c.offsetSamples - first) <= 2;
                sawSecond |= std::llabs (c.offsetSamples - second) <= 2;
                logMessage ("  covered " + juce::String ((double) c.karaokeStart / sr, 1) + "-" + juce::String ((double) c.karaokeEnd / sr, 1)
                            + " s, offset " + juce::String ((double) c.offsetSamples / sr, 4) + " s");
            }
            expect (sawFirst, "part before the cut");
            expect (sawSecond, "part after the cut");
            expectGreaterThan ((double) coveredLen / (double) kar.size(), 0.9);

            // 継ぎ目（カラオケの 18.5 秒）は 0.1 秒以内に見つかる
            bool boundaryOk = false;
            for (auto& c : r.covered)
                boundaryOk |= std::abs ((double) c.karaokeEnd / sr - 18.5) < 0.1 && std::llabs (c.offsetSamples - first) <= 2;
            expect (boundaryOk, "boundary at the cut");
        }

        beginTest ("unrelated audio is not aligned");
        {
            const auto other = karaoke (backing (30.0, 0.0, 99), 0.0, 1.0, 5);
            const auto r = alignReference (orig.data(), (juce::int64) orig.size(), other.data(), (juce::int64) other.size(), sr);
            expect (r.quality != AlignResult::Quality::good);
        }

        beginTest ("karaoke in a different key: semitone shift is estimated");
        {
            for (const int shift : { -2, 0, 3 })
            {
                const auto shifted = karaoke (backing (40.0, shift, 7), 0.5, 1.0, 13);
                const auto k = estimateKeyShift (orig.data(), (juce::int64) orig.size(), shifted.data(), (juce::int64) shifted.size(), sr);
                expectEquals (k.semitones, shift);
                expectGreaterThan (k.confidence, 0.05);
            }
        }
    }
};

static AlignTests alignTests;
} // namespace vb::analysis
