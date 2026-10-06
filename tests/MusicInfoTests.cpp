#include "analysis/MusicInfo.h"
#include "analysis/Decimate.h"
#include "audio/Resample.h"

/*  テンポ・1 小節目・キーの自動推定（B9b）。自作の合成音（キック・ハイハット・和音の進行）で確かめる */

namespace vb::analysis
{
namespace
{
    constexpr double sr = 44100.0;
    constexpr double twoPi = 6.283185307179586;

    double midiHz (double m) { return 440.0 * std::pow (2.0, (m - 69.0) / 12.0); }

    /** lead 秒の無音のあと、bpm で 4/4。キック（1 拍目は強く）・裏のハイハット・小節ごとに変わる三和音。
        chords は根音（MIDI）と短調か、の並び（くり返す） */
    std::vector<float> song (double bpm, double lead, double seconds, const std::vector<std::pair<int, bool>>& chords, int seed = 1)
    {
        juce::Random rnd (seed);
        const auto n = (size_t) (seconds * sr);
        std::vector<float> x (n, 0.0f);
        const auto beat = 60.0 / bpm;
        int k = 0;
        for (double t = lead; t < seconds - 0.2; t += beat, ++k)
        {
            const auto s0 = (size_t) (t * sr);
            const auto amp = k % 4 == 0 ? 0.6f : 0.35f;
            for (size_t i = 0; i < (size_t) (0.12 * sr) && s0 + i < n; ++i)
            {
                const auto tt = (double) i / sr;
                x[s0 + i] += amp * (float) (std::sin (twoPi * (60.0 + 80.0 * std::exp (-tt * 40.0)) * tt) * std::exp (-tt * 25.0));
            }
            const auto h0 = (size_t) ((t + beat * 0.5) * sr);
            for (size_t i = 0; i < (size_t) (0.04 * sr) && h0 + i < n; ++i)
                x[h0 + i] += 0.08f * (rnd.nextFloat() * 2.0f - 1.0f) * std::exp (-(float) i / (float) (0.008 * sr));
        }
        int bar = 0;
        for (double t = lead; t < seconds; t += 4.0 * beat, ++bar)
        {
            const auto [root, isMinor] = chords[(size_t) bar % chords.size()];
            const auto s0 = (size_t) (t * sr), s1 = std::min (n, (size_t) ((t + 4.0 * beat) * sr));
            for (const auto iv : { 0, isMinor ? 3 : 4, 7, 12 })
            {
                const auto f = midiHz (root + iv);
                for (size_t i = s0; i < s1; ++i)
                {
                    const auto ph = twoPi * f * (double) (i - s0) / sr;
                    x[i] += (float) (0.05 * (std::sin (ph) + 0.4 * std::sin (2 * ph)));
                }
            }
        }
        return x;
    }

    void addKick (std::vector<float>& x, double t, float amp)
    {
        const auto s0 = (size_t) (t * sr);
        for (size_t i = 0; i < (size_t) (0.12 * sr) && s0 + i < x.size(); ++i)
        {
            const auto tt = (double) i / sr;
            x[s0 + i] += amp * (float) (std::sin (twoPi * (55.0 + 70.0 * std::exp (-tt * 40.0)) * tt) * std::exp (-tt * 25.0));
        }
    }

    void addSnare (std::vector<float>& x, double t, float amp, juce::Random& rnd)
    {
        const auto s0 = (size_t) (t * sr);
        float last = 0.0f;
        for (size_t i = 0; i < (size_t) (0.15 * sr) && s0 + i < x.size(); ++i)
        {
            const auto tt = (double) i / sr;
            const auto body = std::sin (twoPi * 240.0 * tt) * std::exp (-tt * 30.0);   // 胴（200 Hz より上：キックの帯に入らない）
            const auto white = rnd.nextFloat() * 2.0f - 1.0f;
            const auto noise = (white - last) * 0.5f * std::exp (-tt * 22.0);          // 響き線のざらつき（高い方に寄せる）
            last = white;
            x[s0 + i] += amp * (float) (0.6 * body + 0.6 * noise);
        }
    }

