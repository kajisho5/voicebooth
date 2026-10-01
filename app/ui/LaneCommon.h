#pragma once

#include "DummySession.h"
#include "Timeline.h"

/*  ピッチレーンと波形レーンで共有する描画（ズーム・範囲・再生ヘッドを揃える） */

namespace vb::lane
{
TimeMap makeMap (const dummy::Session&, juce::Rectangle<float> plotArea);

void drawTimeGrid (juce::Graphics&, const dummy::Session&, const TimeMap&, juce::Rectangle<float> area);
void drawRange (juce::Graphics&, const dummy::Session&, const TimeMap&, juce::Rectangle<float> area);
void drawPlayhead (juce::Graphics&, const dummy::Session&, const TimeMap&, juce::Rectangle<float> area);

/** 小節番号ルーラー（ピッチレーン上端） */
void drawRuler (juce::Graphics&, const dummy::Session&, const TimeMap&, juce::Rectangle<float> area);

/** 未録音の斜線 */
void drawHatch (juce::Graphics&, juce::Rectangle<float> area, juce::Colour);
} // namespace vb::lane
