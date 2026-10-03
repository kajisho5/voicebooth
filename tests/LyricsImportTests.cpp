#include "song/LyricsImport.h"

/*  歌詞の読み込み（DESIGN 7.5.3）。歌詞はすべて見本用に書いた架空のもの */

namespace vb::song
{
class LyricsImportTests : public juce::UnitTest
{
public:
    LyricsImportTests() : juce::UnitTest ("LyricsImport", "VoiceBooth") {}

    static juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }

    void runTest() override
    {
        // "【サビ】\r\n夜の窓に灯り\r\nｱｲｳ\r\n" を CP932（Shift_JIS）で
        const uint8_t sjis[] = { 0x81, 0x79, 0x83, 0x54, 0x83, 0x72, 0x81, 0x7A, 0x0D, 0x0A, 0x96, 0xE9, 0x82, 0xCC,
                                 0x91, 0x8B, 0x82, 0xC9, 0x93, 0x94, 0x82, 0xE8, 0x0D, 0x0A, 0xB1, 0xB2, 0xB3, 0x0D, 0x0A };

        beginTest ("encoding: Shift_JIS is detected and decoded (same table on every OS)");
        {
            expect (detectEncoding (sjis, sizeof (sjis)) == TextEncoding::shiftJis);
            expect (isValidShiftJis (sjis, sizeof (sjis)));
            const auto text = decodeText (sjis, sizeof (sjis), TextEncoding::shiftJis);
            expectEquals (text, u8 ("【サビ】\r\n夜の窓に灯り\r\nｱｲｳ\r\n"));
        }

        beginTest ("encoding: UTF-8 with / without BOM, UTF-16 LE / BE");
        {
            const auto s = u8 ("夜の窓に灯り 🎤\n");
            const auto utf8 = s.toStdString();
            expect (detectEncoding (utf8.data(), utf8.size()) == TextEncoding::utf8);
            expectEquals (decodeText (utf8.data(), utf8.size(), TextEncoding::utf8), s);

            const auto bom = std::string ("\xEF\xBB\xBF") + utf8;
            expect (detectEncoding (bom.data(), bom.size()) == TextEncoding::utf8Bom);
            expectEquals (decodeText (bom.data(), bom.size(), TextEncoding::utf8Bom), s);

            for (const bool big : { false, true })
            {
                juce::MemoryOutputStream out;
                out.writeByte ((char) (big ? 0xFE : 0xFF));
                out.writeByte ((char) (big ? 0xFF : 0xFE));
                const auto* p = s.toUTF16().getAddress();
                for (; *p != 0; ++p)
                {
                    if (big) out.writeShortBigEndian ((short) *p);
                    else     out.writeShort ((short) *p);
                }
                const auto block = out.getMemoryBlock();
                const auto e = detectEncoding (block.getData(), block.getSize());
                expect (e == (big ? TextEncoding::utf16be : TextEncoding::utf16le));
                expectEquals (decodeText (block.getData(), block.getSize(), e), s);   // 絵文字（サロゲートペア）も
            }

            const char ascii[] = "hello\n";
            expect (detectEncoding (ascii, 6) == TextEncoding::utf8);
        }

        beginTest ("encoding: broken bytes become U+FFFD instead of crashing");
        {
            const uint8_t broken[] = { 'a', 0xFF, 0xFE, 0x80, 'b' };   // UTF-8 でも Shift_JIS でもない（BOM は先頭だけ）
            const uint8_t neither[] = { 'a', 0x80, 'b' };   // 0x80 は UTF-8 でも Shift_JIS でも不正 → UTF-8 として読む
            expect (detectEncoding (neither, sizeof (neither)) == TextEncoding::utf8);
            const auto t = decodeText (neither, sizeof (neither), TextEncoding::utf8);
            expectEquals (t.length(), 3);
            expect (t[1] == 0xFFFD);
            expect (! isValidShiftJis (broken + 1, 3));
            const auto u = decodeText (broken, sizeof (broken), TextEncoding::shiftJis);
            expect (u.startsWithChar ('a') && u.endsWithChar ('b'));
        }

        beginTest ("headings: section names become sections, ad-libs stay lyrics");
        {
            for (auto* h : { "【サビ】", "[Chorus]", "(Aメロ)", "（Ｂメロ）", "サビ2", "【サビ ２】", "Verse 1:", "[Pre-Chorus]",
                             "〔落ちサビ〕", "大サビ", "[후렴]", "【副歌】", "Outro" })
                expect (sectionHeadingOf (u8 (h)).isNotEmpty(), u8 (h));

            for (auto* l : { "(Yeah)", "（ah）", "[Intro feat. someone]", "夜の窓に灯り", "サビの前で息を吸う", "" })
                expect (sectionHeadingOf (u8 (l)).isEmpty(), u8 (l));

            expectEquals (sectionHeadingOf (u8 ("【サビ】")), u8 ("サビ"));
            expectEquals (sectionHeadingOf (u8 ("Verse 1:")), u8 ("Verse 1"));

            // スペイン語・ポルトガル語の見出し（画面の説明が [Estribillo] を例に出している）
            for (auto* h : { "[Estribillo]", "[Coro 2]", "[Estrofa 1]", "Puente:", "[Refrão]", "[Pré-refrão]", "[Ponte]" })
                expect (sectionHeadingOf (u8 (h)).isNotEmpty(), u8 (h));
            expectEquals (sectionKindOfHeading (u8 ("Estribillo")), juce::String ("chorus"));
            expectEquals (sectionKindOfHeading (u8 ("Refrão")), juce::String ("chorus"));
            expectEquals (sectionKindOfHeading (u8 ("Estrofa 2")), juce::String ("verseA"));
            expectEquals (sectionKindOfHeading (u8 ("Puente")), juce::String ("verseC"));
        }

        beginTest ("plain text: lines, blocks, sections, chorus candidates");
        {
            const auto doc = parseLyrics (u8 (
                "【Aメロ】\n夜の窓に 小さな灯り\n息をととのえて 待つ\n\n"
                "【サビ】\n声を重ねて 遠くまで\n今日の歌を 渡しにいく\n\n"
                "(Yeah)\n短い間奏の合図\n\n"
                "【サビ】\n声を重ねて、遠くまで\n今日の歌を 渡しにいく！\n"));

            expectEquals (doc.lines.size(), 8);
            expect (! doc.timed);
            expectEquals (doc.sections.size(), 3);
            expectEquals (doc.sections[0].name, u8 ("Aメロ"));
            expectEquals (doc.sections[1].name, u8 ("サビ"));
            expectEquals (doc.sections[1].firstLine, 2);
            expectEquals (doc.lines[4].text, u8 ("(Yeah)"));        // 合いの手は歌詞のまま
            expectEquals (doc.lines[2].section, 1);
            expectEquals (doc.lines[6].section, 2);

            // 2 つのサビ（句読点の違いは無視）が候補。1 回だけの塊は候補にしない
            expectEquals (doc.chorusCandidateBlocks.size(), 2);
            expectEquals (doc.chorusCandidateBlocks[0], doc.lines[2].block);
            expectEquals (doc.chorusCandidateBlocks[1], doc.lines[6].block);
        }

        beginTest ("lrc: times, offset, multiple stamps, metadata, sorting");
        {
            const auto doc = parseLyrics (u8 (
                "[ti:見本の歌]\n[ar:だれか]\n[offset:+500]\n"
                "[00:12.50]夜の窓に 小さな灯り\n"
                "[00:30.00][01:10.25]声を重ねて 遠くまで\n"
                "[00:20.0]息をととのえて 待つ\n"
                "[00:40.00]\n"
                "[1:05:50]今日の歌を 渡しにいく\n"));

            expect (doc.timed);
            expectWithinAbsoluteError (doc.offsetSeconds, 0.5, 1e-9);
            expectEquals (doc.lines.size(), 5);   // 時刻 2 つの行は複製、時刻だけの行は歌詞にしない
            expectWithinAbsoluteError (doc.lines[0].timeSeconds, 12.0, 1e-6);
            expectEquals (doc.lines[1].text, u8 ("息をととのえて 待つ"));     // 時刻順
            expectWithinAbsoluteError (doc.lines[1].timeSeconds, 19.5, 1e-6);
            expectWithinAbsoluteError (doc.lines[2].timeSeconds, 29.5, 1e-6);
            expectWithinAbsoluteError (doc.lines[3].timeSeconds, 65.0, 1e-6);   // [1:05:50] の形
            expectWithinAbsoluteError (doc.lines[4].timeSeconds, 69.75, 1e-6);
            expectEquals (doc.lines[4].text, doc.lines[2].text);
        }

        beginTest ("file: Shift_JIS txt on disk (path with spaces and Japanese)");
        {
            auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile (u8 ("vb 歌詞テスト"));
            dir.createDirectory();
            const auto f = dir.getChildFile (u8 ("見本 lyrics.txt"));
            f.replaceWithData (sjis, sizeof (sjis));

            const auto r = loadLyricsFile (f);
            expect (r.ok);
            expect (r.encoding == TextEncoding::shiftJis);
            expectEquals (r.doc.sections.size(), 1);
            expectEquals (r.doc.lines.size(), 2);
            expectEquals (r.doc.lines[0].text, u8 ("夜の窓に灯り"));
            dir.deleteRecursively();

            expect (! loadLyricsFile (dir.getChildFile ("missing.txt")).ok);
        }
    }
};

static LyricsImportTests lyricsImportTests;
} // namespace vb::song
