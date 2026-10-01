#pragma once

#include "Theme.h"

namespace vb
{
/** JUCE 標準部品（ツールチップ・ポップアップ等）を DESIGN 4.9 に合わせる。
    VoiceBooth 独自の部品は parts/ で自前描画する */
class VoiceBoothLookAndFeel : public juce::LookAndFeel_V4
{
public:
    VoiceBoothLookAndFeel();

    juce::Typeface::Ptr getTypefaceForFont (const juce::Font&) override;

    juce::Rectangle<int> getTooltipBounds (const juce::String&, juce::Point<int>, juce::Rectangle<int>) override;
    void drawTooltip (juce::Graphics&, const juce::String&, int width, int height) override;
};
} // namespace vb
