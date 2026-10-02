#include "LyricsAlign.h"

namespace vb::song
{
namespace
{
    constexpr double secondsPerChar = 0.12;

    juce::juce_wchar fold (juce::juce_wchar c)
    {
        if (c >= 0xFF01 && c <= 0xFF5E) c = (juce::juce_wchar) (c - 0xFEE0);    // 全角英数・記号 → 半角
        if (c >= 'A' && c <= 'Z') c = (juce::juce_wchar) (c + 32);
        if (c >= 0x30A1 && c <= 0x30F6) c = (juce::juce_wchar) (c - 0x60);       // カタカナ → ひらがな
        return c;
    }

    bool keep (juce::juce_wchar c)
    {
        if (c == 0x30FC) return true;                                          // 長音「ー」は歌の文字として残す
        if (juce::CharacterFunctions::isLetterOrDigit (c)) return true;
        if (c >= 0x3041 && c <= 0x3096) return true;                           // ひらがな
        if (c >= 0x4E00 && c <= 0x9FFF) return true;                           // 漢字
        if (c >= 0x3400 && c <= 0x4DBF) return true;
        if (c == 0x3005) return true;                                          // 々
        if (c >= 0xAC00 && c <= 0xD7A3) return true;                           // ハングル
        return false;
    }
}

juce::String normaliseForAlign (const juce::String& s)
{
    juce::String out;
    for (auto p = s.getCharPointer(); ! p.isEmpty(); ++p)
    {
        const auto c = fold (*p);
        if (keep (c))
            out += juce::String::charToString (c);
    }
    return out;
}

std::vector<LineTiming> alignLyrics (const juce::StringArray& lines, const std::vector<RecognizedPiece>& recognized)
{
    std::vector<LineTiming> result ((size_t) lines.size());

    // 歌詞の文字と、その文字の行
    std::vector<juce::juce_wchar> a;
    std::vector<int> aLine;
    std::vector<int> lineChars ((size_t) lines.size(), 0);
    for (int i = 0; i < lines.size(); ++i)
    {
        const auto n = normaliseForAlign (lines[i]);
        for (auto p = n.getCharPointer(); ! p.isEmpty(); ++p)
        {
            a.push_back (*p);
            aLine.push_back (i);
            ++lineChars[(size_t) i];
        }
    }

    // 認識の文字と、その時刻（区切りの中で等分）
    std::vector<juce::juce_wchar> b;
    std::vector<double> bTime;
    for (auto& r : recognized)
    {
        const auto n = normaliseForAlign (r.text);
        const auto len = n.length();
        if (len == 0) continue;
        const auto step = juce::jmax (0.0, r.end - r.start) / len;
        int k = 0;
        for (auto p = n.getCharPointer(); ! p.isEmpty(); ++p, ++k)
        {
            b.push_back (*p);
            bTime.push_back (r.start + step * k);
        }
    }

    const auto na = (int) a.size(), nb = (int) b.size();
    if (na == 0 || nb == 0 || (size_t) na * (size_t) nb > 40'000'000)   // 表が大きすぎる（普通の曲は 100 万程度）
        return result;

    // 編集距離の表（行 = 歌詞、列 = 認識）。戻り道は 1 バイトずつ：0 一致/置換、1 歌詞だけ進む、2 認識だけ進む
    std::vector<int> prev ((size_t) nb + 1), cur ((size_t) nb + 1);
    std::vector<uint8_t> from ((size_t) (na + 1) * (size_t) (nb + 1));
    auto at = [nb] (int i, int j) { return (size_t) i * (size_t) (nb + 1) + (size_t) j; };
    for (int j = 0; j <= nb; ++j) { prev[(size_t) j] = j; from[at (0, j)] = 2; }
    for (int i = 1; i <= na; ++i)
    {
        cur[0] = i;
        from[at (i, 0)] = 1;
        for (int j = 1; j <= nb; ++j)
        {
            const int diag = prev[(size_t) j - 1] + (a[(size_t) i - 1] == b[(size_t) j - 1] ? 0 : 1);
            const int up = prev[(size_t) j] + 1, left = cur[(size_t) j - 1] + 1;
            int best = diag; uint8_t f = 0;
            if (up < best)   { best = up;   f = 1; }
            if (left < best) { best = left; f = 2; }
            cur[(size_t) j] = best;
            from[at (i, j)] = f;
        }
        std::swap (prev, cur);
    }

    // 戻り道：一致した歌詞の文字 → 認識の時刻
    std::vector<double> matchTime ((size_t) na, -1.0);
    for (int i = na, j = nb; i > 0 || j > 0;)
    {
        const auto f = from[at (i, j)];
        if (i > 0 && j > 0 && f == 0)
        {
            if (a[(size_t) i - 1] == b[(size_t) j - 1])
                matchTime[(size_t) i - 1] = bTime[(size_t) j - 1];
            --i; --j;
        }
        else if (i > 0 && (f == 1 || j == 0)) --i;
        else --j;
    }

    // 行ごと：最初に一致した文字の時刻から、その前の文字の分を戻す
    std::vector<int> matchedCount ((size_t) lines.size(), 0);
    std::vector<int> firstPos ((size_t) lines.size(), -1);
    std::vector<double> firstTime ((size_t) lines.size(), -1.0);
    std::vector<int> posInLine ((size_t) lines.size(), 0);
    for (int i = 0; i < na; ++i)
    {
        const auto l = (size_t) aLine[(size_t) i];
        if (matchTime[(size_t) i] >= 0.0)
        {
            ++matchedCount[l];
            if (firstPos[l] < 0) { firstPos[l] = posInLine[l]; firstTime[l] = matchTime[(size_t) i]; }
        }
        ++posInLine[l];
    }

    double last = -1.0;
    for (size_t l = 0; l < result.size(); ++l)
    {
        const auto chars = lineChars[l];
        result[l].matched = chars > 0 ? (float) matchedCount[l] / (float) chars : 0.0f;
        const bool known = matchedCount[l] >= 2 && result[l].matched >= 0.25f;
        if (! known)
            continue;
        auto t = juce::jmax (0.0, firstTime[l] - firstPos[l] * secondsPerChar);
        if (t <= last) t = firstTime[l];   // 戻しすぎて前の行を越えたら、一致した文字そのものの時刻
        if (t <= last) continue;           // それでも前の行より前：信用しない
        result[l].start = t;
        last = t;
    }

    // 分からない行：前後の分かった行の間を文字数で割って埋める
    for (size_t l = 0; l < result.size(); ++l)
    {
        if (result[l].start >= 0.0)
            continue;
        size_t p = l, n = l;
        while (p > 0 && result[p - 1].start < 0.0) --p;   // p = 埋める塊の頭
        while (n < result.size() && result[n].start < 0.0) ++n;   // n = 次の分かった行
        const bool hasPrev = p > 0, hasNext = n < result.size();
        if (! hasPrev && ! hasNext)
            break;
        // 前の行の頭から次の行の頭まで（無ければ 1 文字の秒で伸ばす）を文字数で配る
        const auto from0 = hasPrev ? result[p - 1].start : -1.0;
        int before = hasPrev ? juce::jmax (1, lineChars[p - 1]) : 0;
        int total = before;
        for (auto k = p; k < n; ++k) total += juce::jmax (1, lineChars[k]);
        double t0, span;
        if (hasPrev && hasNext) { t0 = from0; span = result[n].start - from0; }
        else if (hasPrev)       { t0 = from0; span = total * secondsPerChar * 2.0; }
        else                    { span = total * secondsPerChar * 2.0; t0 = result[n].start - span; }
        int acc = before;
        for (auto k = p; k < n; ++k)
        {
            result[k].start = juce::jmax (0.0, t0 + span * acc / juce::jmax (1, total));
            result[k].interpolated = true;
            acc += juce::jmax (1, lineChars[k]);
        }
        l = n;
    }
    return result;
}
} // namespace vb::song
