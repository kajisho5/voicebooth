#pragma once

#include "UiSession.h"
#include "LaneCommon.h"

namespace vb
{
/** DESIGN 4.5 波形レーン。現在トラックを大きく、他は細く。全トラック同じ高さで並べない
    クリックで移動、ドラッグで範囲、細い行のクリックでそのトラックを選択 */
class WaveLane : public juce::Component, private SessionView
{
public:
    explicit WaveLane (UiSession&);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    /** モードごとの高さ（簡単は最小） */
    static int preferredHeight (project::Mode);

private:
    void onSessionChanged (juce::uint32 c) override
    {
        if (c & (change::playhead | change::range | change::view | change::tracks | change::mode | change::transport | change::songInfo)) repaint();
    }

    struct Row { project::TrackType type; int trackIndex; juce::Rectangle<float> area; bool current; };

    std::vector<Row> layoutRows() const;
    TimeMap map() const;
    juce::Rectangle<float> plot() const { return getLocalBounds().toFloat().withTrimmedLeft ((float) metrics::gutter); }

    void drawCompBar (juce::Graphics&, const TimeMap&, juce::Rectangle<float>, project::TrackType);
    void drawWave (juce::Graphics&, const TimeMap&, const Row&);
    void drawRecording (juce::Graphics&, const TimeMap&, const Row&);

    lane::RangeGesture gesture;
};

juce::String trackName (project::TrackType);
} // namespace vb
