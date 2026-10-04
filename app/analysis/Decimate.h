#pragma once

#include <juce_core/juce_core.h>
#include <vector>

/*  解析の前に SR を下げる（#21。2026-10-04）
    テンポ・キーの推定と時間合わせは、曲の SR に比例して重い（384 kHz の 20 分の曲でテンポ推定に 12 分）。
    解析に要る帯域は 20 kHz までなので、48 kHz 前後まで整数分の 1 に下げてから解析し、位置は元の SR に戻す */

namespace vb::analysis
{
/** 何分の 1 に下げるか（48 kHz 以上を保つ整数。96 kHz → 2、192 kHz → 4、384 kHz → 8。48 kHz 以下や 88.2 kHz は 1） */
int analysisFactor (double sampleRate);

/** 低域通過（窓付き sinc）してから factor ごとに 1 つ取る。factor が 1 なら写すだけ */
std::vector<float> decimate (const float* x, juce::int64 length, int factor);
} // namespace vb::analysis
