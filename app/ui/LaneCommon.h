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
void drawRuler (juce::Graphics&, const dummy::Session&, const TimeMap&, juce::Rectangle<float> area, bool withPlayhead = true);
/** ルーラーの上の再生ヘッドの頭（ルーラーを画像にためるとき、頭だけ毎フレーム描く。#26） */
void drawRulerPlayhead (juce::Graphics&, const dummy::Session&, const TimeMap&, juce::Rectangle<float> area);

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

/** 再生ヘッドだけが動いたとき（表示範囲はそのまま）に描き直す範囲：前と今の位置の間に、線の光・REC 中の赤い尾（左 70 px）・
    いまの音の丸とセント値（右 104 px）の分を足した縦の帯。前の位置が分からなければ空（全体を描き直す）。
    再生中にレーン全体を毎フレーム描き直していた（1920x1080 でメッセージスレッドが CPU 1 コアの約 6 割。#26） */
juce::Rectangle<int> playheadDirty (float oldX, float newX, int height);

/** ホイール：Ctrl / Cmd ＋ホイール＝マウスの位置を中心に横の拡大・縮小、ホイールだけ（縦・横）＝横に送る。
    ピッチと波形で同じ動き（DESIGN 4.3「ズームはピッチと波形で共有」・PZ） */
void wheel (UiSession&, const TimeMap&, const juce::MouseEvent&, const juce::MouseWheelDetails&);
/** トラックパッドのピンチ＝拡大・縮小 */
void magnify (UiSession&, const TimeMap&, const juce::MouseEvent&, float scaleFactor);

/** 範囲の端の近く（左右 5 px）か：0 = IN、1 = OUT、-1 = どちらでもない（範囲が無ければ -1） */
int rangeEdgeAt (const dummy::Session&, const TimeMap&, float x);

/** 吸い付き（DESIGN 4.10.1 WS）：拍（テンポが分かっていれば）・区間の頭・再生ヘッドのうち 8 px 以内で一番近い所へ。無ければそのまま */
int64 snap (const dummy::Session&, const TimeMap&, int64 sample);

/** レーン上のマウス操作：クリック＝その位置へ移動、ドラッグ＝範囲選択、範囲の端をつまむ＝その端を動かす（DESIGN 6.4・4.10.1 WS）。
    snapOn：拍・区間の頭へ吸い付く（Alt / Option を押している間は false を渡す） */
class RangeGesture
{
public:
    void down (UiSession&, const TimeMap&, float x);
    void drag (UiSession&, const TimeMap&, float x, bool snapOn = true);
    void up (UiSession&, const TimeMap&, float x);

private:
    float startX = 0.0f;
    int64 startSample = 0;
    int edge = -1;            // つまんだ端（0 = IN、1 = OUT）
    int64 fixedSample = 0;    // 端を動かす時、動かさない方の端
    bool dragging = false;
};
} // namespace vb::lane
