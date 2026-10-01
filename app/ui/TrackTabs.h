#pragma once

#include "DummySession.h"
#include "Widgets.h"

namespace vb
{
/** ボーカルトラック 1 本分のタブ（アーム / M / S / テイク数 / モニター量） */
class TrackTab : public juce::Component
{
public:
    TrackTab (const dummy::Session&, const dummy::TrackUi&, bool selected);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { repaint(); }

private:
    const dummy::Session& session;
    const dummy::TrackUi& track;
    bool selected;

    IconButton arm { jp ("録音対象（アーム）"), Icon::rec, colours::rec };
    ChipButton mute { "M", colours::warn }, solo { "S", colours::accent };
    juce::Rectangle<int> textArea, gainArea;
};

/** DESIGN 4.6 トラック行。同時にアームできるのは 1 本 */
class TrackTabs : public juce::Component
{
public:
    explicit TrackTabs (const dummy::Session&);

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int height = 56;

private:
    const dummy::Session& session;
    juce::OwnedArray<TrackTab> tabs;
    ChipButton compare { jp ("テイク比較") };
};
} // namespace vb
