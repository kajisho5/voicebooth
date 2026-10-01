#pragma once

#include "../Theme.h"

namespace vb
{
/** LED リング付きロータリーエンコーダー（テンポ / キー / リバーブ量）
    ドラッグ・ホイール・ダブルクリックで初期値。描画だけ独自で、操作は juce::Slider に任せる */
class Encoder : public juce::Slider
{
public:
    Encoder (double min, double max, double value, double step, bool bipolar = false,
             colours::Tone ledColour = colours::signal);

    void setPreviewHover (bool h) { previewHover = h; repaint(); }

    void paint (juce::Graphics&) override;

private:
    bool bipolar;
    colours::Tone ledColour;
    bool previewHover = false;
};

/** 見出し + エンコーダー + 数値表示 + 補足 */
class EncoderBlock : public juce::Component
{
public:
    EncoderBlock (const juce::String& label, double min, double max, double value, double step,
                  std::function<juce::String (double)> format, const juce::String& unit = {},
                  bool bipolar = false, colours::Tone ledColour = colours::signal);

    Encoder& encoder() { return enc; }
    std::function<void (double)> onChange;
    void setCaption (const juce::String& c) { caption = c; repaint(); }
    void setLocked (bool l) { locked = l; repaint(); }

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::String label, unit, caption;
    std::function<juce::String (double)> format;
    Encoder enc;
    bool locked = false;
    juce::Rectangle<int> labelArea, valueArea, captionArea;
};
} // namespace vb
