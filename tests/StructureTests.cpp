#include "analysis/Structure.h"
#include "analysis/MusicInfo.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <cmath>

/*  曲の構成の推定（Phase C）：合成した曲（和音の正弦波＋拍のノイズ）で、区間の境目とサビを当てる。
    120 BPM・4/4（1 小節 2 秒）：イントロ 4 小節（小さい）→ A 8 → サビ 8（大きい・別の和音）→ A 8 → サビ 8 → アウトロ 4 */

namespace vb::analysis
{
class StructureTests : public juce::UnitTest
{
public:
    StructureTests() : juce::UnitTest ("Structure", "VoiceBooth") {}

    static constexpr double rate = 22050.0;
    static constexpr double barSeconds = 2.0;

    struct Part { int bars; std::vector<std::vector<double>> chords; float level; };

    /** 和音（周波数の並び）を 1 小節ずつ順に鳴らす。拍の頭に短いノイズ */
    static std::vector<float> render (const std::vector<Part>& parts, double startSilence = 0.0)
    {
        int totalBars = 0;
        for (auto& p : parts) totalBars += p.bars;
        const auto n = (size_t) ((startSilence + totalBars * barSeconds) * rate);
        std::vector<float> x (n, 0.0f);
        juce::Random noise (3);
        auto pos = (size_t) (startSilence * rate);
        for (auto& p : parts)
            for (int b = 0; b < p.bars; ++b)
            {
                const auto& chord = p.chords[(size_t) b % p.chords.size()];
                const auto len = (size_t) (barSeconds * rate);
                for (size_t i = 0; i < len && pos + i < n; ++i)
                {
                    const auto t = (double) i / rate;
                    double v = 0.0;
                    for (auto f : chord)
                        v += std::sin (juce::MathConstants<double>::twoPi * f * t) + 0.3 * std::sin (juce::MathConstants<double>::twoPi * 2.0 * f * t);
                    v /= (double) chord.size();
                    const auto beat = std::fmod (t, barSeconds / 4.0);
                    if (beat < 0.02)
                        v += 0.5 * (noise.nextFloat() * 2.0 - 1.0) * (1.0 - beat / 0.02);
                    x[pos + i] = (float) (p.level * v);
                }
                pos += len;
            }
        return x;
    }

    static double hz (int midi) { return 440.0 * std::pow (2.0, (midi - 69) / 12.0); }
    static std::vector<double> triad (int root, bool minor) { return { hz (root), hz (root + (minor ? 3 : 4)), hz (root + 7) }; }

