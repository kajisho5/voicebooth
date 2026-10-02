#include "analysis/KeySuggest.h"
#include <juce_core/juce_core.h>

namespace vb::analysis
{
class KeySuggestTests : public juce::UnitTest
{
public:
    KeySuggestTests() : juce::UnitTest ("KeySuggest", "VoiceBooth") {}

    /** low..high を均等に埋めた点（10 ms ごとの声の点の代わり）に、外れ値を少し混ぜる */
    static std::vector<float> points (float low, float high, int outliers = 0)
    {
        std::vector<float> v;
        for (int i = 0; i < 400; ++i)
            v.push_back (low + (high - low) * (float) i / 399.0f);
        for (int i = 0; i < outliers; ++i)
            v.push_back (i % 2 == 0 ? low - 12.0f : high + 12.0f);
        for (int i = 0; i < 100; ++i)
            v.push_back (0.0f);   // 無声は数えない
        return v;
    }

    void runTest() override
    {
        beginTest ("song range ignores unvoiced points and a few outliers");
        {
            const auto r = songRange (points (57.0f, 69.0f, 6));
            expect (r.known);
            expectWithinAbsoluteError (r.low, 57.0f, 0.6f);
            expectWithinAbsoluteError (r.high, 69.0f, 0.6f);
            expect (! songRange (std::vector<float> (40, 60.0f)).known, "too few points");
        }

        beginTest ("already fits: keep the original key");
        {
            const auto k = suggestKey (songRange (points (57.0f, 69.0f)), 52, 72);
            expect (k.ok && k.fits);
            expectEquals (k.shift, 0);
            expectEquals (k.octave, 0);
        }

        beginTest ("too high: lower just enough, not more");
        {
            // A3..A4 の曲、声域 E3..F4（上が 4 半音足りない）→ -4（下は E3 で収まる）
            const auto k = suggestKey (songRange (points (57.0f, 69.0f)), 52, 65);
            expect (k.fits);
            expectEquals (k.shift, -4);
            expectEquals (k.octave, 0);
        }

        beginTest ("lowering would lose the low notes: report how far it sticks out");
        {
            // 2 オクターブの曲に 1 オクターブ半の声域：どのキーでも収まらない。はみ出しの合計 = 曲の広がり − 声域の広がり
            const auto song = songRange (points (55.0f, 79.0f));
            const auto k = suggestKey (song, 57, 76);
            expect (k.ok && ! k.fits);
            expectWithinAbsoluteError (k.overLow + k.overHigh, (song.high - song.low) - 19.0f, 0.7f);
            expect (k.overLow > 0.0f && k.overHigh > 0.0f, "split between low and high, not all on one side");
        }

        beginTest ("female song, male voice: an octave lower fits better than any key");
        {
            // C4..E5 の曲、声域 C3..G4
            const auto k = suggestKey (songRange (points (60.0f, 76.0f)), 48, 67);
            expect (k.fits);
            expectEquals (k.octave, -1);
            expectEquals (k.shift, 0);
        }

        beginTest ("range limits are respected");
        {
            const auto k = suggestKey (songRange (points (70.0f, 80.0f)), 50, 56, -6, 6);   // 声域が狭すぎる
            expect (! k.fits);
            expect (k.shift >= -6 && k.shift <= 6);
            expect (! suggestKey (songRange (points (60.0f, 70.0f)), -1, 70).ok, "no voice range yet");
        }
    }
};

static KeySuggestTests keySuggestTests;
} // namespace vb::analysis
