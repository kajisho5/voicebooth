#include "Encoder.h"
#include "Icons.h"

namespace vb
{
namespace
{
    constexpr int ledCount = 25;
    constexpr float startAngle = -juce::MathConstants<float>::pi * 0.75f;
    constexpr float endAngle   =  juce::MathConstants<float>::pi * 0.75f;
}

Encoder::Encoder (double min, double max, double value, double step, bool bi, juce::Colour led)
    : bipolar (bi), ledColour (led)
{
    setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setRange (min, max, step);
    setValue (value, juce::dontSendNotification);
    setDoubleClickReturnValue (true, bipolar ? 0.0 : value);
    setRotaryParameters (startAngle + juce::MathConstants<float>::twoPi, endAngle + juce::MathConstants<float>::twoPi, true);
    setMouseDragSensitivity (220);
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    setWantsKeyboardFocus (false);
}

void Encoder::paint (juce::Graphics& g)
{
    const auto size = (float) juce::jmin (getWidth(), getHeight());
    const auto c = getLocalBounds().toFloat().getCentre();
    const auto ringR = size * 0.5f - 3.0f;
    const auto dotR = juce::jmax (1.2f, size * 0.021f);
    const bool hover = previewHover || isMouseOverOrDragging();

    // --- LED リング ---
    const auto pos = (float) valueToProportionOfLength (getValue());
    const auto from = bipolar ? 0.5f : 0.0f;
    const auto lo = juce::jmin (from, pos), hi = juce::jmax (from, pos);

    for (int i = 0; i < ledCount; ++i)
    {
        const auto k = (float) i / (float) (ledCount - 1);
        const auto ang = startAngle + k * (endAngle - startAngle);
        const auto pt = c.getPointOnCircumference (ringR, ang);
        const auto halfStep = 0.5f / (float) (ledCount - 1);
        const bool lit = (k >= lo - halfStep && k <= hi + halfStep) && (hi - lo > 0.001f || std::abs (k - from) < halfStep);

        if (lit)
        {
            g.setColour (ledColour.withAlpha (0.16f));
            g.fillEllipse (juce::Rectangle<float> (dotR * 3.6f, dotR * 3.6f).withCentre (pt));
            g.setColour (ledColour);
        }
        else
        {
            g.setColour (colours::line);
        }
        g.fillEllipse (juce::Rectangle<float> (dotR * 2.0f, dotR * 2.0f).withCentre (pt));
    }

    // --- 本体 ---
    const auto bodyR = ringR - dotR * 2.0f - 5.0f;
    const auto body = juce::Rectangle<float> (bodyR * 2.0f, bodyR * 2.0f).withCentre (c);

    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillEllipse (body.translated (0.0f, 2.0f).expanded (1.0f));

    const auto top = hover ? colours::raisedHi.brighter (0.05f) : colours::raisedHi;
    g.setGradientFill (juce::ColourGradient (top, c.x, body.getY(), colours::raised.darker (0.35f), c.x, body.getBottom(), false));
    g.fillEllipse (body);

    // ローレット（外周の細かい刻み）
    g.setColour (juce::Colours::black.withAlpha (0.28f));
    for (int i = 0; i < 36; ++i)
    {
        const auto a = juce::MathConstants<float>::twoPi * (float) i / 36.0f;
        g.drawLine ({ c.getPointOnCircumference (bodyR - 2.5f, a), c.getPointOnCircumference (bodyR - 0.5f, a) }, 0.8f);
    }

    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawEllipse (body, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.drawEllipse (body.reduced (3.5f), 1.0f);

    // 指標
    const auto ang = startAngle + pos * (endAngle - startAngle);
    g.setColour (colours::text);
    g.drawLine ({ c.getPointOnCircumference (bodyR * 0.30f, ang), c.getPointOnCircumference (bodyR * 0.78f, ang) }, 2.2f);
}

//==============================================================================
EncoderBlock::EncoderBlock (const juce::String& l, double min, double max, double value, double step,
                            std::function<juce::String (double)> fmt, const juce::String& u,
                            bool bipolar, juce::Colour led)
    : label (l), unit (u), format (std::move (fmt)), enc (min, max, value, step, bipolar, led)
{
    enc.onValueChange = [this] { repaint(); if (onChange) onChange (enc.getValue()); };
    addAndMakeVisible (enc);
}

void EncoderBlock::resized()
{
    auto r = getLocalBounds();
    labelArea = r.removeFromTop (18);
    captionArea = r.removeFromBottom (16);
    valueArea = r.removeFromBottom (26);
    const auto s = juce::jmin (r.getWidth(), r.getHeight());
    enc.setBounds (r.withSizeKeepingCentre (s, s));
}

void EncoderBlock::paint (juce::Graphics& g)
{
    g.setColour (colours::textDim);
    g.setFont (sans (11.5f, Weight::medium));
    g.drawText (label, labelArea, juce::Justification::centred, false);

    if (locked)
    {
        const auto w = textWidth (sans (11.5f, Weight::medium), label);
        drawIcon (g, Icon::lock, juce::Rectangle<float> (11.0f, 11.0f).withCentre ({ (float) labelArea.getCentreX() + w * 0.5f + 10.0f,
                                                                                     (float) labelArea.getCentreY() }), colours::warn);
    }

    // 値（Mono）＋単位
    const auto value = format ? format (enc.getValue()) : juce::String (enc.getValue());
    const auto vf = mono (19.0f, Weight::semibold);
    const auto uf = mono (11.0f, Weight::medium);
    const auto vw = textWidth (vf, value), uw = unit.isEmpty() ? 0.0f : textWidth (uf, unit) + 3.0f;
    auto r = valueArea.toFloat().withSizeKeepingCentre (vw + uw, (float) valueArea.getHeight());

    g.setColour (colours::text);
    g.setFont (vf);
    g.drawText (value, r.removeFromLeft (vw), juce::Justification::centredLeft, false);
    if (unit.isNotEmpty())
    {
        g.setColour (colours::textDim);
        g.setFont (uf);
        g.drawText (unit, r.withTrimmedLeft (3.0f).translated (0.0f, 2.0f), juce::Justification::centredLeft, false);
    }

    g.setColour (colours::textMute);
    g.setFont (sans (10.5f));
    g.drawText (caption, captionArea, juce::Justification::centredTop, true);
}
} // namespace vb
