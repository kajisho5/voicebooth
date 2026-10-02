#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include "project/ProjectModel.h"

/*  書き出し（DESIGN 6.5 / 9）。UI から直接 wav を書かない。必ずここを通す（DESIGN 17）。
    B5：1 トラックの採用区間をつないだフル尺の Dry。区間テイクの連結は B10、納品パックは B15。

    守ること（DESIGN 6.5。破るな）
      - 常に曲の sample 0 から end まで（lengthSamples）。未録音は無音
      - モノラル / 24bit PCM（既定）・32bit float・16bit PCM（TPDF ディザー。録っていない無音には掛けない）の WAV / SR はプロジェクトの時間軸（既定は元曲のまま。録音の SR を選んだらそれ。
        テイクの SR が違えば書き出さない）/ ノーマライズしない
      - 自動フェードなし。別のテイク同士の継ぎ目だけクロスフェード（既定 8 ms、等パワー）。
        窓は継ぎ目を中心に置き、片方のテイクに音が無い所（録り始めの前など）にはみ出す時は両方に音がある所へずらす
      - モニターリバーブ・ガイド・オフボを混ぜない（ここにはテイクしか入らない）。練習録音を納品に入れない */

namespace vb::exporter
{
struct ExportResult
{
    bool ok = false;
    juce::String message;         // 失敗の理由（英語の短い文。UI は翻訳した文に添える）
    juce::File file;
    juce::int64 length = 0;       // 書いたサンプル数（= 曲の長さ）
    float peak = 0.0f;            // 書いたファイルの最大振幅（ノーマライズはしない。notes.txt 用）
    bool clipped = false;         // -0.1 dBFS 以上がある
};

struct Options
{
    double crossfadeMs = 8.0;     // 継ぎ目のクロスフェード（プロ 0 / 5 / 8 / 20。DESIGN 6.4）
    std::function<bool (float progress)> progress;   // false を返したら中止
};

class ExportService
{
public:
    /** track の採用区間を destination に書く。テイクのパスはプロジェクトフォルダ相対（Take::path）。
        途中で失敗・中止したら destination は残さない */
    static ExportResult exportTrackDry (const project::Project&, project::TrackType, const juce::File& projectFolder,
                                        const juce::File& destination, const Options& = {});

    /** 書き出しと同じ計算で、track の採用区間をつないだフル尺の音（モノラル、曲の長さ）を out に作る。
        試聴（トラックの再生。B12）用。ディザー・量子化はしない（float のまま） */
    static ExportResult renderTrackDry (const project::Project&, project::TrackType, const juce::File& projectFolder,
                                        juce::AudioBuffer<float>& out, const Options& = {});

    /** 書き出しファイル名。例: Tanuki_mix_demo_vocal_dry.wav（ファイル名に使えない文字は除く） */
    static juce::String dryFileName (const juce::String& song, project::TrackType t)
    {
        const auto safe = juce::File::createLegalFileName (song);
        switch (t)
        {
            case project::TrackType::main:        return safe + "_vocal_dry.wav";
            case project::TrackType::doubleTrack: return safe + "_double_dry.wav";
            case project::TrackType::harm1:       return safe + "_harmony1_dry.wav";
            case project::TrackType::harm2:       return safe + "_harmony2_dry.wav";
            case project::TrackType::backing:
            case project::TrackType::guide:       return {};   // ボーカルファイルには混ぜない（DESIGN 6.5）
        }
        return {};
    }

    static constexpr float clipLevel = 0.98855309f;   // -0.1 dBFS
};
} // namespace vb::exporter
