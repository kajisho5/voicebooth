#pragma once

#include "../Theme.h"

namespace vb
{
/** 色の面（左：彩度×明るさ、右：色相の帯）。スキンエディタで使う小さな色選び
    ドラッグ中は onChange が続けて呼ばれる。値は HSV で持つ（灰色でも色相を失わない） */
class ColourPad : public juce::Component
{
public:
    ColourPad();

    void setCurrentColour (juce::Colour, juce::NotificationType = juce::sendNotification);
    juce::Colour getColour() const { return juce::Colour::fromHSV (hue, sat, val, 1.0f); }

    std::function<void (juce::Colour)> onChange;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> padArea() const;
    juce::Rectangle<float> hueArea() const;
    void dragTo (juce::Point<float>);

    float hue = 0.0f, sat = 0.0f, val = 0.0f;
    enum class Part { none, pad, hue } dragging = Part::none;
};
} // namespace vb
