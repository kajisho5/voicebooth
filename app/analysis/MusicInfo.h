#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <vector>

/*  テンポ・拍・1 小節目・キーの自動推定（DESIGN 7.5.1 / B9b）。学習モデルは使わない（信号処理だけ）
    結果は「推定」として出し、手で直せる（確定した値は上書きしない。呼び出し側で守る）。オフラインの解析（裏のスレッドで）

    テンポ：音の立ち上がりの包絡（10 ms ごと）の自己相関で大まかな周期（50〜220 BPM、120 BPM 付近を少し優先して倍・半分を抑える）→
            曲全体で拍の位置がいちばんそろう BPM（±2%、0.005 刻み）と位相を探す（一定テンポとみなす）
    1 小節目：拍ごとの「立ち上がりの強さ」と「和音の変わり目（クロマの変化）」を 4 拍の周期で足し、いちばん大きい拍を小節の頭に。
            音が鳴り始めた所より前で一番近い小節の頭を 1 小節目にする
    キー：曲全体のクロマ（55 Hz〜2 kHz）を Krumhansl-Kessler の長調・短調の型と比べる（24 通り）。平行調の取り違えはありうる
    入力はモノラル */

namespace vb::analysis
{
struct TempoEstimate
{
    double bpm = 0.0;                  // 0 = 分からない（短すぎる・拍が無い）
    juce::int64 downbeatSample = 0;    // 1 小節目の頭
    int beatsPerBar = 4;
    float confidence = 0.0f;           // 0..1
};

struct KeyEstimate
{
    int tonic = -1;                    // 0 = C … 11 = B
    bool minor = false;
    float confidence = 0.0f;           // 0..1（1 位と 2 位の差）
};

TempoEstimate estimateTempo (const float* x, juce::int64 length, double sampleRate);
KeyEstimate estimateKey (const float* x, juce::int64 length, double sampleRate);

/** 12 音のかたより（クロマ）を frame ごとに。テスト・内部用 */
std::vector<std::array<float, 12>> chromaFrames (const float* x, juce::int64 length, double sampleRate, int& hopOut);
} // namespace vb::analysis
