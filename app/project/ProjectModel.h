#pragma once

#include <juce_core/juce_core.h>
#include <vector>

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

enum class MarkerKind { user, chorus };

struct Marker
{
    int64 sample = 0;
    juce::String name;                     // user のみ。自動マーカーは種類から表示名を引く（翻訳）
    MarkerKind kind = MarkerKind::user;
};

struct LyricLine
{
    int64 startSample = 0;
    int64 endSample   = 0;
    juce::String text;
};

struct Project
{
    juce::String songPath;
    int   sampleRate     = 48000;    // 元曲に合わせる。勝手に変えない
    int   bitDepthExport = 24;
    int64 lengthSamples  = 0;
    int   keyOriginal    = 0;
    double tempoOriginal = 120.0;
    juce::String cacheDir;
    Mode  modeLast = Mode::standard;
    juce::String inputProfileId;
    std::vector<Track>     tracks;
    std::vector<Marker>    markers;
    std::vector<LyricLine> lyrics;

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
