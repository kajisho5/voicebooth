#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "SongLoader.h"
#include "Metronome.h"
#include "MonitorLevel.h"

namespace RubberBand { class RubberBandStretcher; }

/*  オフボの再生（DESIGN 7.2 / Phase B2）。デバイスに依存しない中身
    render() をオーディオスレッドから呼ぶ。ほかはメッセージスレッドから呼ぶ。

    ルール（DESIGN 17）
      - render() ではメモリ確保・ファイル I/O・待つロックをしない（曲の差し替えは try-lock、取れなければ無音）
      - 位置は曲のサンプル（int64）。出力デバイスの SR が曲と違うときだけ試聴用に変換する（線形補間）
      - 練習用のテンポ / キー（B11）：原速・原キーの時は素通し（サンプル単位で今までと同じ）。変えた時だけ
        Rubber Band（R3・リアルタイム）を通す。位置は「いま聞こえている曲の位置」を出力 1 サンプルごとに
        速さ × 曲SR / 出力SR だけ進める（始めに getPreferredStartPad の無音を入れ、getStartDelay の分を捨てて頭をそろえる）。
        出力の SR が曲と違う時の変換も、ストレッチの比とピッチの比に含める
      - 録ったトラック（B12）：採用区間をつないだ曲の長さのモノラル（書き出しと同じ計算。exporter::renderTrackDry）を
        最大 4 本、伴奏と同じ位置で足す（両耳に同じ）。音量はトラックごと。練習のテンポ / キーでは伴奏と一緒に Rubber Band を通す。
        ここで鳴らすのは出力（モニター）だけ。録音は入力の素の声だけ（TakeRecorder。混ざらない）
      - クリック（メトロノーム）とカウントイン（2026-10-02。Metronome）：出力 1 サンプルごとに「聞こえている曲の位置」から拍を数えて、
        出力の側で鳴らす（曲に混ぜてから伸ばさない。練習のキーで高さが変わらず、テンポを変えても拍に合う）。耳だけで録音には入らない。
        カウントインは曲の前に無音で数える分（play の countIn）：その間は曲を鳴らさず位置も進めない（Rendered::lead）
      - モニターの帯のメーター（2026-10-02）：オフボ・お手本・クリックのフェーダー後のピーク（LevelFollower）。
        練習のテンポ / キーの間は Rubber Band に入れる側で測る（聞こえるのは伸ばす遅れの分だけ後） */

namespace vb::audio
{
class PlaybackCore
{
public:
    /** 曲を差し替える（停止して頭へ）。古い曲はこのスレッドで解放される */
    void setSong (std::shared_ptr<const SongAudio>);
    bool hasSong() const;

    /** 出力の準備（デバイスが始まる時に呼ぶ） */
    void prepare (double outputSampleRate);
    double getOutputSampleRate() const { return outputRate.load(); }

    /** 再生を始める。countIn > 0 なら、今の位置から countIn（曲のサンプル）前の所から、曲を鳴らさずにクリックだけで数え（カウントイン）、
        届いたら今の位置から曲を鳴らす。countClicksUntil より前の位置ではクリックが Off でも拍を鳴らす
        （カウントインと、範囲の録り直しの助走を数える。-1 = 使わない）。どのスレッドからでも */
    void play (juce::int64 countIn = 0, juce::int64 countClicksUntil = -1);
    void stop();
    bool isPlaying() const { return playing.load(); }

    /** 次の render の頭でこの位置へ */
    void seek (juce::int64 sample);
    juce::int64 getPosition() const;

    /** ループ範囲 [in, out)。out に来たら in へ戻る */
    void setLoop (juce::int64 in, juce::int64 out, bool enabled);

    /** 練習用のテンポ（速さの倍率。1.0 = 原速、0.5〜1.5）とキー（半音、-6〜+6）。どのスレッドからでも */
    void setPractice (double speed, int semitones);
    /** 原速・原キーでない（Rubber Band を通している） */
    bool isPracticeShifted() const;

