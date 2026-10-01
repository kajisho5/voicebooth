#include "VoiceBoothLookAndFeel.h"

namespace vb
{
VoiceBoothLookAndFeel::VoiceBoothLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, colours::bg0);
    setColour (juce::DocumentWindow::textColourId, colours::text);
    setColour (juce::Slider::trackColourId, colours::accent);
    setColour (juce::Slider::backgroundColourId, colours::grid);
    setColour (juce::Slider::thumbColourId, colours::text);
    setColour (juce::Slider::rotarySliderFillColourId, colours::accent);
    setColour (juce::Slider::rotarySliderOutlineColourId, colours::grid);
    setColour (juce::PopupMenu::backgroundColourId, colours::panel);
    setColour (juce::PopupMenu::textColourId, colours::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::panelHi);
    setColour (juce::TooltipWindow::backgroundColourId, colours::panelHi);
    setColour (juce::TooltipWindow::textColourId, colours::text);
}

juce::Typeface::Ptr VoiceBoothLookAndFeel::getTypefaceForFont (const juce::Font& f)
{
    return typeface (f.isBold() ? FontWeight::bold : FontWeight::regular);
}

void VoiceBoothLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                              float sliderPos, float, float,
                                              juce::Slider::SliderStyle style, juce::Slider& s)
{
    if (style != juce::Slider::LinearHorizontal)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, 0, 0, style, s);
        return;
    }

    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height);
    const auto track  = bounds.withSizeKeepingCentre (bounds.getWidth(), 4.0f);
    const auto fillCol = s.findColour (juce::Slider::trackColourId);

    g.setColour (colours::grid);
    g.fillRoundedRectangle (track, 2.0f);

    g.setColour (fillCol.withAlpha (s.isEnabled() ? 0.85f : 0.3f));
    g.fillRoundedRectangle (track.withRight (sliderPos), 2.0f);

    // 0dB 相当の目印（75%）
    const auto unity = bounds.getX() + bounds.getWidth() * 0.75f;
    g.setColour (colours::textMute);
    g.fillRect (juce::Rectangle<float> (unity - 0.5f, track.getY() - 4.0f, 1.0f, track.getHeight() + 8.0f));

    auto thumb = juce::Rectangle<float> (10.0f, 16.0f).withCentre ({ sliderPos, bounds.getCentreY() });
    g.setColour (s.isMouseOverOrDragging() ? colours::text : colours::text.withAlpha (0.88f));
    g.fillRoundedRectangle (thumb, 3.0f);
    g.setColour (colours::bg0.withAlpha (0.6f));
    g.fillRect (thumb.withSizeKeepingCentre (1.0f, 8.0f));
}

void VoiceBoothLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                              float pos, float startAngle, float endAngle, juce::Slider& s)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (3.0f);
    const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto lineW  = 4.0f;
    const auto arcR   = radius - lineW * 0.5f;
    const auto fillCol = s.findColour (juce::Slider::rotarySliderFillColourId);

    juce::Path bg;
    bg.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (colours::grid);
    g.strokePath (bg, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // 双極（キー等）は中央から、それ以外は左端から塗る
    const bool bipolar = s.getProperties().getWithDefault ("bipolar", false);
    const auto from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
    const auto to   = startAngle + pos * (endAngle - startAngle);

    if (std::abs (to - from) > 0.001f)
    {
        juce::Path arc;
        arc.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, juce::jmin (from, to), juce::jmax (from, to), true);
        g.setColour (fillCol);
        g.strokePath (arc, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    const auto knobR = arcR - lineW - 2.0f;
    g.setColour (s.isMouseOverOrDragging() ? colours::panelHi.brighter (0.15f) : colours::panelHi);
    g.fillEllipse (juce::Rectangle<float> (knobR * 2.0f, knobR * 2.0f).withCentre (centre));

    const auto tip = centre.getPointOnCircumference (knobR - 2.0f, to);
    const auto mid = centre.getPointOnCircumference (knobR * 0.35f, to);
    g.setColour (colours::text);
    g.drawLine ({ mid, tip }, 2.0f);
}
} // namespace vb
