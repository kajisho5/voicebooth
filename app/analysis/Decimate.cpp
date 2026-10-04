#include "Decimate.h"
#include <cmath>

namespace vb::analysis
{
int analysisFactor (double sampleRate)
{
    if (sampleRate <= 0.0)
        return 1;
    return juce::jmax (1, (int) std::floor ((sampleRate + 1.0) / 48000.0));
}

std::vector<float> decimate (const float* x, juce::int64 length, int factor)
{
    if (x == nullptr || length <= 0)
        return {};
    if (factor <= 1)
        return std::vector<float> (x, x + length);

    // 窓付き sinc（Blackman）。通過域の端は新しいナイキストの 0.9 倍
    const int half = 8 * factor;
    const double fc = 0.45 / factor;   // 元の SR に対する割合（0.5 がナイキスト）
    std::vector<float> taps ((size_t) (2 * half + 1));
    double sum = 0.0;
    for (int i = -half; i <= half; ++i)
    {
        const double sinc = i == 0 ? 2.0 * fc : std::sin (2.0 * juce::MathConstants<double>::pi * fc * i) / (juce::MathConstants<double>::pi * i);
        const double w = 0.42 + 0.5 * std::cos (juce::MathConstants<double>::pi * i / half) + 0.08 * std::cos (2.0 * juce::MathConstants<double>::pi * i / half);
        taps[(size_t) (i + half)] = (float) (sinc * w);
        sum += sinc * w;
    }
    for (auto& t : taps)
        t = (float) (t / sum);   // 直流の大きさを 1 に

    const auto outLen = (length + factor - 1) / factor;
    std::vector<float> out ((size_t) outLen);
    for (juce::int64 o = 0; o < outLen; ++o)
    {
        const auto c = o * factor;
        double acc = 0.0;
        const auto from = juce::jmax ((juce::int64) -half, -c), to = juce::jmin ((juce::int64) half, length - 1 - c);
        for (auto i = from; i <= to; ++i)
            acc += (double) taps[(size_t) (i + half)] * x[c + i];
        out[(size_t) o] = (float) acc;
    }
    return out;
}
} // namespace vb::analysis
