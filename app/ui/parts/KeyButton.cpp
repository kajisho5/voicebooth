#include "KeyButton.h"

namespace vb
{
namespace
{
    constexpr float ledRadius = 2.6f;
    constexpr float sidePad = 10.0f;
    constexpr float ledSlot = 12.0f;   // LED の幅＋間隔
}

KeyButton::KeyButton (const juce::String& label, Kind k)
    : juce::Button (label), kind (k), labelFont (sans (12.0f, Weight::medium))
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setWantsKeyboardFocus (false);   // ショートカットはメイン画面が受ける
    if (kind == Kind::rec)
        setClickingTogglesState (true);
}

KeyButton& KeyButton::withIcon (Icon i)                 { icon = i; repaint(); return *this; }
KeyButton& KeyButton::withLed (juce::Colour c)          { ledColour = c; setClickingTogglesState (true); repaint(); return *this; }
KeyButton& KeyButton::withToggle (bool t)               { setClickingTogglesState (t); return *this; }
KeyButton& KeyButton::withFont (juce::Font f)           { labelFont = f; repaint(); return *this; }
KeyButton& KeyButton::withIconColour (juce::Colour c)   { iconColour = c; repaint(); return *this; }
KeyButton& KeyButton::withLatch (juce::Colour c)        { latchColour = c; setClickingTogglesState (true); repaint(); return *this; }

void KeyButton::flash()
{
    flashing = true;
    repaint();
    juce::Component::SafePointer<KeyButton> safe (this);
    juce::Timer::callAfterDelay (120, [safe]
    {
        if (safe != nullptr) { safe->flashing = false; safe->repaint(); }
    });
}

int KeyButton::idealWidth() const
{
    const auto h = (float) juce::jmax (24, getHeight());
    float w = sidePad * 2.0f;
    if (ledColour.has_value()) w += ledSlot;
    if (icon.has_value())      w += h * 0.5f + (getButtonText().isNotEmpty() ? 6.0f : 0.0f);
    w += textWidth (labelFont, getButtonText());

    if (getButtonText().isEmpty() && ! ledColour.has_value())
        w = h;   // アイコンだけのキーは正方形

    return (int) std::ceil (w);
}

void KeyButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    KeyState s { over, down || flashing, getToggleState(), isEnabled() };
    if (preview) s = *preview;

    const auto bounds = getLocalBounds().toFloat();
    const bool recOn = kind == Kind::rec && s.on;
    const bool latched = latchColour.has_value() && s.on;

    // --- 本体 ---
    if (kind == Kind::ghost && ! s.over && ! s.down)
    {
        // 地に溶ける：本体なし
    }
    else if (recOn)
    {
        auto r = bounds.reduced (0.5f);
        g.setColour (colours::rec.withAlpha (0.25f));
        g.fillRoundedRectangle (r.expanded (2.0f), metrics::keyRadius + 2.0f);
        g.setGradientFill (juce::ColourGradient (colours::rec.brighter (0.15f), r.getX(), r.getY(),
                                                 colours::rec.darker (0.25f), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, metrics::keyRadius);
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.drawRoundedRectangle (r, metrics::keyRadius, 1.0f);
    }
    else
    {
        auto bodyState = s;
        if (latched) bodyState.down = true;   // ラッチ中は沈んだまま
        paint::keycap (g, bounds, bodyState);

        if (latched)
        {
            g.setColour (latchColour->withAlpha (0.12f));
            g.fillRoundedRectangle (bounds.reduced (1.5f), metrics::keyRadius - 1.0f);
        }
    }

    // --- 中身 ---
    const auto pad = juce::jmin (sidePad, bounds.getWidth() * 0.15f);
    auto content = bounds.reduced (pad, 0.0f);
    if (s.down || latched) content.translate (0.0f, 1.0f);

    auto fg = s.enabled ? colours::text : colours::textMute;
    if (kind == Kind::ghost && ! s.over) fg = colours::textDim;
    if (recOn) fg = colours::text;
    if (latched) fg = *latchColour;

    const bool iconOnly = getButtonText().isEmpty() && ! ledColour.has_value();

    if (ledColour.has_value())
    {
        const auto slot = content.removeFromLeft (ledSlot);
        paint::led (g, { slot.getX() + ledRadius, slot.getCentreY() }, ledRadius, *ledColour, s.on);
    }

    if (icon.has_value())
    {
        const auto size = bounds.getHeight() * (iconOnly ? 0.46f : 0.5f);
        juce::Rectangle<float> iconArea;
        if (iconOnly)
            iconArea = bounds.withSizeKeepingCentre (size, size).translated (0.0f, s.down ? 1.0f : 0.0f);
        else
            iconArea = content.removeFromLeft (size).withSizeKeepingCentre (size, size);

        auto ic = iconColour.value_or (fg);
        if (kind == Kind::rec) ic = recOn ? colours::text : colours::rec;
        if (! s.enabled) ic = ic.withMultipliedAlpha (0.4f);
        drawIcon (g, *icon, iconArea, ic);

        if (! iconOnly)
            content.removeFromLeft (6.0f);
    }

    if (getButtonText().isNotEmpty())
    {
        g.setColour (fg);
        g.setFont (labelFont);
        const auto just = (ledColour.has_value() || icon.has_value()) ? juce::Justification::centredLeft
                                                                       : juce::Justification::centred;
        g.drawText (getButtonText(), content, just, false);
    }
}

