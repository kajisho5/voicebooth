#pragma once

#include "DummySession.h"
#include "project/ProjectFile.h"
#include "separation/SeparatorClient.h"
#include "lyrics/LyricsClient.h"
#include "models/ModelDownloader.h"
#include "audio/SongLoader.h"

/*  画面の状態（Phase A）
    UI 部品はここから読み、ここを変更し、変更通知で描き直す。
    Phase B では中身が音声エンジン・プロジェクトにつながる（UI 側の書き方は変えない）。 */

namespace vb
{
namespace change
{
    enum : juce::uint32
    {
        transport = 1 << 0,   // 再生 / 録音 / ループ / カウントイン / クリック
        playhead  = 1 << 1,
        range     = 1 << 2,
        view      = 1 << 3,   // 表示範囲・音域・オクターブ表示
        tracks    = 1 << 4,   // 選択 / アーム / M / S
        practice  = 1 << 5,   // テンポ / キー / 録音モード
        mode      = 1 << 6,   // 簡単 / 標準 / プロ
        monitor   = 1 << 7,
        song      = 1 << 8,   // 曲を開いた（全部が変わる）
        device    = 1 << 9,   // 出力 / 入力デバイスの状態・一覧
        meter     = 1 << 10,  // 入力メーターの値（30 Hz）
        songInfo  = 1 << 12,  // 曲の情報：テンポ・拍子・キー・区間・歌詞（B4b。DESIGN 7.5）
        takes     = 1 << 20,  // テイク・採用区間・テイクの波形が変わった（B5）
        notice    = 1 << 21,  // 知らせ（トースト）を出す（noticeText / noticeSerial）
        recordFormat = 1 << 22,  // 録音形式（SR・ビット数）が変わった
        latency   = 1 << 23,  // 往復の遅れ（実測・手入力・測定中）が変わった（B6）
        project   = 1 << 24,  // プロジェクトを保存した・最近の一覧が変わった（B14）
        all       = 0xffffffff
    };
}

class UiSession
{
public:
    UiSession();

    const dummy::Session& get() const { return s; }
    const dummy::Session* operator->() const { return &s; }

    struct Listener
    {
        virtual ~Listener() = default;
        virtual void sessionChanged (juce::uint32 changes) = 0;
    };

    void addListener (Listener* l)    { listeners.add (l); }
    void removeListener (Listener* l) { listeners.remove (l); }

    // --- 曲 -----------------------------------------------------------------
    /** 開いた曲に差し替える（B1）。録音・再生は止まり、位置は頭へ */
    void loadSong (const juce::File&, int sampleRate, int64 lengthSamples,
                   std::shared_ptr<const audio::WaveformOverview>,
                   std::shared_ptr<const audio::SongAudio> = nullptr);

    /** 音声エンジン（B2）。nullptr なら見た目だけ（UI_MOCK）。エンジンはこの UiSession より長く生きること */
    void attachEngine (audio::AudioEngine*);

    /** 再生位置をエンジンから取っているか（曲を開いていて、出力デバイスがある） */
    bool isEngineDriven() const;

    // --- デバイスと入力メーター（B3） ----------------------------------------
    /** 入力メーターを読む（30 Hz。MainComponent のタイマーから）。UI_MOCK ではダミーのまま */
    void pollInput();

    /** 選べるデバイス（エンジンが無ければ空） */
    audio::DeviceList getDeviceList() const;
    void rescanDevices();

    /** 切り替え。再生は止まる。戻り値は失敗の理由（空なら成功） */
    juce::String selectDeviceType (const juce::String&);
    juce::String selectInputDevice (const juce::String&);
    juce::String selectOutputDevice (const juce::String&);
    juce::String selectInputChannel (int);
    juce::String selectBufferSize (int);

    /** クリップ表示を消す（メーターをクリック） */
    void resetInputClip();

    // --- 輸送 ---------------------------------------------------------------
    void setPlaying (bool);
    void setRecording (bool);
    void stop();
    void goToStart();
    void seek (int64 sample);
    /** 見た目の時計。再生中に呼ぶ（テンポに応じて進み、ループ範囲で戻る） */
    void tick (double seconds);

