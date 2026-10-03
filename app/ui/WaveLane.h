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
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMagnify (const juce::MouseEvent&, float scaleFactor) override;
    juce::String getTooltip() override;

    /** モードごとの高さ（簡単は最小） */
    static int preferredHeight (project::Mode);

private:
    void onSessionChanged (juce::uint32 c) override;
    float headX = -1.0e9f;   // 前に描いた再生ヘッドの x（再生ヘッドだけ動いたとき、その間だけ描き直す。#26）

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

    // 再生ヘッドと録音中の帯以外（波形・斜線・採用区間・トラック名）は画像にためておき、変わったときだけ描き直す。
    // 波形は列ごとにピークを数えるので重く、再生中に毎フレーム描き直していた（#26）
    void paintStatic (juce::Graphics&);
    juce::Image staticLayer;
    bool staticDirty = true;
    struct StaticKey { int w = 0, h = 0, skin = -1; float scale = 0.0f, boost = -1.0f; } staticKey;

    lane::RangeGesture gesture;
};

juce::String trackName (project::TrackType);
} // namespace vb
