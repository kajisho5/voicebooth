#pragma once

#include <juce_core/juce_core.h>

/*  歌詞ファイルの読み込み（DESIGN 7.5.3）
    txt / lrc / 貼り付けた文字を、行・時刻・区間の見出しに分ける。画面にはまだつながない（B4b で結線）。

    - 文字コードは自動判定：UTF-8（BOM あり / なし）、UTF-16（BOM）、Shift_JIS（CP932）。
      Shift_JIS は OS の変換に頼らず同梱の表で読む（Win / Mac で同じ結果にする）
    - 1 行＝1 フレーズ。空行＝塊の区切り
    - 「【サビ】」「[Chorus]」「（Aメロ）」のように区間の名前だけの行は、歌詞ではなく見出しとして扱う。
      「(Yeah)」のような合いの手は歌詞のまま（区間の語に当たる時だけ見出しにする）
    - .lrc の [mm:ss.xx] は時刻として使う。[offset:±ms] を反映、1 行に複数の時刻があれば行を複製して時刻順に並べる
    - 同じ歌詞の塊が 2 回以上出てくる所は「サビ」の候補にする（7.5.2）
    - 読むだけ。どこにも送らない */

namespace vb::song
{
enum class TextEncoding { utf8, utf8Bom, utf16le, utf16be, shiftJis };

const char* encodingName (TextEncoding);   // "UTF-8" など（表示用ではなく記録用。画面は tr() で出す）

/** バイト列の文字コードを推定する */
TextEncoding detectEncoding (const void* data, size_t size);

/** 指定の文字コードで文字列にする（壊れたバイトは U+FFFD） */
juce::String decodeText (const void* data, size_t size, TextEncoding);

/** Shift_JIS（CP932）として正しいバイト列か */
bool isValidShiftJis (const void* data, size_t size);

struct LyricLine
{
    juce::String text;
    double timeSeconds = -1.0;   // 時刻なしは負
    int block = 0;               // 空行で区切った塊の番号（0 始まり）
    int section = -1;            // 属する見出し（sections の番号）。無ければ -1

    bool hasTime() const { return timeSeconds >= 0.0; }
};

struct LyricSection
{
    juce::String name;           // 見出しの文字（【サビ】なら「サビ」）
    int firstLine = 0;           // この見出しの次の歌詞の行
};

struct LyricsDoc
{
    juce::Array<LyricLine> lines;
    juce::Array<LyricSection> sections;
    juce::Array<int> chorusCandidateBlocks;   // 同じ中身が 2 回以上ある塊（2 行以上のもの）
    bool timed = false;                       // .lrc の時刻があった
    double offsetSeconds = 0.0;               // [offset:] の値（反映済み）
};

/** 文字列を解析する。.lrc の時刻も自動で見る */
LyricsDoc parseLyrics (const juce::String& text);

/** 1 行が区間の見出しだけなら、その名前（括弧を外したもの）を返す。違えば空 */
juce::String sectionHeadingOf (const juce::String& line);

/** 見出しの名前 → 区間の種類（"chorus" など。SongInfo.h の kind）。
    「サビ2」「[Chorus]」「Aメロ」のような決まった語だけ。当てはまらなければ空（自由入力の名前として使う） */
juce::String sectionKindOfHeading (const juce::String& heading);

/** ファイルを読む（判定した文字コードも返す） */
struct LoadedLyrics
{
    bool ok = false;
    TextEncoding encoding = TextEncoding::utf8;
    LyricsDoc doc;
};
LoadedLyrics loadLyricsFile (const juce::File&);
} // namespace vb::song
