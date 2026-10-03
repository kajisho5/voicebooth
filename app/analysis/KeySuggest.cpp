#include "KeySuggest.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace vb::analysis
{
namespace
{
    constexpr float tolerance = 0.3f;    // 30 セントまでのはみ出しは収まったとみなす（声域は半音単位で測る）
    constexpr size_t minPoints = 50;     // 0.5 秒分（10 ms ごと）。これより少なければ広がりは言えない
    constexpr float maxFromMedian = 15.0f;
}

std::vector<float> sustainedNotes (const std::vector<float>& seq, int minRun, float maxSpread)
{
    std::vector<float> out;
    size_t i = 0;
    while (i < seq.size())
    {
        if (seq[i] <= 0.0f) { ++i; continue; }
        // i から、最初の点から maxSpread 半音以内で続く所
        size_t j = i + 1;
        while (j < seq.size() && seq[j] > 0.0f && std::abs (seq[j] - seq[i]) <= maxSpread)
            ++j;
        if ((int) (j - i) >= minRun)
            out.insert (out.end(), seq.begin() + (long) i, seq.begin() + (long) j);
        i = j;
    }
    return out;
}

SongRange songRange (const std::vector<float>& midi)
{
    std::vector<float> v;
    v.reserve (midi.size());
    for (auto m : midi)
        if (m > 0.0f)
            v.push_back (m);
    SongRange r;
    if (v.size() < minPoints)
        return r;
    std::sort (v.begin(), v.end());
    // 中央値から 15 半音より離れた点は、歌の音域ではなく伴奏の残り・オクターブ違いとみなして捨てる（1 曲の音域はふつう中央値 ±1 オクターブ程度）
    const auto mid = v[v.size() / 2];
    v.erase (std::remove_if (v.begin(), v.end(), [mid] (float m) { return std::abs (m - mid) > maxFromMedian; }), v.end());
    if (v.size() < minPoints)
        return r;
    auto at = [&] (double q) { return v[(size_t) std::llround (q * (double) (v.size() - 1))]; };
    r.low = at (0.05);
    r.high = at (0.95);
    r.known = true;
    return r;
}

void overflowAt (const SongRange& song, int voiceLow, int voiceHigh, int shift, int octave, float& overLow, float& overHigh)
{
    const auto lo = song.low + (float) shift + 12.0f * (float) octave;
    const auto hi = song.high + (float) shift + 12.0f * (float) octave;
    overLow = std::max (0.0f, (float) voiceLow - lo);
    overHigh = std::max (0.0f, hi - (float) voiceHigh);
    if (overLow <= tolerance) overLow = 0.0f;
    if (overHigh <= tolerance) overHigh = 0.0f;
}

KeySuggestion suggestKey (const SongRange& song, int voiceLow, int voiceHigh, int minShift, int maxShift)
{
    KeySuggestion best;
    if (! song.known || voiceLow < 0 || voiceHigh <= voiceLow)
        return best;

    // 小さいほど良い：はみ出しの合計 → 上下のはみ出しの偏り（収まらない時は上下に振り分ける。下げすぎて低い所が全部出ない、を避ける）
    //               → 元の高さ（オクターブ 0）→ 元のキーに近い → 下げる方（同じ距離なら喉に楽な方）
    float bestCost = 1.0e9f;
    for (int octave : { 0, -1, 1 })
        for (int shift = minShift; shift <= maxShift; ++shift)
        {
            float lo = 0.0f, hi = 0.0f;
            overflowAt (song, voiceLow, voiceHigh, shift, octave, lo, hi);
            const auto cost = (lo + hi) * 100.0f + std::abs (lo - hi) * 30.0f + (octave != 0 ? 20.0f : 0.0f) + (float) std::abs (shift) + (shift > 0 ? 0.1f : 0.0f);
            if (cost < bestCost)
            {
                bestCost = cost;
                best.ok = true;
                best.shift = shift;
                best.octave = octave;
                best.overLow = lo;
                best.overHigh = hi;
                best.fits = lo <= 0.0f && hi <= 0.0f;
            }
        }
    return best;
}
} // namespace vb::analysis
