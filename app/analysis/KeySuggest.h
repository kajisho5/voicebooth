#pragma once

#include <vector>

/*  自分の声域に合うキー（2026-10-02。DESIGN 18.1）

    「高い曲を下げると、低い所が歌えなくなる」。キーを動かすだけでなく、お手本の最高音・最低音が
    自分の声域に収まるキー（練習のキー ±6）を探す。両方は収まらなければ、はみ出しが最も小さいキーと、
    上・下に何半音はみ出すかを返す（「下げると最低音が 2 半音はみ出す」を画面に出す）。
    男声で女声の曲を歌う時のように、1 オクターブ下（上）で歌う方が合う時はそれも返す（オクターブ合わせの表示と同じ考え）。 */

namespace vb::analysis
{
/** お手本の音程の広がり（MIDI。外れ値を除くため、中央値 ±15 半音の中の 5% / 95% の所） */
struct SongRange
{
    float low = 0.0f, high = 0.0f;
    bool known = false;
};

/** midi は声のある点だけ（0 は無声として捨てる）。点が少なすぎれば known = false */
SongRange songRange (const std::vector<float>& midi);

/** 10 ms ごとの点の並びから、同じ音を 80 ms 以上伸ばしている所の点だけを残す（0 = 無声）。
    分離・引き算の残りのオクターブ違い・息・子音の一瞬の点で、最高音・最低音を言い過ぎない */
std::vector<float> sustainedNotes (const std::vector<float>& midiSequence, int minRun = 8, float maxSpread = 0.6f);

struct KeySuggestion
{
    bool ok = false;        // 声域とお手本の両方がある
    int shift = 0;          // 練習のキー（-6..+6）
    int octave = 0;         // 歌う高さ（-1 = 1 オクターブ下で歌う、+1 = 上で）
    bool fits = false;      // 最高音・最低音とも声域に収まる
    float overLow = 0.0f;   // 収まらない時、下に何半音はみ出すか
    float overHigh = 0.0f;  // 上に何半音はみ出すか
};

/** voiceLow / voiceHigh：自分の声域（MIDI）。キーは minShift..maxShift から、収まる中で元のキーにいちばん近いものを選ぶ */
KeySuggestion suggestKey (const SongRange&, int voiceLow, int voiceHigh, int minShift = -6, int maxShift = 6);

/** その曲をキー shift・オクターブ octave で歌った時の、上・下のはみ出し（半音。収まれば 0） */
void overflowAt (const SongRange&, int voiceLow, int voiceHigh, int shift, int octave, float& overLow, float& overHigh);
} // namespace vb::analysis
