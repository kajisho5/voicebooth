#pragma once

#include "ProjectModel.h"
#include <optional>

/*  テイク比較（B18c。DESIGN 2 / 3「テイク比較（メイン上のスライドパネル）」）
    範囲（IN / OUT・採用区間の 1 区間・曲全体）について、録ったテイクを並べ、1 本ずつその範囲に入れて聴き比べ、選んだ物を採用する。
    試聴は採用区間そのものを差し替えて行う（トラックの再生 B12 が書き出しと同じ計算で作り直すので、継ぎ目のクロスフェードまで本番と同じ音で聴ける）。
    そのため「やめる」で元の採用区間へそっくり戻せること、保存には試聴中の形を書かないことをここで守る。UI に依存しない（テストから使う） */

namespace vb::project
{
/** テイクが範囲 [from, to) の中で採用できる所（曲の頭より前は除く）。無ければ first >= second */
std::pair<int64, int64> usableSpan (const Take&, int64 from, int64 to);

/** 比べられるテイクか：本番のテイクと、原速・原キーで録ったリハーサルのテイク（練習の速さ・キーで録った物は曲の時間に並ばない） */
bool comparable (const Take&);

/** 比べられるテイク（comparable）で、範囲の中に音がある物。新しい順（録った時刻、同じなら番号の大きい順）。
    リハーサルのテイクを選んで決めた時は、呼ぶ側が本番のテイクに移す（UiSession::endTakeCompare。ファイルの置き場が違う） */
std::vector<const Take*> compareCandidates (const Track&, int64 from, int64 to);

/** 範囲 [from, to) のうち、そのテイクで埋められる割合（0..1。範囲の一部しか録っていないテイクは 1 未満） */
float takeCoverage (const Take&, int64 from, int64 to);

/** 範囲 [from, to) のうち、いまの採用区間でそのテイクが使われている割合（0..1） */
float compShare (const std::vector<CompSegment>&, const juce::String& takeId, int64 from, int64 to);

/** 採用区間のバーでクリックした所の範囲：採用区間の中ならその区間、外なら前後の区間の間（曲の頭・終わりまで）。
    曲の外なら first >= second */
std::pair<int64, int64> compSpanAt (const Track&, int64 sample, int64 songLength);

/** 試聴の状態。begin で今の採用区間を覚え、preview で差し替え、commit / cancel で終える */
class TakeAudition
{
public:
    /** 比べ始める（いまの採用区間を覚える）。範囲が空なら始めない */
    bool begin (const Track&, int64 from, int64 to);
    bool isActive() const { return active; }

    /** そのテイクを範囲に入れて聴く。前の試聴は捨て、覚えた採用区間から作り直す（重ねて差し替えない）。
        id が空なら元の採用区間に戻す。比べられないテイクなら何もしない（false） */
    bool preview (Track&, const juce::String& takeId);

    /** 確定：試聴中の採用区間をそのまま残す。元に戻すための採用区間を返す（何も選んでいない・選んだ物が元と同じなら nullopt＝変更なし） */
    std::optional<std::vector<CompSegment>> commit (Track&);

    /** やめる：覚えた採用区間にそっくり戻す */
    void cancel (Track&);

    /** 試聴中でも、確定している形（元の採用区間） */
    const std::vector<CompSegment>& original() const { return before; }

    /** 保存用：プロジェクトの写しの、試聴中のトラックを確定している形に戻す（試聴の途中で自動保存・終了しても、選んでいない物を書かない） */
    void restoreCommitted (Project&) const;

    const juce::String& previewing() const { return current; }
    TrackType track() const { return type; }
    int64 from() const { return rangeFrom; }
    int64 to() const { return rangeTo; }

private:
    bool active = false;
    TrackType type = TrackType::main;
    int64 rangeFrom = 0, rangeTo = 0;
    std::vector<CompSegment> before;
    juce::String current;
};
} // namespace vb::project
