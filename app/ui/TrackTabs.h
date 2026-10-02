#pragma once

#include "UiSession.h"
#include "parts/KeyButton.h"

namespace vb
{
/** ボーカルトラック 1 本（アーム / 名前 / テイク数 / M S / モニター量）。クリックで選択 */
class TrackCard : public juce::Component, private SessionView
{
public:
    TrackCard (UiSession&, int index);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { repaint(); }
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

    int trackIndex() const { return index; }

private:
    void onSessionChanged (juce::uint32) override;
    bool isSelected() const { return state().selectedTrack == index; }
    const dummy::TrackUi& track() const { return state().trackUi[(size_t) index]; }

    int index;
    KeyButton arm { {}, KeyButton::Kind::rec };
    KeyButton mute { "M" }, solo { "S" };
    juce::Rectangle<int> textArea, gainArea;
    bool draggingGain = false;
    juce::Rectangle<int> gainHitArea() const { return gainArea.expanded (2, 6); }
    void setGainFromX (int x);
};

/** DESIGN 4.6 トラック列。同時にアームできるのは 1 本 */
class TrackTabs : public juce::Component, private SessionView
{
public:
    explicit TrackTabs (UiSession&);

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int height = 62;

private:
    void onSessionChanged (juce::uint32) override;

    juce::OwnedArray<TrackCard> cards;
    KeyButton compare;
};
} // namespace vb
