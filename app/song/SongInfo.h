#pragma once

#include "LyricsImport.h"
#include "Tempo.h"
#include <vector>

/*  曲の情報（DESIGN 7.5）：テンポ・拍・キー・区間・歌詞の「形」と計算。画面に依存しない（juce_core だけ）
    値はプロジェクト（DESIGN 8）にそのまま入る形で持つ。保存（.vbooth）は B14

    - 位置はすべてオフボのサンプル（int64）。秒の小数を真実にしない（DESIGN 17）
    - 自動の結果は「推定」、手で入れた・触った値は「確定」。確定した値は解析をやり直しても上書きしない（DESIGN 7.5。B9b が守る）
    - 手入力のテンポは一定テンポ＋1 小節目の位置（途中でテンポが変わる曲の手入力は v1 ではしない）
    - 拍子 6/8 の BPM は付点四分で数える（1 小節 2 拍。タップで叩く感じ方と同じ） */

namespace vb::song
{
using int64 = juce::int64;

/** 推定（自動）/ 確定（手で入れた・触った）。保存では DESIGN 8 の source: auto | manual */
enum class Source { estimated, confirmed };

inline const char* sourceKey (Source s) { return s == Source::confirmed ? "manual" : "auto"; }

//==============================================================================
// テンポ・拍・キー（7.5.1）

struct TimeSignature
{
    int numerator = 4, denominator = 4;

    /** 1 小節の拍の数（6/8 は 2：付点四分で数える） */
    int beatsPerBar() const
    {
        if (denominator == 8 && numerator % 3 == 0 && numerator > 3)
            return numerator / 3;
        return juce::jmax (1, numerator);
    }

    juce::String toString() const { return juce::String (numerator) + "/" + juce::String (denominator); }
    bool operator== (const TimeSignature& o) const { return numerator == o.numerator && denominator == o.denominator; }
    bool operator!= (const TimeSignature& o) const { return ! operator== (o); }
};

/** 画面で選べる拍子（4/4 が既定） */
inline std::vector<TimeSignature> timeSignatures() { return { { 4, 4 }, { 3, 4 }, { 6, 8 } }; }

struct TempoInfo
{
    double bpm = 0.0;                 // 0 = まだ分からない（目盛りは秒）
    TimeSignature signature;
    int64 downbeatSample = 0;         // 1 小節目の頭
    Source source = Source::estimated;
    float confidence = 0.0f;          // 自動推定の信頼度（手入力は 1）
    std::vector<int64> beats;         // 自動の時の拍の位置（B9b。手入力では空＝一定テンポ）

    bool known() const { return bpm > 0.0; }
    bool confirmed() const { return known() && source == Source::confirmed; }

    /** 1 拍のサンプル数（丸めない） */
    double samplesPerBeat (double sampleRate) const { return known() ? sampleRate * 60.0 / bpm : 0.0; }
};

/** 小節・拍（1 始まり）。1 小節目より前は 0、-1 …（弱起） */
struct BarBeat
{
    int64 bar = 1;
    int beat = 1;
    double fraction = 0.0;            // 拍の中の位置 0..1
};

/** 1 小節目の頭から数えた拍の位置（beatIndex 0 = 1 小節目の 1 拍目。負も可） */
int64 beatSample (const TempoInfo&, int64 beatIndex, double sampleRate);

/** sample 以前で一番近い拍の番号（floor） */
int64 beatIndexAt (const TempoInfo&, int64 sample, double sampleRate);

BarBeat barBeatAt (const TempoInfo&, int64 sample, double sampleRate);

/** 小節の頭（bar 1 = downbeatSample） */
int64 barSample (const TempoInfo&, int64 bar, double sampleRate);

/** 一番近い小節線（テンポが分からなければそのまま） */
int64 snapToBar (const TempoInfo&, int64 sample, double sampleRate);

/** カウントイン・録り直しの助走の頭（2026-10-02）：at を含む小節の頭から bars 小節前。
    数え始めがいつも小節の 1 拍目になる（at が小節線ちょうどなら、ちょうど bars 小節）。
    小節線の 1/4 拍手前より後ろの at は次の小節の頭とみなす（頭ちょうどを少し外した位置で、1 小節まるごと余計に数えない）。
    テンポが分からない・bars <= 0 なら at のまま（数えない） */
int64 countInStart (const TempoInfo&, int64 at, int bars, double sampleRate);

/** 1 小節目の頭を拍単位でずらす（目盛り全体が左右に動く） */
int64 shiftDownbeat (const TempoInfo&, int beats, double sampleRate);

/** 128 → "128"、127.5 → "127.50"（小数 2 桁まで。入力欄と同じ精度） */
juce::String formatBpm (double bpm);

/** 入力欄の文字 → BPM（範囲外・読めなければ 0） */
double parseBpm (const juce::String&);

struct KeyInfo
{
    int tonic = -1;                   // 0 = C … 11 = B、-1 = 分からない
    bool minor = false;
    Source source = Source::estimated;
    float confidence = 0.0f;

