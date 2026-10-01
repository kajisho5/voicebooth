#include "WaveformOverview.h"
#include <juce_audio_basics/juce_audio_basics.h>

namespace vb::audio
{
namespace
{
    void merge (WaveformOverview::Peak& into, const WaveformOverview::Peak& p, bool& first)
    {
        if (first)
        {
            into = p;
            first = false;
            return;
        }
        into.min = juce::jmin (into.min, p.min);
        into.max = juce::jmax (into.max, p.max);
    }
}

WaveformOverview::WaveformOverview (int64 lengthSamples)
    : length (juce::jmax ((int64) 0, lengthSamples))
{
    const auto fineCount = (size_t) ((length + samplesPerBin - 1) / samplesPerBin);
    fine.resize (fineCount);
    fineSq.resize (fineCount);
    coarse.resize ((fineCount + binsPerCoarseBin - 1) / binsPerCoarseBin);
    coarseSq.resize (coarse.size());
}

void WaveformOverview::append (const float* const* channels, int numChannels, int numSamples)
{
    jassert (numChannels > 0 && channels != nullptr);

    // 長さを超えた分は捨てる（読み手が長さより多く返しても表は伸ばさない）
    numSamples = (int) juce::jmin ((int64) numSamples, length - appended);
    int offset = 0;

    while (numSamples > 0)
    {
        // 今のビンの残りまでをまとめて調べる
        const auto n = juce::jmin (numSamples, samplesPerBin - inCurrent);

        for (int c = 0; c < numChannels; ++c)
        {
            const auto r = juce::FloatVectorOperations::findMinAndMax (channels[c] + offset, n);
            if (inCurrent == 0 && c == 0)
            {
                current = { r.getStart(), r.getEnd() };
            }
            else
            {
                current.min = juce::jmin (current.min, r.getStart());
                current.max = juce::jmax (current.max, r.getEnd());
            }

            double sq = 0.0;
            for (int i = 0; i < n; ++i)
                sq += (double) channels[c][offset + i] * channels[c][offset + i];
            currentSq += sq / numChannels;
        }

        inCurrent += n;
        appended += n;
        offset += n;
        numSamples -= n;

        if (inCurrent == samplesPerBin || appended == length)
            finishBin();
    }
}

void WaveformOverview::finishBin()
{
    if (inCurrent == 0)
        return;

    const auto index = (size_t) ((appended - 1) / samplesPerBin);
    fine[index] = current;
    fineSq[index] = (float) currentSq;
    coarseSq[index / binsPerCoarseBin] += currentSq;
    overall = juce::jmax (overall, current.magnitude());

    // 粗い段：その粗いビンの最初の細かいビンなら上書き、それ以外は合わせる
    auto& c = coarse[index / binsPerCoarseBin];
    if (index % binsPerCoarseBin == 0)
    {
        c = current;
    }
    else
    {
        c.min = juce::jmin (c.min, current.min);
        c.max = juce::jmax (c.max, current.max);
    }

    current = {};
    currentSq = 0.0;
    inCurrent = 0;
}

int64 WaveformOverview::binCount (size_t fineIndex) const
{
    const auto start = (int64) fineIndex * samplesPerBin;
    return juce::jlimit ((int64) 0, (int64) samplesPerBin, length - start);
}

float WaveformOverview::getRms (int64 start, int64 end) const
{
    start = juce::jmax ((int64) 0, start);
    end = juce::jmin (length, end);
    if (end <= start)
        return 0.0f;

    auto i = (size_t) (start / samplesPerBin);
    const auto last = (size_t) ((end - 1) / samplesPerBin);

    double sum = 0.0;
    int64 count = 0;
    while (i <= last)
    {
        if (i % binsPerCoarseBin == 0 && i + binsPerCoarseBin - 1 <= last)
        {
            sum += coarseSq[i / binsPerCoarseBin];
            // 曲の最後の粗いビンは半端なことがある
            count += juce::jmin (length, (int64) (i + binsPerCoarseBin) * samplesPerBin) - (int64) i * samplesPerBin;
            i += binsPerCoarseBin;
        }
        else
        {
            sum += fineSq[i];
            count += binCount (i);
            ++i;
        }
    }
    return count > 0 ? (float) std::sqrt (sum / (double) count) : 0.0f;
}

WaveformOverview::Peak WaveformOverview::getPeak (int64 start, int64 end) const
{
    start = juce::jmax ((int64) 0, start);
    end = juce::jmin (length, end);
    if (end <= start)
        return {};

    auto i = (size_t) (start / samplesPerBin);
    const auto last = (size_t) ((end - 1) / samplesPerBin);

    Peak p;
    bool first = true;
    while (i <= last)
    {
        // 粗いビンが丸ごと入るところは粗い段で（長い範囲でも数百回で済む）
        if (i % binsPerCoarseBin == 0 && i + binsPerCoarseBin - 1 <= last)
        {
            merge (p, coarse[i / binsPerCoarseBin], first);
            i += binsPerCoarseBin;
        }
        else
        {
            merge (p, fine[i], first);
            ++i;
        }
    }
    return p;
}
} // namespace vb::audio
