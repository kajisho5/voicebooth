#include "LyricsLane.h"

namespace vb
{
LyricsLane::LyricsLane (UiSession& u) : SessionView (u)
{
    editButton.setButtonText (tr ("lyrics.pad"));
    editButton.withIcon (Icon::edit).withFont (sans (11.5f, Weight::medium));
    editButton.setTooltip (tr ("lyrics.pad.tooltip"));
    addAndMakeVisible (editButton);
}

void LyricsLane::resized()
{
    auto r = getLocalBounds();
    r.removeFromLeft (metrics::gutter);
    editButton.setSize (10, 28);
    const auto w = editButton.idealWidth();
    auto right = r.removeFromRight (w + metrics::pad * 2).reduced (metrics::pad, 0);
    editButton.setBounds (right.withSizeKeepingCentre (w, 28));
    textArea = r.withTrimmedLeft (22);
}

void LyricsLane::paint (juce::Graphics& g)
{
    const auto& s = state();
    g.fillAll (colours::bg0);
    paint::hline (g, (float) getHeight() - 1.0f, 0.0f, (float) getWidth());
    paint::microLabel (g, getLocalBounds().withWidth (metrics::gutter).toFloat().withTrimmedLeft ((float) metrics::pad),
                       tr ("label.lyric"), colours::textMute);

    const auto* cur = s.lyricAt (s.playhead);
    const auto* next = s.lyricAfter (s.playhead);

    if (s.project.lyrics.empty())
    {
        g.setColour (colours::textMute);
        g.setFont (sans (13.0f));
        g.drawText (tr ("lyrics.empty"), textArea, juce::Justification::centredLeft, false);
        return;
    }

    auto r = textArea.toFloat();

    if (cur != nullptr)
    {
        const auto f = sansFor (cur->text, 22.0f, Weight::semibold);
        const auto w = juce::jmin (r.getWidth() * 0.62f, textWidth (f, cur->text));
        const auto line = r.removeFromLeft (w + 2.0f).withSizeKeepingCentre (w + 2.0f, 30.0f).translated (0.0f, -3.0f);

        const auto progress = juce::jlimit (0.0f, 1.0f, (float) (s.playhead - cur->startSample)
                                                          / (float) (cur->endSample - cur->startSample));
        const auto split = line.getX() + line.getWidth() * progress;
        const auto sung = s.isRecording ? colours::rec : colours::signal;

        g.setFont (f);
        g.setColour (colours::text);
        g.drawText (cur->text, line, juce::Justification::centredLeft, true);
        {
            juce::Graphics::ScopedSaveState save (g);
            g.reduceClipRegion (line.withRight (split).getSmallestIntegerContainer());
            g.setColour (sung);
            g.drawText (cur->text, line, juce::Justification::centredLeft, true);
        }

        const auto uy = line.getBottom() + 3.0f;
        paint::hline (g, uy, line.getX(), line.getRight(), colours::line);
        g.setColour (sung);
        g.fillRect (juce::Rectangle<float> (line.getX(), uy - 0.5f, split - line.getX(), 2.0f));

        r.removeFromLeft (36.0f);
    }
    else if (next != nullptr)
    {
        // フレーズの合間：次を大きめに待たせる
        g.setColour (colours::textDim);
        g.setFont (sansFor (next->text, 18.0f, Weight::medium));
        const auto w = juce::jmin (r.getWidth() * 0.62f, textWidth (sansFor (next->text, 18.0f, Weight::medium), next->text));
        g.drawText (next->text, r.removeFromLeft (w + 2.0f), juce::Justification::centredLeft, true);
        r.removeFromLeft (36.0f);
        next = s.lyricAfter (next->startSample);
    }

    if (next != nullptr)
    {
        const auto lw = textWidth (mono (9.5f, Weight::medium, 0.12f), tr ("label.next")) + 10.0f;
        paint::microLabel (g, r.removeFromLeft (lw), tr ("label.next"), colours::textMute);
        g.setColour (colours::textDim);
        g.setFont (sansFor (next->text, 15.0f));
        g.drawText (next->text, r, juce::Justification::centredLeft, true);
    }
}
} // namespace vb
