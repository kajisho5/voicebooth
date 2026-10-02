#pragma once

#include "KeyButton.h"

namespace vb
{
/** 縦のコンソールフェーダー（溝 + 目盛り + キャップ + 横の LED メーター）。値は 0..1（0.75 = 0 dB）

    動き（DESIGN 4.10「フェーダー」）：
    - つまみを掴んだ所から相対で動く（飛ばない）。Shift で微調整
    - 0 dB でカチッと止まる（少しの間だけ吸い付く）。止まった値は 0.75 ちょうど。それ以外の値は丸めない
    - ダブルクリックで 0 dB へ（値はすぐ 0.75、つまみはばねで戻る）
    - 0 dB の時はつまみの線が光る */
class ConsoleFader : public juce::Slider,
                     private motion::Animated
{
public:
    explicit ConsoleFader (double value, colours::Tone capLine = colours::signal);

    /** 横に出すモニター音量メーター（0..1、負なら非表示） */
    void setMeter (float level) { meter = level; repaint(); }
    void setPreviewHover (bool h) { previewHover = h; repaint(); }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void valueChanged() override;

    static constexpr float capW = 30.0f, capH = 16.0f;

    /** 0 dB ちょうどか（吸い付き・ダブルクリックで決まった値） */
    bool isAtUnity() const { return juce::exactlyEqual (getValue(), motion::fader::unity); }

private:
    bool advanceAnimation (float dt) override;
    float travel() const { return juce::jmax (1.0f, (float) getHeight() - capH); }

    colours::Tone capLine;
    float meter = -1.0f;
    bool previewHover = false;

    motion::Spring shown;      // つまみの見た目の位置（0..1）。値そのものは getValue()
    bool springing = false;    // ダブルクリックで戻っている途中
    double raw = 0.0;          // ドラッグ中の生の位置（吸い付きの前）
    float lastY = 0.0f;
    float unityGlow = 0.0f;    // 0 dB の光（0..1）
};

/** 1 チャンネル：名前 / フェーダー / 値 / M S */
class ChannelStrip : public juce::Component
{
public:
    ChannelStrip (const juce::String& name, double value, float meterLevel,
                  colours::Tone capLine = colours::signal, bool withMuteSolo = true,
                  const juce::String& note = {});

    ConsoleFader& fader() { return slider; }
    KeyButton& muteKey() { return mute; }
    KeyButton& soloKey() { return solo; }

    /** フェーダーが動いた（値の表示はこちらで書き直す。fader().onValueChange は上書きしない） */
    std::function<void()> onFaderChange;
    /** S を出さない（M だけを幅いっぱいに） */
    void setSoloShown (bool shown) { soloShown = shown; solo.setVisible (withMuteSolo && shown); resized(); }

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::String name, note;
    ConsoleFader slider;
    KeyButton mute { "M" }, solo { "S" };
    bool withMuteSolo;
    bool soloShown = true;
    juce::Rectangle<int> nameArea, valueArea;
};
} // namespace vb
