#pragma once

#include "Icons.h"
#include <optional>

namespace vb
{
/** キーキャップ型ボタン。状態は枠色ではなく LED の点灯で示す（DESIGN 4.9）

    [LED] [icon] label
    - key   : 通常のキー
    - ghost : 地に溶けるキー（ホバーで浮く）
    - rec   : 録音キー。オンでタリー赤に点灯 */
class KeyButton : public juce::Button
{
public:
    enum class Kind { key, ghost, rec };

    explicit KeyButton (const juce::String& label = {}, Kind = Kind::key);

    KeyButton& withIcon (Icon);
    KeyButton& withLed (colours::Tone = colours::signal);    // トグル状態を LED で表示
    KeyButton& withToggle (bool clickingToggles = true);
    KeyButton& withFont (juce::Font);
    KeyButton& withIconColour (colours::Tone);

    /** ラッチ式（M / S 等の小さなキー）：オンで沈み、文字が点灯色になる */
    KeyButton& withLatch (colours::Tone litColour);

    /** ショートカットで押された時の見た目（一瞬沈む） */
    void flash();

    /** ギャラリー用：状態を固定して描く */
    void setPreview (std::optional<KeyState> s) { preview = s; repaint(); }

    int idealWidth() const;

    void paintButton (juce::Graphics&, bool over, bool down) override;

private:
    Kind kind;
    std::optional<Icon> icon;
    std::optional<colours::Tone> ledColour, iconColour, latchColour;   // スキンを変えても今の色で描く
    juce::Font labelFont;
    std::optional<KeyState> preview;
    bool flashing = false;
};

//==============================================================================
/** スライドスイッチ型の択一（モード / カウントイン / 録音モード） */
class SegmentedKeys : public juce::Component
{
public:
    SegmentedKeys (juce::StringArray options, int selected, colours::Tone led = colours::signal);

    void setSelected (int index, juce::NotificationType = juce::sendNotification);
    int getSelected() const { return selected; }
    std::function<void (int)> onChange;

    void setFont (juce::Font f) { labelFont = f; repaint(); }
    void setPreviewHover (int index) { hover = index; repaint(); }

    int idealWidth() const;

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    int indexAt (juce::Point<int>) const;
    juce::Rectangle<float> segmentBounds (int index) const;

    juce::StringArray options;
    int selected = 0, hover = -1;
    colours::Tone ledColour;
    juce::Font labelFont;
};
} // namespace vb
