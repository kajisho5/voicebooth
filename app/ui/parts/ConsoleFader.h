#pragma once

#include "KeyButton.h"

namespace vb
{
/** 縦のコンソールフェーダー（溝 + 目盛り + キャップ + 横の LED メーター） */
class ConsoleFader : public juce::Slider
{
public:
    explicit ConsoleFader (double value, juce::Colour capLine = colours::signal);

    /** 横に出すモニター音量メーター（0..1、負なら非表示） */
    void setMeter (float level) { meter = level; repaint(); }
    void setPreviewHover (bool h) { previewHover = h; repaint(); }

    void paint (juce::Graphics&) override;

    static constexpr float capW = 30.0f, capH = 16.0f;

private:
    juce::Colour capLine;
    float meter = -1.0f;
    bool previewHover = false;
};

/** 1 チャンネル：名前 / フェーダー / 値 / M S */
class ChannelStrip : public juce::Component
{
public:
    ChannelStrip (const juce::String& name, double value, float meterLevel,
                  juce::Colour capLine = colours::signal, bool withMuteSolo = true,
                  const juce::String& note = {});

    ConsoleFader& fader() { return slider; }
    KeyButton& muteKey() { return mute; }
    KeyButton& soloKey() { return solo; }

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::String name, note;
    ConsoleFader slider;
    KeyButton mute { "M" }, solo { "S" };
    bool withMuteSolo;
    juce::Rectangle<int> nameArea, valueArea;
};
} // namespace vb
