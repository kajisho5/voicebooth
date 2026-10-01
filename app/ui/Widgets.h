#pragma once

#include "Theme.h"

/*  共通ウィジェット。見た目と「押した感触」だけ持つ。
    Phase A では状態は見た目トグルのみで、音声には何も繋がない。 */

namespace vb
{
enum class Icon
{
    play, pause, stop, toStart, rec, loop, gear, mic, edit,
    metronome, chevronDown, close, compare, rangeIn, rangeOut, lock
};

juce::Path makeIcon (Icon, juce::Rectangle<float> area);

//==============================================================================
/** 角丸のトグル/押しボタン */
class ChipButton : public juce::Button
{
public:
    explicit ChipButton (const juce::String& text, juce::Colour onColour = colours::accent);

    void setLeadingIcon (Icon i)      { icon = i; hasIcon = true; repaint(); }
    void setFontSize (float h)        { fontSize = h; repaint(); }
    void setSubtle (bool s)           { subtle = s; repaint(); }

    /** 推奨幅（文字幅から計算） */
    int idealWidth() const;

    void paintButton (juce::Graphics&, bool over, bool down) override;

private:
    juce::Colour onColour;
    Icon icon = Icon::play;
    bool hasIcon = false, subtle = false;
    float fontSize = 12.0f;
};

//==============================================================================
/** アイコンだけのボタン */
class IconButton : public juce::Button
{
public:
    IconButton (const juce::String& name, Icon, juce::Colour onColour = colours::accent);

    void setIcon (Icon i)          { icon = i; repaint(); }
    void setRound (bool r)         { round = r; repaint(); }
    void setFilled (bool f)        { filled = f; repaint(); }
    void setIconColour (juce::Colour c) { iconColour = c; hasIconColour = true; repaint(); }

    void paintButton (juce::Graphics&, bool over, bool down) override;

private:
    Icon icon;
    juce::Colour onColour, iconColour;
    bool round = false, filled = false, hasIconColour = false;
};

//==============================================================================
/** セグメント切替（モード、カウントイン、録音モード等） */
class SegmentedControl : public juce::Component
{
public:
    SegmentedControl (juce::StringArray options, int selected, juce::Colour onColour = colours::accent);

    void setSelected (int index);
    int getSelected() const { return selected; }
    std::function<void (int)> onChange;

    int idealWidth (float fontSize = 12.0f) const;

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    int indexAt (juce::Point<int>) const;

    juce::StringArray options;
    int selected = 0, hover = -1;
    juce::Colour onColour;
};

//==============================================================================
/** ノブ + 見出し + 数値 */
class LabeledKnob : public juce::Component
{
public:
    LabeledKnob (const juce::String& title, double min, double max, double value, double step,
                 std::function<juce::String (double)> format, bool bipolar = false,
                 juce::Colour colour = colours::accent);

    juce::Slider& slider() { return knob; }
    void setCaption (const juce::String& c) { caption = c; repaint(); }

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    int knobSize() const;

    juce::String title, caption;
    juce::Slider knob;
    std::function<juce::String (double)> format;
};

//==============================================================================
/** 横フェーダー 1 行（ラベル / M / S / フェーダー / 値） */
class FaderRow : public juce::Component
{
public:
    FaderRow (const juce::String& label, float value, bool withMuteSolo = true,
              juce::Colour colour = colours::accent);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::String label;
    ChipButton mute { "M", colours::warn }, solo { "S", colours::accent };
    juce::Slider fader;
    bool withMuteSolo;
};

//==============================================================================
/** 入力メーター（ピーク / RMS / クリップ / 目標帯 -12〜-6 dBFS） */
class InputMeter : public juce::Component
{
public:
    explicit InputMeter (bool isCompact = false) : compact (isCompact) {}

    void setLevels (float peakDb, float rmsDb, float holdDb, bool clipped);

    void paint (juce::Graphics&) override;

    static constexpr float minDb = -60.0f;
    static constexpr float targetLow = -12.0f, targetHigh = -6.0f;

private:
    float dbToX (float db, juce::Rectangle<float> r) const;

    bool compact;
    float peakDb = minDb, rmsDb = minDb, holdDb = minDb;
    bool clipped = false;
};
} // namespace vb
