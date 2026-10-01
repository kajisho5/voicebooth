#include "ColourPad.h"

namespace vb
{
namespace
{
    constexpr float hueW = 16.0f, gap = 10.0f;
}

ColourPad::ColourPad()
{
    setMouseCursor (juce::MouseCursor::CrosshairCursor);
    setWantsKeyboardFocus (false);
}

void ColourPad::setCurrentColour (juce::Colour c, juce::NotificationType n)
{
    float h = 0.0f, s = 0.0f, v = 0.0f;
    c.getHSB (h, s, v);

    // 灰色（彩度 0）・黒（明るさ 0）では色相・彩度が決まらないので、前の値を残す
    if (s > 0.0f && v > 0.0f) hue = h;
    if (v > 0.0f) sat = s;
    val = v;
    repaint();

    if (n != juce::dontSendNotification && onChange)
        onChange (getColour());
}

juce::Rectangle<float> ColourPad::padArea() const
{
    return getLocalBounds().toFloat().withTrimmedRight (hueW + gap).reduced (1.0f);
}

juce::Rectangle<float> ColourPad::hueArea() const
{
    return getLocalBounds().toFloat().removeFromRight (hueW).reduced (1.0f);
}

void ColourPad::paint (juce::Graphics& g)
{
    // 彩度×明るさ：横に白→色相、縦に透明→黒
    const auto pad = padArea();
    g.setGradientFill (juce::ColourGradient (juce::Colours::white, pad.getX(), 0.0f,
                                             juce::Colour::fromHSV (hue, 1.0f, 1.0f, 1.0f), pad.getRight(), 0.0f, false));
    g.fillRoundedRectangle (pad, 3.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack, 0.0f, pad.getY(),
                                             juce::Colours::black, 0.0f, pad.getBottom(), false));
    g.fillRoundedRectangle (pad, 3.0f);
    g.setColour (colours::lineHi);
    g.drawRoundedRectangle (pad.expanded (0.5f), 3.0f, 1.0f);

    // 色相の帯
    const auto strip = hueArea();
    juce::ColourGradient hues (juce::Colours::red, 0.0f, strip.getY(), juce::Colours::red, 0.0f, strip.getBottom(), false);
    for (int i = 1; i < 6; ++i)
        hues.addColour ((double) i / 6.0, juce::Colour::fromHSV ((float) i / 6.0f, 1.0f, 1.0f, 1.0f));
    g.setGradientFill (hues);
    g.fillRoundedRectangle (strip, 3.0f);
    g.setColour (colours::lineHi);
    g.drawRoundedRectangle (strip.expanded (0.5f), 3.0f, 1.0f);

    // 印（どの地でも見えるよう白と黒の二重）
    const auto p = juce::Point<float> (pad.getX() + sat * pad.getWidth(), pad.getBottom() - val * pad.getHeight());
    const auto ring = juce::Rectangle<float> (12.0f, 12.0f).withCentre (p);
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawEllipse (ring.expanded (1.0f), 1.0f);
    g.setColour (juce::Colours::white);
    g.drawEllipse (ring, 1.6f);

    const auto y = strip.getY() + hue * strip.getHeight();
    const auto mark = juce::Rectangle<float> (strip.getX() - 3.0f, y - 3.0f, strip.getWidth() + 6.0f, 6.0f);
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawRoundedRectangle (mark.expanded (1.0f), 2.0f, 1.0f);
    g.setColour (juce::Colours::white);
    g.drawRoundedRectangle (mark, 2.0f, 1.6f);
}

void ColourPad::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position;
    dragging = hueArea().expanded (4.0f, 0.0f).contains (p) ? Part::hue : Part::pad;
    dragTo (p);
}

void ColourPad::mouseDrag (const juce::MouseEvent& e)
{
    dragTo (e.position);
}

void ColourPad::dragTo (juce::Point<float> p)
{
    if (dragging == Part::hue)
    {
        const auto strip = hueArea();
        hue = juce::jlimit (0.0f, 0.999f, (p.y - strip.getY()) / strip.getHeight());
    }
    else
    {
        const auto pad = padArea();
        sat = juce::jlimit (0.0f, 1.0f, (p.x - pad.getX()) / pad.getWidth());
        val = juce::jlimit (0.0f, 1.0f, 1.0f - (p.y - pad.getY()) / pad.getHeight());
    }

    repaint();
    if (onChange)
        onChange (getColour());
}
} // namespace vb