    void setLoop (bool);
    void setRange (int64 in, int64 out);
    void clearRange();
    void setRangeInAtPlayhead();
    void setRangeOutAtPlayhead();
    void setCountIn (int bars);
    void setClick (bool);

    // --- 表示 ---------------------------------------------------------------
    void setView (int64 start, int64 end);
    void setOctaveAlign (bool);
    /** 歌詞レーンを出すか（設定。既定は出さない） */
    void setShowLyrics (bool);
    void setCrossfade (double ms);   // 0 / 5 / 8 / 20 ms
    void setOctaveUp (bool);
    void setFullRange (bool);

    // --- トラック -----------------------------------------------------------
    void selectTrack (int index);
    void armTrack (int index);          // アームは同時に 1 本
    void setMute (int index, bool);
    void setSolo (int index, bool);
    /** トラックのモニター量（フェーダーと同じ 0..1、0.75 = 0 dB）。再生に効く（B12） */
    void setTrackGain (int index, float fader);

    /** 新しいバージョンの知らせ（ステータスバー）。空で消す */
    void setUpdateAvailable (const juce::String& version) { s.updateVersion = version; notify (change::device); }

    // --- モニター（B2：オフボ、B4：自分の声とモニターリバーブ） --------------
    void setBackingLevel (float fader);   // 0..1（0.75 = 0 dB）
    void setBackingMuted (bool);
    void setSelfMonitorLevel (float fader);
    void setSelfMonitorMuted (bool);
    void setMonitorReverb (float fader);  // 耳だけ。録音には入らない

    // --- 録音形式（SR・ビット数。2026-10-01） ---------------------------------
    /** rate = 0 は曲に合わせる。曲を開いていて、まだテイクが無ければ伴奏をその SR にそろえ直す（裏で）。
        テイクがある曲は SR を変えない（次に開く曲から）。ビット数はいつでも変えられる（次のテイク・書き出しから） */
    void setRecordFormat (double rate, bool floatSamples);

    // --- 往復の遅れ（B6。DESIGN 5 Step 3） --------------------------------------
    /** 測定音を鳴らして往復の遅れを測る（約 3.6 秒。再生・録音は止める）。結果はこの機器の組み合わせに保存し、録音位置の補正に使う */
    void measureLatency();
    void cancelLatencyMeasure();
    /** 手入力（ms）。測れない時に使う。0〜1000 ms。この機器の組み合わせに保存 */
    void setLatencyManualMs (double ms);
    /** 手入力をやめる（実測があれば実測、無ければデバイスの申告値に戻る） */
    void clearLatencyManual();
    /** アプリ設定との受け渡し（機器ごとの実測・手入力。JSON） */
    void restoreLatencyProfiles (const juce::String& json);
    juce::String latencyProfilesJson() const;

    // --- プロジェクトの保存・開く（B14。DESIGN 8） ---------------------------------
    /** 次に開く曲を、この .vbooth の続きとして開く（起動画面が曲を読み込む前に呼ぶ） */
    void setPendingProject (const juce::File& vboothFile, const project::LoadedProject&);
    /** いま変更があれば保存する（終了時など）。自動保存は変更から 1.5 秒後 */
    void flushSave();
    /** アプリ設定との受け渡し（最近のプロジェクト） */
    void restoreRecentProjects (const juce::StringArray&);

    // --- 区間の録り直し（パンチイン。B10） ------------------------------------------
    /** 直前のテイクを採用から外して、前の採用に戻す（テイクとファイルは残す）。戻せたら true */
    bool undoTake();
    /** リハーサルのテイク（原速・原キーで録った物）を本番のテイクにして採用する（録り間違いの救済）。できなければ false */
    bool promoteRehearsalTake (project::TrackType, const juce::String& takeId);

