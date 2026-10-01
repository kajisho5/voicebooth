#include "Widgets.h"
#include "VoiceBoothLookAndFeel.h"

namespace vb
{
namespace
{
    juce::Path stroked (const juce::Path& p, float w)
    {
        juce::Path out;
        juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded).createStrokedPath (out, p);
        return out;
    }
}

juce::Path makeIcon (Icon icon, juce::Rectangle<float> a)
{
    juce::Path p;
    const auto s = juce::jmin (a.getWidth(), a.getHeight());
    a = a.withSizeKeepingCentre (s, s);
    const auto c = a.getCentre();
    const auto w = juce::jmax (1.4f, s * 0.11f);

    switch (icon)
    {
        case Icon::play:
            p.addTriangle (a.getX() + s * 0.22f, a.getY() + s * 0.12f,
                           a.getX() + s * 0.22f, a.getBottom() - s * 0.12f,
                           a.getRight() - s * 0.1f, c.y);
            break;

        case Icon::pause:
            p.addRoundedRectangle (a.getX() + s * 0.18f, a.getY() + s * 0.14f, s * 0.22f, s * 0.72f, s * 0.04f);
            p.addRoundedRectangle (a.getRight() - s * 0.40f, a.getY() + s * 0.14f, s * 0.22f, s * 0.72f, s * 0.04f);
            break;

        case Icon::stop:
            p.addRoundedRectangle (a.reduced (s * 0.18f), s * 0.06f);
            break;

        case Icon::toStart:
            p.addRectangle (a.getX() + s * 0.14f, a.getY() + s * 0.16f, s * 0.12f, s * 0.68f);
            p.addTriangle (a.getRight() - s * 0.14f, a.getY() + s * 0.16f,
                           a.getRight() - s * 0.14f, a.getBottom() - s * 0.16f,
                           a.getX() + s * 0.30f, c.y);
            break;

        case Icon::rec:
            p.addEllipse (a.reduced (s * 0.2f));
            break;

        case Icon::loop:
        {
            juce::Path l;
            const auto r = a.reduced (s * 0.14f, s * 0.24f);
            l.startNewSubPath (r.getX() + r.getWidth() * 0.62f, r.getY());
            l.lineTo (r.getRight() - r.getHeight() * 0.5f, r.getY());
            l.addCentredArc (r.getRight() - r.getHeight() * 0.5f, r.getCentreY(), r.getHeight() * 0.5f, r.getHeight() * 0.5f,
                             0.0f, 0.0f, juce::MathConstants<float>::pi, false);
            l.lineTo (r.getX() + r.getHeight() * 0.5f, r.getBottom());
            l.addCentredArc (r.getX() + r.getHeight() * 0.5f, r.getCentreY(), r.getHeight() * 0.5f, r.getHeight() * 0.5f,
                             0.0f, juce::MathConstants<float>::pi, juce::MathConstants<float>::twoPi, false);
            l.lineTo (r.getX() + r.getWidth() * 0.38f, r.getY());
            p = stroked (l, w);
            const auto ah = s * 0.16f;
            p.addTriangle (r.getX() + r.getWidth() * 0.62f + ah * 0.2f, r.getY(),
                           r.getX() + r.getWidth() * 0.62f - ah * 0.8f, r.getY() - ah * 0.8f,
                           r.getX() + r.getWidth() * 0.62f - ah * 0.8f, r.getY() + ah * 0.8f);
            break;
        }

        case Icon::gear:
        {
            const auto ro = s * 0.46f, ri = s * 0.34f;
            const int teeth = 8;
            for (int i = 0; i < teeth * 4; ++i)
            {
                const auto ang = juce::MathConstants<float>::twoPi * (float) i / (float) (teeth * 4);
                const auto r = ((i / 2) % 2 == 0) ? ro : ri;
                const auto pt = c.getPointOnCircumference (r, ang);
                if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
            }
            p.closeSubPath();
            p.addEllipse (juce::Rectangle<float> (s * 0.3f, s * 0.3f).withCentre (c));
            p.setUsingNonZeroWinding (false);
            break;
        }

        case Icon::mic:
        {
            p.addRoundedRectangle (juce::Rectangle<float> (s * 0.32f, s * 0.5f).withCentre ({ c.x, a.getY() + s * 0.33f }), s * 0.16f);
            juce::Path u;
            u.addCentredArc (c.x, a.getY() + s * 0.42f, s * 0.27f, s * 0.27f, 0.0f,
                             juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi * 3.0f, true);
            u.startNewSubPath (c.x, a.getY() + s * 0.70f);
            u.lineTo (c.x, a.getBottom() - s * 0.08f);
            u.startNewSubPath (c.x - s * 0.18f, a.getBottom() - s * 0.08f);
            u.lineTo (c.x + s * 0.18f, a.getBottom() - s * 0.08f);
            p.addPath (stroked (u, w));
            break;
        }

        case Icon::edit:
        {
            juce::Path pen;
            pen.addRoundedRectangle (-s * 0.09f, -s * 0.36f, s * 0.18f, s * 0.56f, s * 0.03f);
            pen.addTriangle (-s * 0.09f, s * 0.24f, s * 0.09f, s * 0.24f, 0.0f, s * 0.40f);
            pen.applyTransform (juce::AffineTransform::rotation (juce::MathConstants<float>::pi * 0.25f).translated (c));
            p = pen;
            break;
        }

        case Icon::metronome:
        {
            juce::Path m;
            m.startNewSubPath (c.x - s * 0.12f, a.getY() + s * 0.12f);
            m.lineTo (c.x + s * 0.12f, a.getY() + s * 0.12f);
            m.lineTo (c.x + s * 0.30f, a.getBottom() - s * 0.12f);
            m.lineTo (c.x - s * 0.30f, a.getBottom() - s * 0.12f);
            m.closeSubPath();
            m.startNewSubPath (c.x, a.getBottom() - s * 0.24f);
            m.lineTo (c.x + s * 0.28f, a.getY() + s * 0.16f);
            p = stroked (m, w);
            break;
        }

        case Icon::chevronDown:
        {
            juce::Path v;
            v.startNewSubPath (a.getX() + s * 0.25f, a.getY() + s * 0.38f);
            v.lineTo (c.x, a.getY() + s * 0.62f);
            v.lineTo (a.getRight() - s * 0.25f, a.getY() + s * 0.38f);
            p = stroked (v, w);
            break;
        }

        case Icon::close:
        {
            juce::Path x;
            x.startNewSubPath (a.getX() + s * 0.28f, a.getY() + s * 0.28f);
            x.lineTo (a.getRight() - s * 0.28f, a.getBottom() - s * 0.28f);
            x.startNewSubPath (a.getRight() - s * 0.28f, a.getY() + s * 0.28f);
            x.lineTo (a.getX() + s * 0.28f, a.getBottom() - s * 0.28f);
            p = stroked (x, w);
            break;
        }

        case Icon::compare:
        {
            juce::Path cmp;
            for (int row = 0; row < 2; ++row)
            {
                const auto y = a.getY() + s * (0.34f + 0.32f * (float) row);
                cmp.startNewSubPath (a.getX() + s * 0.12f, y);
                for (int i = 1; i <= 8; ++i)
                    cmp.lineTo (a.getX() + s * (0.12f + 0.095f * (float) i),
                                y + s * 0.08f * std::sin ((float) i * (row == 0 ? 1.4f : 1.9f)));
            }
            p = stroked (cmp, w * 0.9f);
            break;
        }

        case Icon::rangeIn:
        case Icon::rangeOut:
        {
            juce::Path b;
            const bool in = icon == Icon::rangeIn;
            const auto x0 = in ? a.getX() + s * 0.36f : a.getRight() - s * 0.36f;
            const auto x1 = in ? a.getX() + s * 0.62f : a.getRight() - s * 0.62f;
            b.startNewSubPath (x1, a.getY() + s * 0.18f);
            b.lineTo (x0, a.getY() + s * 0.18f);
            b.lineTo (x0, a.getBottom() - s * 0.18f);
            b.lineTo (x1, a.getBottom() - s * 0.18f);
            p = stroked (b, w);
            break;
        }

        case Icon::lock:
        {
            p.addRoundedRectangle (a.getX() + s * 0.2f, c.y - s * 0.04f, s * 0.6f, s * 0.42f, s * 0.06f);
            juce::Path sh;
            sh.addCentredArc (c.x, c.y - s * 0.06f, s * 0.18f, s * 0.2f, 0.0f,
                              -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi, true);
            p.addPath (stroked (sh, w));
            break;
        }
    }

    return p;
}

