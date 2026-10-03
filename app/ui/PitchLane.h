#pragma once

#include "UiSession.h"
#include "LaneCommon.h"
#include "Actions.h"
#include "parts/KeyButton.h"
#include "analysis/TakeStats.h"

namespace vb
{
/** DESIGN 4.3 ピッチレーン（主役）
    お手本 = 音符の棒（伸ばしている音を、いちばん近い半音の行に音名つきで。オートチューン / Melodyne の画面のように）＋
    細かい音程の線（音符の中は濃く、音符の外のしゃくり・フォール・つなぎは薄く）。自分 = 状態色の線（再生ヘッドまで）
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
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMagnify (const juce::MouseEvent&, float scaleFactor) override;

private:
    void onSessionChanged (juce::uint32) override;

    TimeMap map() const;
    /** ルーラーの e の位置にある区間の札（無ければ -1） */
    int tagAt (juce::Point<float>) const;
    float yForMidi (float midi) const;
    juce::Colour colourForCents (float cents) const;
    juce::Colour colourFor (const dummy::PitchPoint&) const;

    /** 選択トラックのお手本のずらし量（ハモリはダミーで +4 半音） */
    bool harmonyGuide() const;   // ハモリのお手本を出す（本物は分離でリードと分けられた時。見本は長 3 度上の作り物）
    float mockHarmonyOffset() const;
    float refOffset() const;
    float mineOffset() const;

    void drawBackground (juce::Graphics&, const TimeMap&);
    void drawNoteGutter (juce::Graphics&);
    void drawMainGhost (juce::Graphics&, const TimeMap&);
    void drawReference (juce::Graphics&, const TimeMap&);
    /** お手本の音符（お手本の点が変わった時だけ作り直す） */
    const std::vector<analysis::NoteSpan>& refNotes() const;
    void drawMine (juce::Graphics&, const TimeMap&);
    bool drawCompareTake (juce::Graphics&, const TimeMap&);   // テイク比較で試聴中のテイクの線（B18c）。描いたら true
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
    mutable std::vector<analysis::NoteSpan> notesCache;
    mutable juce::int64 notesKey = -1;
};
} // namespace vb
