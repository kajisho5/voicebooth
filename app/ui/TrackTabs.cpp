#include "TrackTabs.h"

namespace vb
{
TrackTab::TrackTab (const dummy::Session& s, const dummy::TrackUi& t, bool sel)
    : session (s), track (t), selected (sel)
{
    arm.setRound (true);
    arm.setFilled (true);
    arm.setClickingTogglesState (true);
    arm.setToggleState (t.armed, juce::dontSendNotification);
    arm.setIconColour (t.armed ? colours::text : colours::rec.withAlpha (0.6f));
    arm.onClick = [this] { arm.setIconColour (arm.getToggleState() ? colours::text : colours::rec.withAlpha (0.6f)); };

    mute.setToggleState (t.mute, juce::dontSendNotification);
    solo.setToggleState (t.solo, juce::dontSendNotification);
    for (auto* b : { &mute, &solo })
        b->setFontSize (10.0f);

    addAndMakeVisible (arm);
    addAndMakeVisible (mute);
    addAndMakeVisible (solo);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void TrackTab::resized()
{
    auto r = getLocalBounds().reduced (10, 0);
    arm.setBounds (r.removeFromLeft (26).withSizeKeepingCentre (26, 26));
    r.removeFromLeft (10);

    auto right = r.removeFromRight (50);
    solo.setBounds (right.removeFromRight (22).withSizeKeepingCentre (22, 20));
    right.removeFromRight (4);
    mute.setBounds (right.removeFromRight (22).withSizeKeepingCentre (22, 20));
    r.removeFromRight (8);

    gainArea = r.removeFromBottom (14).withTrimmedBottom (6);
    textArea = r;
}

void TrackTab::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    const bool over = isMouseOver (true);

    g.setColour (selected ? colours::panelHi : (over ? colours::panelHi.withAlpha (0.6f) : colours::panel));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (selected ? colours::accent.withAlpha (0.65f) : colours::border);
    g.drawRoundedRectangle (r, 6.0f, selected ? 1.5f : 1.0f);

    // 名前 + ホットキー
    auto t = textArea;
    auto top = t.removeFromTop (t.getHeight() / 2 + 3);
    g.setColour (selected ? colours::text : colours::text.withAlpha (0.85f));
    g.setFont (font (14.0f, FontWeight::bold));
    const auto nameW = (int) textWidth (font (14.0f, FontWeight::bold), track.name) + 2;
    g.drawText (track.name, top.removeFromLeft (nameW), juce::Justification::bottomLeft, false);

    top.removeFromLeft (6);
    const auto key = top.removeFromLeft (16).withTrimmedTop (top.getHeight() - 16).toFloat().reduced (0.5f);
    g.setColour (colours::border);
    g.drawRoundedRectangle (key, 3.0f, 1.0f);
    g.setColour (colours::textDim);
    g.setFont (font (10.0f, FontWeight::bold));
    g.drawText (juce::String (track.hotkey), key, juce::Justification::centred, false);

    // テイク情報
    const auto* tr = session.project.findTrack (track.type);
    const auto takes = tr != nullptr ? (int) tr->takes.size() : 0;
    const auto segs  = tr != nullptr ? (int) tr->comp.size() : 0;

    juce::String info;
    if (takes == 0) info = jp ("未録音");
    else            info = juce::String (takes) + jp (" テイク · 採用 ") + juce::String (segs) + jp (" 区間");

    g.setColour (takes == 0 ? colours::textMute : colours::textDim);
    g.setFont (font (11.0f));
    g.drawText (info, t, juce::Justification::topLeft, true);

    // モニター音量（細いバー）
    const auto gr = gainArea.toFloat().withSizeKeepingCentre ((float) gainArea.getWidth(), 3.0f);
    g.setColour (colours::grid);
    g.fillRoundedRectangle (gr, 1.5f);
    g.setColour ((selected ? colours::accent : colours::textDim).withAlpha (0.8f));
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

        addAndMakeVisible (tabs.add (new TrackTab (s, t, (int) i == s.selectedTrack)));
    }

    compare.setLeadingIcon (Icon::compare);
    compare.setFontSize (12.0f);
    addAndMakeVisible (compare);
}

void TrackTabs::resized()
{
    auto r = getLocalBounds().reduced (metrics::pad, 8);
    r.removeFromLeft (metrics::gutter - metrics::pad);

    compare.setBounds (r.removeFromRight (compare.idealWidth()).withSizeKeepingCentre (compare.idealWidth(), 32));
    r.removeFromRight (16);

    const auto w = juce::jmin (270, (r.getWidth() - 8 * (tabs.size() - 1)) / juce::jmax (1, tabs.size()));
    for (auto* t : tabs)
    {
        t->setBounds (r.removeFromLeft (w));
        r.removeFromLeft (8);
    }
}

void TrackTabs::paint (juce::Graphics& g)
{
    g.fillAll (colours::bg0);
    g.setColour (colours::textDim);
    g.setFont (font (11.0f, FontWeight::bold));
    g.drawText (jp ("トラック"), getLocalBounds().withWidth (metrics::gutter).reduced (10, 0), juce::Justification::centredRight, false);
}
} // namespace vb