//==============================================================================
ChipButton::ChipButton (const juce::String& label, juce::Colour on)
    : juce::Button (label), onColour (on)
{
    setClickingTogglesState (true);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

int ChipButton::idealWidth() const
{
    const auto tw = textWidth (font (fontSize, FontWeight::bold), getButtonText());
    return (int) std::ceil (tw) + 20 + (hasIcon ? (int) fontSize + 6 : 0);
}

void ChipButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    const bool on = getToggleState();

    auto bg = on ? onColour.withAlpha (0.14f) : (subtle ? juce::Colours::transparentBlack : colours::panelHi);
    if (over) bg = on ? onColour.withAlpha (0.22f) : colours::panelHi.brighter (0.12f);
    if (down) bg = bg.darker (0.2f);

    g.setColour (bg);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (on ? onColour.withAlpha (0.55f) : colours::border);
    g.drawRoundedRectangle (r, 5.0f, 1.0f);

    const auto fg = on ? onColour : (over ? colours::text : (subtle ? colours::textDim : colours::text.withAlpha (0.85f)));
    auto content = getLocalBounds().reduced (juce::jmin (10, getWidth() / 8), 0);

    if (hasIcon)
    {
        const auto iconArea = content.removeFromLeft ((int) fontSize).toFloat().withSizeKeepingCentre (fontSize, fontSize);
        g.setColour (fg);
        g.fillPath (makeIcon (icon, iconArea));
        content.removeFromLeft (6);
    }

    g.setColour (fg);
    g.setFont (font (fontSize, FontWeight::bold));
    g.drawText (getButtonText(), content, hasIcon ? juce::Justification::centredLeft : juce::Justification::centred, false);
}

