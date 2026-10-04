#include "ShortcutsDialog.h"

namespace vb
{
namespace
{
    constexpr int rowH = 40, rowGap = 4, keyW = 150;
}

ShortcutsDialog::ShortcutsDialog (UiSession& u)
    : DialogPanel (tr ("shortcuts.title"), tr ("shortcuts.micro")), SessionView (u)
{
    for (int i = 0; i < shortcuts::numActions; ++i)
    {
        auto& k = keys[i];
        k.withFont (mono (13.0f, Weight::semibold));
        k.setDescription (tr (shortcuts::nameKey ((shortcuts::Action) i)));   // 読み上げ：キーの文字（R など）＋操作の名前
        k.onClick = [this, i] { startCapture (capturing == i ? -1 : i); };
        addAndMakeVisible (k);
    }

    addFooterKey (tr ("common.close"), KeyRole::primary, [this] { if (onCloseRequest) onCloseRequest(); });
    addFooterKey (tr ("shortcuts.reset"), KeyRole::normal, [this]
    {
        capturing = -1;
        note = tr ("shortcuts.resetDone");
        session.resetShortcuts();
    });

    setWantsKeyboardFocus (true);
    setSize (620, headerH + 14 + 58 + shortcuts::numActions * (rowH + rowGap) + 40 + footerH + 6);
    refreshKeys();
}

void ShortcutsDialog::onSessionChanged (juce::uint32 c)
{
    if (c & change::prefs)
        refreshKeys();
}

void ShortcutsDialog::startCapture (int index)
{
    capturing = index;
    if (index >= 0)
        note.clear();
    refreshKeys();
    grabKeyboardFocus();   // 次に押したキーをこの画面で受ける
}

void ShortcutsDialog::refreshKeys()
{
    const auto& m = state().shortcuts;
    for (int i = 0; i < shortcuts::numActions; ++i)
    {
        const auto name = m.keyName ((shortcuts::Action) i);
        keys[i].setButtonText (capturing == i ? tr ("shortcuts.press") : (name.isNotEmpty() ? name : tr ("shortcuts.none")));
        keys[i].setToggleState (capturing == i, juce::dontSendNotification);
    }
    repaint();
}

bool ShortcutsDialog::keyPressed (const juce::KeyPress& key)
{
    if (capturing < 0)
        return false;   // 待っていないとき：Esc などはいつもどおり（画面を閉じる）

    const auto action = (shortcuts::Action) capturing;
    const auto code = key.getKeyCode();
    if (code == juce::KeyPress::tabKey)
        return false;   // Tab はキーの間を移る（待つのはやめない）
    if (code == juce::KeyPress::escapeKey)
    {
        capturing = -1;   // やめる（変えない）
        refreshKeys();
        return true;
    }
    if (code == juce::KeyPress::deleteKey || code == juce::KeyPress::backspaceKey)
    {
        capturing = -1;
        note = tr ("shortcuts.cleared", tr (shortcuts::nameKey (action)));
        session.clearShortcut (action);
        return true;
    }
    const auto c = key.getTextCharacter();
    if (key.getModifiers().isCommandDown() || key.getModifiers().isCtrlDown() || ! shortcuts::assignable (juce::CharacterFunctions::toLowerCase (c)))
    {
        // Space・Enter・矢印・Ctrl / ⌘ との組み合わせは付けられない。待ったままにして、使えるキーを案内する
        note = tr ("shortcuts.notAllowed");
        repaint();
        return true;
    }
    capturing = -1;
    if (const auto displaced = session.setShortcut (action, c))
        note = tr ("shortcuts.moved", shortcuts::keyName (juce::CharacterFunctions::toLowerCase (c)), tr (shortcuts::nameKey (*displaced)));
    else
        note.clear();
    refreshKeys();
    return true;
}

void ShortcutsDialog::layoutBody (juce::Rectangle<int> r)
{
    introArea = r.removeFromTop (52);
    r.removeFromTop (6);
    for (int i = 0; i < shortcuts::numActions; ++i)
    {
        rowAreas[i] = r.removeFromTop (rowH);
        r.removeFromTop (rowGap);
        keys[i].setBounds (rowAreas[i].withLeft (rowAreas[i].getRight() - keyW).reduced (0, 4));
    }
    noteArea = r.removeFromTop (36);
}

void ShortcutsDialog::paintBody (juce::Graphics& g, juce::Rectangle<int>)
{
    g.setColour (colours::textDim);
    g.setFont (sans (12.5f));
    g.drawFittedText (tr ("shortcuts.intro"), introArea, juce::Justification::topLeft, 3, 1.0f);

    for (int i = 0; i < shortcuts::numActions; ++i)
    {
        auto row = rowAreas[i];
        g.setColour (capturing == i ? colours::raised : colours::panel);
        g.fillRoundedRectangle (row.toFloat(), 6.0f);
        g.setColour (colours::text);
        g.setFont (sans (13.5f, Weight::medium));
        g.drawText (tr (shortcuts::nameKey ((shortcuts::Action) i)), row.withTrimmedLeft (14).withTrimmedRight (keyW + 10),
                    juce::Justification::centredLeft, true);
    }

    if (note.isNotEmpty())
    {
        g.setColour (colours::warn);
        g.setFont (sans (12.5f));
        g.drawFittedText (note, noteArea, juce::Justification::centredLeft, 2, 1.0f);
    }
}
} // namespace vb
