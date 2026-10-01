#include "LyricsLane.h"

namespace vb
{
LyricsLane::LyricsLane (const dummy::Session& s) : session (s)
{
    editButton.withIcon (Icon::edit).withFont (sans (11.5f, Weight::medium));
    editButton.setTooltip (jp ("歌詞を貼り付け・修正"));
    addAndMakeVisible (editButton);
}

void LyricsLane::resized()
{
    auto r = getLocalBounds();
    r.removeFromLeft (metrics::gutter);
    auto right = r.removeFromRight (140).reduced (metrics::pad, 0);
    editButton.setSize (10, 28);
    const auto w = editButton.idealWidth();
    editButton.setBounds (right.removeFromRight (w).withSizeKeepingCentre (w, 28));
    textArea = r.withTrimmedLeft (22);
}

void LyricsLane::paint (juce::Graphics& g)
{
    g.fillAll (colours::bg0);
    paint::hline (g, (float) getHeight() - 1.0f, 0.0f, (float) getWidth());
    paint::microLabel (g, getLocalBounds().withWidth (metrics::gutter).toFloat().withTrimmedLeft ((float) metrics::pad),
                       "LYRIC", colours::textMute);

    const auto* cur = session.lyricAt (session.playhead);
    const auto* next = session.lyricAfter (session.playhead);

    if (cur == nullptr && next == nullptr)
    {
        g.setColour (colours::textMute);
        g.setFont (sans (13.0f));
        g.drawText (jp ("歌詞なし ― 歌詞パッドから貼り付けできます"), textArea, juce::Justification::centredLeft, false);
        return;
    }

    auto r = textArea.toFloat();

    if (cur != nullptr)
    {
        const auto f = sans (22.0f, Weight::semibold);
        const auto w = textWidth (f, cur->text);
        const auto line = r.removeFromLeft (w + 2.0f).withSizeKeepingCentre (w + 2.0f, 30.0f).translated (0.0f, -3.0f);

        const auto progress = juce::jlimit (0.0f, 1.0f, (float) (session.playhead - cur->startSample)
                                                          / (float) (cur->endSample - cur->startSample));
        const auto split = line.getX() + line.getWidth() * progress;

        g.setFont (f);
        g.setColour (colours::text);
        g.drawText (cur->text, line, juce::Justification::centredLeft, false);
        {
            juce::Graphics::ScopedSaveState save (g);
            g.reduceClipRegion (line.withRight (split).getSmallestIntegerContainer());
            g.setColour (colours::signal);
            g.drawText (cur->text, line, juce::Justification::centredLeft, false);
        }

        // 進み具合（下線）
        const auto uy = line.getBottom() + 3.0f;
        paint::hline (g, uy, line.getX(), line.getRight(), colours::line);
        g.setColour (colours::signal);
        g.fillRect (juce::Rectangle<float> (line.getX(), uy - 0.5f, split - line.getX(), 2.0f));

        r.removeFromLeft (36.0f);
    }

    if (next != nullptr)
    {
        paint::microLabel (g, r.removeFromLeft (38.0f), "NEXT", colours::textMute);
        g.setColour (colours::textDim);
        g.setFont (sans (15.0f));
        g.drawText (next->text, r, juce::Justification::centredLeft, true);
    }
}
} // namespace vb
