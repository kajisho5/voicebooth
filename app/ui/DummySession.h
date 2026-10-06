#pragma once

#include "Theme.h"
#include "project/ProjectModel.h"
#include "audio/WaveformOverview.h"
#include "audio/AudioEngine.h"
#include "audio/PitchTracker.h"
#include "update/UpdateCheck.h"
#include "Shortcuts.h"
#include <map>

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
    bool judged = true;         // お手本と比べた（false = お手本がまだ無い。線は中立の色。B8）
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

    // 輸送
    int64 playhead = 0;
    int64 rangeIn = 0, rangeOut = 0;      // 範囲なしは rangeOut <= rangeIn
    bool loopOn = true;
    bool isPlaying = false;
    bool isRecording = false;
    int64 recordStart = 0;                // 今回の録音を始めた位置（採用はここから。遡及録音ならフレーズの頭。B7）
    int64 recordEnd = -1;                 // 区間の録り直し（パンチイン。B10）の終わり。-1 = 通し
    bool canUndoTake = false;             // 直前のテイクを採用から外せる（Ctrl / ⌘+Z。B10）
    int countInBars = 1;                  // 録音の前に数える小節（0 = Off）。範囲の録り直しでは助走の小節（設定に保存）
    bool clickOn = false;                 // クリック（メトロノーム）。テンポが分かっている時だけ鳴る（設定に保存）
    float clickLevel = 0.62f;             // クリック・カウントインの音量（フェーダー 0..1、0.75 = 0 dB。設定に保存）
    bool countingIn = false;              // 録音の前のカウントイン中（曲はまだ鳴っていない）
    int64 countInPosition = 0;            // その時に聞こえている拍の位置（BAR.BEAT に出す。曲の頭より前は負）

    // 表示（ピッチ・波形で共有）
    int64 viewStart = 0, viewEnd = 0;
    bool octaveAlign = true;
    shortcuts::Map shortcuts = shortcuts::Map::defaults();   // 1 文字のショートカット（#28。設定で変える・アプリの設定に保存）
    double crossfadeMs = 8.0;             // テイクの継ぎ目のクロスフェード（DESIGN 6.4。書き出し・試聴の両方。設定に保存）
    bool showLyrics = false;              // 歌詞レーン（7.5.3）を出す。既定は出さない（2026-10-02 ユーザー決定：あまり使わない）
    bool octaveUp = false;                // 自分の声を 1 オクターブ上げて重ねる
    int thirdGuide = 0;                   // 3 度ガイド（ハモリのトラック。0 = なし・1 = 3 度上・2 = 3 度下。プロジェクトに入る。#30）
    bool fullRange = false;
    int lowMidi = 48, highMidi = 84;      // C3–C6
    float pitchToleranceCents = 30.0f;    // 緑の範囲（設定 20/30/50、既定 30）。黄は ±50 まで（50 のときは ±70。pitchWarnLimitCents）

    // 練習コントロール
    int tempoPercent = 100;
    int keyShift = 0;
    project::Mode mode = project::Mode::standard;
    project::RecMode recMode = project::RecMode::delivery;
    float offVocalGain = 0.51f, mainGain = 0.72f, harmonyGain = 0.40f, monitorGain = 0.64f;
    float monitorReverb = 0.25f;
    // モニターの帯のメーター（フェーダー後のピーク、dBFS。2026-10-02）。エンジンがあれば 30 Hz で上書き、UI_MOCK はこの見本の値
    float backingMeterDb = -18.2f, guideMeterDb = -23.0f, clickMeterDb = -20.0f, harmGuideMeterDb = -26.0f;
    bool backingMuted = false;
    // お手本の声を聴く（2026-10-02）。取り出した声（オフボの時間・オフボの元の SR・モノラル）。無ければ nullptr。
    // 音量は mainGain。S（ソロ）はオフボ・お手本・録ったトラックのうち、それだけを鳴らす
    std::shared_ptr<const audio::SongAudio> guideVocals;
    bool guideMuted = false, guideSolo = false, backingSolo = false;
    // ハモリのお手本（2026-10-02）：分離した声からリードボーカル（karaoke のモデル）を引いた残り。無ければ nullptr。
    // これがある時、guideVocals はリードだけ。音量は harmonyGain
    std::shared_ptr<const audio::SongAudio> guideHarmVocals;
    bool guideHarmMuted = false, guideHarmSolo = false;
    // 自分の S（ソロ）：自分の声だけを聴く（オフボ・お手本・録ったトラックを止める）。モニターの S は同時に 1 つ。
    // 自分の声はほかの S では消さない（お手本だけを流して重ねて歌う時に、自分が聞こえなくならない）
    bool selfSolo = false;
    // 声域（MIDI。-1 = まだ）。設定に保存。おすすめのキーに使う
    int voiceLow = -1, voiceHigh = -1;

    // 自分の声のモニター（B4）。出力がスピーカーらしい機器に替わったら、ハウリングしないよう最初だけミュート
    bool selfMuted = false;
    bool speakerOutput = false;           // いまの出力がスピーカーらしい（DeviceRules::looksLikeSpeakers）
    juce::String speakerCheckedFor;       // 最後に判定した出力の機器名（同じ機器では二度ミュートしない）

    // 出力デバイス（B2。UI_MOCK では open = false のまま）
    audio::OutputStatus output;
    bool engineAttached = false;

    // 録音形式（2026-10-01：機器が対応すれば 44.1〜384 kHz、24bit / 32bit float。DESIGN 6.5）
    double recordRate = 0.0;              // 0 = 曲に合わせる。違う値なら伴奏をこの SR にそろえ、時間軸もこの SR
    bool recordFloat = false;             // 32bit float（false = 24bit PCM）
    // 使う人が設定で選んだ録音形式（アプリの設定に保存し、次に開く曲に使う）。上の 2 つは開いているプロジェクトのもので、
    // テイクのあるプロジェクトを開くとその SR になる（それをアプリの設定に書いていた。バグチェック 2026-10-05）
    double prefRecordRate = 0.0;
    bool prefRecordFloat = false;
    int songRate = 0;                     // 曲ファイルの SR（project.sampleRate は時間軸）
    std::shared_ptr<const audio::SongAudio> songOriginal;   // そろえ直す時の元の伴奏
    std::shared_ptr<const audio::SongAudio> songCurrent;    // いまの伴奏（プロジェクトの SR。確認用ミックス B15）
    bool conforming = false;              // 伴奏の SR をそろえている途中（録音できない）
    int songSerial = 0;                   // 曲を開くたびに増える（裏の作業が古い曲に戻ってこないように）

    /** いまの録音形式で録る SR（曲に合わせる時は曲の SR） */
    int deviceFallbackRate = 0;           // 「曲に合わせる」なのに機器が曲の SR で開けない時に、代わりに録る機器の SR（曲ごと。0 = 使わない）
    int targetRate() const { return recordRate > 0.0 ? juce::roundToInt (recordRate) : (deviceFallbackRate > 0 ? deviceFallbackRate : songRate); }

    // 録音（B5）・保存（B14）。曲ごとのプロジェクトフォルダ（書類/VoiceBooth/Projects/{曲名}/）に曲のコピー・テイク・{曲名}.vbooth
    juce::File projectFolder;
    juce::File projectFile;                             // {曲名}.vbooth（自動保存。B14）
    juce::String guidePath;                             // お手本（声入りの原曲）のコピー。プロジェクトフォルダ相対（B9 / B14）
    juce::StringArray recentProjects;                   // 最近のプロジェクト（.vbooth のフルパス。新しい順、8 件まで）
    juce::String recordingTake, recordingPath;          // 録音中のテイク（"take3"、フォルダ相対のパス）
    TrackType recordingTrack = TrackType::main;
    std::map<juce::String, std::shared_ptr<const audio::WaveformOverview>> takeWaves;   // "main/take3" → 概形（曲頭基準ではなくテイク頭から）
    // テイクの音程（B18。曲の時間・10 ms ごと。原速・原キーのテイクだけ）と、お手本と比べた結果
    std::map<juce::String, std::shared_ptr<const std::vector<audio::PitchFrame>>> takePitch;   // "main/take3" →
    struct TakeStats
    {
        int entries = 0, matched = 0;     // お手本の入りの数・合った数
        double onsetMs = 0.0;             // 入りのずれの真ん中（+ は遅い）
        float inBand = 0.0f, meanAbsCents = 0.0f;
        int pitchFrames = 0;
        float vibRateHz = 0.0f, vibDepthCents = 0.0f;   // ビブラートのある音符の平均（無ければ 0）
        int vibNotes = 0;
    };
    std::map<juce::String, TakeStats> takeStats;
    // テイク比較（B18c）：比べている間の範囲と、いま試聴で入れているテイク（採用区間はその形に差し替わっている）
    struct TakeCompare
    {
        bool active = false;
        int serial = 0;                               // 比べ始めるたびに増える（閉じたパネルが次の比較を止めないように）
        TrackType track = TrackType::main;
        int64 from = 0, to = 0;
        enum class Scope { song, inOut, segment } scope = Scope::song;
        juce::String previewing;                      // 空 = いまの採用（元のまま）
        std::vector<project::CompSegment> original;   // 比べ始めた時の採用区間（「採用中」の印・元に戻す形）
    } compare;
    bool exporting = false;

    // 画面下に一度だけ出す知らせ（トースト）。noticeSerial が増えたら出す
    juce::String noticeText;
    int noticeSerial = 0;

    // 新しいバージョンの知らせ（DESIGN 11.7。GitHub のリリースを見る。UiSessionUpdate.cpp）。updateVersion が空なら出さない
    juce::String updateVersion;
    update::Release updateRelease;        // 知らせているバージョン（ページ・この OS のインストーラー・本文）
    bool updateAutoCheck = true;          // 起動時に確かめる（24 時間に 1 回まで。設定で切れる）
    bool updateBetas = false;             // ベータも知らせる（いまのバージョンがベータなら切っていても知らせる）
    bool updateChecking = false;
    juce::String updateSkipped;           // 「このバージョンを飛ばす」で飛ばしたバージョン
    juce::int64 updateLastCheck = 0;      // 最後に確かめられた時刻（ms。つながらなかった時は進めない）
    int updateNoticeSerial = -1;          // この番号の知らせには「見る」キーを付ける（今すぐ確かめた時）

    // アプリ共通のキャッシュの場所（作ったオフボなど。DESIGN 8）。空 = 既定（system::defaultCacheFolder）
    juce::File cacheFolder;

    // 入力。UI_MOCK ではこのダミーのまま。エンジンがあれば UiSession が実デバイスの値で上書きする（B3）
    juce::String inputDevice, driver;
    int bufferSize = 256;
    float inputPeakDb = -12.0f, inputRmsDb = -18.4f, inputPeakHoldDb = -9.6f;
    bool inputClipped = false;
    int64 latencySamples = 538;           // UI_MOCK のダミーの実測値（実デバイスでは latencyProfiles）

    // 往復の遅れ（B6）。機器の組み合わせ（latencyProfileKey）ごとに、実測と手入力を覚える（アプリ設定に保存）
    struct LatencyProfile
    {
        int64 measured = -1;              // 実測（サンプル。-1 = 測っていない）
        double manualMs = -1.0;           // 手入力（ms。-1 = 使わない）。実測より優先
    };
    std::map<juce::String, LatencyProfile> latencyProfiles;
    bool latencyMeasuring = false;        // 測定音を鳴らしている・解析している
    bool latencyHasResult = false;        // この画面を開いてから測った（結果を出す）
    audio::latency::Result latencyResult; // 最後の測定の結果（失敗の理由を出す）
    int64 recordingLatency = 0;           // 録音を始めた時の補正量（テイクの頭をこの分だけ前へ）
    int recordingTempo = 100, recordingKey = 0;   // 録音を始めた時の練習のテンポ・キー（B11）

    // 入力デバイス（B3。UI_MOCK では open = false のまま）
    audio::InputStatus input;
    int deviceLostCount = 0;              // 使っていた機器が外れた回数（増えたら知らせる）

    /** 実際の入力を表示しているか（エンジンがあり、入力が開いている） */
    bool inputLive() const { return engineAttached && input.open; }

    // 曲の情報（B4b。値そのものは project の tempo / key / sections / lyrics。ここは画面の状態だけ）
    int selectedSection = -1;       // ルーラーで選んだ区間（Delete で消す）
    int lyricCursor = 0;            // 時刻の無い歌詞の今の行（↑ ↓ で手送り）／タップで合わせる時に次に叩く行
    bool lyricSyncing = false;      // 「タップで合わせる」の最中（Enter で行の歌い出し）

    std::vector<TrackUi> trackUi;   // タブに出すボーカルトラック
    int selectedTrack = 0;          // trackUi の添字

    std::vector<RefNote> refNotes;
    std::vector<PitchPoint> refPitch;
    std::vector<PitchPoint> refPitchHarm;   // ハモリのお手本の音程（オフボの時間。2026-10-02）。空ならハモリのトラックもメインと比べる
    // お手本の線（refPitch・refPitchHarm）を作り直す・ずらす・時間軸をそろえ直すたびに増やす。音符とキーの提案のキャッシュの鍵。
    // 点の数・頭・終わりだけの鍵では、同じ曲の線は必ず同じ並び（10 ms ごと）なので、差し替えても古いまま残っていた（#24）
    int refPitchSerial = 0;
    std::vector<PitchPoint> myPitch;

    // お手本（声入りの原曲。B9。DESIGN 7.1.1）。refPitch はオフボの時間
    juce::String guideName;               // 読み込んだ原曲のファイル名（空 = まだ）
    bool guideBusy = false;               // 時間合わせ・声の取り出しの最中
    // ボーカル分離（B16）：引き算では声が取れない時に勧める。分離は別プロセスで裏で進む
    int separationOfferSerial = 0;        // 「分離しますか？」を出す合図（増えたら出す）
    int leadOfferSerial = 0;              // 「ハモリの分離も続けますか？」を出す合図（引き算でお手本が取れた後。#27）
    // 上の 2 つを表示した回数（画面を作り直しても、まだ出していない確認が消えないよう、画面ではなくここに持つ）
    int separationOfferShown = 0, leadOfferShown = 0;
    // 分離モデルのダウンロード（B16。使う人が押した時だけ）
    struct ModelDownload
    {
        int dialogSerial = 0;             // 増えたらダウンロードの画面を開く（確認から）
        bool known = false;               // 一覧（署名を確かめた manifest）を読めた
        juce::String title, license;
        juce::int64 size = 0;             // 合計のバイト数
        int stage = -1;                   // models::DownloadStatus::Stage（-1 = 始めていない）
        juce::int64 received = 0;
        double bytesPerSecond = 0.0;
        int retryIn = 0, attempt = 0;
        bool paused = false;
        juce::String error;
        int noticeSerial = -1;            // この番号の知らせには「分離モデルを入れる」キーを付ける
    } modelDl;
    bool guideNeedsSeparation = false;    // 引き算で声が取れなかった（モデルが入ったら分離を勧める）
    // 原曲で聴く（聞き比べ・時間合わせの確認。DESIGN 4.6 Original）：合わせた原曲（オフボの時間・オフボの元の SR・モノラル）と、オフボの代わりに鳴らすか
    std::shared_ptr<const audio::SongAudio> guideOriginal;
    bool listenOriginal = false;
    bool guideAlignRough = false;
    // カット版（DESIGN 7.1.1）：お手本が使える所（オフボの時間・曲の SR）。曲のほぼ全部なら空（「お手本なし」を描かない）
    std::vector<std::pair<int64, int64>> guideCovered;          // 時間合わせの確かさが低い（「推定」と出す。DESIGN 7.1.1）
    double guideNudgeMs = 0.0;            // お手本の位置の手直し（ms。+ で後ろへ。DESIGN 7.1.1。プロジェクトに保存）
    int guideKaraokeKey = 0;              // キー違いのカラオケ：カラオケ = 原曲 + この半音（分離した線をその分ずらす。DESIGN 7.1.1）

    // リハーサルで録ったテイクの救済：この番号の知らせには「本番に入れる」を付ける
    int rescueNoticeSerial = -1;
    project::TrackType rescueTrack = project::TrackType::main;
    juce::String rescueTakeId;
    bool separating = false;
    int separationKind = 0;               // 0 = お手本の声と伴奏・1 = 原曲からオフボ・2 = リードとハモリ（状態バーの言い方）
    float separationProgress = 0.0f;      // 0..1
    double separationEta = -1.0;          // 残りの秒（分からなければ < 0）
    bool leadAnalysing = false;           // リードを取り出した後、リードとハモリの線を作っている間（ハモリのトラックで「作成中」と表示する）
    double separationStartedMs = 0.0;     // 始めた時刻（Time::getMillisecondCounterHiRes）。残り時間が出るまでの「準備中」の経過に使う
    int64 myPitchLag = 0;                 // 自分のピッチの点が再生ヘッドより遅れて届く分（遅れ + 検出。今の音の点を出す許し幅。B8）

    int sampleRate() const { return project.sampleRate; }
    int64 sec (double s) const { return (int64) std::llround (s * project.sampleRate); }
    double toSec (int64 samples) const { return (double) samples / project.sampleRate; }
    double bpm() const { return project.tempo.bpm; }
    /** テンポが分かっているか（分からなければ目盛りは秒、BAR.BEAT は「-.-」） */
    bool tempoKnown() const { return project.tempo.known(); }
    bool keyKnown() const { return project.key.known(); }
    int beatsPerBar() const { return project.tempo.signature.beatsPerBar(); }
    song::BarBeat barBeatAt (int64 sample) const { return song::barBeatAt (project.tempo, sample, sampleRate()); }

    const TrackUi& currentTrack() const { return trackUi[(size_t) selectedTrack]; }
    bool hasRange() const { return rangeOut > rangeIn; }
    bool isHarmonySelected() const
    {
        const auto t = currentTrack().type;
        return t == TrackType::harm1 || t == TrackType::harm2;
    }
    /** そのトラックが比べるお手本：ハモリのトラックはハモリのお手本（あれば）、ほかはメイン */
    const std::vector<PitchPoint>& refFor (TrackType t) const
    {
        return (t == TrackType::harm1 || t == TrackType::harm2) && ! refPitchHarm.empty() ? refPitchHarm : refPitch;
    }
    const std::vector<PitchPoint>& activeRef() const { return refFor (currentTrack().type); }
    const song::Line* lyricAt (int64 sample) const;
    const song::Line* lyricAfter (int64 sample) const;
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

/** ボーカルの [start, end) の最大振幅（0..1。1 を超えたらクリップ）。録ったテイクがあればその概形、無ければダミー */
float vocalPeak (const Session&, TrackType, int64 start, int64 end);

/** テイクの概形の鍵（"main/take3"） */
juce::String takeWaveKey (TrackType, const juce::String& takeId);

/** 指定区間がテイクで覆われているか（未録音=斜線の判定用） */
bool isRecorded (const Session&, TrackType, int64 sample);

juce::String noteName (float midi);

/** 現在位置の自分のピッチ（無ければ nullptr） */
const PitchPoint* myPitchAt (const Session&, int64 sample);
} // namespace vb::dummy