    bool known() const { return tonic >= 0; }

    /** 表記（"C" "F#m" "Bb"）。音名は記号なので翻訳しない。分からなければ空 */
    juce::String shortName() const;
};

/** 主音の表記（0 = C … 11 = B。シャープ / フラットは歌の世界でよく使う方） */
const char* tonicName (int tonic);

/** 3 度ガイド（2026-10-05 持ち主の OK。#30）：曲のキーの音階の上で 3 度上・下の音（音階の音を 2 つ先へ）。
    長調は長音階、短調は自然的短音階。長 3 度か短 3 度かは音によって変わる（C の長調で C→E、D→F）。
    音階にない音（臨時記号）は、近い音階の音（同じ近さなら下）に寄せてから数える。キーが分からなければ -1 */
int diatonicThird (int midi, const KeyInfo& key, bool up);

//==============================================================================
// 区間（7.5.2）

/** 区間の種類。名前は画面の言葉で引く（tr）。custom は自由入力の名前、generic は「区間 1, 2…」 */
namespace kind
{
    constexpr const char* intro      = "intro";
    constexpr const char* verseA     = "verseA";
    constexpr const char* verseB     = "verseB";
    constexpr const char* chorus     = "chorus";
    constexpr const char* interlude  = "interlude";
    constexpr const char* verseC     = "verseC";
    constexpr const char* dropChorus = "dropChorus";   // 落ちサビ
    constexpr const char* lastChorus = "lastChorus";   // 大サビ
    constexpr const char* outro      = "outro";
    constexpr const char* custom     = "custom";
    constexpr const char* generic    = "section";
}

/** 名前の一覧に出す順（DESIGN 7.5.2：イントロ / Aメロ / Bメロ / サビ / 間奏 / Cメロ / 落ちサビ / 大サビ / アウトロ） */
const std::vector<juce::String>& presetKinds();

struct Section
{
    int64 startSample = 0;
    juce::String kind = kind::generic;
    juce::String name;                // kind == custom の時だけ（データ。翻訳しない）
    Source source = Source::confirmed;
    int heading = -1;                 // 歌詞の見出しから作った時、その見出しの番号（同じ見出しから二重に作らない）