//==============================================================================
IconButton::IconButton (const juce::String& name, Icon i, juce::Colour on)
    : juce::Button (name), icon (i), onColour (on)
{
    setTooltip (name);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void IconButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    const bool on = getToggleState();

    auto bg = colours::panelHi;
    if (filled && on) bg = onColour;
    else if (on)      bg = onColour.withAlpha (0.16f);
    if (over)         bg = bg.brighter (0.1f);
    if (down)         bg = bg.darker (0.2f);

    const auto bc = on ? onColour.withAlpha (0.6f) : colours::border;

    if (round)
    {
        g.setColour (bg);
        g.fillEllipse (r);
        g.setColour (bc);
        g.drawEllipse (r, 1.0f);
    }
    else
    {
        g.setColour (bg);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (bc);
        g.drawRoundedRectangle (r, 6.0f, 1.0f);
    }

    auto fg = hasIconColour ? iconColour : (on ? onColour : colours::text);
    if (filled && on) fg = colours::bg0;

    const auto s = juce::jmin (r.getWidth(), r.getHeight()) * 0.5f;
    g.setColour (fg);
    g.fillPath (makeIcon (icon, r.withSizeKeepingCentre (s, s)));
}

//==============================================================================
SegmentedControl::SegmentedControl (juce::StringArray opts, int sel, juce::Colour on)
    : options (std::move (opts)), selected (sel), onColour (on)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void SegmentedControl::setSelected (int index)
{
    if (index == selected || ! juce::isPositiveAndBelow (index, options.size()))
        return;

    selected = index;
    repaint();
    if (onChange) onChange (selected);
}

int SegmentedControl::idealWidth (float fontSize) const
{
    float w = 4.0f;
    for (auto& o : options)
        w += textWidth (font (fontSize, FontWeight::bold), o) + 22.0f;
    return (int) std::ceil (w);
}

int SegmentedControl::indexAt (juce::Point<int> p) const
{
    if (options.isEmpty() || ! getLocalBounds().contains (p))
        return -1;
    return juce::jlimit (0, options.size() - 1, p.x * options.size() / juce::jmax (1, getWidth()));
}

