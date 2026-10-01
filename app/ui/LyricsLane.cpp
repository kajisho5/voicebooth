#include "LyricsLane.h"

namespace vb
{
LyricsLane::LyricsLane (const dummy::Session& s) : session (s)
{
    editButton.setLeadingIcon (Icon::edit);
    editButton.setClickingTogglesState (false);
    editButton.setFontSize (11.0f);
    editButton.setTooltip (jp ("歌詞を貼り付け・修正"));
    addAndMakeVisible (editButton);
}

void LyricsLane::resized()
{
    auto r = getLocalBounds();
    r.removeFromLeft (metrics::gutter);
    auto right = r.removeFromRight (130).reduced (metrics::pad, 0);
    editButton.setBounds (right.removeFromRight (editButton.idealWidth()).withSizeKeepingCentre (editButton.idealWidth(), 26));
    textArea = r;
}

void LyricsLane::paint (juce::Graphics& g)
{
    g.fillAll (colours::panel);
    g.setColour (colours::border);
    g.fillRect (getLocalBounds().removeFromBottom (1));

    g.setColour (colours::textDim);
    g.setFont (font (11.0f, FontWeight::bold));
    g.drawText (jp ("歌詞"), getLocalBounds().withWidth (metrics::gutter).reduced (10, 0), juce::Justification::centredRight, false);

    const auto* cur = session.lyricAt (session.playhead);
    const auto* next = session.lyricAfter (session.playhead);

    if (cur == nullptr && next == nullptr)
    {
        g.setColour (colours::textMute);
        g.setFont (font (13.0f));
        g.drawText (jp ("歌詞なし（歌詞パッドから貼り付けできます）"), textArea, juce::Justification::centred, false);
        return;
    }

    // 現在フレーズ（歌った分だけ accent でワイプ）
    if (cur != nullptr)
    {
        const auto f = font (24.0f, FontWeight::bold);
        const auto w = textWidth (f, cur->text);
        const auto line = textArea.toFloat().withSizeKeepingCentre (w + 4.0f, 34.0f).withX (textArea.getCentreX() - w * 0.5f - 60.0f);

        const auto progress = juce::jlimit (0.0f, 1.0f, (float) (session.playhead - cur->startSample)
                                                          / (float) (cur->endSample - cur->startSample));
        g.setFont (f);
        g.setColour (colours::text.withAlpha (0.92f));
        g.drawText (cur->text, line, juce::Justification::centredLeft, false);

        {
            juce::Graphics::ScopedSaveState save (g);
            g.reduceClipRegion (line.withWidth (line.getWidth() * progress).getSmallestIntegerContainer());
            g.setColour (colours::accent);
            g.drawText (cur->text, line, juce::Justification::centredLeft, false);
        }

        // 次のフレーズ
        if (next != nullptr)
        {
            auto r = textArea.toFloat().withLeft (line.getRight() + 28.0f);
            g.setColour (colours::textMute);
            g.setFont (font (11.0f, FontWeight::bold));
            g.drawText (jp ("次"), r.removeFromLeft (20.0f), juce::Justification::centredLeft, false);
            g.setColour (colours::textDim);
            g.setFont (font (15.0f));
            g.drawText (next->text, r, juce::Justification::centredLeft, true);
        }
    }
    else if (next != nullptr)
    {
        g.setColour (colours::textDim);
        g.setFont (font (18.0f));
        g.drawText (next->text, textArea, juce::Justification::centred, false);
    }
}
} // namespace vb
