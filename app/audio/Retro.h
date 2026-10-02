#pragma once

#include <juce_core/juce_core.h>

/*  遡及録音（DESIGN 6.3 / B7）：REC を押し遅れても、歌い出し（フレーズの頭）から採る

    再生中、アームしたトラックの入力は裏で録っている（TakeRecorder にそのまま書く。メモリには 10 ms ごとのピークだけ持つ）。
    REC を押した時、その直前（0.3 秒以内）に声があれば、そこから遡って「250 ms 以上の無音」の直後をフレーズの頭とし、
    50 ms 手前（子音・息）から採用する。声が無ければ押した所から（まだ歌っていない）。
    遡るのは 6 秒まで。それより長く切れ目なく続いていれば、押した所から（フレーズの頭が分からない時に、前のテイクを大きく消さない）

    声かどうか：ピークが -50 dBFS 以上、かつ窓の中の静かな所（下から 10%）より 12 dB 以上大きい（ただし -30 dBFS 以上なら声） */

namespace vb::audio::retro
{
constexpr double frameSeconds = 0.01;      // ピークを 1 つにまとめる長さ
constexpr double recentSeconds = 0.3;      // 押す直前のこの間に声があれば遡る
constexpr double gapSeconds = 0.25;        // フレーズの切れ目とみなす無音
constexpr double handleSeconds = 0.05;     // 頭の手前に足す分
constexpr double maxBackSeconds = 6.0;     // 遡る最大
constexpr float minVoiceDb = -50.0f;
constexpr float aboveFloorDb = 12.0f;
constexpr float maxThresholdDb = -30.0f;   // 床から決めたしきい値の上限

/** 1 フレームのサンプル数 */
inline int frameLength (double sampleRate) { return juce::jmax (1, juce::roundToInt (sampleRate * frameSeconds)); }

/** peaks[0..count) のうち pressFrame で REC を押した時、採用を始めるフレーム（pressFrame 以下）。
    pressFrame が count 以上なら、録れている最後（count）を押した所とみなす */
int phraseStartFrame (const float* peaks, int count, int pressFrame);
} // namespace vb::audio::retro