    // --- お手本（声入りの原曲。B9。DESIGN 7.1.1） ---------------------------------
    /** 原曲を読み、オフボと時間を合わせて声を取り出し（原曲 − カラオケ）、お手本の音程を重ねる（裏で）。
        結果は知らせで出す。引けない組（別のミックス・キー違い・別の曲）では線を出さない */
    void loadGuide (const juce::File&);
    /** お手本の原曲から声を分離して、お手本の線を作る（B16。引き算では取れない時）。裏で進む */
    void separateGuide();
    /** 原曲だけ（B16。DESIGN 7.1.1）：原曲を分離して、声を引いたオフボ（原曲と同じ SR・長さの WAV）を作る。曲を開く前に使う（裏で進む。
        進み具合は separating / separationProgress）。終わったら done（作ったファイル、失敗なら空と理由）。stopSeparation で中止 */
    void makeOffVocal (const juce::File& original, std::function<void (juce::File, juce::String)> done);
    void stopSeparation();
    // --- テイクの解析（B18） ---------------------------------------------------
    /** お手本と比べた結果を作り直す（onlyKey があればそのテイクだけ） */
    void updateTakeStats (const juce::String& onlyKey = {});
    /** いま選んでいるトラックのいちばん新しいテイクの結果（無ければ nullptr） */
    const dummy::Session::TakeStats* latestTakeStats() const;
    /** お手本から取り出した声（オフボの時間、16 kHz モノラル）。歌詞の自動合わせ（B17）に使う。お手本を合わせ終えると作られる */
    juce::File guideVocalsFile() const { return s.projectFolder == juce::File() ? juce::File() : s.projectFolder.getChildFile ("Cache/lyrics/guide-vocals-16k.wav"); }
    /** 分離モデル（B16）：一覧を取りに行き（署名を確かめる）、ダウンロードの確認を出す。押した時だけ呼ぶ */
    void requestSeparationModel() { requestModel (0); }
    /** モデル（0 = 分離 B16、1 = 歌詞の認識 B17）：署名した一覧を見て、ダウンロードの確認を出す。押した時だけ呼ぶ */
    void requestModel (int kind);
    // --- 歌詞の自動合わせ（B17。DESIGN 7.5.3 / 11.3） ---------------------------------
    /** お手本から取り出した声を認識して、歌詞の行の時刻を推定する（確定した時刻は変えない）。モデルが無ければ確認を出す */
    void alignLyricsAuto();
    void stopLyricsAlign();
    /** 自動で合わせられない理由の翻訳キー（空なら合わせられる。モデルが無いのは理由にしない＝押すと確認を出す） */
    juce::String lyricsAutoProblem() const;
    void startModelDownload();
    void cancelModelDownload();
    /** 「声を分離して取り出しますか？」を出す（モデルが入った後など） */
    void offerSeparation() { if (separationAvailable()) { ++s.separationOfferSerial; notify (change::notice); } }
    /** 分離に要る物（分離プロセスとモデル）がそろっている */
    bool separationAvailable() const;
    /** 分離にかかる時間の目安（秒。このパソコンで測る前の見込み） */
    double separationEstimateSeconds() const;

    // --- 録音・書き出し（B5） -------------------------------------------------
    /** 録音を始められない理由の翻訳キー（空なら録れる）。曲・入力・アーム・SR を見る */
    juce::String recordProblem() const;

    /** トラックの採用区間をフル尺の WAV に書き出す（裏のスレッドで。終わったら知らせる）。
        書き出し先は曲のプロジェクトフォルダの export_YYYYMMDD/。曲が無ければ何もしない */
    void exportTracks (const std::vector<project::TrackType>&, int bitDepth = 0);   // bitDepth：16 / 24 / 32（0 = 録音形式）
    /** 納品パック（B15。DESIGN 9）：export_YYYYMMDD/ に各トラックの Dry・確認用ミックス・notes.txt（プロは take_map.txt）と zip */
    void exportPack (const std::vector<project::TrackType>&, int bitDepth = 0, bool refmix = true);

    /** 曲ごとの作業フォルダ（テイク・書き出し）。.vbooth の保存（B14）までの仮の置き場：
        書類フォルダ/VoiceBooth/Projects/{曲名}/ */
    static juce::File projectFolderFor (const juce::String& songName);

    // --- 練習 / モード ------------------------------------------------------
    void setTempo (int percent);
    void setKey (int semitones);
    /** 練習のテンポ（50〜150 %）とキー（-6〜+6）をまとめて。再生に効く（B11） */
    void setPractice (int tempoPercent, int keyShift);
    void setRecMode (project::RecMode);
    void setMode (project::Mode);
    void setPitchTolerance (float cents);

