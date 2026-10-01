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
    隣り合う同じテイクの区間はまとめる。長さ 0 のテイクは足さない */
void applyTake (Track&, const Take&);

/** 採用区間が正しい形か（重ならない・start の順・長さ > 0・テイクがある）。テストと読み込みの点検用 */
bool compIsValid (const Track&);

/** その位置で採用されているテイク（無ければ nullptr） */
const Take* takeAt (const Track&, int64 sample);
} // namespace vb::project
