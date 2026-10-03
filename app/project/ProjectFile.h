#pragma once

#include "ProjectModel.h"

/*  プロジェクトファイル `.vbooth`（DESIGN 8 / B14）：UTF-8 の JSON、format_version 付き
    形は app/project/project.example.json。パスはすべてプロジェクトフォルダからの相対（"/" 区切り。Win と Mac で同じ）。
    サンプル位置は整数のまま（秒の小数にしない）。

    読み込みは寛容に：知らない項目は無視、欠けた項目は既定値。ただし新しい版（format_version が大きい）や
    別の形式のファイルは読まずに理由を返す（古い版で開いて上書きし、新しい情報を消さないため） */

namespace vb::project
{
constexpr const char* fileExtension = ".vbooth";
constexpr int formatVersion = 1;

/** プロジェクトに一緒に入れる、画面の状態ではない設定 */
struct ProjectExtras
{
    juce::String guidePath;       // お手本（声入りの原曲。B9）のコピー。プロジェクトフォルダ相対。空 = なし
    double recordRate = 0.0;      // 録音の SR（0 = 曲に合わせる）
    bool recordFloat = false;     // 32bit float で録る
    int deviceFallbackRate = 0;   // 機器が曲の SR で開けず、代わりに使った SR（0 = 使っていない）

    /** 録ったトラックのモニター（B12）：音量（フェーダー 0..1、0.75 = 0 dB）・M・S */
    struct TrackMix
    {
        TrackType type = TrackType::main;
        float gain = 0.75f;
        bool mute = false, solo = false;
    };
    std::vector<TrackMix> trackMix;   // 空 = 既定のまま（古いファイル）
    int practiceTempo = 100;          // 練習のテンポ（%）・キー（半音）。B11
    int practiceKey = 0;
};

juce::String toJson (const Project&, const ProjectExtras& = {});

struct LoadedProject
{
    bool ok = false;
    juce::String error;           // 失敗の理由（翻訳キー：project.error.*）
    Project project;
    ProjectExtras extras;
};

LoadedProject fromJson (const juce::String&);

/** 一時ファイルに書いてから置き換える（途中で落ちても前のファイルが壊れない） */
bool writeAtomically (const juce::File&, const juce::String& text);
} // namespace vb::project
