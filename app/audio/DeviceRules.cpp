#include "DeviceRules.h"

namespace vb::audio
{
bool looksLikeBluetooth (const juce::String& deviceName)
{
    const auto n = deviceName.toLowerCase();
    for (auto* word : { "airpods", "bluetooth", "hands-free", "handsfree", "a2dp", "bluez" })
        if (n.contains (word))
            return true;

    // 「BT」は単語のときだけ（"Subtle" などに当てない）
    juce::StringArray tokens;
    tokens.addTokens (n, " _-()[]/.,:", {});
    return tokens.contains ("bt");
}

int resolveInputChannel (int wanted, int available)
{
    if (available <= 0)
        return -1;
    return juce::isPositiveAndBelow (wanted, available) ? wanted : 0;
}

juce::BigInteger inputChannelMask (int channel)
{
    juce::BigInteger mask;
    if (channel >= 0)
        mask.setBit (channel);
    return mask;
}

ReportedLatency reportedLatency (int inputLatency, int outputLatency, int bufferSize)
{
    const auto in = juce::jmax (0, inputLatency), out = juce::jmax (0, outputLatency);
    if (in + out > 0)
        return { in + out, false };
    return { juce::jmax (0, bufferSize) * 2, true };
}

bool deviceLost (const DeviceSnapshot& before, const DeviceSnapshot& after)
{
    if (! before.open)
        return false;   // もともと開いていない
    if (! after.open)
        return true;
    return before.type != after.type
        || (before.input.isNotEmpty() && before.input != after.input)
        || (before.output.isNotEmpty() && before.output != after.output);
}
} // namespace vb::audio
