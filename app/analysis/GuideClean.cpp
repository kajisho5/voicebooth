#include "GuideClean.h"
#include <algorithm>
#include <cmath>

namespace vb::analysis
{
namespace
{
    constexpr float minConfidence = 0.5f;   // 画面・判定と同じ「声あり」
    constexpr int window = 15;              // 前後 150 ms の真ん中の値を「いまの音」とみなす
    constexpr int maxOctaveBlip = 7;        // 70 ms まで（80 ms 未満）の飛びはオクターブの誤り
    constexpr int maxSpike = 4;             // 40 ms までのオクターブでない外れは消す
    constexpr float spikeSemitones = 7.0f;
    constexpr int minRun = 4;               // 40 ms 未満の孤立したかたまりは消す
    constexpr int maxGap = 4;               // 40 ms までの切れ目はつなぐ
    constexpr float gapJoinSemitones = 1.5f;

    bool voiced (const audio::PitchFrame& f) { return f.midi > 0.0f && f.confidence >= minConfidence; }

    float median (std::vector<float>& v)
    {
        std::nth_element (v.begin(), v.begin() + (long) (v.size() / 2), v.end());
        return v[v.size() / 2];
    }

    void unvoice (audio::PitchFrame& f) { f.midi = 0.0f; f.confidence = 0.0f; }

    /** 1 区間（点の間が空かない並び）を整える */
    void cleanRegion (audio::PitchFrame* f, int n)
    {
        if (n <= 0)
            return;

        // 1・2. 前後の音（声のある点の真ん中）からの外れ
        std::vector<float> dev ((size_t) n, 0.0f);
        {
            std::vector<float> near;
            for (int i = 0; i < n; ++i)
            {
                if (! voiced (f[i]))
                    continue;
                near.clear();
                for (int j = std::max (0, i - window); j <= std::min (n - 1, i + window); ++j)
                    if (voiced (f[j]))
                        near.push_back (f[j].midi);
                dev[(size_t) i] = f[i].midi - median (near);
            }
        }
        for (int i = 0; i < n;)
        {
            if (! voiced (f[i]) || std::abs (dev[(size_t) i]) <= spikeSemitones)
            {
                ++i;
                continue;
            }
            // 同じ向きに外れている続き
            const bool up = dev[(size_t) i] > 0.0f;
            int j = i;
            while (j < n && voiced (f[j]) && std::abs (dev[(size_t) j]) > spikeSemitones && (dev[(size_t) j] > 0.0f) == up)
                ++j;
            const auto len = j - i;
            std::vector<float> d (dev.begin() + i, dev.begin() + j);
            const auto mid = median (d);
            const auto octaves = (int) std::lround (mid / 12.0f);
            const bool octave = octaves != 0 && std::abs (mid - 12.0f * (float) octaves) <= 2.0f;

            // 外れの前後：両側に声があり同じくらいの音なら「戻った」（誤り）。片側だけなら声のかたまりの端
            const bool hasBefore = i > 0 && voiced (f[i - 1]);
            const bool hasAfter = j < n && voiced (f[j]);
            const bool returns = hasBefore && hasAfter && std::abs (f[i - 1].midi - f[j].midi) <= 2.0f;
            const bool edge = hasBefore != hasAfter;

            if (octave && len <= maxOctaveBlip && (returns || edge))
            {
                for (int k = i; k < j; ++k)
                    f[k].midi -= 12.0f * (float) octaves;
            }
            else if (! octave && len <= maxSpike && returns)   // 大きく跳ぶ短い装飾は、戻らないので残る
            {
                for (int k = i; k < j; ++k)
                    unvoice (f[k]);
            }
            i = j;
        }

        // 3. 孤立した短いかたまり
        for (int i = 0; i < n;)
        {
            if (! voiced (f[i])) { ++i; continue; }
            int j = i;
            while (j < n && voiced (f[j]))
                ++j;
            if (j - i < minRun)
                for (int k = i; k < j; ++k)
                    unvoice (f[k]);
            i = j;
        }

        // 4. 小さな切れ目をつなぐ（両側の音が近い時だけ。違う音への移りはつながない）
        for (int i = 1; i < n;)
        {
            if (voiced (f[i]) || ! voiced (f[i - 1])) { ++i; continue; }
            int j = i;
            while (j < n && ! voiced (f[j]))
                ++j;
            if (j < n && j - i <= maxGap && std::abs (f[j].midi - f[i - 1].midi) <= gapJoinSemitones)
            {
                const auto a = f[i - 1], b = f[j];
                for (int k = i; k < j; ++k)
                {
                    const auto t = (float) (k - i + 1) / (float) (j - i + 1);
                    f[k].midi = a.midi + (b.midi - a.midi) * t;
                    f[k].confidence = std::min (a.confidence, b.confidence);
                }
            }
            i = j;
        }

        // 5. 軽いならし（1 点だけとがった点を真ん中の値に → 1:2:1）。声のかたまりの中だけ。
        //    とがりは前後どちらからも同じ向きに 1 半音より離れた点だけ（ビブラートの山を削らない：6 Hz・±50 セントでも 1 点の差は 0.2 半音）
        for (int i = 0; i < n;)
        {
            if (! voiced (f[i])) { ++i; continue; }
            int j = i;
            while (j < n && voiced (f[j]))
                ++j;
            if (j - i >= 3)
            {
                std::vector<float> m ((size_t) (j - i));
                for (int k = i; k < j; ++k)
                {
                    m[(size_t) (k - i)] = f[k].midi;
                    if (k == i || k == j - 1)
                        continue;
                    const auto a = f[k].midi - f[k - 1].midi, b = f[k].midi - f[k + 1].midi;
                    if (a * b > 0.0f && std::abs (a) > 1.0f && std::abs (b) > 1.0f)
                    {
                        float v[] = { f[k - 1].midi, f[k].midi, f[k + 1].midi };
                        std::sort (std::begin (v), std::end (v));
                        m[(size_t) (k - i)] = v[1];
                    }
                }
                for (int k = i + 1; k < j - 1; ++k)
                    f[k].midi = (m[(size_t) (k - i - 1)] + 2.0f * m[(size_t) (k - i)] + m[(size_t) (k - i + 1)]) * 0.25f;
                f[i].midi = m.front();
                f[j - 1].midi = m.back();
            }
            i = j;
        }
    }
}

std::vector<audio::PitchFrame> cleanGuideContour (const std::vector<audio::PitchFrame>& frames, double sampleRate)
{
    auto out = frames;
    if (out.empty() || sampleRate <= 0.0)
        return out;

    const auto maxStep = (juce::int64) std::llround (1.5 * audio::pitch::hopSeconds * sampleRate);
    size_t start = 0;
    for (size_t i = 1; i <= out.size(); ++i)
        if (i == out.size() || out[i].songSample - out[i - 1].songSample > maxStep || out[i].songSample <= out[i - 1].songSample)
        {
            cleanRegion (out.data() + start, (int) (i - start));
            start = i;
        }
    return out;
}
} // namespace vb::analysis
