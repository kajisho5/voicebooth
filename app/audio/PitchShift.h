#pragma once

#include <juce_core/juce_core.h>
#include <vector>

/*  音の高さだけを変える（長さは変えない。オフライン）
    キー違いのカラオケ（DESIGN 7.1.1）：分離したお手本の声は原曲のキーのままなので、カラオケのキーへずらして鳴らす。
    Rubber Band の R3（OptionEngineFiner）をオフラインで使う。重いので UI のスレッドでは呼ばない */

namespace vb::audio
{
/** モノラルを semitones 半音ずらす。長さは入力と同じ（足りなければ無音で埋め、余れば切る）。0 半音ならそのまま返す */
std::vector<float> shiftPitch (const float* mono, juce::int64 length, double sampleRate, int semitones);
} // namespace vb::audio
