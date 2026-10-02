#pragma once

#include "UiSession.h"
#include "LaneCommon.h"
#include "Actions.h"
#include "parts/KeyButton.h"

namespace vb
{
/** DESIGN 4.3 ピッチレーン（主役）
    お手本 = アイスブルーの許容帯（±ピッチ許容）＋中心線、自分 = 状態色の線（再生ヘッドまで）
    ハモリ選択中は Main を薄い白で残す。ルーラー・面のクリックで移動、ドラッグで範囲
    ルーラーの区間の札（B4b）：クリックでその頭へ、ドラッグで動かす（小節線に吸い付く。Alt / Option で吸い付かない）、
    ダブルクリックで名前、右クリックでメニュー。札のない所を右クリックで「ここから○○」「ここを 1 小節目の頭に」 */
class PitchLane : public juce::Component, private SessionView
{
public:
    PitchLane (UiSession&, Actions&);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

private:
    void onSessionChanged (juce::uint32) override;

    TimeMap map() const;
    /** ルーラーの e の位置にある区間の札（無ければ -1） */
    int tagAt (juce::Point<float>) const;
    float yForMidi (float midi) const;
    juce::Colour colourForCents (float cents) const;
    juce::Colour colourFor (const dummy::PitchPoint&) const;

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

    Actions& actions;
    juce::Rectangle<int> rulerArea, gutterArea, plotArea, footerArea, legendArea, analysisArea;
    KeyButton octaveAlign, octaveUp, fullRange;
    lane::RangeGesture gesture;
    bool draggingRuler = false;
    int draggingTag = -1;          // 掴んでいる区間の札
    bool tagMoved = false;
    float tagGrabOffset = 0.0f;
};
} // namespace vb
