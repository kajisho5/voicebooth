#pragma once

#include "DummySession.h"
#include "HeaderBar.h"
#include "TransportBar.h"
#include "PitchLane.h"
#include "LyricsLane.h"
#include "WaveLane.h"
#include "TrackTabs.h"
#include "ControlPanel.h"
#include "StatusBar.h"

namespace vb
{
/** DESIGN 4 メイン画面（練習兼録音）。骨格はモードで変えず、密度だけ変える */
class MainComponent : public juce::Component
{
public:
    MainComponent();

    const juce::String& getSongName() const { return session.songName; }

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int defaultWidth = 1440, defaultHeight = 900;

private:
    // Phase A: ダミーセッション。Phase B で実データに差し替える（DESIGN 20）
    dummy::Session session = dummy::makeSession();

    HeaderBar header { session };
    TransportBar transport { session };
    PitchLane pitch { session };
    LyricsLane lyrics { session };
    WaveLane wave { session };
    TrackTabs tracks { session };
    ControlPanel controls { session };
    StatusBar status { session };

    juce::TooltipWindow tooltips { this, 600 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
} // namespace vb
