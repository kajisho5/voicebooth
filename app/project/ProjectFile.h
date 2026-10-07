#pragma once

#include "ProjectModel.h"

/*  プロジェクトファイル `.vbooth`（DESIGN 8 / B14）：UTF-8 の JSON、format_version 付き
    形は app/project/project.example.json。パスはすべてプロジェクトフォルダからの相対（"/" 区切り。Win と Mac で同じ）。
    サンプル位置は整数のまま（秒の小数にしない）。

    読み込みは寛容に：知らない項目は無視、欠けた項目は既定値。ただし新しいバージョン（format_version が大きい）や
    別の形式のファイルは読まずに理由を返す（古いバージョンで開いて上書きし、新しい情報を消さないため） */

namespace vb::project
{
constexpr const char* fileExtension = ".vbooth";
constexpr int formatVersion = 1;

/** プロジェクトに一緒に入れる、画面の状態ではない設定 */
struct ProjectExtras
{
    juce::String guidePath;       // お手本（声入りの原曲。B9）のコピー。プロジェクトフォルダ相対。空 = なし
    double guideNudgeMs = 0.0;    // お手本の位置の手直し（ms。+ で後ろへ）
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
    juce::String songHash;            // 曲の音の中身のハッシュ（同じ名前・同じ長さの別の曲を、前のプロジェクトとして開かないため。空 = 古いファイル）
    int practiceTempo = 100;          // 練習のテンポ（%）・キー（半音）。B11
    int practiceKey = 0;

    /** モニターの音量（フェーダー 0..1、0.75 = 0 dB）と M（2026-10-04。それまでは開き直すと既定に戻っていた）。
        自分の声の M は入れない（スピーカーの時のハウリング対策は開くたびに判定する）。S も入れない（一時的な聴き比べ） */
    struct Monitor
    {
        bool has = false;             // ファイルにあった（古いファイルは既定のまま）
        float backing = 0.75f, guide = 0.72f, harmony = 0.40f, self = 0.64f, reverb = 0.25f;
        bool backingMute = false, guideMute = false, harmonyMute = false;
    } monitor;

    /** 作業の続き（2026-10-04）：範囲（IN / OUT）・ループ・選んでいたトラック・自分の声を 1 オクターブ上げて重ねる・再生位置 */
    struct Work
    {
        bool has = false;
        juce::int64 rangeIn = -1, rangeOut = -1;   // -1 = 範囲なし
        bool loop = false;
        juce::String track;                        // trackKey（"main" など）。空 = 既定
        bool octaveUp = false;
        int thirdGuide = 0;                        // 3 度ガイド（0 = なし・1 = 上・2 = 下。#30）
        juce::int64 playhead = 0;
    } work;
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

/** 開いた時、Audio/Takes/ と Practice/ のうちプロジェクトが使っていない WAV を片付ける。消さずに Audio/Recovered/ へ移す。
    前に落ちた・終わった時に残った裏録り（.retro-*.wav。B7）と、録音中に落ちて .vbooth に入らなかったテイクが当たる。
    使っているもの（usedPaths：プロジェクトフォルダ相対）はそのまま。移した数を返す */
int recoverUnusedTakes (const juce::File& projectFolder, const juce::StringArray& usedPaths);

/** テイクの場所として使ってよいか：プロジェクトのフォルダの中を指す相対パスだけ（#17）。
    絶対パス（/…・C:…・\\server…）・ホーム（~）・".." の段・":" を含む物は使わない。
    人に渡された .vbooth に書かれた場所で、外のファイルを読み込んだり「本番に入れる」でプロジェクトへ移したりしないため。
    曲とお手本は、コピーし終える前に保存された元の場所（絶対パス）も読む（読むだけで、移したり削除したりしない） */
bool isInsideProject (const juce::String& relativePath);

/** プロジェクトの曲・お手本のファイル。path（相対か、コピーし終える前に保存された元の場所）に無ければ、
    プロジェクトの中の folder/ にある同じ名前のファイル（コピーし終えた物）。どちらにも無ければ path のまま */
juce::File findMedia (const juce::File& projectFolder, const juce::String& path, const juce::String& folder);
} // namespace vb::project