    bool isCustom() const { return kind == kind::custom; }
};

using Sections = std::vector<Section>;

/** 同じ位置とみなす幅（これより近い頭は 1 つにまとめる） */
constexpr double sameSectionSeconds = 0.05;

/** 時刻順に入れる。同じ位置にあれば置き換える。入った位置の番号を返す */
int addSection (Sections&, Section, double sampleRate);

/** 頭を動かす（並び直す）。動いた後の番号を返す。ほかの頭と重なれば動かさない */
int moveSection (Sections&, int index, int64 newStart, double sampleRate);

void removeSection (Sections&, int index);

/** sample を含む区間（頭が sample 以前で一番近いもの）。無ければ -1 */
int sectionIndexAt (const Sections&, int64 sample);

/** 区間の終わり（次の頭、最後は曲の長さ） */
int64 sectionEnd (const Sections&, int index, int64 lengthSamples);

/** 同じ名前が 2 つ以上ある時の番号（サビ 1、サビ 2）。1 つだけなら 0。generic は常に番号 */
std::vector<int> sectionNumbers (const Sections&);

//==============================================================================
// 歌詞（7.5.3）

struct Line
{
    juce::String text;
    int64 startSample = -1;           // 歌い出し。-1 = まだ時刻なし
    int64 endSample = -1;             // 表示用（次の行の頭など。updateLineEnds が作る。保存しない）
    int block = 0;                    // 空行で区切った塊
    int heading = -1;                 // 属する見出し（headings の番号）
    Source source = Source::confirmed;   // 時刻：lrc・タップは確定。歌詞認識（B17）は推定

    bool timed() const { return startSample >= 0; }
};

struct Heading
{
    juce::String name;                // 【サビ】なら「サビ」（データ）
    int firstLine = 0;
    bool noSection = false;           // この見出しから作った区間を消した：もう区間にしない（2026-10-05）
};

struct Lyrics
{
    juce::String sourceFileName;      // 読み込んだファイル名（貼り付けは空）
    juce::String encoding;            // "UTF-8" / "Shift_JIS" など（記録用）
    std::vector<Line> lines;
    std::vector<Heading> headings;
    std::vector<int> chorusBlocks;    // 同じ中身が 2 回以上ある塊（サビの候補）
    bool sectionsFromHeadings = true; // 見出し・サビの候補を区間にする（読み込む時に選ぶ）

    bool empty() const { return lines.empty(); }
    int numTimed() const;
    bool allTimed() const { return ! empty() && numTimed() == (int) lines.size(); }
};

/** 歌詞の行が終わったとみなす最長（次の塊まで間がある時。間奏の間ずっと光らせない） */
constexpr double maxLineSeconds = 6.0;

/** 読み込んだ文書から作る（lrc の時刻はサンプルに） */
Lyrics lyricsFromDoc (const LyricsDoc&, double sampleRate, int64 lengthSamples);

/** 表示用の行の終わりを作り直す（同じ塊は次の行の頭まで、塊の終わりは maxLineSeconds まで） */
void updateLineEnds (Lyrics&, int64 lengthSamples, double sampleRate);

/** sample で歌っている行（時刻のある行だけ）。無ければ -1 */
int lineAt (const Lyrics&, int64 sample);

/** sample より後に始まる最初の行。無ければ -1 */
int nextTimedLineAfter (const Lyrics&, int64 sample);

/** タップで合わせる時に次に叩く行：時刻が無いか、playhead 以降に始まる最初の行。全部済みなら lines.size() */
int firstLineToSync (const Lyrics&, int64 playhead);

/** 行 index の歌い出しを sample に（タップ）。後ろの行で sample 以前になったものは時刻を外す（順番を崩さない）。
    次に叩く行の番号を返す */
int tapLine (Lyrics&, int index, int64 sample, int64 lengthSamples, double sampleRate);

/** 行 index の時刻を外す（1 行戻す） */
void clearLineTime (Lyrics&, int index, int64 lengthSamples, double sampleRate);

/** 歌詞パッドで直す時の文字（時刻のある行は [mm:ss.xx]、見出しは【名前】の行に戻す）。parseLyrics で読み直せる */
juce::String toLyricText (const Lyrics&, double sampleRate);

/** 時刻のある見出し・サビの候補を区間にする。見出しは確定、サビの候補は推定。
    テンポが分かっていれば（bars）頭は一番近い小節線に吸い付く（叩いた時刻のずれ・弱起を小節の頭にそろえる）。
    すでにある見出しの区間・同じ位置の区間は作らない。足した数を返す */
int applyLyricSections (Sections&, const Lyrics&, double sampleRate, const TempoInfo* bars = nullptr);
} // namespace vb::song
