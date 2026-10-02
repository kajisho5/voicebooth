#pragma once

#include "UiSession.h"
#include "LaneCommon.h"
#include "Actions.h"

namespace vb
{
/** DESIGN 4.5 波形レーン。現在トラックを大きく、他は細く。全トラック同じ高さで並べない
    クリックで移動、ドラッグで範囲、細い行のクリックでそのトラックを選択 */
class WaveLane : public juce::Component, public juce::TooltipClient, private SessionView
{
public:
    WaveLane (UiSession&, Actions&);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    juce::String getTooltip() override;

    /** モードごとの高さ（簡単は最小） */
    static int preferredHeight (project::Mode);

private:
    void onSessionChanged (juce::uint32 c) override
    {
        if (c & (change::playhead | change::range | change::view | change::tracks | change::mode | change::transport | change::takes | change::songInfo)) repaint();
    }

    struct Row { project::TrackType type; int trackIndex; juce::Rectangle<float> area; bool current; };
    void showTakeMenu();   // 右クリック：リハーサルのテイクを本番に入れる（録り間違いの救済）
    bool menuGesture = false;

    // 採用区間のバー（「テイク」の行）：クリックでその区間のテイクを選び直す（テイク比較。B18c）。ドラッグは今までどおり範囲
    Actions& actions;
    juce::Rectangle<float> compBarArea() const;
    bool overCompBar (juce::Point<float>) const;
    bool compBarPress = false;

    std::vector<Row> layoutRows() const;
    TimeMap map() const;
    juce::Rectangle<float> plot() const { return getLocalBounds().toFloat().withTrimmedLeft ((float) metrics::gutter); }

    void drawCompBar (juce::Graphics&, const TimeMap&, juce::Rectangle<float>, project::TrackType);
    void drawCompareRange (juce::Graphics&, const TimeMap&, juce::Rectangle<float>, project::TrackType);
    void drawWave (juce::Graphics&, const TimeMap&, const Row&);
    void drawRecording (juce::Graphics&, const TimeMap&, const Row&);

    lane::RangeGesture gesture;
};

juce::String trackName (project::TrackType);
} // namespace vb
