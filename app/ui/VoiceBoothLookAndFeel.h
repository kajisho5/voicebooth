#pragma once

#include "Theme.h"

namespace vb
{
/** 標準ウィジェット（スライダー等）を DESIGN 4.9 のトークンで描く */
class VoiceBoothLookAndFeel : public juce::LookAndFeel_V4
{
public:
    VoiceBoothLookAndFeel();

    juce::Typeface::Ptr getTypefaceForFont (const juce::Font&) override;

    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;
};
} // namespace vb
