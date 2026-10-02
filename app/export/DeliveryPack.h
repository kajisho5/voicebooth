#pragma once

#include "ExportService.h"
#include <map>

/*  納品パック（DESIGN 9 / 12「ピーク確認 → 未録音警告 → zip」/ B15）
    export_YYYYMMDD/（同じ日に 2 回目なら export_YYYYMMDD_2 …。前の納品を上書きしない）に
      vocal_dry.wav / double_dry.wav / harmony1_dry.wav / harmony2_dry.wav … 録ってあるトラックだけ（exportTrackDry と同じ。6.5 を守る）
      refmix.wav    … 確認用（本人向け）。伴奏 + ボーカルをモニターの音量で足したステレオ。-1 dBFS を超える時だけ全体を下げる
                      （ボーカルのファイルはノーマライズしない。refmix は聞くためだけ）
      notes.txt     … 形式・ピーク・遅れの補正など（下の notesText）
      take_map.txt  … どこがどのテイクか（プロ）
    を書き、同じ名前の zip（export_YYYYMMDD.zip。WAV は無圧縮で格納）も作る。
    UI に依存しない（テストから使う）。裏のスレッドで呼ぶ */

namespace vb::exporter
{
struct PackOptions
{
    juce::String songName;
    std::vector<project::TrackType> tracks;     // 書き出すボーカル（録ってある物）
    int bitDepth = 24;                          // 16（ディザー）/ 24 / 32（float）
    double crossfadeMs = 8.0;

    // 確認用ミックス。backing が無ければ作らない
    std::shared_ptr<const juce::AudioBuffer<float>> backing;   // プロジェクトの SR・曲の長さ（1 か 2 ch）
    float backingGain = 1.0f;
    std::map<project::TrackType, float> vocalGains;            // 無いトラックは 1.0（0 なら混ぜない）

    bool takeMap = false;                       // take_map.txt（プロ）
    bool zip = true;
    juce::String songKey;                       // notes.txt 用（"C" / "F#m"。分からなければ空）
    double bpm = 0.0;                           // notes.txt 用（0 = 分からない）
    std::function<juce::String (project::int64)> sectionAt;   // take_map の区間名（無ければ付けない）
    std::function<bool (float)> progress;       // false で中止
};

struct PackResult
{
    bool ok = false;
    juce::String message;                       // 失敗の理由（英語の短い文）
    juce::File folder, zipFile;
    juce::StringArray files;                    // 書いたファイル名（フォルダの中）
    std::map<project::TrackType, float> peaks;  // ボーカルごとの最大振幅
    float refmixGainDb = 0.0f;                  // refmix を下げた量（0 = 下げていない）
};

class DeliveryPack
{
public:
    static PackResult write (const project::Project&, const juce::File& projectFolder, const PackOptions&);

    /** パックの中のファイル名（曲名を付けない。DESIGN 9） */
    static juce::String packFileName (project::TrackType);

    /** 同じ日のパックがあれば _2, _3 …（前の納品を上書きしない） */
    static juce::File nextFolder (const juce::File& projectFolder, juce::Time when);

    static juce::String notesText (const project::Project&, const PackOptions&, const PackResult&);
    static juce::String takeMapText (const project::Project&, const PackOptions&);

    /** 0:48.250 の形（1 時間を超えたら 1:02:03.000） */
    static juce::String timeText (project::int64 sample, int sampleRate);
};
} // namespace vb::exporter
