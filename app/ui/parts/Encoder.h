#pragma once

#include "../Theme.h"
#include "../Animator.h"

namespace vb
{
/** LED リング付きロータリーエンコーダー（テンポ / キー / リバーブ量）

    動き（DESIGN 4.10「ツマミ」）：
    - 上下のドラッグ。速く回すと大きく、ゆっくりだと細かく（Shift でさらに細かく）
    - 既定値（テンポ 100%＝原速、キー 0）でカチッと止まる。ダブルクリックで既定値へ
    - 指標・LED の輪・数値は値をばねでなめらかに追う（値そのものは setRange の刻みのまま） */
class Encoder : public juce::Slider,
                private motion::Animated
{
public:
    Encoder (double min, double max, double value, double step, bool bipolar = false,
             colours::Tone ledColour = colours::signal);

    /** 既定値（吸い付き・ダブルクリックの行き先）。作った時は bipolar なら 0、そうでなければ最初の値 */
    void setDefaultValue (double v) { defaultValue = v; }
    double getDefaultValue() const { return defaultValue; }

    void setPreviewHover (bool h) { previewHover = h; repaint(); }

    /** 見た目の値（ばねで追っている途中の値。数値の表示に使う） */
    double getShownValue() const { return isAnimating() ? (double) shown.x : getValue(); }

    /** 既定値に吸い付いた直後の光（0..1。見出しの色に使う） */
    float getDetentFlash() const { return flash; }

    /** 見た目の値が変わった（EncoderBlock が数値を描き直す） */
    std::function<void()> onShownChange;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void valueChanged() override;

private:
    bool advanceAnimation (float dt) override;
    double range() const { return getMaximum() - getMinimum(); }

    bool bipolar;
    colours::Tone ledColour;
    bool previewHover = false;
    double defaultValue = 0.0;

    motion::Spring shown;   // 見た目の値
    float flash = 0.0f;
    double raw = 0.0, lastDragMs = 0.0;
    float lastY = 0.0f;
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
