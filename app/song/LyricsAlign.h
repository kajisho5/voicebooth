#pragma once

#include <juce_core/juce_core.h>
#include <vector>

/*  歌詞の行の時刻合わせ（B17。DESIGN 7.5.3 / 11.3）。読み込んだ歌詞（行）と、歌の認識結果（文字と時刻）を突き合わせて、
    行ごとの歌い出しを推定する。UI・認識エンジンに依存しない（テストから使う）。

    - 比べる前にそろえる：全角英数 → 半角・小文字、カタカナ → ひらがな、空白・記号・句読点は捨てる
    - 文字単位の大域アラインメント（編集距離。置換 1・挿入 1・削除 1）。認識が間奏に作った文字や、歌っていない行は挿入・削除で吸収する
    - 行の時刻 = その行で最初に一致した文字の時刻から、その前の一致しなかった文字の分（1 文字 0.12 秒）を戻した所。前の行より前にはしない
    - 一致した文字が少ない行（2 文字未満か、行の 25% 未満）は「分からない」とし、前後の分かった行の間を文字数で割って埋める（推定の印）
    - 漢字とかなの違い（窓 / まど）は一致しない。前後の行で補う */

namespace vb::song
{
struct RecognizedPiece
{
    juce::String text;
    double start = 0.0, end = 0.0;   // 秒
};

struct LineTiming
{
    double start = -1.0;             // 秒。-1 = 分からない（前後も無い）
    float matched = 0.0f;            // 行の文字のうち一致した割合（0..1）
    bool interpolated = false;       // 一致が少なく、前後から埋めた
};

/** 比べるためにそろえた文字列 */
juce::String normaliseForAlign (const juce::String&);

/** lines の順に LineTiming を返す（大きさは lines と同じ） */
std::vector<LineTiming> alignLyrics (const juce::StringArray& lines, const std::vector<RecognizedPiece>& recognized);
} // namespace vb::song
