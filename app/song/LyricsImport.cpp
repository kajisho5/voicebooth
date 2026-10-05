#include "LyricsImport.h"
#include <algorithm>
#include <map>
#include <vector>

/*  このファイルは区間の見出しの語（サビ・Aメロ…）を照合用のデータとして持つ。
    画面に出す文字ではないので tools/check_i18n.py の DATA_FILES に入れてある */

namespace vb::song
{
namespace
{
#include "Cp932Table.inc"

    constexpr juce::juce_wchar replacement = 0xFFFD;

    juce::String fromCodepoints (std::vector<juce::juce_wchar>& cps)
    {
        cps.push_back (0);
        return juce::String (juce::CharPointer_UTF32 (cps.data()));
    }

    /** 厳密な UTF-8 の 1 文字を読む。不正なら -1 を返して 1 バイト進める */
    int readUtf8 (const uint8_t* p, size_t size, size_t& i)
    {
        const auto b0 = p[i];
        if (b0 < 0x80) { ++i; return b0; }

        int need = 0;
        uint32_t cp = 0, min = 0;
        if ((b0 & 0xE0) == 0xC0)      { need = 1; cp = b0 & 0x1F; min = 0x80; }
        else if ((b0 & 0xF0) == 0xE0) { need = 2; cp = b0 & 0x0F; min = 0x800; }
        else if ((b0 & 0xF8) == 0xF0) { need = 3; cp = b0 & 0x07; min = 0x10000; }
        else { ++i; return -1; }

        if (i + (size_t) need >= size) { ++i; return -1; }   // 途中で切れている
        for (int k = 1; k <= need; ++k)
        {
            const auto b = p[i + (size_t) k];
            if ((b & 0xC0) != 0x80) { ++i; return -1; }
            cp = (cp << 6) | (b & 0x3F);
        }
        if (cp < min || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) { ++i; return -1; }
        i += (size_t) need + 1;
        return (int) cp;
    }

    bool isValidUtf8 (const uint8_t* p, size_t size)
    {
        for (size_t i = 0; i < size;)
            if (readUtf8 (p, size, i) < 0)
                return false;
        return true;
    }

    juce::String decodeUtf8 (const uint8_t* p, size_t size)
    {
        std::vector<juce::juce_wchar> cps;
        cps.reserve (size);
        for (size_t i = 0; i < size;)
        {
            const auto cp = readUtf8 (p, size, i);
            cps.push_back (cp < 0 ? replacement : (juce::juce_wchar) cp);
        }
        return fromCodepoints (cps);
    }

    juce::String decodeUtf16 (const uint8_t* p, size_t size, bool bigEndian)
    {
        std::vector<juce::juce_wchar> cps;
        cps.reserve (size / 2);
        auto unit = [&] (size_t i) -> uint32_t
        {
            return bigEndian ? (uint32_t) ((p[i] << 8) | p[i + 1]) : (uint32_t) (p[i] | (p[i + 1] << 8));
        };
        for (size_t i = 0; i + 1 < size; i += 2)
        {
            const auto u = unit (i);
            if (u >= 0xD800 && u <= 0xDBFF && i + 3 < size)
            {
                const auto lo = unit (i + 2);
                if (lo >= 0xDC00 && lo <= 0xDFFF)
                {
                    cps.push_back ((juce::juce_wchar) (0x10000 + ((u - 0xD800) << 10) + (lo - 0xDC00)));
                    i += 2;
                    continue;
                }
            }
            cps.push_back ((u >= 0xD800 && u <= 0xDFFF) ? replacement : (juce::juce_wchar) u);
        }
        return fromCodepoints (cps);
    }

    int leadIndex (uint8_t b)
    {
        if (b >= 0x81 && b <= 0x9F) return b - 0x81;
        if (b >= 0xE0 && b <= 0xFC) return b - 0xE0 + 31;
        return -1;
    }

    /** CP932 の 2 バイト文字。無ければ 0 */
    uint32_t cp932Pair (uint8_t lead, uint8_t trail)
    {
        const auto li = leadIndex (lead);
        if (li < 0 || trail < 0x40 || trail > 0xFC || trail == 0x7F)
            return 0;
        return cp932Table[li * cp932TrailCount + (trail - 0x40)];
    }