    void addHat (std::vector<float>& x, double t, float amp, juce::Random& rnd)
    {
        const auto s0 = (size_t) (t * sr);
        float last = 0.0f;
        for (size_t i = 0; i < (size_t) (0.03 * sr) && s0 + i < x.size(); ++i)
        {
            const auto white = rnd.nextFloat() * 2.0f - 1.0f;
            x[s0 + i] += amp * (white - last) * 0.5f * std::exp (-(float) i / (float) (0.006 * sr));   // 高い音だけ
            last = white;
        }
    }

    /** バラード（2026-10-02、実際の曲で拍の線が裏にずれた形）：キックは 1・3 拍目と、2・4 拍目の裏にも同じ強さで入り、
        8 小節ごとの頭にだけ特大の 1 発（キメ）。スネアは 2・4 拍目、ハイハットは 8 分。和音は 8 分音符だけ食って変わる */
    std::vector<float> ballad (double bpm, double lead, double seconds, const std::vector<std::pair<int, bool>>& chords, int seed = 3)
    {
        juce::Random rnd (seed);
        const auto n = (size_t) (seconds * sr);
        std::vector<float> x (n, 0.0f);
        const auto beat = 60.0 / bpm;
        int k = 0;
        for (double t = lead; t < seconds - 0.3; t += beat, ++k)
        {
            const auto b = k % 4;
            if (b == 0) addKick (x, t, k % 32 == 0 ? 1.0f : 0.35f);
            if (b == 2) addKick (x, t, 0.35f);
            if (b == 1 || b == 3) addKick (x, t + 0.5 * beat, 0.35f);   // 裏のキック（シンコペーション）
            if (b == 1 || b == 3) addSnare (x, t, 0.3f, rnd);
            addHat (x, t, 0.05f, rnd);
            addHat (x, t + 0.5 * beat, 0.07f, rnd);
        }
        int bar = 0;
        for (double t = lead; t < seconds; t += 4.0 * beat, ++bar)
        {
            const auto [root, isMinor] = chords[(size_t) bar % chords.size()];
            const auto from = bar == 0 ? t : t - 0.5 * beat;   // 和音は 8 分音符だけ食って変わる（J-POP によくある）
            const auto s0 = (size_t) (from * sr), s1 = std::min (n, (size_t) ((t + 3.5 * beat) * sr));
            for (const auto iv : { 0, isMinor ? 3 : 4, 7, 12 })
            {
                const auto f = midiHz (root + iv);
                for (size_t i = s0; i < s1; ++i)
                {
                    const auto ph = twoPi * f * (double) (i - s0) / sr;
                    x[i] += (float) (0.05 * (std::sin (ph) + 0.4 * std::sin (2 * ph)));
                }
            }
        }
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

class MusicInfoTests : public juce::UnitTest
{
public:
    MusicInfoTests() : juce::UnitTest ("MusicInfo", "VoiceBooth") {}

    void runTest() override
    {
        const std::vector<std::pair<int, bool>> cMajor { { 48, false }, { 57, true }, { 53, false }, { 55, false } };   // C Am F G

        beginTest ("tempo: 90 / 128 / 150 BPM within 0.05, bar 1 on the first chord (within 15 ms)");
        {
            for (auto bpm : { 90.0, 128.0, 150.0 })
            {
                const auto x = song (bpm, 0.73, 45.0, cMajor);
                const auto t = estimateTempo (x.data(), (juce::int64) x.size(), sr);
                expectWithinAbsoluteError (t.bpm, bpm, 0.05, juce::String (bpm));
                expectWithinAbsoluteError ((double) t.downbeatSample / sr, 0.73, 0.015, juce::String (bpm));
                expectGreaterThan (t.confidence, 0.2f);
                expectEquals (t.beatsPerBar, 4);
            }
        }

        beginTest ("tempo: ballad with off-beat kicks and a big accent keeps the beat lines on the beats");
        {
            for (auto bpm : { 92.0, 100.0, 108.0 })
            {
                const auto lead = 0.61;
                const auto x = ballad (bpm, lead, 50.0, cMajor);
                const auto t = estimateTempo (x.data(), (juce::int64) x.size(), sr);
                expectWithinAbsoluteError (t.bpm, bpm, 0.05, juce::String (bpm));
                // 拍の線（小節の頭から 1 拍ごと）が本当の拍の上にあること（どの拍が 1 拍目かは問わない）
                const auto beat = 60.0 / t.bpm;
                auto off = std::fmod ((double) t.downbeatSample / sr - lead, beat);
                if (off < 0.0) off += beat;
                if (off > 0.5 * beat) off -= beat;
                logMessage ("  ballad " + juce::String (bpm) + ": estimated " + juce::String (t.bpm) + " BPM, beat-line offset " + juce::String (off * 1000.0, 1) + " ms");
                expectWithinAbsoluteError (off, 0.0, 0.03, "beat phase at " + juce::String (bpm) + " BPM");
            }
        }

        beginTest ("tempo: too short or silent gives nothing");
        {
            std::vector<float> silence ((size_t) (30 * sr), 0.0f);
            expectEquals (estimateTempo (silence.data(), (juce::int64) silence.size(), sr).bpm, 0.0);
            const auto shortSong = song (120.0, 0.0, 5.0, cMajor);
            expectEquals (estimateTempo (shortSong.data(), (juce::int64) shortSong.size(), sr).bpm, 0.0);
        }

        beginTest ("tempo and key at 176.4 kHz: the same result as at 44.1 kHz, and not slower (#21)");
        {
            expectEquals (analysisFactor (44100.0), 1);
            expectEquals (analysisFactor (48000.0), 1);
            expectEquals (analysisFactor (96000.0), 2);
            expectEquals (analysisFactor (176400.0), 3);
            expectEquals (analysisFactor (192000.0), 4);
            expectEquals (analysisFactor (384000.0), 8);

            const auto x = song (128.0, 0.73, 45.0, cMajor);
            const auto hi = upsample4 (x);
            auto t0 = juce::Time::getMillisecondCounterHiRes();
            const auto lo = estimateTempo (x.data(), (juce::int64) x.size(), sr);
            const auto msLo = juce::Time::getMillisecondCounterHiRes() - t0;
            t0 = juce::Time::getMillisecondCounterHiRes();
            const auto h = estimateTempo (hi.data(), (juce::int64) hi.size(), sr * 4.0);
            const auto msHi = juce::Time::getMillisecondCounterHiRes() - t0;
            logMessage ("    tempo 44.1 kHz " + juce::String (msLo, 0) + " ms, 176.4 kHz " + juce::String (msHi, 0) + " ms");
            expectWithinAbsoluteError (h.bpm, lo.bpm, 0.05);
            expectWithinAbsoluteError ((double) h.downbeatSample / (sr * 4.0), 0.73, 0.015);
            expect (msHi < msLo * 3.0 + 200.0, juce::String (msHi) + " ms");
            const auto k = estimateKey (hi.data(), (juce::int64) hi.size(), sr * 4.0);
            expectEquals (k.tonic, 0);
            expect (! k.minor);
        }

        beginTest ("key: C major, A minor, and a transposed song");
        {
            const auto c = song (120.0, 0.5, 30.0, cMajor);
            const auto kc = estimateKey (c.data(), (juce::int64) c.size(), sr);
            expectEquals (kc.tonic, 0);
            expect (! kc.minor);

            const std::vector<std::pair<int, bool>> aMinor { { 57, true }, { 50, true }, { 52, false }, { 57, true } };   // Am Dm E Am
            const auto a = song (100.0, 0.5, 30.0, aMinor);
            const auto ka = estimateKey (a.data(), (juce::int64) a.size(), sr);
            expectEquals (ka.tonic, 9);
            expect (ka.minor);

            std::vector<std::pair<int, bool>> dMajor;   // 全音上げた C → D
            for (auto [r, m] : cMajor) dMajor.push_back ({ r + 2, m });
            const auto d = song (120.0, 0.5, 30.0, dMajor);
            const auto kd = estimateKey (d.data(), (juce::int64) d.size(), sr);
            expectEquals (kd.tonic, 2);
            expect (! kd.minor);

            // 調のない音（無音）は分からないまま（前は C と推定していた）
            const std::vector<float> silent ((size_t) (sr * 10.0), 0.0f);
            expectEquals (estimateKey (silent.data(), (juce::int64) silent.size(), sr).tonic, -1);
        }
    }
};

static MusicInfoTests musicInfoTests;
} // namespace vb::analysis
