#include "Comp.h"

namespace vb::project
{
namespace
{
    const Take* findTake (const Track& t, const juce::String& id)
    {
        for (auto& k : t.takes)
            if (k.id == id)
                return &k;
        return nullptr;
    }
}

juce::String nextTakeId (const Track& t)
{
    int highest = 0;
    for (auto& k : t.takes)
        if (k.id.startsWith ("take"))
            highest = juce::jmax (highest, k.id.substring (4).getIntValue());
    return "take" + juce::String (highest + 1);
}

void applyTake (Track& t, const Take& take)
{
    applyTake (t, take, take.startSample);
}

void applyTake (Track& t, const Take& take, int64 useFrom)
{
    if (take.endSample <= take.startSample)
        return;

    t.takes.push_back (take);

    // 採用するのは曲の中だけ（頭より前は補正で出た分。ファイルは切らない）
    const auto start = std::max ({ (int64) 0, take.startSample, useFrom }), end = take.endSample;
    if (end <= start)
        return;

    // 新しいテイクの範囲を切り抜いて、はみ出した前後だけ残す
    std::vector<CompSegment> out;
    out.reserve (t.comp.size() + 2);
    for (auto& c : t.comp)
    {
        if (c.endSample <= start || c.startSample >= end)
        {
            out.push_back (c);
            continue;
        }
        if (c.startSample < start)
            out.push_back ({ c.startSample, start, c.takeId });
        if (c.endSample > end)
            out.push_back ({ end, c.endSample, c.takeId });
    }
    out.push_back ({ start, end, take.id });
    std::sort (out.begin(), out.end(), [] (const CompSegment& a, const CompSegment& b) { return a.startSample < b.startSample; });

    // 隣り合う同じテイクはまとめる
    std::vector<CompSegment> merged;
    for (auto& c : out)
    {
        if (! merged.empty() && merged.back().takeId == c.takeId && merged.back().endSample == c.startSample)
            merged.back().endSample = c.endSample;
        else
            merged.push_back (c);
    }
    t.comp = std::move (merged);
}

bool compIsValid (const Track& t)
{
    int64 last = std::numeric_limits<int64>::min();
    for (auto& c : t.comp)
    {
        if (c.endSample <= c.startSample || c.startSample < last || findTake (t, c.takeId) == nullptr)
            return false;
        last = c.endSample;
    }
    return true;
}

const Take* takeAt (const Track& t, int64 sample)
{
    for (auto& c : t.comp)
        if (sample >= c.startSample && sample < c.endSample)
            return findTake (t, c.takeId);
    return nullptr;
}
} // namespace vb::project
