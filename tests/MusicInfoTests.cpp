#include "analysis/MusicInfo.h"

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

        beginTest ("tempo: too short or silent gives nothing");
        {
            std::vector<float> silence ((size_t) (30 * sr), 0.0f);
            expectEquals (estimateTempo (silence.data(), (juce::int64) silence.size(), sr).bpm, 0.0);
            const auto shortSong = song (120.0, 0.0, 5.0, cMajor);
            expectEquals (estimateTempo (shortSong.data(), (juce::int64) shortSong.size(), sr).bpm, 0.0);
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
        }
    }
};

static MusicInfoTests musicInfoTests;
} // namespace vb::analysis
