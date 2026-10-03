#include "SongInfo.h"
#include <algorithm>
#include <map>

namespace vb::song
{
namespace
{
    /** 負でも下へ丸める割り算（弱起の小節番号を 0、-1 … にする） */
    int64 floorDiv (int64 a, int64 b)
    {
        auto q = a / b;
        if ((a % b != 0) && ((a < 0) != (b < 0)))
            --q;
        return q;
    }

    int64 sameTolerance (double sampleRate)
    {
        return (int64) std::llround (sameSectionSeconds * sampleRate);
    }

    bool hasSectionNear (const Sections& list, int64 sample, double sampleRate, int except = -1)
    {
        const auto tol = sameTolerance (sampleRate);
        for (int i = 0; i < (int) list.size(); ++i)
            if (i != except && std::abs (list[(size_t) i].startSample - sample) <= tol)
                return true;
        return false;
    }

    int insertSorted (Sections& list, Section s)
    {
        auto it = std::upper_bound (list.begin(), list.end(), s.startSample,
                                    [] (int64 v, const Section& x) { return v < x.startSample; });
        it = list.insert (it, std::move (s));
        return (int) std::distance (list.begin(), it);
    }

    /** [mm:ss.xxx]（歌詞パッドで直す時の時刻。parseLyrics が読める形） */
    juce::String lrcTag (int64 sample, double sampleRate)
    {
        const auto ms = (int64) std::llround ((double) sample * 1000.0 / sampleRate);
        const auto m = ms / 60000, s = (ms / 1000) % 60, f = ms % 1000;
        return "[" + juce::String (m).paddedLeft ('0', 2) + ":" + juce::String (s).paddedLeft ('0', 2)
               + "." + juce::String (f).paddedLeft ('0', 3) + "]";
    }
}

//==============================================================================
int64 beatSample (const TempoInfo& t, int64 beatIndex, double sampleRate)
{
    return t.downbeatSample + (int64) std::llround ((double) beatIndex * t.samplesPerBeat (sampleRate));
}

int64 beatIndexAt (const TempoInfo& t, int64 sample, double sampleRate)
{
    const auto spb = t.samplesPerBeat (sampleRate);
    if (spb <= 0.0)
        return 0;

    // 拍の位置は丸めて置くので、ちょうど拍の上の位置がひとつ前に落ちないよう前後を確かめる
    auto k = (int64) std::floor ((double) (sample - t.downbeatSample) / spb);
    if (beatSample (t, k + 1, sampleRate) <= sample) ++k;
    if (beatSample (t, k, sampleRate) > sample)      --k;
    return k;
}

BarBeat barBeatAt (const TempoInfo& t, int64 sample, double sampleRate)
{
    BarBeat b;
    if (! t.known())
        return b;

    const auto k = beatIndexAt (t, sample, sampleRate);
    const auto perBar = (int64) t.signature.beatsPerBar();
    const auto barIndex = floorDiv (k, perBar);
    b.bar = barIndex + 1;
    b.beat = (int) (k - barIndex * perBar) + 1;
    b.fraction = juce::jlimit (0.0, 1.0, (double) (sample - beatSample (t, k, sampleRate)) / t.samplesPerBeat (sampleRate));
    return b;
}

int64 barSample (const TempoInfo& t, int64 bar, double sampleRate)
{
    return beatSample (t, (bar - 1) * t.signature.beatsPerBar(), sampleRate);
}

int64 snapToBar (const TempoInfo& t, int64 sample, double sampleRate)
{
    if (! t.known())
        return sample;

    const auto bar = barBeatAt (t, sample, sampleRate).bar;
    const auto a = barSample (t, bar, sampleRate), b = barSample (t, bar + 1, sampleRate);
    return (sample - a) <= (b - sample) ? a : b;
}

int64 countInStart (const TempoInfo& t, int64 at, int bars, double sampleRate)
{
    if (! t.known() || bars <= 0)
        return at;
    const auto quarterBeat = (int64) std::llround (0.25 * t.samplesPerBeat (sampleRate));
    const auto bar = barBeatAt (t, at + quarterBeat, sampleRate).bar;
    return barSample (t, bar - bars, sampleRate);
}

int64 shiftDownbeat (const TempoInfo& t, int beats, double sampleRate)
{
    return t.downbeatSample + (int64) std::llround ((double) beats * t.samplesPerBeat (sampleRate));
}

juce::String formatBpm (double bpm)
{
    const auto r = roundBpm (bpm);
    if (std::abs (r - std::round (r)) < 0.005)
        return juce::String ((int) std::lround (r));
    return juce::String (r, 2);
}

double parseBpm (const juce::String& text)
{
    const auto t = text.trim().replaceCharacter (',', '.');
    if (t.isEmpty() || ! t.containsOnly ("0123456789.") || t.indexOfChar ('.') != t.lastIndexOfChar ('.'))
        return 0.0;
    const auto v = roundBpm (t.getDoubleValue());
    return v >= minBpm && v <= maxBpm ? v : 0.0;
}

const char* tonicName (int tonic)
{
    static const char* const names[] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
    return juce::isPositiveAndBelow (tonic, 12) ? names[tonic] : "";
}

juce::String KeyInfo::shortName() const
{
    if (! known())
        return {};
    return juce::String (tonicName (tonic)) + (minor ? "m" : "");
}

//==============================================================================
const std::vector<juce::String>& presetKinds()
{
    static const std::vector<juce::String> k { kind::intro, kind::verseA, kind::verseB, kind::chorus, kind::interlude,
                                               kind::verseC, kind::dropChorus, kind::lastChorus, kind::outro };
    return k;
}

int addSection (Sections& list, Section s, double sampleRate)
{
    s.startSample = juce::jmax ((int64) 0, s.startSample);
    const auto tol = sameTolerance (sampleRate);
    for (int i = 0; i < (int) list.size(); ++i)
        if (std::abs (list[(size_t) i].startSample - s.startSample) <= tol)
        {
            s.startSample = list[(size_t) i].startSample;   // 同じ位置：名前だけ入れ替える
            list[(size_t) i] = std::move (s);
            return i;
        }
    return insertSorted (list, std::move (s));
}

int moveSection (Sections& list, int index, int64 newStart, double sampleRate)
{
    if (! juce::isPositiveAndBelow (index, (int) list.size()))
        return -1;

    newStart = juce::jmax ((int64) 0, newStart);
    if (hasSectionNear (list, newStart, sampleRate, index))
        return index;   // ほかの頭と重なる所へは動かさない

    auto s = list[(size_t) index];
    list.erase (list.begin() + index);
    s.startSample = newStart;
    s.source = Source::confirmed;   // 触ったら確定
    return insertSorted (list, std::move (s));
}

void removeSection (Sections& list, int index)
{
    if (juce::isPositiveAndBelow (index, (int) list.size()))
        list.erase (list.begin() + index);
}

int sectionIndexAt (const Sections& list, int64 sample)
{
    int found = -1;
    for (int i = 0; i < (int) list.size(); ++i)
        if (list[(size_t) i].startSample <= sample)
            found = i;
    return found;
}

int64 sectionEnd (const Sections& list, int index, int64 lengthSamples)
{
    if (! juce::isPositiveAndBelow (index, (int) list.size()))
        return lengthSamples;
    return index + 1 < (int) list.size() ? list[(size_t) index + 1].startSample : lengthSamples;
}

std::vector<int> sectionNumbers (const Sections& list)
{
    auto keyOf = [] (const Section& s) { return s.isCustom() ? "custom:" + s.name : s.kind; };

    std::map<juce::String, int> total, seen;
    for (auto& s : list)
        ++total[keyOf (s)];

    std::vector<int> numbers;
    numbers.reserve (list.size());
    for (auto& s : list)
    {
        const auto key = keyOf (s);
        const auto n = ++seen[key];
        numbers.push_back (total[key] > 1 || s.kind == kind::generic ? n : 0);
    }
    return numbers;
}

//==============================================================================
int Lyrics::numTimed() const
{
    int n = 0;
    for (auto& l : lines)
        n += l.timed() ? 1 : 0;
    return n;
}

Lyrics lyricsFromDoc (const LyricsDoc& doc, double sampleRate, int64 lengthSamples)
{
    Lyrics ly;
    for (auto& l : doc.lines)
    {
        Line line;
        line.text = l.text;
        line.block = l.block;
        line.heading = l.section;
        if (l.hasTime())
            line.startSample = juce::jlimit ((int64) 0, juce::jmax ((int64) 0, lengthSamples),
                                             (int64) std::llround (l.timeSeconds * sampleRate));
        ly.lines.push_back (line);
    }
    for (auto& h : doc.sections)
        ly.headings.push_back ({ h.name, h.firstLine });
    for (auto b : doc.chorusCandidateBlocks)
        ly.chorusBlocks.push_back (b);

    updateLineEnds (ly, lengthSamples, sampleRate);
    return ly;
}

void updateLineEnds (Lyrics& ly, int64 lengthSamples, double sampleRate)
{
    const auto maxLen = (int64) std::llround (maxLineSeconds * sampleRate);
    auto& lines = ly.lines;

    for (size_t i = 0; i < lines.size(); ++i)
    {
        auto& l = lines[i];
        if (! l.timed())
        {
            l.endSample = -1;
            continue;
        }

        size_t j = i + 1;
        while (j < lines.size() && ! lines[j].timed())
            ++j;

        if (j < lines.size())
        {
            const auto next = lines[j].startSample;
            // すぐ次の行が同じ塊なら次の頭まで。塊の終わり・間に時刻の無い行がある時は長くても maxLineSeconds
            l.endSample = (j == i + 1 && lines[j].block == l.block) ? next : juce::jmin (next, l.startSample + maxLen);
        }
        else
        {
            l.endSample = juce::jmin (juce::jmax (lengthSamples, l.startSample), l.startSample + maxLen);
        }
    }
}

int lineAt (const Lyrics& ly, int64 sample)
{
    for (int i = 0; i < (int) ly.lines.size(); ++i)
    {
        const auto& l = ly.lines[(size_t) i];
        if (l.timed() && sample >= l.startSample && sample < l.endSample)
            return i;
    }
    return -1;
}

int nextTimedLineAfter (const Lyrics& ly, int64 sample)
{
    for (int i = 0; i < (int) ly.lines.size(); ++i)
        if (ly.lines[(size_t) i].timed() && ly.lines[(size_t) i].startSample > sample)
            return i;
    return -1;
}

int firstLineToSync (const Lyrics& ly, int64 playhead)
{
    for (int i = 0; i < (int) ly.lines.size(); ++i)
    {
        const auto& l = ly.lines[(size_t) i];
        if (! l.timed() || l.startSample >= playhead)
            return i;
    }
    return (int) ly.lines.size();
}

int tapLine (Lyrics& ly, int index, int64 sample, int64 lengthSamples, double sampleRate)
{
    if (! juce::isPositiveAndBelow (index, (int) ly.lines.size()))
        return index;

    sample = juce::jlimit ((int64) 0, juce::jmax ((int64) 0, lengthSamples), sample);
    auto& lines = ly.lines;
    lines[(size_t) index].startSample = sample;
    lines[(size_t) index].source = Source::confirmed;

    // 順番を崩さない：前の行はこれより前、後ろの行はこれより後でなければ時刻を外す
    for (int i = 0; i < (int) lines.size(); ++i)
    {
        auto& l = lines[(size_t) i];
        if (! l.timed() || i == index)
            continue;
        if ((i < index && l.startSample >= sample) || (i > index && l.startSample <= sample))
            l.startSample = -1;
    }

    updateLineEnds (ly, lengthSamples, sampleRate);
    return index + 1;
}

void clearLineTime (Lyrics& ly, int index, int64 lengthSamples, double sampleRate)
{
    if (! juce::isPositiveAndBelow (index, (int) ly.lines.size()))
        return;
    ly.lines[(size_t) index].startSample = -1;
    updateLineEnds (ly, lengthSamples, sampleRate);
}

juce::String toLyricText (const Lyrics& ly, double sampleRate)
{
    // 見出しは【】で囲む（parseLyrics が見出しとして読み直す）。文字はデータなのでコードに直書きしない
    static const juce::String open = juce::String::fromUTF8 ("\xe3\x80\x90"), close = juce::String::fromUTF8 ("\xe3\x80\x91");

    juce::StringArray out;
    int block = -1;
    for (int i = 0; i < (int) ly.lines.size(); ++i)
    {
        const auto& l = ly.lines[(size_t) i];

        bool headingHere = false;
        for (auto& h : ly.headings)
            if (h.firstLine == i)
            {
                if (! out.isEmpty()) out.add ({});
                out.add (open + h.name + close);
                headingHere = true;
            }

        if (! headingHere && block >= 0 && l.block != block)
            out.add ({});
        block = l.block;

        out.add (l.timed() ? lrcTag (l.startSample, sampleRate) + l.text : l.text);
    }

    // 歌詞の後ろにある見出し（中身の無い見出し）も残す
    for (auto& h : ly.headings)
        if (h.firstLine >= (int) ly.lines.size())
        {
            out.add ({});
            out.add (open + h.name + close);
        }

    return out.joinIntoString ("\n");
}

int applyLyricSections (Sections& list, const Lyrics& ly, double sampleRate, const TempoInfo* bars)
{
    if (! ly.sectionsFromHeadings)
        return 0;

    auto headOf = [&] (int64 lineStart)
    {
        return bars != nullptr && bars->known() ? juce::jmax ((int64) 0, snapToBar (*bars, lineStart, sampleRate)) : lineStart;
    };
    int added = 0;

    // 見出し：その見出しの最初の行に時刻があれば、そこを区間の頭に（確定。歌詞は自分で用意したもの）
    for (int h = 0; h < (int) ly.headings.size(); ++h)
    {
        const auto& heading = ly.headings[(size_t) h];
        if (! juce::isPositiveAndBelow (heading.firstLine, (int) ly.lines.size()))
            continue;
        const auto& line = ly.lines[(size_t) heading.firstLine];
        if (! line.timed() || line.heading != h)
            continue;   // 時刻がまだ無い・自分の行が無い見出し（【間奏】の直後に【サビ】など）

        const auto head = headOf (line.startSample);
        const bool made = std::any_of (list.begin(), list.end(), [h] (const Section& s) { return s.heading == h; });
        if (made || hasSectionNear (list, head, sampleRate))
            continue;

        Section s;
        s.startSample = head;
        s.kind = sectionKindOfHeading (heading.name);
        if (s.kind.isEmpty())
        {
            s.kind = kind::custom;
            s.name = heading.name;
        }
        s.source = Source::confirmed;
        s.heading = h;
        insertSorted (list, s);
        ++added;
    }

    // サビの候補（同じ歌詞の塊のくり返し）：推定として
    for (auto b : ly.chorusBlocks)
    {
        auto it = std::find_if (ly.lines.begin(), ly.lines.end(), [b] (const Line& l) { return l.block == b; });
        if (it == ly.lines.end() || ! it->timed() || hasSectionNear (list, headOf (it->startSample), sampleRate))
            continue;

        Section s;
        s.startSample = headOf (it->startSample);
        s.kind = kind::chorus;
        s.source = Source::estimated;
        insertSorted (list, s);
        ++added;
    }
    return added;
}
} // namespace vb::song
