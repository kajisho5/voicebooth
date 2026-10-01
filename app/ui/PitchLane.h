#pragma once

#include "UiSession.h"
#include "LaneCommon.h"
#include "parts/KeyButton.h"

namespace vb
{
/** DESIGN 4.3 ピッチレーン（主役）
    お手本 = アイスブルーの許容帯（±ピッチ許容）＋中心線、自分 = 状態色の線（再生ヘッドまで）
    ハモリ選択中は Main を薄い白で残す。ルーラー・面のクリックで移動、ドラッグで範囲 */
class PitchLane : public juce::Component, private SessionView
{
public:
    explicit PitchLane (UiSession&);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void onSessionChanged (juce::uint32) override;

    TimeMap map() const;
    float yForMidi (float midi) const;
    juce::Colour colourForCents (float cents) const;

    /** 選択トラックのお手本のずらし量（ハモリはダミーで +4 半音） */
    float refOffset() const;
    float mineOffset() const;

    void drawBackground (juce::Graphics&, const TimeMap&);
    void drawNoteGutter (juce::Graphics&);
    void drawMainGhost (juce::Graphics&, const TimeMap&);
    void drawReference (juce::Graphics&, const TimeMap&);
    void drawMine (juce::Graphics&, const TimeMap&);
    void drawCurrent (juce::Graphics&, const TimeMap&);
    void drawFooter (juce::Graphics&);

    juce::Rectangle<int> rulerArea, gutterArea, plotArea, footerArea, legendArea, analysisArea;
    KeyButton octaveAlign, octaveUp, fullRange;
    lane::RangeGesture gesture;
    bool draggingRuler = false;
};
} // namespace vb
