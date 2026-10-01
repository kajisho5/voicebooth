#pragma once

#include "DummySession.h"
#include "parts/KeyButton.h"

namespace vb
{
/** ボーカルトラック 1 本（アーム / 名前 / テイク数 / M S / モニター量） */
class TrackCard : public juce::Component
{
public:
    TrackCard (const dummy::Session&, const dummy::TrackUi&, bool selected);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { repaint(); }

private:
    const dummy::Session& session;
    const dummy::TrackUi& track;
    bool selected;

    KeyButton arm { {}, KeyButton::Kind::rec };
    KeyButton mute { "M" }, solo { "S" };
    juce::Rectangle<int> textArea, gainArea;
};

/** DESIGN 4.6 トラック列。同時にアームできるのは 1 本 */
class TrackTabs : public juce::Component
{
public:
    explicit TrackTabs (const dummy::Session&);

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int height = 62;

private:
    const dummy::Session& session;
    juce::OwnedArray<TrackCard> cards;
    KeyButton compare { jp ("テイク比較") };
};
} // namespace vb
