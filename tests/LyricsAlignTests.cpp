#include "song/LyricsAlign.h"

namespace vb::song
{
class LyricsAlignTests : public juce::UnitTest
{
public:
    LyricsAlignTests() : juce::UnitTest ("LyricsAlign", "VoiceBooth") {}

    // テスト用の歌詞（自作）
    static juce::StringArray lines()
    {
        juce::StringArray a;
        for (auto* t : { "しろい紙ひこうき 窓をこえて", "まだ名前のない 風にのる",
                         "きのうの約束 ポケットにしまって", "あしたの地図を 描きなおす" })
            a.add (juce::String (juce::CharPointer_UTF8 (t)));
        return a;
    }

    /** 歌詞の行をそのまま、starts の時刻に 1 文字 0.2 秒で歌ったことにする */
    static std::vector<RecognizedPiece> sung (const juce::StringArray& text, const std::vector<double>& starts)
    {
        std::vector<RecognizedPiece> out;
        for (int i = 0; i < text.size(); ++i)
        {
            const auto n = normaliseForAlign (text[i]).length();
            out.push_back ({ text[i], starts[(size_t) i], starts[(size_t) i] + 0.2 * n });
        }
        return out;
    }

    void runTest() override
    {
        beginTest ("normalise: width, case, katakana, punctuation");
        {
            expectEquals (normaliseForAlign (juce::String (CharPointer_UTF8 ("ＡＢｃ　ポケット、！ x-1"))), juce::String (CharPointer_UTF8 ("abcぽけっとx1")));
            expectEquals (normaliseForAlign (juce::String (CharPointer_UTF8 ("ラーメン"))), juce::String (CharPointer_UTF8 ("らーめん")));
        }

        beginTest ("exact recognition gives each line's start");
        {
            const std::vector<double> starts { 1.0, 5.0, 9.0, 13.0 };
            const auto r = alignLyrics (lines(), sung (lines(), starts));
            expectEquals ((int) r.size(), 4);
            for (size_t i = 0; i < 4; ++i)
            {
                expectWithinAbsoluteError (r[i].start, starts[i], 0.01);
                expect (! r[i].interpolated);
                expectWithinAbsoluteError (r[i].matched, 1.0f, 0.001f);
            }
        }

        beginTest ("mistakes, katakana, extra text in the interlude, pieces split differently");
        {
            std::vector<RecognizedPiece> rec {
                { CharPointer_UTF8 ("シロイカミヒコウキ"), 1.0, 2.8 },          // カタカナ・漢字をかなで
                { CharPointer_UTF8 ("窓を越えて"), 2.8, 3.8 },                   // 送り仮名違い
                { CharPointer_UTF8 ("（拍手）ありがとうございました"), 4.0, 4.8 }, // 間奏の作り話
                { CharPointer_UTF8 ("まだ名前の無い風に乗る"), 5.0, 7.2 },
                { CharPointer_UTF8 ("昨日の約束ポケットにしまって"), 9.0, 11.6 },
                { CharPointer_UTF8 ("明日の地図を描き直す"), 13.0, 14.8 },
            };
            const auto r = alignLyrics (lines(), rec);
            logMessage ("    starts: " + juce::String (r[0].start, 2) + ", " + juce::String (r[1].start, 2) + ", "
                        + juce::String (r[2].start, 2) + ", " + juce::String (r[3].start, 2) + "  (sung at 1, 5, 9, 13)");
            expectWithinAbsoluteError (r[1].start, 5.0, 0.3);
            expectWithinAbsoluteError (r[2].start, 9.0, 0.6);
            expect (r[0].start >= 0.0 && r[0].start < 5.0);
            expect (r[3].start > r[2].start && r[3].start < 15.0, juce::String (r[3].start));
            for (size_t i = 1; i < r.size(); ++i)
                expect (r[i].start > r[i - 1].start);   // 順番は崩さない
        }

        beginTest ("a line that wasn't recognised is filled in between its neighbours");
        {
            auto rec = sung (lines(), { 1.0, 5.0, 9.0, 13.0 });
            rec.erase (rec.begin() + 2);   // 3 行目が聞き取れなかった
            const auto r = alignLyrics (lines(), rec);
            expect (r[2].interpolated);
            expect (r[2].start > r[1].start && r[2].start < r[3].start, juce::String (r[2].start));
            expectWithinAbsoluteError (r[3].start, 13.0, 0.01);
        }

        beginTest ("nothing recognised: nothing guessed");
        {
            const auto r = alignLyrics (lines(), {});
            for (auto& t : r)
                expect (t.start < 0.0);
        }
    }

    using CharPointer_UTF8 = juce::CharPointer_UTF8;
};

static LyricsAlignTests lyricsAlignTests;
} // namespace vb::song