    void runTest() override
    {
        // A：Em - Bm - D - A（低め）。サビ：C - G - F - G（大きく、別の和音）
        const std::vector<std::vector<double>> verse { triad (52, true), triad (59, true), triad (50, false), triad (57, false) };
        const std::vector<std::vector<double>> chorus { triad (60, false), triad (55, false), triad (53, false), triad (55, false) };
        const std::vector<std::vector<double>> intro { triad (52, true) };
        const std::vector<Part> song {
            { 4, intro, 0.15f }, { 8, verse, 0.25f }, { 8, chorus, 0.6f }, { 8, verse, 0.25f }, { 8, chorus, 0.6f }, { 4, intro, 0.15f } };
        const auto bar = (juce::int64) (barSeconds * rate);

        beginTest ("section boundaries fall on the bar lines where the music changes, and the loud repeated part is the chorus");
        {
            const auto x = render (song);
            const auto r = estimateStructure (x.data(), (juce::int64) x.size(), rate, 120.0, 0);
            juce::String got;
            for (auto& s : r.sections)
                got << juce::String ((double) s.startSample / (double) bar, 1) << (s.chorus ? "c " : " ");
            logMessage ("  sections (bar, c = chorus): " + got);

            auto has = [&] (int barIndex, bool chorusExpected)
            {
                for (auto& s : r.sections)
                    if (std::abs (s.startSample - barIndex * bar) <= bar / 2)
                        return s.chorus == chorusExpected;
                return false;
            };
            expect (has (0, false), "starts at the top: " + got);
            expect (has (4, false), "verse at bar 4: " + got);
            expect (has (12, true), "chorus at bar 12: " + got);
            expect (has (20, false), "verse at bar 20: " + got);
            expect (has (28, true), "chorus at bar 28: " + got);
            int choruses = 0;
            for (auto& s : r.sections) choruses += s.chorus ? 1 : 0;
            expectEquals (choruses, 2, "only the two choruses: " + got);
            expectGreaterThan (r.chorusConfidence, 0.3f);
        }

        beginTest ("the downbeat moves the bar lines: a song that starts 1 s late still splits on its bars");
        {
            const auto x = render (song, 1.0);
            const auto r = estimateStructure (x.data(), (juce::int64) x.size(), rate, 120.0, (juce::int64) (1.0 * rate));
            bool chorusAt12 = false;
            for (auto& s : r.sections)
                if (std::abs (s.startSample - (juce::int64) (1.0 * rate) - 12 * bar) <= bar / 2 && s.chorus)
                    chorusAt12 = true;
            expect (chorusAt12);
        }

        beginTest ("no chorus is named when nothing repeats louder (one chord progression at one level)");
        {
            const std::vector<Part> flat { { 32, verse, 0.3f } };
            const auto x = render (flat);
            const auto r = estimateStructure (x.data(), (juce::int64) x.size(), rate, 120.0, 0);
            for (auto& s : r.sections)
                expect (! s.chorus);
            expectEquals (r.chorusConfidence, 0.0f);
        }

        // 手元の曲で見る（VB_STRUCTURE_SONGS に wav のフォルダを指定したときだけ。結果は表示するだけで、合否は付けない）
        if (const auto dir = juce::SystemStats::getEnvironmentVariable ("VB_STRUCTURE_SONGS", {}); dir.isNotEmpty())
        {
            beginTest ("songs in VB_STRUCTURE_SONGS (shown only)");
            for (auto& f : juce::File (dir).findChildFiles (juce::File::findFiles, false, "*.wav"))
            {
                juce::WavAudioFormat wav;
                std::unique_ptr<juce::AudioFormatReader> r (wav.createReaderFor (f.createInputStream().release(), true));
                if (r == nullptr)
                    continue;
                juce::AudioBuffer<float> b ((int) r->numChannels, (int) r->lengthInSamples);
                r->read (&b, 0, (int) r->lengthInSamples, 0, true, true);
                std::vector<float> m ((size_t) b.getNumSamples(), 0.0f);
                for (int c = 0; c < b.getNumChannels(); ++c)
                    for (int i = 0; i < b.getNumSamples(); ++i)
                        m[(size_t) i] += b.getSample (c, i) / (float) b.getNumChannels();
                const auto t = estimateTempo (m.data(), (juce::int64) m.size(), r->sampleRate);
                const auto st = estimateStructure (m.data(), (juce::int64) m.size(), r->sampleRate, t.bpm, t.downbeatSample);
                juce::String line;
                for (auto& sec : st.sections)
                {
                    const auto sec10 = juce::roundToInt ((double) sec.startSample / r->sampleRate);
                    line << juce::String (sec10 / 60) << ":" << juce::String (sec10 % 60).paddedLeft ('0', 2) << (sec.chorus ? "c " : " ");
                }
                logMessage ("  " + juce::String (t.bpm, 1) + " BPM, chorus " + juce::String (st.chorusConfidence, 2) + ": " + line);
            }
        }

        beginTest ("nothing without a tempo, or when the song is too short");
        {
            const auto x = render (song);
            expect (estimateStructure (x.data(), (juce::int64) x.size(), rate, 0.0, 0).sections.empty());
            const auto shortSong = render ({ { 4, verse, 0.3f } });
            expect (estimateStructure (shortSong.data(), (juce::int64) shortSong.size(), rate, 120.0, 0).sections.empty());
        }
    }
};

static StructureTests structureTests;
} // namespace vb::analysis
