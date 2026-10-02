#include "TakeCompare.h"
#include "Comp.h"

namespace vb::project
{
std::pair<int64, int64> usableSpan (const Take& t, int64 from, int64 to)
{
    // useTake と同じ切り方（曲の頭より前は補正で出た分なので使わない）
    return { std::max ({ (int64) 0, t.startSample, from }), std::min (t.endSample, to) };
}

std::vector<const Take*> compareCandidates (const Track& track, int64 from, int64 to)
{
    std::vector<const Take*> out;
    for (auto& k : track.takes)
    {
        if (k.recMode != RecMode::delivery || k.endSample <= k.startSample)
            continue;
        const auto span = usableSpan (k, from, to);
        if (span.second > span.first)
            out.push_back (&k);
    }
    // 新しい順。録った時刻が同じ（古い形式で時刻が無い・見本）なら番号の大きい方を新しいとみなす
    std::stable_sort (out.begin(), out.end(), [] (const Take* a, const Take* b)
    {
        if (a->created != b->created)
            return a->created > b->created;
        return a->id.substring (4).getIntValue() > b->id.substring (4).getIntValue();
    });
    return out;
}

float takeCoverage (const Take& t, int64 from, int64 to)
{
    if (to <= from)
        return 0.0f;
    const auto span = usableSpan (t, from, to);
    return span.second > span.first ? (float) ((double) (span.second - span.first) / (double) (to - from)) : 0.0f;
}

float compShare (const std::vector<CompSegment>& comp, const juce::String& takeId, int64 from, int64 to)
{
    if (to <= from)
        return 0.0f;
    int64 used = 0;
    for (auto& c : comp)
        if (c.takeId == takeId)
            used += juce::jmax ((int64) 0, juce::jmin (c.endSample, to) - juce::jmax (c.startSample, from));
    return (float) ((double) used / (double) (to - from));
}

std::pair<int64, int64> compSpanAt (const Track& t, int64 sample, int64 songLength)
{
    if (sample < 0 || sample >= songLength)
        return { 0, 0 };
    // 採用区間は start の順で重ならない（compIsValid）。中ならその区間、外なら前後の区間の間
    int64 gapFrom = 0;
    for (auto& c : t.comp)
    {
        if (sample < c.startSample)
            return { gapFrom, juce::jmin (c.startSample, songLength) };
        if (sample < c.endSample)
            return { c.startSample, juce::jmin (c.endSample, songLength) };
        gapFrom = c.endSample;
    }
    return { gapFrom, songLength };
}

//==============================================================================
bool TakeAudition::begin (const Track& t, int64 from, int64 to)
{
    if (to <= from)
        return false;
    active = true;
    type = t.type;
    rangeFrom = from;
    rangeTo = to;
    before = t.comp;
    current.clear();
    return true;
}

bool TakeAudition::preview (Track& t, const juce::String& takeId)
{
    if (! active || t.type != type)
        return false;
    if (takeId.isEmpty())
    {
        t.comp = before;
        current.clear();
        return true;
    }
    for (auto* k : compareCandidates (t, rangeFrom, rangeTo))
        if (k->id == takeId)
        {
            // 毎回、覚えた形から作る（take2 → take3 と選び直しても take2 の切れ端が残らない）
            t.comp = before;
            useTake (t, *k, rangeFrom, rangeTo);
            current = takeId;
            return true;
        }
    return false;
}

std::optional<std::vector<CompSegment>> TakeAudition::commit (Track& t)
{
    if (! active)
        return std::nullopt;
    active = false;
    if (t.type != type)
        return std::nullopt;   // 別のトラック（呼び間違い）には触らない
    if (current.isEmpty())
    {
        t.comp = before;       // 何も選んでいない＝元のまま
        return std::nullopt;
    }
    current.clear();
    // 選んだテイクが元からその範囲に入っていた（形が変わらない）なら、取り消しの対象にしない
    const bool same = t.comp.size() == before.size()
                   && std::equal (t.comp.begin(), t.comp.end(), before.begin(), [] (const CompSegment& a, const CompSegment& b)
                                  { return a.startSample == b.startSample && a.endSample == b.endSample && a.takeId == b.takeId; });
    if (same)
        return std::nullopt;
    return std::move (before);
}

void TakeAudition::restoreCommitted (Project& p) const
{
    if (! active)
        return;
    for (auto& t : p.tracks)
        if (t.type == type)
            t.comp = before;
}

void TakeAudition::cancel (Track& t)
{
    if (! active)
        return;
    active = false;
    if (t.type == type)
        t.comp = before;
    current.clear();
}
} // namespace vb::project