    juce::String decodeShiftJis (const uint8_t* p, size_t size)
    {
        std::vector<juce::juce_wchar> cps;
        cps.reserve (size);
        for (size_t i = 0; i < size;)
        {
            const auto b = p[i];
            if (b < 0x80) { cps.push_back (b); ++i; continue; }
            if (b >= 0xA1 && b <= 0xDF) { cps.push_back ((juce::juce_wchar) (0xFF61 + (b - 0xA1))); ++i; continue; }   // 半角カナ
            if (leadIndex (b) >= 0 && i + 1 < size)
            {
                const auto cp = cp932Pair (b, p[i + 1]);
                if (cp != 0) { cps.push_back ((juce::juce_wchar) cp); i += 2; continue; }
            }
            cps.push_back (replacement);
            ++i;
        }
        return fromCodepoints (cps);
    }

    //==========================================================================
    /** 全角の英数字・記号を半角に、空白を消し、英字は小文字に（見出しの照合用） */
    juce::String normaliseForMatch (const juce::String& s)
    {
        juce::String out;
        for (auto c : s)
        {
            if (c >= 0xFF01 && c <= 0xFF5E) c = c - 0xFF01 + 0x21;
            if (c == 0x3000 || juce::CharacterFunctions::isWhitespace (c)) continue;
            out << juce::String::charToString (juce::CharacterFunctions::toLowerCase (c));
        }
        return out;
    }

    /** 区間を表す語（照合用。normaliseForMatch 済みの形） */
    const juce::StringArray& sectionWords()
    {
        static const char* const utf8Words[] {
            // 日本語
            "イントロ", "前奏", "aメロ", "bメロ", "cメロ", "dメロ", "サビ", "プレサビ", "落ちサビ", "大サビ", "ラスサビ",
            "間奏", "ブリッジ", "アウトロ", "後奏",
            // 英語
            "intro", "verse", "pre-chorus", "prechorus", "chorus", "hook", "refrain", "bridge", "interlude",
            "instrumental", "breakdown", "drop", "outro",
            // 韓国語
            "인트로", "벌스", "프리코러스", "코러스", "후렴", "브릿지", "간주", "아웃트로",
            // 中国語
            "主歌", "副歌", "间奏", "桥段", "橋段", "尾奏", "导歌", "導歌",
            // スペイン語・ポルトガル語（画面の説明が [Estribillo] を例に出している。2026-10-03）
            "estribillo", "coro", "estrofa", "verso", "precoro", "puente", "interludio",
            "refrão", "pré-refrão", "ponte"
        };
        static const juce::StringArray words = []
        {
            juce::StringArray w;
            for (auto* s : utf8Words)
                w.add (juce::String::fromUTF8 (s));   // 文字列リテラルは UTF-8 として読む
            return w;
        }();
        return words;
    }

    /** 末尾の番号（2 / ２ / #2 / -2 / ②）を外す */
    juce::String stripNumber (juce::String s)
    {
        for (;;)
        {
            const auto last = s.getLastCharacter();
            const bool digit = juce::CharacterFunctions::isDigit (last) || (last >= 0x2460 && last <= 0x2473);
            if (s.isNotEmpty() && (digit || last == '#' || last == '-' || last == '.' || last == '_'))
                s = s.dropLastCharacters (1);
            else
                return s;
        }
    }

    struct Bracket { juce::juce_wchar open, close; };
    constexpr Bracket brackets[] = {
        { 0x3010, 0x3011 },   // 【】
        { '[', ']' }, { 0xFF3B, 0xFF3D },   // [] ［］
        { '(', ')' }, { 0xFF08, 0xFF09 },   // () （）
        { 0x3014, 0x3015 },   // 〔〕
        { 0x300A, 0x300B },   // 《》
        { '<', '>' }, { 0x3008, 0x3009 },   // <> 〈〉
    };

