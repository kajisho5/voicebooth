#include "TrackTabs.h"

namespace vb
{
TrackCard::TrackCard (const dummy::Session& s, const dummy::TrackUi& t, bool sel)
    : session (s), track (t), selected (sel)
{
    arm.withIcon (Icon::rec);
    arm.setToggleState (t.armed, juce::dontSendNotification);
    arm.setTooltip (jp ("録音対象（アーム）"));

    mute.withLatch (colours::warn).withFont (mono (10.5f, Weight::semibold));
    solo.withLatch (colours::signal).withFont (mono (10.5f, Weight::semibold));
    mute.setToggleState (t.mute, juce::dontSendNotification);
    solo.setToggleState (t.solo, juce::dontSendNotification);

    addAndMakeVisible (arm);
    addAndMakeVisible (mute);
    addAndMakeVisible (solo);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void TrackCard::resized()
{
    auto r = getLocalBounds().reduced (10, 0).withTrimmedLeft (selected ? 3 : 0);
    arm.setBounds (r.removeFromLeft (30).withSizeKeepingCentre (30, 28));
    r.removeFromLeft (10);

    auto right = r.removeFromRight (52);
    solo.setBounds (right.removeFromRight (24).withSizeKeepingCentre (24, 22));
    right.removeFromRight (4);
    mute.setBounds (right.removeFromRight (24).withSizeKeepingCentre (24, 22));
    r.removeFromRight (8);

    gainArea = r.removeFromBottom (12).withTrimmedBottom (7);
    textArea = r.withTrimmedTop (5);
}

void TrackCard::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const bool over = isMouseOver (true);

    if (selected)
    {
        paint::keycap (g, b, { over, false, true, true });
        g.setColour (colours::signal);
        g.fillRoundedRectangle (b.reduced (1.5f).withWidth (3.0f), 1.5f);
    }
    else
    {
        g.setColour (over ? colours::raised.withAlpha (0.6f) : colours::panel);
        g.fillRoundedRectangle (b.reduced (0.5f), metrics::keyRadius);
        g.setColour (colours::line);
        g.drawRoundedRectangle (b.reduced (0.5f), metrics::keyRadius, 1.0f);
    }

    // 名前 + ホットキー
    auto t = textArea;
    auto top = t.removeFromTop (t.getHeight() / 2 + 2);
    const auto nf = sans (13.5f, Weight::semibold);
    g.setColour (selected ? colours::text : colours::text.withAlpha (0.82f));
    g.setFont (nf);
    const auto nw = (int) textWidth (nf, track.name) + 2;
    g.drawText (track.name, top.removeFromLeft (nw), juce::Justification::bottomLeft, false);

    top.removeFromLeft (7);
    const auto key = top.removeFromLeft (15).withTrimmedTop (top.getHeight() - 15).toFloat();
    paint::inset (g, key, 2.0f);
    g.setColour (colours::textDim);
    g.setFont (mono (9.5f, Weight::semibold));
    g.drawText (juce::String (track.hotkey), key, juce::Justification::centred, false);

    // テイク情報
    const auto* tr = session.project.findTrack (track.type);
    const auto takes = tr != nullptr ? (int) tr->takes.size() : 0;
    const auto segs  = tr != nullptr ? (int) tr->comp.size() : 0;

    g.setFont (sans (11.0f));
    if (takes == 0)
    {
        g.setColour (colours::textMute);
        g.drawText (jp ("未録音"), t, juce::Justification::topLeft, true);
    }
    else
    {
        g.setColour (colours::textDim);
        g.drawText (juce::String (takes) + jp (" テイク ・ 採用 ") + juce::String (segs) + jp (" 区間"), t, juce::Justification::topLeft, true);
    }

    // モニター量
    const auto gr = gainArea.toFloat().withSizeKeepingCentre ((float) gainArea.getWidth(), 3.0f);
    g.setColour (colours::bgDeep);
    g.fillRoundedRectangle (gr, 1.5f);
    g.setColour ((selected ? colours::signal : colours::textDim).withAlpha (0.85f));
    g.fillRoundedRectangle (gr.withWidth (gr.getWidth() * track.monitorGain), 1.5f);
}

//==============================================================================
TrackTabs::TrackTabs (const dummy::Session& s) : session (s)
{
    for (size_t i = 0; i < s.trackUi.size(); ++i)
    {
        const auto& t = s.trackUi[i];
        if (t.type == project::TrackType::harm2 && s.mode != project::Mode::pro)
            continue;   // 標準は Harm 1 本まで（DESIGN 2）

        addAndMakeVisible (cards.add (new TrackCard (s, t, (int) i == s.selectedTrack)));
    }

    compare.withIcon (Icon::compare).withToggle (false);
    addAndMakeVisible (compare);
}

void TrackTabs::resized()
{
    auto r = getLocalBounds().reduced (metrics::pad, 9);
    r.removeFromLeft (metrics::gutter - metrics::pad);

    compare.setSize (10, 34);
    compare.setBounds (r.removeFromRight (compare.idealWidth()).withSizeKeepingCentre (compare.idealWidth(), 34));
    r.removeFromRight (16);

    const auto w = juce::jmin (264, (r.getWidth() - 8 * (cards.size() - 1)) / juce::jmax (1, cards.size()));
    for (auto* c : cards)
    {
        c->setBounds (r.removeFromLeft (w));
        r.removeFromLeft (8);
    }
}

void TrackTabs::paint (juce::Graphics& g)
{
    g.fillAll (colours::bg0);
    paint::microLabel (g, getLocalBounds().withWidth (metrics::gutter).toFloat().withTrimmedLeft ((float) metrics::pad),
                       "TRACK", colours::textMute);
}
} // namespace vb
