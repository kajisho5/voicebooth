#include "VoiceBoothLookAndFeel.h"
#include "parts/Icons.h"

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

//==============================================================================
void VoiceBoothLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    const auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height);
    g.fillAll (colours::panel);
    g.setColour (colours::lineHi);
    g.drawRect (r, 1.0f);
}

juce::Font VoiceBoothLookAndFeel::getPopupMenuFont()
{
    return sans (13.0f);
}

void VoiceBoothLookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area,
                                               bool isSeparator, bool isActive, bool isHighlighted, bool isTicked,
                                               bool, const juce::String& text, const juce::String&,
                                               const juce::Drawable*, const juce::Colour*)
{
    if (isSeparator)
    {
        paint::hline (g, (float) area.getCentreY(), (float) area.getX() + 8.0f, (float) area.getRight() - 8.0f);
        return;
    }

    auto r = area.toFloat().reduced (3.0f, 1.0f);
    if (isHighlighted && isActive)
    {
        g.setColour (colours::raisedHi);
        g.fillRoundedRectangle (r, 3.0f);
    }

    r.removeFromLeft (10.0f);
    const auto ledArea = r.removeFromLeft (14.0f);
    paint::led (g, ledArea.getCentre(), 2.8f, colours::signal, isTicked);
    r.removeFromLeft (8.0f);

    g.setColour (isActive ? (isTicked ? colours::text : colours::text.withAlpha (0.85f)) : colours::textMute);
    g.setFont (sansForLanguageName (text, 13.0f, isTicked ? Weight::semibold : Weight::regular));
    g.drawText (text, r, juce::Justification::centredLeft, true);
}
} // namespace vb
