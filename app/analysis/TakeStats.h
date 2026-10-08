#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include "audio/PitchTracker.h"

/*  テイクの解析（B18。DESIGN 2「入りタイミング・テイク比較・解析」/ 18.1）
    お手本の音程（RefPitch。オフボの時間）と、録ったテイクの音程（同じ時間・10 ms ごと）を比べる。UI に依存しない（テストから使う）。

    - 音符に分ける：声のある点（確かさ 0.5 以上）の続き。3 点以上続けて 0.7 半音より離れたら次の音符。80 ms 未満は捨てる
    - 入り：お手本で休み（150 ms 以上）の後に始まる音符の頭ごとに、前後 250 ms の中でテイクが同じ音（オクターブ違いも同じ・±1.5 半音）で
      声を出し始めて 60 ms 以上その音が続いた所のうち、お手本の頭にいちばん近い所との差（ms、+ は遅い）。真ん中の値を代表にする
    - 音程：両方に声のある点で、お手本とのずれ（セント。オクターブは合わせる）が許容の中にある割合と、ずれの絶対値の平均
    - ビブラート：テイクの 500 ms 以上の音符で、150 ms の移動平均を引いた揺れの速さ（Hz）と深さ（セント）。4〜8 Hz・15 セント以上だけ */

namespace vb::analysis
{
struct NoteSpan
{
    juce::int64 start = 0, end = 0;   // 曲のサンプル
    float midi = 0.0f;                // 真ん中の値
};

/** 10 ms ごとの点 → 音符 */
std::vector<NoteSpan> segmentNotes (const std::vector<audio::PitchFrame>&, double sampleRate);

struct OnsetStats
{
    struct Item { juce::int64 guideStart = 0; double offsetMs = 0.0; };
    std::vector<Item> items;          // 合った入りだけ
    int entries = 0;                  // お手本の入りの数（範囲の中）
    double medianMs = 0.0;            // items が空なら 0
};

/** from..to（曲のサンプル）の中のお手本の入りについて */
OnsetStats onsetStats (const std::vector<audio::PitchFrame>& guide, const std::vector<audio::PitchFrame>& take,
                       double sampleRate, juce::int64 from, juce::int64 to);

struct PitchAccuracy
{
    int frames = 0;                   // 両方に声のある点
    float inBand = 0.0f;              // 0..1
    float meanAbsCents = 0.0f;
};

/** foldOctave：オクターブ違いを同じ音として数える（画面の「オクターブ合わせ」と同じにする。#25） */
PitchAccuracy pitchAccuracy (const std::vector<audio::PitchFrame>& guide, const std::vector<audio::PitchFrame>& take,
                             double sampleRate, juce::int64 from, juce::int64 to, float toleranceCents, bool foldOctave = true);

/** 苦手な所（Phase C「苦手小節ループ」）：区切り barStarts（小節線。小さい順）で take を小節に分け、続いた spanBars 小節ずつ
    （1 小節ずつずらす）の合う割合を数える（数え方は pitchAccuracy と同じ）。声のある点が minFrames 未満の所と、
    合う割合が below 以上の所は外す。合う割合の低い順（同じなら点の多い順、それも同じなら前から）に、重ならない所だけ残す */
struct WeakSpan
{
    juce::int64 start = 0, end = 0;   // 曲のサンプル（小節線）
    int firstBar = 0;                 // barStarts の添字
    int frames = 0;
    float inBand = 0.0f;
};

std::vector<WeakSpan> weakSpans (const std::vector<audio::PitchFrame>& guide, const std::vector<audio::PitchFrame>& take,
                                 double sampleRate, const std::vector<juce::int64>& barStarts, float toleranceCents,
                                 bool foldOctave = true, int spanBars = 2, int minFrames = 50, float below = 0.8f);

/** 黄（少しずれている）の上限。緑の幅（20 / 30 / 50 セント）より必ず広い：50 にしても黄が残る（#25） */
inline float pitchWarnLimitCents (float toleranceCents) { return juce::jmax (50.0f, toleranceCents + 20.0f); }

struct Vibrato
{
    juce::int64 start = 0, end = 0;
    float rateHz = 0.0f, depthCents = 0.0f;
};

std::vector<Vibrato> vibratos (const std::vector<audio::PitchFrame>& take, double sampleRate);
} // namespace vb::analysis