    /** 録ったトラックの音（B12）。slot は 0..maxStems-1、buffer は曲の SR・曲の長さのモノラル（nullptr で外す）。メッセージスレッド */
    static constexpr int maxStems = 7;   // 0..3 = 録ったトラック（Main / Double / Harm 1 / Harm 2）、4 = お手本の声（リード）、5 = ハモリのお手本、6 = 原曲（聞き比べ）
    static constexpr int guideSlot = 4;
    static constexpr int harmGuideSlot = 5;
    static constexpr int originalSlot = 6;
    void setStem (int slot, std::shared_ptr<const juce::AudioBuffer<float>> buffer);
    /** トラックの音量（直線の倍率。0 で鳴らさない）。どのスレッドからでも。20 ms でなめらかに */
    void setStemGain (int slot, float linearGain);

    /** オフボの音量（直線の倍率）とミュート。急に変えず数 ms でなめらかに */
    void setGain (float linearGain);
    void setMuted (bool);

    /** カウントインで数えている途中（曲はまだ鳴らしていない） */
    bool isCountingIn() const { return playing.load() && countLeftOut.load() > 0; }
    /** カウントイン中に聞こえている拍の位置（曲のサンプル。曲の頭より前なら負）。数えていなければ getPosition() */
    juce::int64 getCountInPosition() const { return getPosition() - (playing.load() ? countLeftOut.load() : 0); }

    /** クリックの拍の並び（曲のサンプル。テンポが分からなければ samplesPerBeat = 0）と、鳴らすか・音量（直線の倍率）。どのスレッドからでも */
    void setClickGrid (Metronome::Grid g) { metronome.setGrid (g); }
    void setClick (bool on, float linearGain);

    /** モニターの帯のメーター（フェーダー後のピーク。dBFS、下限 -100）。UI が 30 Hz で読む */
    struct Levels
    {
        float backingDb = LevelFollower::floorDb, guideDb = LevelFollower::floorDb, clickDb = LevelFollower::floorDb,
              harmGuideDb = LevelFollower::floorDb;
    };
    Levels getLevels() const { return { backingLevel.readDb(), guideLevel.readDb(), clickLevel.readDb(), harmGuideLevel.readDb() }; }

    /** 曲の終わりまで行って止まったら、1 度だけ true */
    bool consumeReachedEnd() { return reachedEnd.exchange (false); }

    /** render が鳴らした範囲（録音の位置合わせ用。B5）。start は 1 サンプル目の曲の位置、played は曲を鳴らした出力のサンプル数
        （止まっていれば 0、曲の終わりに来たらそこまで）。ループで戻ったら wrapped。
        step は出力 1 サンプルで進む曲のサンプル数（ふつう 1。練習のテンポ・SR 変換の時だけ違う）。
        lead はカウントインの残りで、曲を鳴らす前の出力のサンプル数（曲はブロックの lead サンプル目から。入力もそこから合わせる） */
    struct Rendered
    {
        juce::int64 start = 0;
        int played = 0;
        bool wrapped = false;
        double step = 1.0;
        int lead = 0;
        bool busy = false;   // 曲の差し替え中で鳴らせなかった（曲が止まったのではない。録音はこのブロックを飛ばす）
    };

    PlaybackCore();
    ~PlaybackCore();

    /** オーディオスレッド。out は numChannels 本 × numSamples（必ず全部書く） */
    Rendered render (float* const* out, int numChannels, int numSamples) noexcept;

    /** フェーダーの位置（0..1）→ 倍率。0.75 で 0 dB、1.0 で +6 dB、0 で無音 */
    static float faderToGain (float position) noexcept;

    /** 練習のテンポ・キーで使う Rubber Band のエンジン（RubberBandStretcher::Option…）。96 kHz を超えると軽い方（#21） */
    static int stretchEngineFor (double sampleRate) noexcept;

private:
    void rebuildStretcher();                        // 曲・出力の SR が決まった時（メッセージスレッド等。確保してよい所）
    Rendered renderSong (float* const* out, int numChannels, int numSamples, juce::int64 pos) noexcept;
    Rendered renderStretched (float* const* out, int numChannels, int numSamples, const SongAudio&) noexcept;
    /** クリックを足す（サンプル i に、両耳へ同じ値）。メーター用にピークも取る */
    void addClick (float* const* out, int numChannels, int i, float value) noexcept;
    /** 曲が止まった・終わった後の [from, to)：鳴りかけのクリックの残りだけ */
    void addClickTail (float* const* out, int numChannels, int from, int to) noexcept;
    /** このサンプルでクリックを鳴らすか（On か、カウントインの範囲） */
    bool clickAudible (double songPos) const noexcept { return clickOn.load (std::memory_order_relaxed) || songPos < (double) countUntil; }

