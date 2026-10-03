#pragma once

#include "UiSession.h"
#include "Actions.h"
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
    TrackTabs (UiSession&, Actions&);

    void paint (juce::Graphics&) override;
    void resized() override;

    static int height() { return 62 + juce::roundToInt (10.0f * textBoostAmount()); }   // 文字を大きくしている分（1920x1080）だけ高く

private:
    void onSessionChanged (juce::uint32) override;

    Actions& actions;
    juce::OwnedArray<TrackCard> cards;
    KeyButton compare;   // テイク比較（B18c）：いまのトラックのテイクを並べて聴き比べるパネルを開く
};
} // namespace vb
