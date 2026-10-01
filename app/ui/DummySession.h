#pragma once

#include "Theme.h"
#include "project/ProjectModel.h"
#include "audio/WaveformOverview.h"

/*  見た目フェーズ専用の固定ダミー（DESIGN 20）
    UI はこのヘッダ経由でのみダミーデータを読む。結線時（Phase B）に差し替える。
    乱数は使わない（毎回同じスクリーンショットになること）。 */

namespace vb::dummy
{
using project::TrackType;

struct RefNote
{
    int64 start = 0, end = 0;
    float midi = 60.0f;
    bool vibrato = false;
    bool consonant = false;     // 頭に子音（ピッチ検出の信頼度が落ちる）
};

struct PitchPoint
{
    int64 sample = 0;
    float midi = 0.0f;
    float confidence = 0.0f;    // 0..1。低い点は描かない（嘘でつながない）
    float centsOff = 0.0f;      // 自分ピッチのみ：お手本とのずれ
};

/** トラックの UI 状態（ミュート等。永続化の形は B14 で決める） */
struct TrackUi
{
    TrackType type;
    bool armed = false, mute = false, solo = false;
    float monitorGain = 0.75f;
    int hotkey = 0;             // 1–4
};

struct Session
{
    project::Project project;
    juce::String songName;

    // 開いた曲（B1）。無ければダミーの概形を描く
    std::shared_ptr<const audio::WaveformOverview> backingWave;
    bool tempoKnown = true;                // 解析前（テンポ推定前）の曲は false → 拍ではなく秒で目盛る
    bool keyKnown = true;

    // 輸送
    int64 playhead = 0;
    int64 rangeIn = 0, rangeOut = 0;      // 範囲なしは rangeOut <= rangeIn
    bool loopOn = true;
    bool isPlaying = false;
    bool isRecording = false;
    int64 recordStart = 0;                // 今回の録音を始めた位置（見た目用）
    int countInBars = 1;
    bool clickOn = false;
    int beatsPerBar = 4;

    // 表示（ピッチ・波形で共有）
    int64 viewStart = 0, viewEnd = 0;
    bool octaveAlign = true;
    bool octaveUp = false;                // 自分の声を 1 オクターブ上げて重ねる
    bool fullRange = false;
    int lowMidi = 48, highMidi = 84;      // C3–C6
    float pitchToleranceCents = 30.0f;    // 緑の範囲（設定 20/30/50、既定 30）。黄は ±50 まで

    // 練習コントロール
    int tempoPercent = 100;
    int keyShift = 0;
    project::Mode mode = project::Mode::standard;
    project::RecMode recMode = project::RecMode::delivery;
    float offVocalGain = 0.51f, mainGain = 0.72f, harmonyGain = 0.40f, monitorGain = 0.64f;
    float monitorReverb = 0.25f;

    // 入力
    juce::String inputDevice, driver;
    int bufferSize = 256;
    float inputPeakDb = -12.0f, inputRmsDb = -18.4f, inputPeakHoldDb = -9.6f;
    int64 latencySamples = 538;

    std::vector<TrackUi> trackUi;   // タブに出すボーカルトラック
    int selectedTrack = 0;          // trackUi の添字

    std::vector<RefNote> refNotes;
    std::vector<PitchPoint> refPitch;
    std::vector<PitchPoint> myPitch;

    int sampleRate() const { return project.sampleRate; }
    int64 sec (double s) const { return (int64) std::llround (s * project.sampleRate); }
    double toSec (int64 samples) const { return (double) samples / project.sampleRate; }
    double bpm() const { return project.tempoOriginal; }

    const TrackUi& currentTrack() const { return trackUi[(size_t) selectedTrack]; }
    bool hasRange() const { return rangeOut > rangeIn; }
    bool isHarmonySelected() const
    {
        const auto t = currentTrack().type;
        return t == TrackType::harm1 || t == TrackType::harm2;
    }
    const project::LyricLine* lyricAt (int64 sample) const;
    const project::LyricLine* lyricAfter (int64 sample) const;
};

Session makeSession();

/** 開いた曲で作り直した状態（テイク・歌詞・お手本ピッチは空。解析はまだ）
    表示の好み（モード・許容幅など）と入力（B3 まではダミー）は prev から引き継ぐ */
Session makeSongSession (const Session& prev, const juce::String& name, const juce::String& path,
                         int sampleRate, int64 lengthSamples,
                         std::shared_ptr<const audio::WaveformOverview>);

/** オフボ概形の振幅 0..1（決定的） */
float backingAmplitude (const Session&, int64 sample);

/** オフボの [start, end) の最大振幅 0..1。開いた曲があれば実波形、無ければダミー */
float backingPeak (const Session&, int64 start, int64 end);

/** オフボの [start, end) の RMS 0..1（曲の起伏。ピークだけでは市販曲は平らに見える） */
float backingRms (const Session&, int64 start, int64 end);

/** ボーカル概形の振幅 0..1。1 を超えるとクリップ扱い */
float vocalAmplitude (const Session&, TrackType, int64 sample);

/** 指定区間がテイクで覆われているか（未録音=斜線の判定用） */
bool isRecorded (const Session&, TrackType, int64 sample);

juce::String noteName (float midi);

/** 現在位置の自分のピッチ（無ければ nullptr） */
const PitchPoint* myPitchAt (const Session&, int64 sample);
} // namespace vb::dummy
