#include "TakeStats.h"
#include <algorithm>
#include <cmath>

namespace vb::analysis
{
namespace
{
    using int64 = juce::int64;
    constexpr float minConfidence = 0.5f;

    bool voiced (const audio::PitchFrame& f) { return f.midi > 0.0f && f.confidence >= minConfidence; }

    float median (std::vector<float> v)
    {
        if (v.empty()) return 0.0f;
        const auto mid = v.begin() + (std::ptrdiff_t) (v.size() / 2);
        std::nth_element (v.begin(), mid, v.end());
        return *mid;
    }

    /** オクターブを合わせたずれ（セント、-600..600） */
    float centsApart (float a, float b)
    {
        auto c = (a - b) * 100.0f;
        c = std::fmod (c + 600.0f, 1200.0f);
        if (c < 0.0f) c += 1200.0f;
        return c - 600.0f;
    }

    /** sample に近い点（無ければ nullptr）。frames は sample の昇順 */
    const audio::PitchFrame* frameNear (const std::vector<audio::PitchFrame>& frames, int64 sample, int64 within)
    {
        auto it = std::lower_bound (frames.begin(), frames.end(), sample,
                                    [] (const audio::PitchFrame& f, int64 s) { return f.songSample < s; });
        const audio::PitchFrame* best = nullptr;
        int64 bestD = within + 1;
        for (auto k : { it, it == frames.begin() ? it : std::prev (it) })
        {
            if (k == frames.end()) continue;
            const auto d = std::abs (k->songSample - sample);
            if (d < bestD) { bestD = d; best = &*k; }
        }
        return best;
    }
}

std::vector<NoteSpan> segmentNotes (const std::vector<audio::PitchFrame>& frames, double sampleRate)
{
    std::vector<NoteSpan> notes;
    if (frames.empty() || sampleRate <= 0.0)
        return notes;
    const auto gap = (int64) (0.03 * sampleRate);      // 点の間がこれより空いたら切る（10 ms ごとなので 3 点分）
    const auto minLength = (int64) (0.08 * sampleRate);

    std::vector<float> pitches;
    int64 start = -1, last = -1;
    int away = 0;
    auto close = [&] (int64 end)
    {
        if (start >= 0 && end - start >= minLength && ! pitches.empty())
            notes.push_back ({ start, end, median (pitches) });
        start = -1;
        pitches.clear();
        away = 0;
    };

    for (size_t i = 0; i < frames.size(); ++i)
    {
        const auto& f = frames[i];
        if (! voiced (f) || (last >= 0 && f.songSample - last > gap))
        {
            close (last);
            if (! voiced (f)) { last = -1; continue; }
        }
        if (start < 0)
        {
            start = f.songSample;
            pitches.push_back (f.midi);
            last = f.songSample;
            continue;
        }
        // いまの音符の音（最近の点の真ん中）から 0.7 半音より 3 点続けて離れたら、離れ始めた所で次の音符
        const auto n = pitches.size();
        std::vector<float> recent (pitches.begin() + (std::ptrdiff_t) (n > 8 ? n - 8 : 0), pitches.end());
        const auto ref = median (recent);
        if (std::abs (f.midi - ref) > 0.7f)
        {
            if (++away >= 3)
            {
                const auto splitAt = frames[i - 2].songSample;
                pitches.resize (pitches.size() >= 2 ? pitches.size() - 2 : 0);
                const auto nextStart = splitAt;
                close (splitAt);
                start = nextStart;
                pitches = { frames[i - 2].midi, frames[i - 1].midi, f.midi };
                away = 0;
            }
            else
                pitches.push_back (f.midi);
        }
        else
        {
            away = 0;
            pitches.push_back (f.midi);
        }
        last = f.songSample;
    }
    close (last);
    return notes;
}

OnsetStats onsetStats (const std::vector<audio::PitchFrame>& guide, const std::vector<audio::PitchFrame>& take,
                       double sampleRate, int64 from, int64 to)
{
    OnsetStats out;
    const auto notes = segmentNotes (guide, sampleRate);
    const auto rest = (int64) (0.15 * sampleRate), search = (int64) (0.25 * sampleRate);
    const auto hold = (int64) (0.06 * sampleRate);
    constexpr int holdFrames = 5;   // 10 ms ごとの点で 60 ms のうち 5 点以上
    std::vector<float> offsets;
    int64 prevEnd = std::numeric_limits<int64>::min() / 2;
    for (auto& n : notes)
    {
        const bool entry = n.start - prevEnd >= rest;
        prevEnd = n.end;
        if (! entry || n.start < from || n.start >= to)
            continue;
        ++out.entries;

        // テイクで、前後 250 ms の中の「声が無い → 同じ音で声が出て 60 ms 以上その音が続く」所のうち、お手本の頭にいちばん近い所。
        // （いちばん早い所を取ると、手前の息・前のフレーズの残りを拾って大きく早めに出る）
        auto it = std::lower_bound (take.begin(), take.end(), n.start - search,
                                    [] (const audio::PitchFrame& f, int64 s) { return f.songSample < s; });
        bool wasSilent = it == take.begin() || ! voiced (*std::prev (it));
        double best = 0.0;
        bool found = false;
        for (; it != take.end() && it->songSample <= n.start + search; ++it)
        {
            if (! voiced (*it)) { wasSilent = true; continue; }
            if (wasSilent && std::abs (centsApart (it->midi, n.midi)) <= 150.0f)
            {
                int held = 0;
                for (auto k = it; k != take.end() && voiced (*k) && std::abs (centsApart (k->midi, n.midi)) <= 150.0f
                                  && k->songSample - it->songSample < hold; ++k)
                    ++held;
                if (held >= holdFrames)
                {
                    const auto ms = (double) (it->songSample - n.start) * 1000.0 / sampleRate;
                    if (! found || std::abs (ms) < std::abs (best)) { best = ms; found = true; }
                }
            }
            wasSilent = false;
        }
        if (found)
        {
            out.items.push_back ({ n.start, best });
            offsets.push_back ((float) best);
        }
    }
    out.medianMs = offsets.empty() ? 0.0 : (double) median (offsets);
    return out;
}

PitchAccuracy pitchAccuracy (const std::vector<audio::PitchFrame>& guide, const std::vector<audio::PitchFrame>& take,
                             double sampleRate, int64 from, int64 to, float toleranceCents, bool foldOctave)
{
    PitchAccuracy out;
    const auto within = (int64) (0.006 * sampleRate);   // 同じ時刻とみなす（点は 10 ms ごと）
    double sumAbs = 0.0;
    int inBand = 0;
    for (auto& t : take)
    {
        if (t.songSample < from || t.songSample >= to || ! voiced (t))
            continue;
        const auto* g = frameNear (guide, t.songSample, within);
        if (g == nullptr || ! voiced (*g))
            continue;
        const auto c = std::abs (foldOctave ? centsApart (t.midi, g->midi) : (t.midi - g->midi) * 100.0f);
        ++out.frames;
        sumAbs += c;
        inBand += c <= toleranceCents ? 1 : 0;
    }
    if (out.frames > 0)
    {
        out.inBand = (float) inBand / (float) out.frames;
        out.meanAbsCents = (float) (sumAbs / out.frames);
    }
    return out;
}

std::vector<Vibrato> vibratos (const std::vector<audio::PitchFrame>& take, double sampleRate)
{
    std::vector<Vibrato> out;
    for (auto& n : segmentNotes (take, sampleRate))
    {
        if ((double) (n.end - n.start) / sampleRate < 0.5)
            continue;
        std::vector<float> p;
        for (auto& f : take)
            if (f.songSample >= n.start && f.songSample <= n.end && voiced (f))
                p.push_back (f.midi * 100.0f);
        const int half = 7;   // 移動平均 ±70 ms（150 ms）
        if ((int) p.size() < 2 * half + 20)
            continue;
        std::vector<float> d;
        for (int i = half; i + half < (int) p.size(); ++i)
        {
            float m = 0.0f;
            for (int k = -half; k <= half; ++k) m += p[(size_t) (i + k)];
            d.push_back (p[(size_t) i] - m / (2 * half + 1));
        }
        int crossings = 0;
        double sq = 0.0;
        for (size_t i = 0; i < d.size(); ++i)
        {
            sq += (double) d[i] * d[i];
            if (i > 0 && ((d[i - 1] < 0.0f) != (d[i] < 0.0f))) ++crossings;
        }
        const auto seconds = d.size() * 0.01;
        const auto rate = (float) (crossings / 2.0 / seconds);
        const auto depth = (float) (std::sqrt (sq / d.size()) * std::sqrt (2.0));   // 正弦波なら振幅（片側のセント）
        if (rate >= 4.0f && rate <= 8.0f && depth >= 15.0f)
            out.push_back ({ n.start, n.end, rate, depth });
    }
    return out;
}
} // namespace vb::analysis
