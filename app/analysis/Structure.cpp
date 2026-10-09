#include "Structure.h"
#include "MusicInfo.h"
#include "Decimate.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <map>
#include <numeric>

namespace vb::analysis
{
namespace
{
    using int64 = juce::int64;
    constexpr int kernelBars = 4;        // 境目を探す窓（片側の小節数）
    constexpr int minSectionBars = 4;    // これより短い区間は作らない
    constexpr int maxSections = 16;

    struct Bar
    {
        int64 start = 0, end = 0;
        std::array<float, 12> chroma {};   // 曲全体の平均を引いて長さ 1 に
        float db = -100.0f;                // 平均の音量
        bool sound = false;
    };

    float dot12 (const std::array<float, 12>& a, const std::array<float, 12>& b)
    {
        float s = 0.0f;
        for (int i = 0; i < 12; ++i) s += a[(size_t) i] * b[(size_t) i];
        return s;
    }

    StructureEstimate estimateAtRate (const float* x, int64 length, double rate, double bpm, int64 downbeat, int beatsPerBar)
    {
        StructureEstimate out;
        if (x == nullptr || length <= 0 || rate <= 0.0 || bpm <= 0.0 || beatsPerBar <= 0)
            return out;

        int hop = 1;
        const auto chroma = chromaFrames (x, length, rate, hop);
        if (chroma.empty())
            return out;

        // 小節の線（1 小節目の前に弱起があれば、その前の小節から）
        const auto barLen = rate * 60.0 / bpm * beatsPerBar;
        if (barLen < rate * 0.5)
            return out;
        auto firstBar = (int64) std::floor ((double) -downbeat / barLen);
        std::vector<Bar> bars;
        for (auto k = firstBar;; ++k)
        {
            const auto a = (int64) std::llround ((double) downbeat + (double) k * barLen);
            const auto b = (int64) std::llround ((double) downbeat + (double) (k + 1) * barLen);
            if (a >= length)
                break;
            if (b <= 0)
                continue;
            Bar bar;
            bar.start = juce::jmax ((int64) 0, a);
            bar.end = juce::jmin (length, b);
            bars.push_back (bar);
        }
        const auto n = (int) bars.size();
        if (n < 2 * minSectionBars)
            return out;

        // 小節ごとの響きと音量
        std::array<float, 12> mean {};
        int voicedBars = 0;
        for (auto& bar : bars)
        {
            const auto f0 = (size_t) (bar.start / hop), f1 = juce::jmin (chroma.size(), (size_t) (bar.end / hop));
            for (auto f = f0; f < f1; ++f)
                for (int p = 0; p < 12; ++p)
                    bar.chroma[(size_t) p] += chroma[f][(size_t) p];
            double sq = 0.0;
            for (auto i = bar.start; i < bar.end; ++i)
                sq += (double) x[i] * x[i];
            const auto rms = std::sqrt (sq / (double) juce::jmax ((int64) 1, bar.end - bar.start));
            bar.db = rms > 1.0e-9 ? (float) (20.0 * std::log10 (rms)) : -180.0f;
            float sum = 0.0f;
            for (auto v : bar.chroma) sum += v;
            if (sum > 0.0f)
                for (auto& v : bar.chroma) v /= sum;
        }
        float loudest = -180.0f;
        for (auto& bar : bars) loudest = juce::jmax (loudest, bar.db);
        for (auto& bar : bars)
        {
            bar.sound = bar.db > loudest - 40.0f;
            if (bar.sound)
            {
                for (int p = 0; p < 12; ++p) mean[(size_t) p] += bar.chroma[(size_t) p];
                ++voicedBars;
            }
        }
        if (voicedBars < 2 * minSectionBars)
            return out;
        for (auto& v : mean) v /= (float) voicedBars;
        for (auto& bar : bars)
        {
            float len = 0.0f;
            for (int p = 0; p < 12; ++p)
            {
                bar.chroma[(size_t) p] = bar.sound ? bar.chroma[(size_t) p] - mean[(size_t) p] : 0.0f;
                len += bar.chroma[(size_t) p] * bar.chroma[(size_t) p];
            }
            len = std::sqrt (len);
            if (len > 1.0e-6f)
                for (auto& v : bar.chroma) v /= len;
        }

        // 似かた（2 小節ずつ並べて比べる：和音の進みが同じ所ほど高い）
        auto sim = [&] (int i, int j)
        {
            if (i < 0 || j < 0 || i >= n || j >= n)
                return 0.0f;
            auto one = [&] (int p, int q) { return dot12 (bars[(size_t) p].chroma, bars[(size_t) q].chroma); };
            const auto a = one (i, j);
            const auto b = (i + 1 < n && j + 1 < n) ? one (i + 1, j + 1) : a;
            return 0.5f * (a + b);
        };

        // 鳴り始めの小節（最初の区間の頭）
        int first = 0;
        while (first < n && ! bars[(size_t) first].sound) ++first;
        int last = n - 1;
        while (last > first && ! bars[(size_t) last].sound) --last;
        if (last - first + 1 < 2 * minSectionBars)
            return out;

        // 境目らしさ：市松の核（前の窓どうし・後の窓どうしが似て、前と後が似ていない所が高い）＋音量の段差
        std::vector<float> novelty ((size_t) n, 0.0f);
        for (int i = first + 1; i <= last; ++i)
        {
            float within = 0.0f, across = 0.0f;
            int wn = 0, an = 0;
            for (int a = -kernelBars; a < kernelBars; ++a)
                for (int b = -kernelBars; b < kernelBars; ++b)
                {
                    const auto p = i + a, q = i + b;
                    if (p < first || q < first || p > last || q > last || p == q)
                        continue;
                    if ((a < 0) == (b < 0)) { within += sim (p, q); ++wn; }
                    else                    { across += sim (p, q); ++an; }
                }
            const auto chord = (wn > 0 && an > 0) ? juce::jmax (0.0f, within / (float) wn - across / (float) an) : 0.0f;
            float before = 0.0f, after = 0.0f;
            int bn = 0, fn = 0;
            for (int k = 1; k <= 2; ++k)
            {
                if (i - k >= first) { before += bars[(size_t) (i - k)].db; ++bn; }
                if (i + k - 1 <= last) { after += bars[(size_t) (i + k - 1)].db; ++fn; }
            }
            const auto step = (bn > 0 && fn > 0) ? std::abs (after / (float) fn - before / (float) bn) : 0.0f;
            novelty[(size_t) i] = chord + 0.05f * juce::jmin (step, 12.0f);   // 6 dB の段差 ≒ 0.3
        }

        // 山を拾う：前後 2 小節で一番高く、全体の平均＋標準偏差の半分より高い所。強い順に、近すぎるものは捨てる
        std::vector<float> vals;
        for (int i = first + 1; i <= last; ++i) vals.push_back (novelty[(size_t) i]);
        const auto avg = std::accumulate (vals.begin(), vals.end(), 0.0f) / (float) juce::jmax ((size_t) 1, vals.size());
        float var = 0.0f;
        for (auto v : vals) var += (v - avg) * (v - avg);
        const auto threshold = avg + 0.5f * std::sqrt (var / (float) juce::jmax ((size_t) 1, vals.size()));
        std::vector<int> peaks;
        for (int i = first + minSectionBars; i <= last - minSectionBars + 1; ++i)
        {
            const auto v = novelty[(size_t) i];
            if (v <= threshold || v <= 0.0f)
                continue;
            bool top = true;
            for (int k = -2; k <= 2 && top; ++k)
                if (k != 0 && i + k >= 0 && i + k < n && novelty[(size_t) (i + k)] > v)
                    top = false;
            if (top)
                peaks.push_back (i);
        }
        std::sort (peaks.begin(), peaks.end(), [&] (int a, int b) { return novelty[(size_t) a] > novelty[(size_t) b]; });
        std::vector<int> bounds { first };
        for (auto p : peaks)
        {
            if ((int) bounds.size() >= maxSections)
                break;
            if (std::all_of (bounds.begin(), bounds.end(), [&] (int b) { return std::abs (b - p) >= minSectionBars; }))
                bounds.push_back (p);
        }
        std::sort (bounds.begin(), bounds.end());

        // くり返し：「L 小節あとに同じ響き」が続く所（ずれ L ごとの縞）。4 小節の移動平均が閾値を超えて 4 回以上続く所を拾い、
        // 境目で切って組にする（頭をそろえて比べると、境目が少しずれただけで似ていないことになる）
        std::vector<float> pairSims;
        for (int i = first; i <= last; ++i)
            for (int j = i + minSectionBars; j <= last; ++j)
                pairSims.push_back (sim (i, j));
        std::sort (pairSims.begin(), pairSims.end());
        const auto stripeThreshold = juce::jlimit (0.3f, 0.55f, pairSims.empty() ? 0.5f : pairSims[pairSims.size() * 85 / 100]);

        struct Piece { int a = 0, b = 0; };
        std::vector<Piece> pieces;
        std::vector<std::pair<int, int>> links;   // 同じ響きの 2 つの断片
        auto cutAndLink = [&] (int a, int b, int lag)
        {
            // [a, b) と [a + lag, b + lag) を、どちらかの境目で切って断片の組にする
            std::vector<int> cuts { a, b };
            for (auto edge : bounds)
            {
                if (edge > a && edge < b) cuts.push_back (edge);
                if (edge - lag > a && edge - lag < b) cuts.push_back (edge - lag);
            }
            std::sort (cuts.begin(), cuts.end());
            cuts.erase (std::unique (cuts.begin(), cuts.end()), cuts.end());
            for (size_t c = 0; c + 1 < cuts.size(); ++c)
            {
                if (cuts[c + 1] - cuts[c] < minSectionBars)
                    continue;
                pieces.push_back ({ cuts[c], cuts[c + 1] });
                pieces.push_back ({ cuts[c] + lag, cuts[c + 1] + lag });
                links.push_back ({ (int) pieces.size() - 2, (int) pieces.size() - 1 });
            }
        };
        for (int lag = 2 * minSectionBars; lag <= last - first - 2 * minSectionBars + 1; ++lag)
        {
            int runStart = -1;
            for (int i = first; i + lag + 3 <= last + 1; ++i)
            {
                float window = 0.0f;
                for (int k = 0; k < 4; ++k) window += sim (i + k, i + k + lag);
                const bool high = window / 4.0f >= stripeThreshold;
                if (high && runStart < 0)
                    runStart = i;
                const bool endHere = runStart >= 0 && (! high || i + lag + 4 > last + 1);
                if (endHere)
                {
                    const auto runEnd = (high ? i : i - 1) + 4;   // 移動平均の窓の分だけ後ろまで
                    if (runEnd - runStart >= 6)
                        cutAndLink (runStart, juce::jmin (runEnd, last + 1 - lag), lag);
                    runStart = -1;
                }
            }
        }

        // 断片をまとめる：半分以上重なるものは同じ所（1 回の出現）とみなす。縞でつながった出現どうしを 1 つの組に
        const auto pn = (int) pieces.size();
        std::vector<int> parent ((size_t) pn);
        std::iota (parent.begin(), parent.end(), 0);
        std::function<int (int)> root = [&] (int v) { return parent[(size_t) v] == v ? v : parent[(size_t) v] = root (parent[(size_t) v]); };
        auto join = [&] (int a, int b) { parent[(size_t) root (a)] = root (b); };
        for (auto& l : links)
            join (l.first, l.second);
        auto overlapHalf = [&] (const Piece& p1, const Piece& p2)
        {
            const auto ov = juce::jmin (p1.b, p2.b) - juce::jmax (p1.a, p2.a);
            return ov * 2 >= juce::jmin (p1.b - p1.a, p2.b - p2.a);
        };
        for (int u = 0; u < pn; ++u)
            for (int v = u + 1; v < pn; ++v)
                if (overlapHalf (pieces[(size_t) u], pieces[(size_t) v]))
                    join (u, v);

        // 組ごとの出現（重なる断片を合わせた区間）と音量
        std::vector<float> loud;
        for (int i = first; i <= last; ++i) loud.push_back (bars[(size_t) i].db);
        std::nth_element (loud.begin(), loud.begin() + (long) loud.size() / 2, loud.end());
        const auto median = loud[loud.size() / 2];
        struct Group { std::vector<Piece> places; float db = 0.0f; };
        std::map<int, Group> groups;
        for (int u = 0; u < pn; ++u)
        {
            auto& g = groups[root (u)];
            auto p = pieces[(size_t) u];
            bool merged = false;
            for (auto& q : g.places)
                if (overlapHalf (p, q)) { q.a = juce::jmin (q.a, p.a); q.b = juce::jmax (q.b, p.b); merged = true; break; }
            if (! merged)
                g.places.push_back (p);
        }
        const Group* chorus = nullptr;
        for (auto& [r, g] : groups)
        {
            std::sort (g.places.begin(), g.places.end(), [] (const Piece& p1, const Piece& p2) { return p1.a < p2.a; });
            float sum = 0.0f;
            int cnt = 0;
            for (auto& p : g.places)
                for (int i = p.a; i < p.b; ++i) { sum += bars[(size_t) i].db; ++cnt; }
            g.db = cnt > 0 ? sum / (float) cnt : -180.0f;
            if (g.places.size() >= 2 && (chorus == nullptr || g.db > chorus->db))
                chorus = &g;
        }
        const bool chorusOk = chorus != nullptr && chorus->db >= median + 1.0f;

        // 境目にサビの頭と終わりを入れる（2 小節以内の境目は動かす）。サビの頭から近い（4 小節未満の）ほかの境目は消す
        std::vector<int> chorusStarts;
        if (chorusOk)
        {
            out.chorusConfidence = juce::jlimit (0.0f, 1.0f, 0.3f + 0.05f * (chorus->db - median) + 0.1f * (float) (chorus->places.size() - 1));
            for (auto& p : chorus->places)
            {
                chorusStarts.push_back (p.a);
                for (int edge : { p.a, p.b })
                {
                    if (edge > last)
                        continue;
                    auto near = std::find_if (bounds.begin(), bounds.end(), [&] (int b) { return std::abs (b - edge) <= 2; });
                    if (near != bounds.end() && *near != first) *near = edge;
                    else if (near == bounds.end())              bounds.push_back (edge);
                }
            }
            std::sort (bounds.begin(), bounds.end());
            bounds.erase (std::unique (bounds.begin(), bounds.end()), bounds.end());
            std::vector<int> kept;
            for (auto b : bounds)
            {
                const bool isChorus = std::find (chorusStarts.begin(), chorusStarts.end(), b) != chorusStarts.end();
                const bool tooClose = std::any_of (chorusStarts.begin(), chorusStarts.end(), [&] (int c) { return c != b && std::abs (c - b) < minSectionBars; });
                if (b == first || isChorus || ! tooClose)
                    kept.push_back (b);
            }
            bounds = kept;
        }

        for (auto b : bounds)
            out.sections.push_back ({ bars[(size_t) b].start, std::find (chorusStarts.begin(), chorusStarts.end(), b) != chorusStarts.end() });
        return out;
    }
}

StructureEstimate estimateStructure (const float* x, int64 length, double sampleRate, double bpm, int64 downbeatSample, int beatsPerBar)
{
    const auto k = analysisFactor (sampleRate);
    if (k <= 1)
        return estimateAtRate (x, length, sampleRate, bpm, downbeatSample, beatsPerBar);
    const auto d = decimate (x, length, k);
    auto r = estimateAtRate (d.data(), (int64) d.size(), sampleRate / k, bpm, downbeatSample / k, beatsPerBar);
    for (auto& s : r.sections)
        s.startSample *= k;
    return r;
}
} // namespace vb::analysis