    /** 納品 REC 中か（テンポ / キーがロックされる） */
    bool deliveryLocked() const { return s.isRecording && s.recMode == project::RecMode::delivery; }

    /** そのモードで見せるトラックか（DESIGN 2） */
    bool isTrackVisible (project::TrackType) const;

    // --- 曲の情報（B4b。DESIGN 7.5）。手で入れた・触った値は「確定」（解析をやり直しても上書きしない） ---
    // 実装は UiSessionSong.cpp。音声の処理には触れない（位置はオフボのサンプル）
    void setBpm (double bpm);                  // 0 で「分からない」に戻す（目盛りは秒へ）
    /** タップテンポ（T）。叩いた時刻（秒）。4 回目から BPM を確定して返す。まだなら 0 */
    double tapTempo (double nowSeconds);
    int tapCount() const { return tapper.count(); }
    void doubleBpm();
    void halveBpm();
    void setTimeSignature (song::TimeSignature);
    void setDownbeat (int64 sample);           // 1 小節目の頭
    void setDownbeatAtPlayhead();
    void shiftDownbeat (int beats);            // 目盛り全体を拍単位でずらす
    void setSongKey (int tonic, bool minor);   // tonic -1 = 分からない

    /** 区間の頭を打つ。テンポが分かっていれば小節線に吸い付く（snap = false で吸い付かない）。入った番号を返す */
    int addSection (int64 sample, const juce::String& kind, const juce::String& name = {}, bool snap = true);
    int addSectionAtPlayhead (bool snap = true);
    void renameSection (int index, const juce::String& kind, const juce::String& name = {});
    int moveSection (int index, int64 sample, bool snap = true);
    void removeSection (int index);
    void selectSection (int index);            // -1 で選ばない
    void goToSection (int index);              // 「サビへ」
    void loopSection (int index);              // 「この区間をループ」：範囲を区間に合わせてループ

    /** 歌詞を差し替える（見出し・サビの候補を区間にするかは lyrics.sectionsFromHeadings） */
    void setLyrics (song::Lyrics);
    void clearLyrics();
    /** 「タップで合わせる」：再生しながら各行の歌い出しで tapLyric（Enter） */
    void setLyricSyncing (bool);
    /** 次の行の歌い出しを今の位置に。全部の行が済んだら合わせ終わり（false を返す） */
    bool tapLyric();
    void undoLyricTap();                       // 1 行戻す（その行の時刻を外す）
    void stepLyric (int delta);                // 今の行を手で送る（↑ ↓）

private:
    void notify (juce::uint32 changes);
    void keepPlayheadInView();
    void followPlayhead (double seconds);
    void syncLoopToEngine();
    void syncPracticeToEngine();
    bool practiceShifted() const;
    void refreshOutputStatus();
    void checkSpeakerOutput();
    void pushMonitorToEngine();
    void finishRecording();
    void conformSong();
    void checkDeviceRate();
    void estimateSongInfo();
    bool hasTakes() const;
    void loadTakeWave (project::TrackType, const project::Take&);
    void postNotice (const juce::String& text);
    void refreshInputStatus();
    void deviceChanged (bool lost);
    juce::String afterDeviceSelect (juce::String error);

    void songInfoChanged (juce::uint32 also = 0);
    void pollLatencyProbe();
    void updateShadow();
    void pollPitch();
    void saveProject();
    void renderStems();            // 録ったトラックを裏で作り直す（B12）
    void syncStemGains();          // トラックの音量・M / S・録音中を再生に反映
    void restoreProject();
    void markDirty();
    void copyIntoProject (const juce::File& source, const juce::String& relativePath);
    int64 prerollSamples() const;
    void judge (dummy::PitchPoint&) const;
    void rejudgeAll();
    int64 retroStart (int64 pressSample) const;
    void latencyMeasured (const audio::latency::Result&, const juce::String& key);

