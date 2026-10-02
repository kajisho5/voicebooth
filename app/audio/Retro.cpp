#include "Retro.h"
#include <juce_audio_basics/juce_audio_basics.h>

namespace vb::audio::retro
{
int phraseStartFrame (const float* peaks, int count, int pressFrame)
{
    if (peaks == nullptr || count <= 0 || pressFrame <= 0)
        return juce::jmax (0, pressFrame);

    const auto press = juce::jmin (pressFrame, count);
    const auto frames = [] (double seconds) { return juce::jmax (1, juce::roundToInt (seconds / frameSeconds)); };
    const auto windowStart = juce::jmax (0, press - frames (maxBackSeconds) - frames (gapSeconds));

    // 窓の中の静かな所（下から 10%）を雑音の床とする
    std::vector<float> sorted (peaks + windowStart, peaks + press);
    std::sort (sorted.begin(), sorted.end());
    const auto floor = sorted.empty() ? 0.0f : sorted[sorted.size() / 10];
    // 窓が全部声（録り始めから歌っている）だと床が声になるので、床からのしきい値は -30 dBFS で頭打ち
    const auto threshold = juce::jmax (juce::Decibels::decibelsToGain (minVoiceDb),
                                       juce::jmin (floor * juce::Decibels::decibelsToGain (aboveFloorDb),
                                                   juce::Decibels::decibelsToGain (maxThresholdDb)));
    auto voiced = [&] (int i) { return peaks[i] >= threshold; };

    // 押す直前に声が無い：まだ歌っていない。押した所から
    bool recent = false;
    for (int i = juce::jmax (windowStart, press - frames (recentSeconds)); i < press && ! recent; ++i)
        recent = voiced (i);
    if (! recent)
        return pressFrame;

    // 遡って、250 ms 以上の無音の直後を探す
    const auto gap = frames (gapSeconds);
    const auto limit = juce::jmax (0, press - frames (maxBackSeconds));
    int quiet = 0;
    int firstVoiced = press;
    for (int i = press - 1; i >= windowStart; --i)
    {
        if (voiced (i))
        {
            quiet = 0;
            firstVoiced = i;
            if (i < limit)
                return pressFrame;       // 6 秒より前まで切れ目なく続いている
            continue;
        }
        if (++quiet >= gap)
            return juce::jmax (0, firstVoiced - frames (handleSeconds));
    }

    // 窓の頭（録り始め）まで切れ目が無い：録り始めがフレーズの中。録れている頭から
    return firstVoiced < limit ? pressFrame : juce::jmax (0, firstVoiced - frames (handleSeconds));
}
} // namespace vb::audio::retro
