#pragma once

#include "Icons.h"
#include "../Animator.h"
#include "Focus.h"
#include <optional>

namespace vb
{
/** キーキャップ型ボタン。状態は枠色ではなく LED の点灯で示す（DESIGN 4.9）

    [LED] [icon] label
    - key   : 通常のキー
    - ghost : 地に溶けるキー（ホバーで浮く）
    - rec   : 録音キー。オンでタリー赤に点灯

    動き（DESIGN 4.10「キー」）：押すとばねで沈んで戻る。LED は消える時に余韻を残す。REC 中は LED がゆっくり呼吸する。
    フォーカスの枠はキーボード（Tab）で移った時だけ。ショートカットはツールチップに添える */
class KeyButton : public juce::Button,
                  private motion::Animated
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

    /** オンの間、LED（REC キーの ●）がゆっくり呼吸する（録音キー。DESIGN 4.10「キー」） */
    KeyButton& withBreathing (bool b = true) { breathes = b; return *this; }

    /** ショートカットで押された時の見た目（一瞬沈む） */
    void flash();

    /** ショートカット（"Space" "R" など）。ツールチップの右にキーの形で添える */
    KeyButton& withShortcut (const juce::String& keyName) { shortcut = keyName; return *this; }
    juce::String getTooltip() override;
    /** アイコンだけのキーと「M」「S」のような短いキーは、ツールチップの文を読み上げの名前にする（#28） */
    void setTooltip (const juce::String&) override;

    /** ギャラリー用：状態を固定して描く */
    void setPreview (std::optional<KeyState> s) { preview = s; repaint(); }

    int idealWidth() const;

    void paintButton (juce::Graphics&, bool over, bool down) override;
    void buttonStateChanged() override;
    void focusGained (FocusChangeType) override;
    void focusLost (FocusChangeType) override;

private:
    bool advanceAnimation (float dt) override;

    Kind kind;
    std::optional<Icon> icon;
    std::optional<colours::Tone> ledColour, iconColour, latchColour;   // スキンを変えても今の色で描く
    juce::Font labelFont;
    std::optional<KeyState> preview;
    juce::String shortcut;

    motion::Spring press;          // 0 = 浮いている、1 = 沈んだ（行き過ぎると少し負）
    float ledLevel = 0.0f;         // LED・点灯色の明るさ（余韻）
    float flashLeft = 0.0f;        // ショートカットで沈んでいる残り時間
    bool focusRing = false;        // キーボードで移ってきた
    bool breathes = false;
};

//==============================================================================
/** スライドスイッチ型の択一（モード / カウントイン / 録音モード）
    動き（DESIGN 4.10「切り替え」）：キーキャップごとばねで滑り、少し行き過ぎて戻る。止まってから LED が点く */
class SegmentedKeys : public juce::Component,
                      public juce::SettableTooltipClient,
                      private motion::Animated
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
    /** ← → で選び直す（Tab で移ってきたとき。#28） */
    bool keyPressed (const juce::KeyPress&) override;
    void focusGained (FocusChangeType c) override { ring.gained (*this, c); }
    void focusLost (FocusChangeType) override     { ring.lost (*this); }
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

private:
    bool advanceAnimation (float dt) override;
    int indexAt (juce::Point<int>) const;
    juce::Rectangle<float> segmentBounds (float index) const;

    juce::StringArray options;
    int selected = 0, hover = -1;
    colours::Tone ledColour;
    juce::Font labelFont;

    motion::Spring slide;          // キーキャップの位置（区画の番号。途中は小数）
    float ledLevel = 1.0f;
    focus::Ring ring;
};
} // namespace vb
