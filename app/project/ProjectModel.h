#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include "song/SongInfo.h"

/*  データモデル（DESIGN 8）
    Phase A では「形」だけ。保存/読み込み（JSON）は B14 で実装する。
    サンプル位置は必ず int64。秒の float を真実にしない（DESIGN 17）。 */

namespace vb::project
{
using int64 = juce::int64;

enum class Mode      { easy, standard, pro };
enum class RecMode   { delivery, practice };   // 納品 / 練習
enum class TrackType { backing, guide, main, doubleTrack, harm1, harm2 };

struct Take
{
    juce::String id;                 // "take4"
    juce::String path;               // プロジェクトフォルダ相対
    int64 startSample = 0;           // 曲頭基準
    int64 endSample   = 0;
    juce::Time created;
    bool clip = false;
    float peak = 0.0f;               // 最大振幅（ノーマライズはしない。書き出しの表示・notes.txt 用）
    RecMode recMode = RecMode::delivery;
    int64 latencySamples = 0;        // 適用済みのレイテンシ補正
};

/** 採用区間。ユーザーには 1 本の波形として見せる */
struct CompSegment
{
    int64 startSample = 0;
    int64 endSample   = 0;
    juce::String takeId;
};

struct Track
{
    TrackType type = TrackType::main;
    std::vector<Take> takes;
    std::vector<CompSegment> comp;
};

struct Project
{
    juce::String songPath;
    int   sampleRate     = 48000;    // 時間軸の SR。既定は元曲。録音の SR を選んだらそれ（伴奏をそろえる。勝手には変えない）
    int   bitDepthExport = 24;       // 24（PCM）か 32（float）。録音形式で選ぶ。書き出しダイアログで 16（ディザー付き）にもできる
    int64 lengthSamples  = 0;
    song::KeyInfo   key;             // key_original（曲そのもののキー。練習用のキー変更とは別。DESIGN 7.5.1）
    song::TempoInfo tempo;           // BPM・拍子・1 小節目の位置（DESIGN 7.5.1）
    juce::String cacheDir;
    Mode  modeLast = Mode::standard;
    juce::String inputProfileId;
    std::vector<Track>     tracks;
    song::Sections         sections;  // 区間（DESIGN 7.5.2）
    song::Lyrics           lyrics;    // 歌詞と各行の時刻（DESIGN 7.5.3）

    const Track* findTrack (TrackType t) const
    {
        for (auto& tr : tracks)
            if (tr.type == t)
                return &tr;
        return nullptr;
    }
};

inline const char* trackKey (TrackType t)
{
    switch (t)
    {
        case TrackType::backing:     return "backing";
        case TrackType::guide:       return "guide";
        case TrackType::main:        return "main";
        case TrackType::doubleTrack: return "double";
        case TrackType::harm1:       return "harm1";
        case TrackType::harm2:       return "harm2";
    }
    return "main";
}
} // namespace vb::project
