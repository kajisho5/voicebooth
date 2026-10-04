#pragma once

#include "../Theme.h"

/*  キーボードで使えるようにする（#28）。部品は Tab で移れて、マウスで押してもフォーカスを取らない
    （Space の再生・R の録音などのショートカットは、今までどおりメイン画面が受ける）。
    枠は Tab で移ってきたときだけ描く（DESIGN 4.10.1 FC） */

namespace vb::focus
{
/** Tab で移れる。マウスで押してもフォーカスは移らない */
inline void tabOnly (juce::Component& c)
{
    c.setWantsKeyboardFocus (true);
    c.setMouseClickGrabsKeyboardFocus (false);
}

/** マウスで押したら、Tab で移っていたフォーカスを親（メイン画面・ダイアログ）へ返す。
    返さないと Enter（歌詞を合わせる）・↑ ↓ などのショートカットを部品が取り続ける */
inline void handBack (juce::Component& c)
{
    if (! c.hasKeyboardFocus (true))
        return;
    for (auto* p = c.getParentComponent(); p != nullptr; p = p->getParentComponent())
        if (p->getWantsKeyboardFocus())
        {
            p->grabKeyboardFocus();
            return;
        }
    c.giveAwayKeyboardFocus();
}

/** Tab で移ってきたときだけ描くフォーカスの枠 */
struct Ring
{
    bool shown = false;

    void gained (juce::Component& c, juce::Component::FocusChangeType cause)
    {
        shown = cause == juce::Component::focusChangedByTabKey;
        c.repaint();
    }
    void lost (juce::Component& c)
    {
        shown = false;
        c.repaint();
    }
    void paint (juce::Graphics& g, const juce::Component& c, juce::Rectangle<float> r, float radius) const
    {
        if (! shown || ! c.hasKeyboardFocus (false))
            return;
        g.setColour (colours::signal);
        g.drawRoundedRectangle (r.reduced (1.0f), radius, 2.0f);
    }
};

/** 選択肢の部品（プルダウン・切り替え）を読み上げで「名前・今の値」として読ませる。値の文字で選び直せる */
class ChoiceValue : public juce::AccessibilityTextValueInterface
{
public:
    ChoiceValue (std::function<juce::StringArray()> optionsIn, std::function<int()> selectedIn, std::function<void (int)> selectIn)
        : options (std::move (optionsIn)), selected (std::move (selectedIn)), select (std::move (selectIn)) {}

    bool isReadOnly() const override { return false; }
    juce::String getCurrentValueAsString() const override { return options()[selected()]; }
    void setValueAsString (const juce::String& v) override
    {
        const auto index = options().indexOf (v);
        if (index >= 0)
            select (index);
    }

private:
    std::function<juce::StringArray()> options;
    std::function<int()> selected;
    std::function<void (int)> select;
};
} // namespace vb::focus