    //==========================================================================
    /** "mm:ss.xx" / "mm:ss:xx" / "mm:ss" を秒に。違えば負 */
    double parseLrcTime (const juce::String& tag)
    {
        const auto colon = tag.indexOfChar (':');
        if (colon <= 0)
            return -1.0;
        const auto minutes = tag.substring (0, colon);
        auto rest = tag.substring (colon + 1);
        if (! minutes.containsOnly ("0123456789") || rest.isEmpty())
            return -1.0;

        // mm:ss:xx の形（区切りがコロン）も受け付ける
        auto restDot = rest.replaceCharacter (':', '.');
        if (! restDot.containsOnly ("0123456789.") || restDot.indexOfChar ('.') != restDot.lastIndexOfChar ('.'))
            return -1.0;
        const auto secs = restDot.getDoubleValue();
        if (secs >= 60.0)
            return -1.0;
        return minutes.getIntValue() * 60.0 + secs;
    }
}

//==============================================================================
const char* encodingName (TextEncoding e)
{
    switch (e)
    {
        case TextEncoding::utf8:     return "UTF-8";
        case TextEncoding::utf8Bom:  return "UTF-8 (BOM)";
        case TextEncoding::utf16le:  return "UTF-16 LE";
        case TextEncoding::utf16be:  return "UTF-16 BE";
        case TextEncoding::shiftJis: return "Shift_JIS";
    }
    return "";
}

bool isValidShiftJis (const void* data, size_t size)
{
    const auto* p = static_cast<const uint8_t*> (data);
    for (size_t i = 0; i < size;)
    {
        const auto b = p[i];
        if (b < 0x80 || (b >= 0xA1 && b <= 0xDF)) { ++i; continue; }
        if (leadIndex (b) < 0 || i + 1 >= size || cp932Pair (b, p[i + 1]) == 0)
            return false;
        i += 2;
    }
    return true;
}

TextEncoding detectEncoding (const void* data, size_t size)
{
    const auto* p = static_cast<const uint8_t*> (data);
    if (size >= 3 && p[0] == 0xEF && p[1] == 0xBB && p[2] == 0xBF) return TextEncoding::utf8Bom;
    if (size >= 2 && p[0] == 0xFF && p[1] == 0xFE)                  return TextEncoding::utf16le;
    if (size >= 2 && p[0] == 0xFE && p[1] == 0xFF)                  return TextEncoding::utf16be;
    // BOM の無い UTF-16：ふつうの文章に 0 のバイトは無い。0 が奇数番目に偏っていれば LE、偶数番目なら BE
    // （改行・空白・英数字の上の桁が 0 になる。前は UTF-8 と判定し、最初の 0 で文字列が終わって 1 文字になった。バグチェック 2026-10-05）
    {
        size_t zeroEven = 0, zeroOdd = 0;
        const auto n = std::min (size, (size_t) 8192) & ~(size_t) 1;
        for (size_t i = 0; i < n; ++i)
            if (p[i] == 0)
                ++((i & 1) != 0 ? zeroOdd : zeroEven);
        const auto pairs = n / 2;
        if (pairs >= 2 && zeroOdd + zeroEven >= std::max ((size_t) 1, pairs / 50))
        {
            if (zeroOdd >= 9 * zeroEven)  return TextEncoding::utf16le;
            if (zeroEven >= 9 * zeroOdd)  return TextEncoding::utf16be;
        }
    }
    if (isValidUtf8 (p, size))                                      return TextEncoding::utf8;      // ASCII だけもここ
    if (isValidShiftJis (p, size))                                  return TextEncoding::shiftJis;
    return TextEncoding::utf8;   // どれでもなければ UTF-8 として読み、壊れた所は U+FFFD
}

juce::String decodeText (const void* data, size_t size, TextEncoding e)
{
    const auto* p = static_cast<const uint8_t*> (data);
    switch (e)
    {
        case TextEncoding::utf8Bom:  return size >= 3 ? decodeUtf8 (p + 3, size - 3) : juce::String();
        // BOM は有るときだけ飛ばす（BOM の無い UTF-16 もある）
        case TextEncoding::utf16le:  { const auto skip = size >= 2 && p[0] == 0xFF && p[1] == 0xFE ? 2u : 0u; return decodeUtf16 (p + skip, size - skip, false); }
        case TextEncoding::utf16be:  { const auto skip = size >= 2 && p[0] == 0xFE && p[1] == 0xFF ? 2u : 0u; return decodeUtf16 (p + skip, size - skip, true); }
        case TextEncoding::shiftJis: return decodeShiftJis (p, size);
        case TextEncoding::utf8:     break;
    }
    return decodeUtf8 (p, size);
}

juce::String sectionHeadingOf (const juce::String& line)
{
    auto s = line.trim();
    if (s.isEmpty() || s.length() > 24)
        return {};

    bool bracketed = false;
    for (const auto& b : brackets)
        if (s.length() >= 3 && s[0] == b.open && s.getLastCharacter() == b.close)
        {
            s = s.substring (1, s.length() - 1).trim();
            bracketed = true;
            break;
        }

    // 「Chorus:」「サビ：」の形
    if (! bracketed && (s.endsWithChar (':') || s.endsWithChar (0xFF1A)))
        s = s.dropLastCharacters (1).trim();

    if (s.isEmpty())
        return {};

    const auto key = stripNumber (normaliseForMatch (s));
    return sectionWords().contains (key) ? s : juce::String();
}

juce::String sectionKindOfHeading (const juce::String& heading)
{
    // 見出しの語 → 区間の種類（SongInfo.h の kind）。Aメロ ≒ verse、Bメロ ≒ pre-chorus、Cメロ ≒ bridge とみなす
    struct WordKind { const char* word; const char* kind; };
    static const WordKind table[] {
        { "イントロ", "intro" }, { "前奏", "intro" }, { "intro", "intro" }, { "인트로", "intro" },
        { "aメロ", "verseA" }, { "verse", "verseA" }, { "벌스", "verseA" }, { "主歌", "verseA" },
        { "bメロ", "verseB" }, { "プレサビ", "verseB" }, { "pre-chorus", "verseB" }, { "prechorus", "verseB" },
        { "프리코러스", "verseB" }, { "导歌", "verseB" }, { "導歌", "verseB" },
        { "サビ", "chorus" }, { "chorus", "chorus" }, { "hook", "chorus" }, { "refrain", "chorus" },
        { "코러스", "chorus" }, { "후렴", "chorus" }, { "副歌", "chorus" },
        { "間奏", "interlude" }, { "interlude", "interlude" }, { "instrumental", "interlude" }, { "간주", "interlude" },
        { "间奏", "interlude" },
        { "cメロ", "verseC" }, { "ブリッジ", "verseC" }, { "bridge", "verseC" }, { "브릿지", "verseC" },
        { "桥段", "verseC" }, { "橋段", "verseC" },
        { "落ちサビ", "dropChorus" },
        { "大サビ", "lastChorus" }, { "ラスサビ", "lastChorus" },
        { "アウトロ", "outro" }, { "後奏", "outro" }, { "outro", "outro" }, { "아웃트로", "outro" }, { "尾奏", "outro" },
        // スペイン語・ポルトガル語
        { "estribillo", "chorus" }, { "coro", "chorus" }, { "refrão", "chorus" },
        { "estrofa", "verseA" }, { "verso", "verseA" },
        { "precoro", "verseB" }, { "pré-refrão", "verseB" },
        { "puente", "verseC" }, { "ponte", "verseC" },
        { "interludio", "interlude" },
    };

    const auto key = stripNumber (normaliseForMatch (heading));
    for (const auto& w : table)
        if (key == juce::String::fromUTF8 (w.word))
            return w.kind;
    return {};
}

LyricsDoc parseLyrics (const juce::String& text)
{
    LyricsDoc doc;
    const auto normalised = text.replace ("\r\n", "\n").replaceCharacter ('\r', '\n');
    juce::StringArray rawLines;
    rawLines.addLines (normalised);

    int block = 0;
    bool blockHasLines = false;
    auto newBlock = [&]
    {
        if (blockHasLines) { ++block; blockHasLines = false; }
    };

    for (auto raw : rawLines)
    {
        auto line = raw.trim();

        // .lrc のタグ（行頭の [..] を順に読む）
        juce::Array<double> times;
        bool metaOnly = false;
        while (line.startsWithChar ('['))
        {
            const auto close = line.indexOfChar (']');
            if (close < 0)
                break;
            const auto tag = line.substring (1, close).trim();
            const auto t = parseLrcTime (tag);
            if (t >= 0.0)
                times.add (t);
            else if (tag.containsChar (':') && tag.upToFirstOccurrenceOf (":", false, false).containsOnly ("abcdefghijklmnopqrstuvwxyz#"))
            {
                const auto name = tag.upToFirstOccurrenceOf (":", false, false);
                if (name == "offset")
                    doc.offsetSeconds = tag.fromFirstOccurrenceOf (":", false, false).trim().trimCharactersAtStart ("+").getIntValue() / 1000.0;
                metaOnly = true;
            }
            else
                break;   // [Chorus] などは見出しの判定へ
            line = line.substring (close + 1).trim();
        }

        if (metaOnly && times.isEmpty() && line.isEmpty())
            continue;

        if (line.isEmpty())
        {
            if (times.isEmpty())
                newBlock();
            continue;   // 時刻だけの行（間奏の印）は歌詞にしない
        }

        const auto heading = sectionHeadingOf (line);
        if (heading.isNotEmpty())
        {
            newBlock();
            doc.sections.add ({ heading, doc.lines.size() });
            continue;
        }

        if (! times.isEmpty())
            doc.timed = true;

        const auto section = doc.sections.size() - 1;
        if (times.isEmpty())
            doc.lines.add ({ line, -1.0, block, section });
        else
            for (auto t : times)
                doc.lines.add ({ line, t, block, section });
        blockHasLines = true;
    }

    // 時刻：offset を反映し、時刻順に（時刻の無い行は直前の行の位置に留める）
    if (doc.timed)
    {
        double last = 0.0;
        struct Keyed { double key; int order; LyricLine line; };
        std::vector<Keyed> keyed;
        for (int i = 0; i < doc.lines.size(); ++i)
        {
            auto l = doc.lines[i];
            if (l.hasTime())
            {
                l.timeSeconds = juce::jmax (0.0, l.timeSeconds - doc.offsetSeconds);
                last = l.timeSeconds;
            }
            keyed.push_back ({ l.hasTime() ? l.timeSeconds : last, i, l });
        }
        std::stable_sort (keyed.begin(), keyed.end(), [] (const Keyed& a, const Keyed& b) { return a.key < b.key; });
        // 見出しの「次の行」を並べ替えた後の行番号に付け直す（前は並べ替える前の番号のままで、1 行に時刻が 2 つある lrc
        // などで見出しが違う行に付き、区間が作られない・歌詞を保存すると構造が崩れた。バグチェック 2026-10-05）。
        // その見出しに属する行のうち、並べ替えた後でいちばん前の行。属する行が無ければ、元の次の行の新しい位置
        std::vector<int> newIndexOf ((size_t) doc.lines.size(), 0);
        for (int i = 0; i < (int) keyed.size(); ++i)
            newIndexOf[(size_t) keyed[(size_t) i].order] = i;
        for (int sIdx = 0; sIdx < doc.sections.size(); ++sIdx)
        {
            int first = -1;
            for (int i = 0; i < (int) keyed.size() && first < 0; ++i)
                if (keyed[(size_t) i].line.section == sIdx)
                    first = i;
            auto& sec = doc.sections.getReference (sIdx);
            if (first >= 0)
                sec.firstLine = first;
            else if (sec.firstLine < (int) newIndexOf.size())
                sec.firstLine = newIndexOf[(size_t) sec.firstLine];
        }
        doc.lines.clearQuick();
        for (auto& k : keyed)
            doc.lines.add (k.line);
    }

    // 同じ中身の塊（2 行以上）が 2 回以上 → サビの候補
    std::map<juce::String, juce::Array<int>> byText;
    {
        std::map<int, juce::String> blockText;
        std::map<int, int> blockLines;
        for (const auto& l : doc.lines)
        {
            // 句読点・記号の違いは無視（、。！？・… と半角の記号）
            static const juce::String punctuation (juce::CharPointer_UTF8 (",.!?~\xe3\x80\x81\xe3\x80\x82\xe3\x83\xbb\xe2\x80\xa6"));
            auto key = normaliseForMatch (l.text).removeCharacters (punctuation);
            blockText[l.block] << key << "\n";
            ++blockLines[l.block];
        }
        for (const auto& [b, t] : blockText)
            if (blockLines[b] >= 2)
                byText[t].add (b);
    }
    for (const auto& [t, blocks] : byText)
        if (blocks.size() >= 2)
            doc.chorusCandidateBlocks.addArray (blocks);
    doc.chorusCandidateBlocks.sort();

    return doc;
}

LoadedLyrics loadLyricsFile (const juce::File& file)
{
    LoadedLyrics r;
    juce::MemoryBlock mb;
    if (! file.existsAsFile() || file.getSize() > 4 * 1024 * 1024 || ! file.loadFileAsData (mb))
        return r;

    r.encoding = detectEncoding (mb.getData(), mb.getSize());
    r.doc = parseLyrics (decodeText (mb.getData(), mb.getSize(), r.encoding));
    r.ok = true;
    return r;
}
} // namespace vb::song