    dummy::Session s;
    song::TapTempo tapper;
    audio::AudioEngine* engine = nullptr;
    double sinceStatus = 0.0;
    // 録ったトラックの再生（B12）。トラックごとに、最後に作った時の採用区間（変わった物だけ作り直す）
    bool stemsDirty = false;
    juce::uint32 stemGeneration = 0, stemSlotGeneration[4] {};
    juce::String stemSignature[4];
    std::unique_ptr<models::ModelDownloader> modelDownloader;   // B16
    std::unique_ptr<models::ModelEntry> modelEntries[2];        // 署名を確かめた一覧の中のモデル（0 = 分離、1 = 歌詞の認識）
    std::unique_ptr<lyrics::LyricsClient> lyricsRunner;          // B17
    bool lyricsAfterModel = false;                               // モデルが入ったら歌詞を合わせる
    std::unique_ptr<separation::SeparatorClient> separator;   // B16
    void analyseSeparated (const juce::File& vocals, const juce::File& backing);
    std::shared_ptr<bool> alive = std::make_shared<bool> (true);   // 裏のスレッドから戻ってきた時に、まだ生きているか
    bool loopBeforeRecording = false;
    juce::uint32 tailWaitStart = 0;
    juce::uint32 latencyStartMs = 0;  // 測定音を鳴らし始めた時刻
    bool analysingLatency = false;    // 録り終えた測定音を裏で解析している

    // 遡及録音（B7）：再生中、アームしたトラックがあれば裏で録っている（REC でテイクになる。押さずに止めたら消す）
    bool shadowActive = false;
    juce::File shadowFile;
    int shadowSerial = 0;
    std::vector<audio::PitchFrame> pitchFrames;   // 取り出し用（毎回確保しない）

    // 保存（B14）
    bool dirty = false, restoring = false;
    juce::uint32 dirtySince = 0, lastBackupMs = 0;
    std::unique_ptr<project::LoadedProject> pendingProject;   // 開く途中のプロジェクト（伴奏の SR をそろえてから戻す）
    juce::File pendingProjectFile;

    // 直前のテイクを採用する前の採用区間（Ctrl / ⌘+Z で戻す。B10）
    std::vector<project::CompSegment> compBeforeTake;
    project::TrackType undoTrack = project::TrackType::main;   // 曲の終わりの後、遅れて届く歌を録り足している間（0 = 待っていない）
    juce::ListenerList<Listener> listeners;
};

//==============================================================================
/** 入力チャンネルの表示（2 ch は L / R、それ以上は番号。1 ch は空） */
juce::String inputChannelLabel (int channel, int numChannels);

/** 入力デバイスの表示名（機器名 — チャンネル）。UI_MOCK はダミー、入力が無ければ「入力なし」 */
juce::String inputDisplayName (const dummy::Session&);

/** 入力が使えない理由（短い文。ステータスバー用）。使えていれば空 */
juce::String inputProblemShort (const dummy::Session&);

/** 録音位置の補正に使う往復の遅れ（B6）。優先順：手入力 → 実測 → デバイスの申告値。UI_MOCK はダミーの実測 */
struct LatencyDisplay
{
    bool known = false;       // 入力が無いと出せない
    bool reported = false;    // デバイスの申告値（実測ではない）
    bool estimated = false;   // 申告が無く、バッファから推定
    bool measured = false;    // この機器の組み合わせで測った値
    bool manual = false;      // 手入力
    int64 samples = 0;
    double ms = 0.0;
};
LatencyDisplay latencyDisplay (const dummy::Session&);

/** 取り消しのキーの表記（Mac は ⌘Z、ほかは Ctrl+Z） */
juce::String undoKeyName();

/** 機器の組み合わせ（ドライバ・入力・出力・SR・バッファ）の名前。遅れはこの組み合わせごとに覚える */
juce::String latencyProfileKey (const dummy::Session&);

/** UiSession を購読する部品の共通部分（登録・解除の書き忘れを防ぐ） */
class SessionView : private UiSession::Listener
{
public:
    explicit SessionView (UiSession& u) : session (u) { session.addListener (this); }
    ~SessionView() override { session.removeListener (this); }

protected:
    UiSession& session;
    const dummy::Session& state() const { return session.get(); }

private:
    void sessionChanged (juce::uint32 changes) override { onSessionChanged (changes); }
    virtual void onSessionChanged (juce::uint32 changes) = 0;
};
} // namespace vb
