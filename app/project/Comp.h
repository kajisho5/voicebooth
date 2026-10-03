#pragma once

#include "ProjectModel.h"

/*  テイクと採用区間（DESIGN 6.4 / 8。Phase B5）
    採用区間（comp）は曲頭基準の [start, end) の並び。重ならず、start の順。ユーザーには 1 本の波形に見える。
    新しいテイクを入れると、その範囲は新しいテイクに置き換わる（前のテイクは消さず、選び直しは B10）。 */

namespace vb::project
{
/** 次のテイクの名前（"take1" "take2" …。消したテイクの番号は使い回さない） */
juce::String nextTakeId (const Track&);

/** テイクを足し、その範囲 [startSample, endSample) を採用区間で置き換える。
    曲の頭より前（遅れの補正で負になった所）は採用区間に入れない。隣り合う同じテイクの区間はまとめる。
    曲の中に長さが無いテイクは採用区間に入れない（テイクとしては残す） */
void applyTake (Track&, const Take&);

/** 同じだが、採用は useFrom から（遡及録音で、裏で録っていた頭のうち REC で採る所から。B7）。テイク自体の位置は変えない */
void applyTake (Track&, const Take&, int64 useFrom);

/** 区間の録り直し（パンチイン。B10）：採用は [useFrom, useTo) だけ。前後のプリロール・余韻はファイルに残す */
void applyTake (Track&, const Take&, int64 useFrom, int64 useTo);

/** もうあるテイクを [useFrom, useTo) で採用し直す（テイクは足さない。テイク比較の選び直し。B18） */
void useTake (Track&, const Take&, int64 useFrom, int64 useTo);

/** 採用区間が正しい形か（重ならない・start の順・長さ > 0・テイクがある）。テストと読み込みの点検用 */
bool compIsValid (const Track&);

/** その位置で採用されているテイク（無ければ nullptr） */
const Take* takeAt (const Track&, int64 sample);

/** 区間 [start, end) */
struct Span
{
    int64 start = 0, end = 0;
};

/** 声のある所（お手本の声の区間）のうち、採用区間で覆われていない所（書き出し前の「未録音」の確認。DESIGN 12）。
    minLength より短い抜けは数えない（息継ぎ・継ぎ目の端）。voiced は start の順でなくてもよい */
std::vector<Span> uncoveredSpans (const Track&, std::vector<Span> voiced, int64 minLength);
} // namespace vb::project
