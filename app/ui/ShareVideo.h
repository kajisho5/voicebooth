#pragma once

#include "Theme.h"
#include "analysis/TakeStats.h"
#include <juce_audio_basics/juce_audio_basics.h>

/*  共有用の動画（DESIGN 9.1）のコマを描いて MP4 にする
    形：縦 9:16（1080×1920）/ 正方形 1:1（1080×1080）/ 横 16:9（1920×1080）。30 コマ / 秒
    画：曲名・歌詞（いまの行と次の行。歌詞レーンと同じ塗り）・自分の音程の線とお手本の音符（任意）・波形の棒と時間・
        背景（スキンの色か、画像 1 枚を暗くして敷く）
    材料（Scene）はメッセージスレッドで作り、描くときはセッションを読まない（バックグラウンドで 1 コマずつ描く） */

namespace vb::share
{
enum class Shape { portrait, square, landscape };

/** コマの大きさ（px） */
juce::Point<int> frameSize (Shape);

struct Scene
{
    Shape shape = Shape::portrait;
    double sampleRate = 48000.0;            // プロジェクトの時間軸
    int64 from = 0, to = 0;                 // 動画にする区間 [from, to)
    juce::String title;                     // 曲名（データ）

    struct Lyric { int64 start = 0, end = 0; juce::String text; };
    std::vector<Lyric> lyrics;              // 時刻のある行（時刻順。end は表示の終わり）

    std::vector<analysis::NoteSpan> guide;  // お手本の音符（原キー）
    struct Voice { int64 sample = 0; float midi = 0.0f; };   // midi が 0 以下は声なし（線を切る）
    std::vector<Voice> voice;               // 採用した声の音程（メインのトラック。10 ms ごと、時刻順）
    bool showPitch = true;

    std::vector<float> wave;                // 区間の音の大きさ（0..1。左から等間隔）
    skin::Colours colours {};               // スキンの 16 色
    juce::Image background;                 // 背景の画像（無効ならスキンの色）
    float lowMidi = 48.0f, highMidi = 72.0f;   // 音程の欄の上下（fitPitchRange で決める）

    bool hasLyrics() const { return ! lyrics.empty(); }
    bool hasPitch() const  { return showPitch && (! guide.empty() || ! voice.empty()); }
    double seconds() const { return sampleRate > 0.0 ? (double) juce::jmax<int64> (0, to - from) / sampleRate : 0.0; }

    /** 区間の音符・声から音程の欄の上下を決める（上下に 2 半音の余白。1 オクターブより狭くしない） */
    void fitPitchRange();
};

/** 背景・曲名など動かない部分。1 回だけ作り、毎コマ最初に敷く */
juce::Image paintStatic (const Scene&);

/** sample の時点の 1 コマ（staticLayer は paintStatic の結果） */
void paintFrame (juce::Graphics&, const Scene&, const juce::Image& staticLayer, int64 sample);

/** 書く。audio は区間の音（video::audioRate のステレオ。nullptr なら音なし）。
    progress（0..1）が false を返したら中止して "cancelled"。戻り値は失敗の理由（空なら成功）。途中で失敗・中止したら dest は残さない */
juce::String write (const Scene&, std::shared_ptr<const juce::AudioBuffer<float>> audio, const juce::File& dest, int fps,
                    const std::function<bool (float)>& progress);

/** 波形の棒：bins 本に分けた音の大きさ（RMS を、いちばん大きい棒で割る） */
std::vector<float> waveBins (const juce::AudioBuffer<float>&, int bins);

/** 形ごとの波形の棒の数 */
int waveBinCount (Shape);

/** X に無料のアカウントで載せられる長さの目安（秒）。2026-10 時点の二次情報（DESIGN 9.1）。超えたら案内するだけで切らない */
constexpr double xFreeLimitSeconds = 140.0;

/** ファイル名："{曲名}_9x16.mp4" など（ファイル名に使えない文字は除く。空なら Untitled） */
juce::String fileName (const juce::String& song, Shape);

/** 0:42 の形（1 時間を超えたら 1:02:03） */
juce::String clockText (double seconds);
} // namespace vb::share