void SegmentedControl::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (colours::bgDeep);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (colours::border);
    g.drawRoundedRectangle (r, 6.0f, 1.0f);

    const auto segW = r.getWidth() / (float) juce::jmax (1, options.size());
    g.setFont (font (12.0f, FontWeight::bold));

    for (int i = 0; i < options.size(); ++i)
    {
        const auto seg = juce::Rectangle<float> (r.getX() + segW * (float) i, r.getY(), segW, r.getHeight()).reduced (2.0f);

        if (i == selected)
        {
            g.setColour (onColour.withAlpha (0.16f));
            g.fillRoundedRectangle (seg, 4.5f);
            g.setColour (onColour.withAlpha (0.55f));
            g.drawRoundedRectangle (seg, 4.5f, 1.0f);
        }
        else if (i == hover)
        {
            g.setColour (colours::panelHi);
            g.fillRoundedRectangle (seg, 4.5f);
        }

        g.setColour (i == selected ? onColour : (i == hover ? colours::text : colours::textDim));
        g.drawText (options[i], seg, juce::Justification::centred, false);
    }
}

void SegmentedControl::mouseMove (const juce::MouseEvent& e) { hover = indexAt (e.getPosition()); repaint(); }
void SegmentedControl::mouseExit (const juce::MouseEvent&)   { hover = -1; repaint(); }
void SegmentedControl::mouseUp (const juce::MouseEvent& e)   { setSelected (indexAt (e.getPosition())); }

//==============================================================================
LabeledKnob::LabeledKnob (const juce::String& t, double min, double max, double value, double step,
                          std::function<juce::String (double)> fmt, bool bipolar, juce::Colour colour)
    : title (t), format (std::move (fmt))
{
    knob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    knob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    knob.setRange (min, max, step);
    knob.setValue (value, juce::dontSendNotification);
    knob.setDoubleClickReturnValue (true, bipolar ? 0.0 : value);
    knob.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    knob.setColour (juce::Slider::rotarySliderFillColourId, colour);
    knob.getProperties().set ("bipolar", bipolar);
    knob.onValueChange = [this] { repaint(); };
    addAndMakeVisible (knob);
}

void LabeledKnob::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (20);
    knob.setBounds (r.removeFromLeft (knobSize()).reduced (2));
}

int LabeledKnob::knobSize() const
{
    return juce::jmin (getHeight() - 20, juce::roundToInt ((float) getWidth() * 0.45f));
}

void LabeledKnob::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    drawCardTitle (g, r.removeFromTop (20), title);

    r.removeFromLeft (knobSize() + 10);
    auto valueArea = r.removeFromTop (r.getHeight() / 2 + 6);

    g.setColour (colours::text);
    g.setFont (font (22.0f, FontWeight::bold));
    g.drawFittedText (format ? format (knob.getValue()) : juce::String (knob.getValue()), valueArea,
                      juce::Justification::bottomLeft, 1, 0.6f);

    g.setColour (colours::textDim);
    g.setFont (font (11.0f));
    g.drawFittedText (caption, r, juce::Justification::topLeft, 2, 1.0f);
}

//==============================================================================
FaderRow::FaderRow (const juce::String& l, float value, bool ms, juce::Colour colour)
    : label (l), withMuteSolo (ms)
{
    fader.setSliderStyle (juce::Slider::LinearHorizontal);
    fader.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    fader.setRange (0.0, 1.0, 0.01);
    fader.setValue (value, juce::dontSendNotification);
    fader.setColour (juce::Slider::trackColourId, colour);
    fader.onValueChange = [this] { repaint(); };
    addAndMakeVisible (fader);

    for (auto* b : { &mute, &solo })
    {
        b->setFontSize (10.0f);
        addChildComponent (b);
        b->setVisible (withMuteSolo);
    }
}

void FaderRow::resized()
{
    auto r = getLocalBounds();
    r.removeFromLeft (92);
    r.removeFromRight (44);

    if (withMuteSolo)
    {
        mute.setBounds (r.removeFromLeft (22).withSizeKeepingCentre (22, 18));
        r.removeFromLeft (3);
        solo.setBounds (r.removeFromLeft (22).withSizeKeepingCentre (22, 18));
        r.removeFromLeft (8);
    }
    else
    {
        r.removeFromLeft (55);
    }

    fader.setBounds (r);
}

