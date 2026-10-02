#pragma once

#include "UiSession.h"
#include "Timeline.h"

/*  ピッチレーンと波形レーンで共有する描画（ズーム・範囲・再生ヘッドを揃える） */

namespace vb::lane
{
TimeMap makeMap (const dummy::Session&, juce::Rectangle<float> plotArea);

/** 拍線と小節線（テンポが分かっていれば 1 小節目の位置から、分からなければ秒）と区間の頭の線 */
void drawTimeGrid (juce::Graphics&, const dummy::Session&, const TimeMap&, juce::Rectangle<float> area);

/** ループ範囲（薄い面＋破線の境界） */
void drawRange (juce::Graphics&, const dummy::Session&, const TimeMap&, juce::Rectangle<float> area);

/** 再生ヘッド（REC 中は赤） */
void drawPlayhead (juce::Graphics&, const dummy::Session&, const TimeMap&, juce::Rectangle<float> area);

/** ルーラー：小節番号（テンポ未設定なら秒）と区間の札（DESIGN 7.5.2） */
void drawRuler (juce::Graphics&, const dummy::Session&, const TimeMap&, juce::Rectangle<float> area);

/** ルーラーの区間の札（描画と当たり判定で同じものを使う） */
struct SectionTag
{
    int index;
    juce::Rectangle<float> area;
};
std::vector<SectionTag> sectionTags (const dummy::Session&, const TimeMap&, juce::Rectangle<float> rulerArea);

/** 未録音の斜線 */
void drawHatch (juce::Graphics&, juce::Rectangle<float> area, juce::Colour);

juce::Colour playheadColour (const dummy::Session&);

/** レーン上のマウス操作：クリック＝その位置へ移動、ドラッグ＝範囲選択（DESIGN 6.4） */
class RangeGesture
{
public:
    void down (UiSession&, const TimeMap&, float x);
    void drag (UiSession&, const TimeMap&, float x);
    void up (UiSession&, const TimeMap&, float x);

private:
    float startX = 0.0f;
    int64 startSample = 0;
    bool dragging = false;
};
} // namespace vb::lane
