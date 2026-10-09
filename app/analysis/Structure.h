#pragma once

#include <juce_core/juce_core.h>
#include <vector>

/*  曲の構成の推定（Phase C「曲構造」。DESIGN 7.5.2）。学習モデルは使わない（信号処理だけ）
    小節ごとの響き（クロマ）と音量から：
    - 境目：響きの移り変わり（自己類似行列の対角に沿った市松の核。Foote）と音量の段差が大きい小節線。4 小節より近い境目は作らない
    - サビ：「L 小節あとに同じ響きが続く」縞（ずれごと）を拾って境目で切り、同じ所どうしをまとめた組のうち、2 回以上出てきて
      いちばん大きい音の組。曲全体（鳴っている小節の真ん中の値）より 1 dB 以上大きいときだけ。
      ほかの区間は名前を付けない（「区間 1, 2…」。Aメロ・Bメロの決め付けはしない）
    テンポ（BPM と 1 小節目）が分かっているときだけ使う。入力はモノラル。結果は「推定」で、手で直せる
    2026-10-08：合成した曲では境目とサビが当たるが、手元の実際の曲 3 曲ではサビを付けられず、境目も正解と比べられていない。
    アプリにはまだつないでいない（DESIGN Phase C） */

namespace vb::analysis
{
struct StructureSection
{
    juce::int64 startSample = 0;
    bool chorus = false;
};

struct StructureEstimate
{
    std::vector<StructureSection> sections;   // 時刻順。先頭は音の鳴り始めの小節
    float chorusConfidence = 0.0f;            // サビの付けかたの確かさ 0..1（付けなければ 0）
};

StructureEstimate estimateStructure (const float* x, juce::int64 length, double sampleRate,
                                     double bpm, juce::int64 downbeatSample, int beatsPerBar = 4);
} // namespace vb::analysis
