#include "RefPitch.h"
#include "GuideClean.h"

namespace vb::analysis
{
namespace
{
    using int64 = juce::int64;
    constexpr double blockSeconds = 0.5;
    constexpr float cleanLimitDb = -15.0f;     // 伴奏だけの所で、残りがこれより小さければ「引けた」
    constexpr double quietBlock = 1.0e-5;      // 平均の 2 乗がこれ未満（約 -50 dBFS）のブロックは判定に使わない
    constexpr float residualGateDb = 24.0f;    // 残りの山が原曲の山よりこれ以上小さい点は声が無いとみなす（mp3 / m4a の符号化の残りを拾わない）

    struct Block
    {
        int64 start = 0, end = 0;   // オフボの時間
        double gain = 0.0;          // 原曲 ≒ 声 + gain × オフボ
        double ratioDb = 0.0;       // 残り / 原曲（dB）
        bool loud = false;
    };
}

RefPitchResult referencePitch (const float* reference, int64 referenceLength,
                               const float* karaoke, int64 karaokeLength,
                               double sampleRate, const AlignResult& align,
                               const std::function<bool (float)>& progress,
                               std::vector<float>* vocalsOut)
{
    RefPitchResult result;
    if (! align.found() || align.covered.empty() || sampleRate <= 0.0)
        return result;   // notAligned

    // 速さの違う版は同じミックスではない（引けない）
    if (std::abs (align.tempoRatio - 1.0) > 1.0e-5)
    {
        result.status = RefPitchResult::Status::needsSeparation;
        return result;
    }

    // 1 回目：0.5 秒ごとに、引く量（最小二乗）と残りの大きさ
    const auto blockLength = juce::jmax ((int64) 1, (int64) std::llround (blockSeconds * sampleRate));
    std::vector<std::vector<Block>> regions;
    int64 total = 0;
    for (auto& c : align.covered)
    {
        // オフボの k ↔ 原曲の k + offset。両方の中にある所だけ
        const auto from = std::max ({ c.karaokeStart, (int64) 0, -c.offsetSamples });
        const auto to = std::min ({ c.karaokeEnd, karaokeLength, referenceLength - c.offsetSamples });
        std::vector<Block> blocks;
        for (auto a = from; a < to; a += blockLength)
        {
            Block b;
            b.start = a;
            b.end = juce::jmin (to, a + blockLength);
            double rk = 0.0, kk = 0.0, rr = 0.0;
            for (auto k = b.start; k < b.end; ++k)
            {
                const double r = reference[k + c.offsetSamples], v = karaoke[k];
                rk += r * v;
                kk += v * v;
                rr += r * r;
            }
            b.gain = kk > 0.0 ? juce::jlimit (0.0, 2.0, rk / kk) : 0.0;
            const auto residual = juce::jmax (0.0, rr - 2.0 * b.gain * rk + b.gain * b.gain * kk);
            b.loud = rr / (double) (b.end - b.start) > quietBlock;
            b.ratioDb = rr > 0.0 && residual > 0.0 ? 10.0 * std::log10 (residual / rr) : -100.0;
            blocks.push_back (b);
        }
        // 引く量はゆっくりしか変わらない（マスタリング・フェード）。声のあるブロックは声の分だけずれるので、
        // 前後 4 ブロック（±2 秒）の中央値でならす
        std::vector<double> smoothed (blocks.size());
        for (size_t i = 0; i < blocks.size(); ++i)
        {
            std::vector<double> g;
            for (size_t j = i >= 4 ? i - 4 : 0; j < std::min (blocks.size(), i + 5); ++j)
                if (blocks[j].loud)
                    g.push_back (blocks[j].gain);
            if (g.empty())
            {
                smoothed[i] = blocks[i].gain;
                continue;
            }
            std::nth_element (g.begin(), g.begin() + (std::ptrdiff_t) (g.size() / 2), g.end());
            smoothed[i] = g[g.size() / 2];
        }
        for (size_t i = 0; i < blocks.size(); ++i)
            blocks[i].gain = smoothed[i];

        total += to - from;
        regions.push_back (std::move (blocks));
    }

    // 引けたか：大きい音のブロックのうち、残りが小さい方から 10% の所（伴奏だけの所）
    std::vector<double> ratios;
    for (auto& blocks : regions)
        for (auto& b : blocks)
            if (b.loud)
                ratios.push_back (b.ratioDb);
    if (ratios.empty() || total <= 0)
        return result;   // notAligned（重なる所に音が無い）
    std::sort (ratios.begin(), ratios.end());
    result.cleanDb = (float) ratios[ratios.size() / 10];
    if (result.cleanDb > cleanLimitDb)
    {
        result.status = RefPitchResult::Status::needsSeparation;
        return result;
    }

    // 2 回目：残り（声）を作って、自分の声と同じ検出に通す。ゲインはブロックの真ん中どうしを直線でつなぐ
    if (vocalsOut != nullptr)
        vocalsOut->assign ((size_t) karaokeLength, 0.0f);
    audio::PitchAnalyzer analyzer;
    std::vector<float> chunk;
    std::vector<int64> pos;
    int64 done = 0;
    for (size_t ri = 0; ri < regions.size(); ++ri)
    {
        const auto& blocks = regions[ri];
        if (blocks.empty())
            continue;
        const auto offset = align.covered[ri].offsetSamples;
        analyzer.prepare (sampleRate);   // 区間ごとに作り直す（区間をまたいで線をつながない）
        auto gainAt = [&blocks, blockLength] (int64 k)
        {
            const auto x = (double) (k - blocks.front().start) / (double) blockLength - 0.5;
            const auto i = juce::jlimit (0, (int) blocks.size() - 1, (int) std::floor (x));
            const auto j = juce::jmin ((int) blocks.size() - 1, i + 1);
            const auto t = juce::jlimit (0.0, 1.0, x - i);
            return blocks[(size_t) i].gain + (blocks[(size_t) j].gain - blocks[(size_t) i].gain) * t;
        };

        const auto from = blocks.front().start, to = blocks.back().end;
        for (auto a = from; a < to; a += 8192)
        {
            const auto n = (int) juce::jmin ((int64) 8192, to - a);
            chunk.resize ((size_t) n);
            pos.resize ((size_t) n);
            for (int i = 0; i < n; ++i)
            {
                const auto k = a + i;
                chunk[(size_t) i] = (float) (reference[k + offset] - gainAt (k) * karaoke[k]);
                pos[(size_t) i] = k;
                if (vocalsOut != nullptr && k >= 0 && k < karaokeLength)
                    (*vocalsOut)[(size_t) k] = chunk[(size_t) i];
            }
            analyzer.process (chunk.data(), pos.data(), n, result.points);
            done += n;
            if (progress && ! progress ((float) done / (float) total))
            {
                result.points.clear();
                result.status = RefPitchResult::Status::cancelled;
                return result;
            }
        }
    }

    // 声の無い所（前奏・間奏）に残る符号化の差の音程を消す：同じ時刻の原曲の山（±10 ms）と比べて小さすぎる点
    {
        const auto half = (int64) (0.01 * sampleRate);
        for (auto& p : result.points)
        {
            if (p.confidence <= 0.0f)
                continue;
            float peak = 0.0f;
            const auto k = p.songSample;
            for (const auto& c : align.covered)
            {
                if (k < c.karaokeStart || k >= c.karaokeEnd)
                    continue;
                for (auto j = juce::jmax ((int64) 0, k + c.offsetSamples - half); j < juce::jmin (referenceLength, k + c.offsetSamples + half); ++j)
                    peak = juce::jmax (peak, std::abs (reference[j]));
                break;
            }
            const auto refDb = peak > 0.0f ? 20.0f * std::log10 (peak) : -100.0f;
            if (p.levelDb < refDb - residualGateDb)
                p.confidence = 0.0f;
        }
    }

    // 検出の誤り（子音・息・短いオクターブの飛び・小さな切れ目）だけを取り、歌い方は残す（GuideClean）
    result.points = cleanGuideContour (result.points, sampleRate);

    int voiced = 0;
    for (auto& p : result.points)
        voiced += p.confidence >= 0.5f ? 1 : 0;
    result.voicedRatio = result.points.empty() ? 0.0f : (float) voiced / (float) result.points.size();
    result.status = RefPitchResult::Status::ok;
    return result;
}

RefPitchResult pitchFromVocals (const float* vocals, int64 vocalsLength, int64 karaokeLength,
                                double sampleRate, const AlignResult& align,
                                const std::function<bool (float)>& progress,
                                std::vector<float>* vocalsOut)
{
    RefPitchResult result;
    if (! align.found() || align.covered.empty() || sampleRate <= 0.0 || vocalsLength <= 0)
        return result;   // notAligned

    int64 total = 0;
    for (auto& c : align.covered)
        total += juce::jmax ((int64) 0, juce::jmin (c.karaokeEnd, karaokeLength) - c.karaokeStart);

    if (vocalsOut != nullptr)
        vocalsOut->assign ((size_t) juce::jmax ((int64) 0, karaokeLength), 0.0f);
    audio::PitchAnalyzer analyzer;
    std::vector<float> chunk;
    std::vector<int64> pos;
    int64 done = 0;
    for (auto& c : align.covered)
    {
        analyzer.prepare (sampleRate);   // 区間ごとに作り直す（区間をまたいで線をつながない）
        const auto from = juce::jmax ((int64) 0, c.karaokeStart), to = juce::jmin (c.karaokeEnd, karaokeLength);
        for (auto a = from; a < to; a += 8192)
        {
            const auto n = (int) juce::jmin ((int64) 8192, to - a);
            chunk.resize ((size_t) n);
            pos.resize ((size_t) n);
            for (int i = 0; i < n; ++i)
            {
                const auto k = a + i;
                const auto r = k + c.offsetSamples;   // 原曲の位置
                chunk[(size_t) i] = r >= 0 && r < vocalsLength ? vocals[r] : 0.0f;
                pos[(size_t) i] = k;
                if (vocalsOut != nullptr)
                    (*vocalsOut)[(size_t) k] = chunk[(size_t) i];
            }
            analyzer.process (chunk.data(), pos.data(), n, result.points);
            done += n;
            if (progress && ! progress ((float) done / (float) juce::jmax ((int64) 1, total)))
            {
                result.points.clear();
                result.status = RefPitchResult::Status::cancelled;
                return result;
            }
        }
    }

    // 検出の誤り（子音・息・短いオクターブの飛び・小さな切れ目）だけを取り、歌い方は残す（GuideClean）
    result.points = cleanGuideContour (result.points, sampleRate);

    int voiced = 0;
    for (auto& p : result.points)
        voiced += p.confidence >= 0.5f ? 1 : 0;
    result.voicedRatio = result.points.empty() ? 0.0f : (float) voiced / (float) result.points.size();
    result.status = RefPitchResult::Status::ok;
    return result;
}
} // namespace vb::analysis
