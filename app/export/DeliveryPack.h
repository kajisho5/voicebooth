#pragma once

#include "ExportService.h"
#include <map>
#include <memory>

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
    bool zipTooLarge = false;                   // 4 GiB を超えるので zip を作らなかった（フォルダだけ。#22）
};

/** 確認用ミックス：伴奏 + ボーカルをモニターの音量で足したステレオ（refmix.wav と、共有用の動画の音。DESIGN 9 / 9.1）。
    ボーカルは書き出しと同じ計算（採用区間をつないだフル尺）。バックグラウンドのスレッドで作る */
class RefMix
{
public:
    /** o.backing・o.backingGain・o.vocalGains・o.tracks・o.crossfadeMs を使う。失敗したら false（error に英語の短い理由） */
    bool prepare (const project::Project&, const juce::File& projectFolder, const PackOptions& o, juce::String& error);

    float at (int channel, project::int64 sample) const
    {
        const auto i = (int) sample;
        float v = backing != nullptr && i >= 0 && i < backing->getNumSamples()
                      ? backing->getSample (juce::jmin (channel, backing->getNumChannels() - 1), i) * backingGain : 0.0f;
        for (auto& [b, g] : vocals)
            if (i >= 0 && i < b.getNumSamples())
                v += b.getSample (0, i) * g;
        return v;
    }

    /** [from, to) の最大振幅（両チャンネル） */
    float peak (project::int64 from, project::int64 to) const;

    /** [from, to) を scale 倍してステレオで out に（大きさは out に合わせて変える） */
    void render (project::int64 from, project::int64 to, float scale, juce::AudioBuffer<float>& out) const;

private:
    std::shared_ptr<const juce::AudioBuffer<float>> backing;
    float backingGain = 1.0f;
    std::vector<std::pair<juce::AudioBuffer<float>, float>> vocals;
};

class DeliveryPack
{
public:
    static PackResult write (const project::Project&, const juce::File& projectFolder, const PackOptions&);

    /** パックの中のファイル名（曲名を付けない。DESIGN 9） */
    static juce::String packFileName (project::TrackType);

    /** 同じ日のパックがあれば _2, _3 …（前の納品を上書きしない） */
    static juce::File nextFolder (const juce::File& projectFolder, juce::Time when);

    /** zip に入れられる大きさか。JUCE の ZipFile::Builder は大きさ・位置を 32 bit で書き Zip64 に対応しないので、
        合計が 4 GiB を超えると知らせなしに壊れた zip ができる（192 kHz・32bit float・20 分で約 5.5 GB。#22）。
        WAV は無圧縮で入れるので、中身の合計に見出しの分の余裕を足して比べる */
    static bool fitsInZip (juce::int64 totalBytes, int numFiles);

    static juce::String notesText (const project::Project&, const PackOptions&, const PackResult&);
    static juce::String takeMapText (const project::Project&, const PackOptions&);

    /** 0:48.250 の形（1 時間を超えたら 1:02:03.000） */
    static juce::String timeText (project::int64 sample, int sampleRate);
};
} // namespace vb::exporter
