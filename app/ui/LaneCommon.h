#pragma once

#include "DummySession.h"
#include "Timeline.h"

/*  ピッチレーンと波形レーンで共有する描画（ズーム・範囲・再生ヘッドを揃える） */

namespace vb::lane
{
TimeMap makeMap (const dummy::Session&, juce::Rectangle<float> plotArea);

/** 拍線と小節線 */
void drawTimeGrid (juce::Graphics&, const dummy::Session&, const TimeMap&, juce::Rectangle<float> area);

/** ループ範囲（薄い面＋破線の境界） */
void drawRange (juce::Graphics&, const dummy::Session&, const TimeMap&, juce::Rectangle<float> area);

/** 再生ヘッド（REC 中は赤） */
void drawPlayhead (juce::Graphics&, const dummy::Session&, const TimeMap&, juce::Rectangle<float> area);

/** 小節番号ルーラー */
void drawRuler (juce::Graphics&, const dummy::Session&, const TimeMap&, juce::Rectangle<float> area);

/** 未録音の斜線 */
void drawHatch (juce::Graphics&, juce::Rectangle<float> area, juce::Colour);

juce::Colour playheadColour (const dummy::Session&);
} // namespace vb::lane