    juce::SpinLock songLock;
    std::shared_ptr<const SongAudio> song;          // songLock で保護
    std::atomic<bool> loaded { false };             // song があるか（lock なしで読む。setSong が lock の中で書く）
    std::shared_ptr<const juce::AudioBuffer<float>> stems[maxStems];   // songLock で保護
    std::unique_ptr<RubberBand::RubberBandStretcher> stretcher;   // songLock で保護
    juce::AudioBuffer<float> stretchIn, stretchOut; // songLock で保護（rebuildStretcher で確保）
    static constexpr int stretchBlock = 1024;       // process / retrieve の 1 回の最大

    std::atomic<double> outputRate { 0.0 };
    std::atomic<bool> playing { false }, reachedEnd { false }, muted { false }, loopOn { false };
    std::atomic<juce::int64> position { 0 }, pendingSeek { -1 }, loopIn { 0 }, loopOut { 0 };
    std::atomic<float> gain { 1.0f };
    std::atomic<float> stemGain[maxStems] { { 1.0f }, { 1.0f }, { 1.0f }, { 1.0f }, { 1.0f }, { 1.0f }, { 0.0f } };
    std::atomic<double> speed { 1.0 };
    std::atomic<int> semitones { 0 };
    std::atomic<bool> stretchReset { true };        // 次のブロックでストレッチを頭からやり直す（再生開始・シーク・曲替え）

    // クリックとカウントイン（2026-10-02）
    Metronome metronome;
    std::atomic<bool> clickOn { false }, startRequested { false };
    std::atomic<juce::int64> countInRequest { 0 }, countUntilRequest { -1 };
    std::atomic<juce::int64> countLeftOut { 0 };    // カウントインの残り（曲のサンプル。UI の表示用）

    // モニターの帯のメーター（2026-10-02）
    LevelFollower backingLevel, guideLevel, clickLevel, harmGuideLevel;

    // オーディオスレッドだけが触る
    double fraction = 0.0;                          // SR 変換時の小数部
    juce::SmoothedValue<float> smoothedGain { 1.0f };      // オフボ
    juce::SmoothedValue<float> stemSmoothed[maxStems];     // 録ったトラック
    juce::SmoothedValue<float> fade { 1.0f };              // 全体（素通しとストレッチの切り替え・やり直しの頭）

    /** 曲の pos の音（オフボ × 音量 + トラック × 音量）。gains は今のサンプルの音量。メーター用にオフボ・お手本のピークも取る */
    struct Gains { float backing; float stem[maxStems]; };
    Gains nextGains() noexcept;
    float mixAt (const SongAudio&, int channel, juce::int64 pos, const Gains&) noexcept;
    float peakBacking = 0.0f, peakGuide = 0.0f, peakClick = 0.0f, peakHarmGuide = 0.0f;   // このブロックの最大（フェーダー後）
    double countLeft = 0.0;                         // カウントインの残り（曲のサンプル。出力 1 サンプルで step ずつ減る）
    juce::int64 countUntil = -1;                    // この位置まではクリック Off でも拍を鳴らす
    bool prepared = false;
    bool stretching = false;                        // 前のブロックで Rubber Band を通したか
    double heard = 0.0;                             // 聞こえている曲の位置（ストレッチ中）
    juce::int64 feedPos = 0;                        // 次に Rubber Band へ入れる曲の位置
    int dropLeft = 0;                               // 頭で捨てる出力（getStartDelay）
    double appliedTime = 0.0, appliedPitch = 0.0;
};
} // namespace vb::audio
