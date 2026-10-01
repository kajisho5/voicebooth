#include "VoiceBoothLookAndFeel.h"

namespace vb
{
VoiceBoothLookAndFeel::VoiceBoothLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, colours::bg0);
    setColour (juce::DocumentWindow::textColourId, colours::text);
    setColour (juce::PopupMenu::backgroundColourId, colours::panel);
    setColour (juce::PopupMenu::textColourId, colours::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::raisedHi);
    setColour (juce::PopupMenu::highlightedTextColourId, colours::text);
}

juce::Typeface::Ptr VoiceBoothLookAndFeel::getTypefaceForFont (const juce::Font& f)
{
    return sansTypeface (f.isBold() ? Weight::semibold : Weight::regular);
}

juce::Rectangle<int> VoiceBoothLookAndFeel::getTooltipBounds (const juce::String& text, juce::Point<int> pos, juce::Rectangle<int> parent)
{
    const auto w = (int) textWidth (sans (12.0f), text) + 20;
    const auto h = 26;
    return juce::Rectangle<int> (pos.x > parent.getCentreX() ? pos.x - (w + 12) : pos.x + 18,
                                 pos.y > parent.getCentreY() ? pos.y - (h + 6) : pos.y + 6, w, h)
        .constrainedWithin (parent);
}

void VoiceBoothLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height)
{
    const auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height);
    g.setColour (colours::raisedHi);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (colours::lineHi);
    g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
    g.setColour (colours::text);
    g.setFont (sans (12.0f));
    g.drawText (text, r, juce::Justification::centred, false);
}
} // namespace vb
