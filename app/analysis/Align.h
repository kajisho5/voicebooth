#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <functional>
#include <vector>

/*  お手本（声入りの原曲）とオフボ（カラオケ）の時間合わせ・キー違いの推定（DESIGN 7.1.1）
    オフラインの解析。画面にはまだつながない（B9 で結線）。オーディオスレッドでは呼ばない。

    時間の対応：原曲のサンプル位置 = オフボの位置 × tempoRatio + offsetSamples
      - 粗く：音の立ち上がりの並び（スペクトルフラックスの包絡、約 11.6 ms 刻み）の相互相関で、数秒単位のずれを探す
      - 細かく：オフボの数か所の窓で波形の相互相関（GCC-PHAT）を取り、サンプル単位に詰める
      - 窓ごとのずれが時間とともに変わるなら、速さの違う版として比率を出す
      - 区間ごとに合うかを確かめ、合う所だけ covered に入れる（カット版のカラオケ）。合わない所は「お手本なし」

    キー：曲全体の 12 音のかたより（クロマ）を 12 通りずらして比べる。
      karaoke が原曲より何半音ずれているか（-6〜+5）。

    入力はモノラル（呼び出し側で混ぜる）。2 つの SR は同じであること（違えば呼び出し側でそろえる） */

namespace vb::analysis
{
struct Covered
{
    juce::int64 karaokeStart = 0, karaokeEnd = 0;   // オフボの時間で [start, end)
    juce::int64 offsetSamples = 0;                  // この区間のずれ（カット版では区間ごとに違う）
};

struct AlignResult
{
    enum class Quality { none, rough, good };

    Quality quality = Quality::none;
    juce::int64 offsetSamples = 0;   // 原曲の位置 = オフボの位置 × tempoRatio + offsetSamples
    double tempoRatio = 1.0;
    double confidence = 0.0;         // 0..1（粗い相関の山の際立ち）
    int windowsUsed = 0, windowsAgreeing = 0;
    std::vector<Covered> covered;    // お手本が使える所（オフボの時間）

    bool found() const { return quality != Quality::none; }

    /** オフボの位置 → 原曲の位置 */
    double referencePosition (juce::int64 karaokeSample) const
    {
        return (double) karaokeSample * tempoRatio + (double) offsetSamples;
    }
};

AlignResult alignReference (const float* reference, juce::int64 referenceLength,
                            const float* karaoke, juce::int64 karaokeLength,
                            double sampleRate);

struct KeyShiftResult
{
    int semitones = 0;          // karaoke = 原曲 + semitones
    double confidence = 0.0;    // 0..1（1 位と 2 位の差）
};

KeyShiftResult estimateKeyShift (const float* reference, juce::int64 referenceLength,
                                 const float* karaoke, juce::int64 karaokeLength,
                                 double sampleRate);

/** 音の立ち上がりの包絡（テスト・表示用に公開）。hop サンプルごとに 1 つ */
std::vector<float> onsetEnvelope (const float* x, juce::int64 length, int frameSize, int hop);
} // namespace vb::analysis
