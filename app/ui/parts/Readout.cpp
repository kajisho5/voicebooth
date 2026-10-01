#include "Readout.h"

namespace vb
{
Readout::Readout (const juce::String& l) : label (l) {}

void Readout::setValue (const juce::String& m, const juce::String& s)
{
    main = m; sub = s;
    repaint();
}

int Readout::idealWidth() const
{
    const auto w = textWidth (mono (mainSize, Weight::semibold), main)
                 + (sub.isEmpty() ? 0.0f : textWidth (mono (11.0f), sub) + 6.0f);
    return (int) std::ceil (juce::jmax (w, textWidth (mono (9.5f, Weight::medium, 0.12f), label)) + 24.0f);
}

void Readout::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    paint::inset (g, b);

    auto r = b.reduced (10.0f, 4.0f);
    paint::microLabel (g, r.removeFromTop (11.0f), label, colours::textMute);

    const auto mf = mono (mainSize, Weight::semibold);
    const auto mw = textWidth (mf, main);
    g.setColour (valueColour);
    g.setFont (mf);
    g.drawText (main, r.removeFromLeft (mw + 1.0f), juce::Justification::centredLeft, false);

    if (sub.isNotEmpty())
    {
        g.setColour (colours::textDim);
        g.setFont (mono (11.0f));
        g.drawText (sub, r.withTrimmedLeft (5.0f).translated (0.0f, 2.0f), juce::Justification::centredLeft, false);
    }
}

//==============================================================================
void TallyLamp::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat().reduced (0.5f);
    const auto f = mono (11.0f, Weight::semibold, 0.16f);

    if (state == State::rec)
    {
        g.setColour (colours::rec.withAlpha (0.22f));
        g.fillRoundedRectangle (b.expanded (3.0f), 6.0f);
        g.setGradientFill (juce::ColourGradient (colours::rec.brighter (0.2f), b.getX(), b.getY(),
                                                 colours::rec.darker (0.3f), b.getX(), b.getBottom(), false));
        g.fillRoundedRectangle (b, 4.0f);
        g.setColour (colours::text);
        g.setFont (f);
        g.drawText ("REC", b, juce::Justification::centred, false);
        return;
    }

    paint::inset (g, b, 4.0f);

    const bool play = state == State::play;
    const auto ledC = juce::Point<float> (b.getX() + 14.0f, b.getCentreY());
    paint::led (g, ledC, 3.2f, play ? colours::signal : colours::textDim, play);

    g.setColour (play ? colours::text : colours::textDim);
    g.setFont (f);
    g.drawText (play ? "PLAY" : "STANDBY", b.withTrimmedLeft (24.0f), juce::Justification::centredLeft, false);
}
} // namespace vb