//==============================================================================
SegmentedKeys::SegmentedKeys (juce::StringArray opts, int sel, juce::Colour led)
    : options (std::move (opts)), selected (sel), ledColour (led), labelFont (sans (12.0f, Weight::medium))
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setWantsKeyboardFocus (false);
}

void SegmentedKeys::setSelected (int index, juce::NotificationType n)
{
    if (index == selected || ! juce::isPositiveAndBelow (index, options.size()))
        return;

    selected = index;
    repaint();
    if (n != juce::dontSendNotification && onChange)
        onChange (selected);
}

int SegmentedKeys::idealWidth() const
{
    float w = 6.0f;
    float widest = 0.0f;
    for (auto& o : options)
        widest = juce::jmax (widest, textWidth (labelFont, o));
    w += (widest + 2.0f * sidePad + ledSlot + 4.0f) * (float) options.size();
    return (int) std::ceil (w);
}

juce::Rectangle<float> SegmentedKeys::segmentBounds (int i) const
{
    const auto r = getLocalBounds().toFloat().reduced (3.0f);
    const auto w = r.getWidth() / (float) juce::jmax (1, options.size());
    return { r.getX() + w * (float) i, r.getY(), w, r.getHeight() };
}

int SegmentedKeys::indexAt (juce::Point<int> p) const
{
    for (int i = 0; i < options.size(); ++i)
        if (segmentBounds (i).contains (p.toFloat()))
            return i;
    return -1;
}

void SegmentedKeys::paint (juce::Graphics& g)
{
    paint::inset (g, getLocalBounds().toFloat(), metrics::keyRadius + 1.0f);

    for (int i = 0; i < options.size(); ++i)
    {
        auto seg = segmentBounds (i);
        const bool sel = i == selected;

        if (sel)
        {
            paint::keycap (g, seg, { i == hover, false, true, isEnabled() });

            // LED と文字をひとかたまりで中央に置く
            const auto tw = textWidth (labelFont, options[i]);
            auto group = seg.withSizeKeepingCentre (juce::jmin (seg.getWidth() - 4.0f, ledSlot + tw), seg.getHeight());
            const auto slot = group.removeFromLeft (ledSlot);
            paint::led (g, { slot.getX() + ledRadius, slot.getCentreY() }, ledRadius, ledColour, true);
            g.setColour (colours::text);
            g.setFont (labelFont);
            g.drawText (options[i], group, juce::Justification::centredLeft, false);
        }
        else
        {
            g.setColour (i == hover ? colours::text.withAlpha (0.9f) : colours::textDim);
            g.setFont (labelFont);
            g.drawText (options[i], seg, juce::Justification::centred, false);
        }

        // 区切り（選択の隣は描かない）
        if (i > 0 && ! sel && i - 1 != selected)
            paint::vline (g, seg.getX(), seg.getY() + 6.0f, seg.getBottom() - 6.0f, colours::line);
    }
}

void SegmentedKeys::mouseMove (const juce::MouseEvent& e) { hover = indexAt (e.getPosition()); repaint(); }
void SegmentedKeys::mouseExit (const juce::MouseEvent&)   { hover = -1; repaint(); }
void SegmentedKeys::mouseDown (const juce::MouseEvent& e) { setSelected (indexAt (e.getPosition())); }
} // namespace vb
