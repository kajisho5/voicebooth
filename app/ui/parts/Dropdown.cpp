#include "Dropdown.h"

namespace vb
{
Dropdown::Dropdown (juce::StringArray i, int sel)
    : items (std::move (i)), selected (sel), labelFont (sans (13.0f, Weight::medium))
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    focus::tabOnly (*this);
}

void Dropdown::setSelected (int index, juce::NotificationType n)
{
    if (index == selected || ! juce::isPositiveAndBelow (index, items.size()))
        return;

    selected = index;
    repaint();
    if (auto* h = getAccessibilityHandler())
        h->notifyAccessibilityEvent (juce::AccessibilityEvent::valueChanged);   // 読み上げに新しい値を伝える（#28）
    if (n != juce::dontSendNotification && onChange)
        onChange (selected);
}

int Dropdown::idealWidth() const
{
    float widest = 0.0f;
    for (auto& s : items)
        widest = juce::jmax (widest, textWidth (labelFont, s));
    return (int) std::ceil (widest) + 56;
}

void Dropdown::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    paint::keycap (g, b, { isMouseOver() || open, false, false, isEnabled() });

    auto r = b.reduced (14.0f, 0.0f);
    drawIcon (g, Icon::chevronDown, r.removeFromRight (16.0f).withSizeKeepingCentre (14.0f, 14.0f),
              open ? colours::signal : colours::textDim);
    r.removeFromRight (6.0f);

    g.setColour (colours::text);
    g.setFont (sansForLanguageName (items[selected], labelFont.getHeight(), Weight::medium));
    g.drawText (items[selected], r, juce::Justification::centredLeft, true);
    ring.paint (g, *this, b, metrics::keyRadius);
}

void Dropdown::mouseDown (const juce::MouseEvent&)
{
    focus::handBack (*this);
    showMenu();
}

bool Dropdown::keyPressed (const juce::KeyPress& key)
{
    if (key.getModifiers().isAnyModifierKeyDown())
        return false;
    if (key == juce::KeyPress::returnKey || key == juce::KeyPress::spaceKey || key == juce::KeyPress::downKey)
    {
        showMenu();
        return true;
    }
    return false;
}

std::unique_ptr<juce::AccessibilityHandler> Dropdown::createAccessibilityHandler()
{
    return std::make_unique<juce::AccessibilityHandler> (
        *this, juce::AccessibilityRole::comboBox,
        juce::AccessibilityActions().addAction (juce::AccessibilityActionType::press, [this] { showMenu(); }),
        juce::AccessibilityHandler::Interfaces { std::make_unique<focus::ChoiceValue> (
            [this] { return items; }, [this] { return selected; }, [this] (int i) { setSelected (i); }) });
}

void Dropdown::showMenu()
{
    // JUCE は無効な部品にもクリックを渡す（灰色のプルダウンで選べ、録音中にスキンが変わっていた。バグチェック 2026-10-05）
    if (! isEnabled())
        return;
    juce::PopupMenu menu;
    for (int i = 0; i < items.size(); ++i)
        menu.addItem (i + 1, items[i], true, i == selected);

    open = true;
    repaint();

    juce::Component::SafePointer<Dropdown> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options()
                            .withTargetComponent (this)
                            .withMinimumWidth (getWidth())
                            .withStandardItemHeight (32),
                        [safe] (int result)
                        {
                            if (safe == nullptr) return;
                            safe->open = false;
                            safe->repaint();
                            if (result > 0)
                                safe->setSelected (result - 1);
                        });
}
} // namespace vb
