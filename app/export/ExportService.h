#pragma once

#include "project/ProjectModel.h"

/*  書き出し（DESIGN 6.5 / 9）。UI から直接 wav を書かない。必ずここを通す。
    Phase A はスタブ。実装は B5（通し Dry）/ B10（区間テイク連結）/ B15（納品パック）。

    守ること（DESIGN 6.5）
      - 常に曲の sample 0 から end まで。未録音は無音
      - モノラル / 24bit PCM WAV / SR は元曲のまま / ノーマライズしない
      - 自動フェードなし（継ぎ目 CF のみ）
      - モニターリバーブ・ガイド・オフボを混ぜない。練習録音を納品に入れない */

namespace vb::exporter
{
struct ExportResult
{
    bool ok = false;
    juce::String message;
    juce::File file;
};

class ExportService
{
public:
    ExportResult exportTrackDry (const project::Project&, project::TrackType, const juce::File& destination)
    {
        juce::ignoreUnused (destination);
        return { false, "ExportService: not implemented in Phase A", {} };
    }

    /** 書き出しファイル名。例: Demo_song_vocal_dry.wav */
    static juce::String dryFileName (const juce::String& song, project::TrackType t)
    {
        switch (t)
        {
            case project::TrackType::main:        return song + "_vocal_dry.wav";
            case project::TrackType::doubleTrack: return song + "_double_dry.wav";
            case project::TrackType::harm1:       return song + "_harmony1_dry.wav";
            case project::TrackType::harm2:       return song + "_harmony2_dry.wav";
            default:                              return {};
        }
    }
};
} // namespace vb::exporter
