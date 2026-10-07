#pragma once

#include "UiSession.h"
#include "LaneCommon.h"
#include "Actions.h"
#include "parts/KeyButton.h"
#include "analysis/TakeStats.h"

namespace vb
{
/** DESIGN 4.3 ピッチレーン（主役）
    お手本 = 音符の棒（伸ばしている音を、いちばん近い半音の行に音名つきで。音程補正ソフトの画面のように）＋
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
    void showGuideMenu (int64 at);
    void drawUncovered (juce::Graphics&, const TimeMap&);
    void drawHarmonyHint (juce::Graphics&);
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMagnify (const juce::MouseEvent&, float scaleFactor) override;

private:
    void onSessionChanged (juce::uint32) override;
    float headX = -1.0e9f;   // 前に描いた再生ヘッドの x（再生ヘッドだけ動いたとき、その間だけ描き直す。#26）
    std::pair<int, int> litKeys() const;   // いま歌っている音・いまのお手本の音（鍵盤で光らせる。無ければ -1）
    std::pair<int, int> gutterKeys { -2, -2 };

    // 再生ヘッドで変わらない物（背景・範囲・お手本の線・ルーラー・下の欄）は画像にためておき、変わったときだけ描き直す。
    // お手本の線は点を全部たどるので重く、再生中に毎フレーム組み立て直していた（#26）
    void paintStatic (juce::Graphics&);
    juce::Image staticLayer;
    bool staticDirty = true;
    struct StaticKey { int w = 0, h = 0, skin = -1; float scale = 0.0f, boost = -1.0f; } staticKey;

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
    /** メインのお手本の音符（ハモリのトラックでも。3 度ガイドの元） */
    const std::vector<analysis::NoteSpan>& mainNotes() const;
    /** 3 度ガイド（#30）：ハモリのトラックで、メインのお手本の音から曲のキーの音階で 3 度上・下を点線の音符で */
    bool thirdGuideShown() const;
    void drawThirdGuide (juce::Graphics&, const TimeMap&);
    void drawMine (juce::Graphics&, const TimeMap&);
    bool drawCompareTake (juce::Graphics&, const TimeMap&);   // テイク比較で試聴中のテイクの線（B18c）。描いたら true
    void drawCurrent (juce::Graphics&, const TimeMap&);
    void drawFooter (juce::Graphics&);

    Actions& actions;
    juce::Rectangle<int> rulerArea, gutterArea, plotArea, footerArea, legendArea, analysisArea;
    KeyButton octaveAlign, octaveUp, fullRange, listenOriginal, thirdKey;
    lane::RangeGesture gesture;
    bool draggingRuler = false;
    bool menuGesture = false;   // 右クリックのメニュー（ドラッグ・離した時は何もしない）
    int draggingTag = -1;          // 掴んでいる区間の札
    bool tagMoved = false;
    float tagGrabOffset = 0.0f;
    mutable std::vector<analysis::NoteSpan> notesCache, mainNotesCache;
    mutable juce::int64 notesKey = -1, mainNotesKey = -1;
};
} // namespace vb
