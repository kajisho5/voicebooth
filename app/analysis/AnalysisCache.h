#pragma once

#include "Align.h"
#include <juce_core/juce_core.h>
#include <utility>
#include <vector>

/*  解析の結果の保存（2026-10-04）。プロジェクトを開き直すたびに、お手本の時間合わせと音程の線（RMVPE）を
    最初からやり直していた（4 分の曲で RMVPE だけで 10〜30 秒 × 3 回）のを、同じ入力なら保存した結果を使う。
    - 鍵は入力の音そのもの（サンプルの並びのハッシュ）と SR・モデル。ファイル名や日付ではないので、入力が少しでも違えば作り直す
    - 置き場所は <プロジェクト>/Cache/analysis/（DESIGN 8：Cache/ は消しても作り直せる）。setFolder で決める（空なら保存しない）
    - 読めない・壊れている・形式が古いファイルは無かったことにする（作り直して上書き）
    - どのスレッドからでも呼べる */

namespace vb::analysis::cache
{
/** サンプルの並びのハッシュ（64 bit、FNV-1a を 4 バイトずつ）。seed に前のハッシュを渡すとつなげられる */
juce::uint64 hashSamples (const float* data, juce::int64 count, juce::uint64 seed = 14695981039346656037ull);
/** 文字列・数値を混ぜる（モデルの名前・SR など） */
juce::uint64 mix (juce::uint64 h, const juce::String& text);
juce::uint64 mix (juce::uint64 h, double value);

/** 置き場所（プロジェクトを開いたとき）。juce::File() で保存をやめる */
void setFolder (const juce::File& folder);
juce::File folder();

/** RMVPE の生の出力（10 ms ごとの (cents, 確からしさ)）。分離プロセスの --pitch と同じ並び */
bool loadPitch (juce::uint64 key, std::vector<std::pair<float, float>>& frames);
void savePitch (juce::uint64 key, const std::vector<std::pair<float, float>>& frames);

/** 時間合わせ（alignReference）の結果 */
bool loadAlign (juce::uint64 key, AlignResult& result);
void saveAlign (juce::uint64 key, const AlignResult& result);

/** ファイルの中身の形（テスト用に外から呼べる） */
juce::MemoryBlock encodePitch (const std::vector<std::pair<float, float>>& frames);
bool decodePitch (const juce::MemoryBlock& data, std::vector<std::pair<float, float>>& frames);
juce::MemoryBlock encodeAlign (const AlignResult& result);
bool decodeAlign (const juce::MemoryBlock& data, AlignResult& result);
} // namespace vb::analysis::cache