void FaderRow::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    g.setColour (colours::text.withAlpha (0.9f));
    g.setFont (font (12.0f));
    g.drawText (label, r.removeFromLeft (92), juce::Justification::centredLeft, true);

    g.setColour (colours::textDim);
    g.setFont (font (12.0f, FontWeight::bold));
    g.drawText (juce::String (juce::roundToInt (fader.getValue() * 100.0)) + "%", r.removeFromRight (40),
                juce::Justification::centredRight, false);
}

//==============================================================================
void InputMeter::setLevels (float p, float r, float h, bool c)
{
    peakDb = p; rmsDb = r; holdDb = h; clipped = c;
    repaint();
}

float InputMeter::dbToX (float db, juce::Rectangle<float> r) const
{
    const auto k = juce::jlimit (0.0f, 1.0f, (db - minDb) / -minDb);
    return r.getX() + k * r.getWidth();
}

void InputMeter::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    auto ledArea = area.removeFromRight (compact ? 8.0f : 14.0f);
    area.removeFromRight (4.0f);

    auto bar = compact ? area.withSizeKeepingCentre (area.getWidth(), 6.0f)
                       : area.removeFromTop (16.0f);

    g.setColour (colours::bgDeep);
    g.fillRoundedRectangle (bar, 2.0f);

    // 目標帯
    const auto tx0 = dbToX (targetLow, bar), tx1 = dbToX (targetHigh, bar);
    g.setColour (colours::ok.withAlpha (0.13f));
    g.fillRect (juce::Rectangle<float> (tx0, bar.getY(), tx1 - tx0, bar.getHeight()));
    if (! compact)
    {
        g.setColour (colours::ok.withAlpha (0.7f));
        g.fillRect (juce::Rectangle<float> (tx0, bar.getBottom() + 1.0f, tx1 - tx0, 2.0f));
    }

    // 区間ごとの色：通常 accent / -6 以上 warn / -3 以上 bad
    auto drawLevel = [&] (float db, float alpha, juce::Rectangle<float> lane)
    {
        struct Zone { float lo, hi; juce::Colour c; };
        const Zone zones[] = { { minDb, -6.0f, colours::accent }, { -6.0f, -3.0f, colours::warn }, { -3.0f, 0.0f, colours::bad } };

        for (auto& z : zones)
        {
            if (db <= z.lo) break;
            const auto x0 = dbToX (z.lo, lane), x1 = dbToX (juce::jmin (db, z.hi), lane);
            g.setColour (z.c.withAlpha (alpha));
            g.fillRect (juce::Rectangle<float> (x0, lane.getY(), x1 - x0, lane.getHeight()));
        }
    };

    const auto inner = bar.reduced (0.0f, compact ? 0.0f : 3.0f);
    drawLevel (peakDb, 0.35f, inner);
    drawLevel (rmsDb, 0.9f, inner.reduced (0.0f, compact ? 1.0f : 2.0f));

    // ピークホールド
    g.setColour (colours::text);
    g.fillRect (juce::Rectangle<float> (dbToX (holdDb, bar) - 1.0f, bar.getY(), 2.0f, bar.getHeight()));

    // クリップ LED
    const auto led = ledArea.withSizeKeepingCentre (ledArea.getWidth(), compact ? 8.0f : 16.0f).withY (bar.getY());
    g.setColour (clipped ? colours::bad : colours::grid);
    g.fillRoundedRectangle (led, 2.0f);

    if (compact)
        return;

    // 目盛
    g.setFont (font (10.0f));
    for (auto db : { -48.0f, -36.0f, -24.0f, -12.0f, -6.0f, -3.0f, 0.0f })
    {
        const auto x = dbToX (db, bar);
        const bool target = juce::approximatelyEqual (db, targetLow) || juce::approximatelyEqual (db, targetHigh);
        g.setColour (target ? colours::ok : colours::textMute);
        g.fillRect (juce::Rectangle<float> (x - 0.5f, bar.getBottom() + 3.0f, 1.0f, 3.0f));
        g.setColour (target ? colours::ok.withAlpha (0.9f) : colours::textDim);
        g.drawText (juce::String ((int) db), juce::Rectangle<float> (x - 14.0f, bar.getBottom() + 6.0f, 28.0f, 12.0f),
                    juce::Justification::centred, false);
    }
}
} // namespace vb
