#pragma once

#include <juce_core/juce_core.h>
#include <functional>
#include <vector>
#include "Align.h"
#include "audio/PitchTracker.h"

/*  お手本の音程（DESIGN 7.1.1 / B9）：声入りの原曲とカラオケから、声だけを推定して音程を取る

    公式のオフボーカル（カラオケ）は原曲と同じミックスから声を抜いたもの。時間を合わせて（Align）原曲から
    カラオケを引けば、残りはほぼ声になる。分離（B16）を待たずに、お手本の音程が取れる。
      - 引く量（ゲイン）は 0.5 秒ごとに最小二乗で求めて、なめらかにつなぐ（マスタリングの差・フェードに追う）
      - 引けたかどうか：伴奏だけの所（前奏など）で、残りが原曲より十分小さいか（下から 10% のブロックで -15 dB 以下）。
        カラオケが別の録音・別のミックス・速さ違い・キー違いなら引けないので、お手本は出さず「分離が要る」と返す（嘘の線を出さない）
      - 音程は残り（声）に、自分の声と同じ検出（PitchAnalyzer）を掛ける。位置はカラオケ（オフボ）の時間

    入力はモノラル・同じ SR（呼び出し側でそろえる）。重いので裏のスレッドで呼ぶ */

namespace vb::analysis
{
struct RefPitchResult
{
    enum class Status
    {
        ok,
        notAligned,       // 時間が合わない（別の曲・合う所が無い）
        needsSeparation,  // 合うが引けない（別のミックス・速さ違い・キー違い）。分離（B16）でお手本を取る
        cancelled
    };

    Status status = Status::notAligned;
    std::vector<audio::PitchFrame> points;   // オフボの時間。10 ms ごと（合う区間だけ）
    float cleanDb = 0.0f;                    // 伴奏だけの所で、残り / 原曲（dB。小さいほどよく引けた）
    float voicedRatio = 0.0f;                // 声のある点の割合
};

/** reference = 原曲、karaoke = オフボ。align は alignReference の結果。progress は 0..1、false で中止。
    vocalsOut を渡すと、取り出した声をオフボの時間で入れる（長さ = karaokeLength、合わない所は 0。歌詞の認識 B17 に使う） */
RefPitchResult referencePitch (const float* reference, juce::int64 referenceLength,
                               const float* karaoke, juce::int64 karaokeLength,
                               double sampleRate, const AlignResult& align,
                               const std::function<bool (float)>& progress = {},
                               std::vector<float>* vocalsOut = nullptr);

/** 分離（B16）で取り出した声（原曲の時間、モノラル）から音程を取り、オフボの時間に置く（合う区間だけ）。
    引き算ができない組（別のミックス・EQ 違い）でも、時間が合えばお手本が取れる */
RefPitchResult pitchFromVocals (const float* vocals, juce::int64 vocalsLength, juce::int64 karaokeLength,
                                double sampleRate, const AlignResult& align,
                                const std::function<bool (float)>& progress = {},
                                std::vector<float>* vocalsOut = nullptr);
} // namespace vb::analysis
