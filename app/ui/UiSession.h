#pragma once

#include "DummySession.h"
#include "project/ProjectFile.h"
#include "project/TakeCompare.h"
#include "separation/SeparatorClient.h"
#include "models/ModelDownloader.h"
#include "audio/SongLoader.h"
#include "analysis/KeySuggest.h"

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
        prefs     = 1 << 25,  // アプリ設定：更新の確認・キャッシュの場所（DESIGN 11.7 / 8）
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
    /** アプリを終える前（メッセージスレッド）。録音中ならそこまでをテイクとして入れ、裏録りは消し、保存する */
    void closeForQuit();
    /** 録音を止めて、いま録っているテイクを捨てる（Esc →「破棄する」。ファイルも消し、採用もしない） */
    void discardRecording();
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
    /** 録音の前に数える小節（0 = Off、1、2）。範囲の録り直しでは助走の小節。テンポが分からない間は数えない（2026-10-02） */
    void setCountIn (int bars);
    /** クリック（メトロノーム）。テンポが分からない時は入れずに、理由を知らせる（2026-10-02） */
    void setClick (bool);
    /** クリック・カウントインの音量（フェーダーと同じ 0..1、0.75 = 0 dB）。耳だけ */
    void setClickLevel (float fader);
    /** 起動時に設定から戻す（テンポを見ない。鳴るのはテンポの分かる曲を開いてから） */
    void restoreClickOn (bool on) { s.clickOn = on; notify (change::transport); }

    // --- 表示 ---------------------------------------------------------------
    /** お手本の位置の手直し（±1 / ±10 ms。DESIGN 7.1.1）。線・判定・お手本の声をずらす。0 で元へ */
    void nudgeGuide (double deltaMs);
    /** 原曲で聴く（オフボの代わりに、時間を合わせた原曲を同じ音量で鳴らす。聞き比べ。合わせた原曲がある時だけ） */
    void setListenOriginal (bool);
    /** 「ここが同じ所」（DESIGN 7.1.1）：sample の前後 1.5 秒で原曲とカラオケの波形を比べ、ずれ（±250 ms まで）をお手本の位置の手直しに足す。裏で計算 */
    void alignGuideAt (int64 sample);
    void resetGuideNudge();
    void setView (int64 start, int64 end);
    /** 横の拡大・縮小（ピッチと波形で共有）。anchor の位置は画面の同じ所に残す。factor < 1 で寄る。幅は 2 秒〜曲の長さ */
    void zoomView (int64 anchor, double factor);
    /** 横に送る（表示の幅に対する割合。+ で右へ） */
    void scrollView (double fraction);
    void setOctaveAlign (bool);
    /** 歌詞レーンを出すか（設定。既定は出さない） */
    void setShowLyrics (bool);
    void setCrossfade (double ms);   // 0 / 5 / 8 / 20 ms

    // お手本の声を聴く（モニターの「お手本」）。オフボ・お手本の S は、それだけを鳴らす
    void setGuideLevel (float fader);
    void setGuideMuted (bool);
    void setGuideSolo (bool);
    // ハモリのお手本（2026-10-02）：分離でリードとハモリを分けられた時だけ鳴る
    void setHarmGuideLevel (float fader);
    void setHarmGuideMuted (bool);
    void setHarmGuideSolo (bool);
    bool hasHarmGuideVocals() const { return s.guideHarmVocals != nullptr; }
    void setBackingSolo (bool);
    /** 自分の S：自分の声だけを聴く（オフボ・お手本・録ったトラックを止める）。モニターの S は同時に 1 つ */
    void setSelfSolo (bool);
    bool hasGuideVocals() const { return s.guideVocals != nullptr; }

    // 声域（MIDI）とおすすめのキー
    void setVoiceRange (int low, int high);
    analysis::KeySuggestion keySuggestion() const;
    analysis::SongRange guideRange() const;   // お手本の最低音・最高音（MIDI）
    void applySuggestedKey();
    // 声域を測る：測っている間は曲を止め、マイクの音程を集める（曲の線には入れない）
    void startRangeMeasure();
    void stopRangeMeasure();
    const std::vector<float>& rangeSamples() const { return rangeNotes; }

    // 待ち時間のゲーム（起動画面で分離を待つ間。WaitGame）
    /** マイクの音程を曲の外で取る（曲を開く前でも）。止めるまで gameVoiceMidi が新しくなる */
    void startGameVoice();
    void stopGameVoice();
    /** いまの声の音程（MIDI）。声が無い・0.25 秒より古ければ 0 */
    float gameVoiceMidi() const;
    /** ゲームのクリック（BPM。0 で止める）と目標の音。エンジンが無ければ何もしない */
    void setGameBeat (double bpm);
    void playGameTone (float midi, double seconds);
    /** クリックを始めてから耳に届いている位置（秒）。鳴らせない（エンジン・出力が無い）なら < 0 */
    double gameBeatClock() const;
    void setOctaveUp (bool);
    void setFullRange (bool);

    // --- トラック -----------------------------------------------------------
    void selectTrack (int index);
    void armTrack (int index);          // アームは同時に 1 本
    void setMute (int index, bool);
    void setSolo (int index, bool);
    /** トラックのモニター量（フェーダーと同じ 0..1、0.75 = 0 dB）。再生に効く（B12） */
    void setTrackGain (int index, float fader);

    // --- 更新の確認（DESIGN 11.7。GitHub のリリース）とアプリ共通のキャッシュ。実装は UiSessionUpdate.cpp ----
    /** アプリ設定から戻す（起動時）。found：前に見つけて覚えておいたバージョン（Release::toJson。まだ新しければ知らせを出し直す） */
    void restoreAppPrefs (bool autoCheck, bool betas, const juce::String& skipped, juce::int64 lastCheckMs,
                          const juce::String& found, const juce::File& cacheFolder);
    /** 起動時：自動の確認が入っていて、前回から 24 時間たっていれば裏で確かめる（失敗しても何も言わない） */
    void checkForUpdatesIfDue();
    /** 「今すぐ確かめる」：結果（最新・新しいバージョン・つながらない）を知らせで出す */
    void checkForUpdatesNow();
    void setUpdateAutoCheck (bool);
    void setUpdateBetas (bool);
    /** 新しいバージョンの知らせ（ステータスバー）。found でなければ消す（見本の画面にも使う） */
    void setUpdateAvailable (const update::Release&);
    /** 「このバージョンを飛ばす」：覚えておき、次のバージョンが出るまで知らせない */
    void skipUpdate();
    /** アプリ共通のキャッシュの場所（設定で選んだ所、無ければ既定）。曲ごとの <プロジェクト>/Cache/ とは別 */
    juce::File cacheFolder() const;
    /** 場所を変える（空で既定に戻す）。前の場所にある分は動かさない（次から新しい場所に作る） */
    void setCacheFolder (const juce::File&);

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
    /** 開く途中のプロジェクトを捨てる（読み込みをやめた・失敗した時） */
    void clearPendingProject();
    /** いま変更があれば保存する（終了時など）。自動保存は変更から 1.5 秒後 */
    void flushSave();
    /** アプリ設定との受け渡し（最近のプロジェクト） */
    void restoreRecentProjects (const juce::StringArray&);

    // --- 区間の録り直し（パンチイン。B10） ------------------------------------------
    /** 直前のテイクを採用から外して、前の採用に戻す（テイクとファイルは残す）。戻せたら true */
    bool undoTake();
    /** リハーサルのテイク（原速・原キーで録った物）を本番のテイクにして採用する（録り間違いの救済）。できなければ false */
    bool promoteRehearsalTake (project::TrackType, const juce::String& takeId);

    // --- テイク比較（B18c。DESIGN 2 / 3） ------------------------------------------------
    /** いまのトラックのテイクを範囲 [from, to) で比べ始める（IN / OUT・採用区間の 1 区間・曲全体）。
        loopRange：範囲をループで聴く（範囲と IN / OUT を一時的にそろえ、終わったら元に戻す）。比べられるテイクが無ければ false */
    bool beginTakeCompare (int64 from, int64 to, dummy::Session::TakeCompare::Scope);
    /** そのテイクを範囲に入れて聴く（空 = いまの採用）。採用区間を差し替え、トラックの再生（B12）が作り直す */
    void previewCompareTake (const juce::String& takeId);
    /** 試聴の再生 / 停止。止まっていれば、ループなら範囲の頭、曲全体なら試聴中のテイクの頭から（そこが聞こえる所） */
    void toggleCompareAudition();
    /** 終える。commit なら試聴中のテイクを採用（Ctrl / ⌘+Z で戻せる）、そうでなければ元の採用区間へそっくり戻す */
    void endTakeCompare (bool commit);
    /** いまのトラックに比べられるテイクがあるか（簡単モード・録音中・曲が無い時は false） */
    bool canCompareTakes() const;
    /** 範囲の中だけでお手本と比べた結果（お手本・テイクの音程がまだ無ければ nullopt。見本は見本の値） */
    std::optional<dummy::Session::TakeStats> takeStatsIn (project::TrackType, const juce::String& takeId, int64 from, int64 to) const;

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
    /** 分離の入口を差し替える（テスト用。偽の分離で、準備中の中止・失敗の片付けなどを確かめる） */
    void setSeparationService (std::unique_ptr<separation::Service> service) { stopSeparation(); separator.reset(); separationService = std::move (service); }
    // --- テイクの解析（B18） ---------------------------------------------------
    /** お手本と比べた結果を作り直す（onlyKey があればそのテイクだけ） */
    void updateTakeStats (const juce::String& onlyKey = {});
    /** いま選んでいるトラックのいちばん新しいテイクの結果（無ければ nullptr） */
    const dummy::Session::TakeStats* latestTakeStats() const;
    /** 分離モデル（B16）：一覧を取りに行き（署名を確かめる）、ダウンロードの確認を出す。
        onlyIfMissing：まだ入っていない物が一覧にある時だけ出す（起動した時に勧める） */
    void requestSeparationModel (bool onlyIfMissing = false);
    /** 分離・リードボーカル・音程のモデルのどれかがまだ無く、入れられる（本物のアプリ・配布の鍵・分離プロセスがある・受け取り中でない） */
    bool modelsMissing() const;
    void startModelDownload();
    void cancelModelDownload();
    /** 「声を分離して取り出しますか？」を出す（モデルが入った後など） */
    void offerSeparation() { if (separationAvailable()) { ++s.separationOfferSerial; notify (change::notice); } }
    /** 分離に要る物（分離プロセスとモデル）がそろっている */
    bool separationAvailable() const;
    /** 分離にかかる時間の見込み（このパソコンで測る前。分は短い方と長い方。#27） */
    struct SeparationEstimate
    {
        double lowSeconds = 0.0, highSeconds = 0.0;
        int lowMinutes() const  { return juce::jmax (1, juce::roundToInt (lowSeconds / 60.0)); }
        int highMinutes() const { return juce::jmax (lowMinutes() + 1, juce::roundToInt (highSeconds / 60.0)); }
    };
    /** songSeconds の曲に models 個のモデル（声と伴奏・リード）を回すときの見込み。cores = CPU のコア数 */
    static SeparationEstimate estimateSeparation (double songSeconds, int models, int cores);
    /** 開いている曲のお手本を分離するとき（リードのモデルがあれば、同じ分離の中でリードも分ける） */
    SeparationEstimate separationEstimate() const;
    /** 原曲だけで始めるとき（オフボを作る分。リードとハモリ分けは開いた後に別に動く） */
    SeparationEstimate originalSeparationEstimate (const juce::File& original) const;
    /** リードとハモリを分けるモデルが入っている */
    bool leadModelInstalled() const { return separationService->karaokeInstalled(); }

    // --- 録音・書き出し（B5） -------------------------------------------------
    /** 録音を始められない理由の翻訳キー（空なら録れる）。曲・入力・アーム・SR を見る */
    juce::String recordProblem() const;

    /** トラックの採用区間をフル尺の WAV に書き出す（裏のスレッドで。終わったら知らせる）。
        書き出し先は曲のプロジェクトフォルダの export_YYYYMMDD/。曲が無ければ何もしない */
    void exportTracks (const std::vector<project::TrackType>&, int bitDepth = 0);   // bitDepth：16 / 24 / 32（0 = 録音形式）
    /** 納品パック（B15。DESIGN 9）：export_YYYYMMDD/ に各トラックの Dry・確認用ミックス・notes.txt（プロは take_map.txt）と zip */
    void exportPack (const std::vector<project::TrackType>&, int bitDepth = 0, bool refmix = true);
    /** 書き出す前の確認（DESIGN 12「未録音警告」）：お手本の声がある所で、選んだトラックが録っていない所。
        トラックごとに 1 行（「Main：0:48–1:02、1:30–1:41 ほか 2 か所」）。無ければ空。お手本が無ければ確かめようがないので空 */
    juce::StringArray unrecordedSummary (const std::vector<project::TrackType>&) const;

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
    /** リハーサルのテイクを本番のテイクの所へ移す（ファイル・番号・採用区間の参照）。新しい番号、移せなければ空（B18c） */
    juce::String moveRehearsalToTakes (project::Track&, const juce::String& takeId);
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
    static juce::String cacheKey (const juce::File& guide, const juce::String& modelId);
    enum class CopyTarget { song, guide };
    /** 曲・お手本をプロジェクトの中へコピーする（裏で）。終わるまでは元のファイルを指し、終わったら relativePath にする。
        失敗したら知らせて、元のファイルを指したままにする */
    void copyIntoProject (const juce::File& source, const juce::String& relativePath, CopyTarget);
    int64 prerollSamples() const;
    void startWithCountIn (bool punch);   // 止まった所から録る時の再生の頭（カウントイン。2026-10-02）
    void syncClickToEngine();
    void pollMonitorLevels();
    double sentClickSpb = -1.0;            // エンジンに渡したクリックの拍の並び（変わった時だけ渡し直す）
    int sentClickPerBar = 0;
    int64 sentClickDownbeat = 0;
    int countInWarnedSong = -1;            // 「テンポが分からないので数えません」を出した曲（songSerial。曲ごとに 1 度）
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
    std::unique_ptr<models::ModelEntry> modelEntry;              // 署名を確かめた一覧の中の分離モデル
    std::unique_ptr<models::ModelEntry> karaokeEntry;            // 同じ一覧のリードボーカルのモデル（ハモリのお手本。2026-10-02）
    std::unique_ptr<models::ModelEntry> pitchEntry;              // 同じ一覧の音程モデル（RMVPE。分離と続けて入れる。2026-10-02）
    int downloadGeneration = 0;   // ダウンロードを始める・キャンセルするたびに進める（前の受け取りの遅れた知らせを使わない）
    std::vector<std::pair<models::ModelEntry, juce::File>> downloadQueue;   // まとめて入れる物（分離 → リード → 音程）
    std::vector<std::pair<models::ModelEntry, juce::File>> modelsToDownload() const;
    void startQueuedModel (size_t index, juce::int64 offset, juce::int64 total);
    void startQueuedModelWhenFree (size_t index, juce::int64 offset, juce::int64 total);
    juce::File separationCacheFolder() const;
    bool separationCached() const;
    juce::File leadCacheFolder() const;
    void extractLead();
    void analyseLead (const juce::File& leadFile);
    std::unique_ptr<separation::Separator> separator;   // B16
    // 分離の 3 つの流れに共通の部分（UiSession.cpp。2026-10-04）
    enum class SeparationPrep { ready, failed, stopped };
    void beginSeparation (int kind);
    /** source を 44.1 kHz ステレオの mix に書き（バックグラウンド）、メッセージスレッドで next を呼ぶ（アプリを閉じていれば mix を削除するだけ） */
    void prepareSeparationInput (const juce::File& source, const juce::File& mix, std::function<void (SeparationPrep)> next);
    /** 分離プロセスを始める。終わったら mix と scratch を削除してから done（アプリを閉じていれば削除だけ） */
    bool startSeparator (const juce::File& mix, const juce::File& outA, const juce::File& outB, const juce::File& lead,
                         const juce::File& model, std::vector<juce::File> scratch, std::function<void (bool ok, const juce::String& error)> done);
    static juce::String separationError (const juce::String& error);
    std::unique_ptr<separation::Service> separationService = std::make_unique<separation::Service>();   // 分離が使えるか・分離を作る（テストは偽物）
    int separationGeneration = 0;   // stopSeparation で進める（準備中・引き算中に止めたら、次の段階へ進まない）
    void analyseSeparated (const juce::File& vocals, const juce::File& backing);
    std::shared_ptr<bool> alive = std::make_shared<bool> (true);   // 裏のスレッドから戻ってきた時に、まだ生きているか
    bool loopBeforeRecording = false;
    juce::uint32 tailWaitStart = 0;
    juce::uint32 latencyStartMs = 0;  // 測定音を鳴らし始めた時刻
    bool analysingLatency = false;    // 録り終えた測定音を裏で解析している

    // 遡及録音（B7）：再生中、アームしたトラックがあれば裏で録っている（REC でテイクになる。押さずに止めたら消す）
    /** お手本の線と声を d サンプル（時間軸）ずらす（手直しの分。データだけ。判定・エンジンは呼ぶ側） */
    void shiftGuideData (int64 d, bool withOriginal = true);
    void setGuideCovered (const std::vector<std::pair<int64, int64>>&, double songRate);
    /** 解析し直した直後のお手本に、保存してある手直しを当てる。当てたら true */
    bool applyGuideNudge();
    bool shadowActive = false;
    int recoveredTakes = 0;                // 開いた時に Audio/Recovered へ移したテイク（続きから開いた知らせの後に知らせる）
    juce::uint32 lastNoSeekNotice = 0;     // 録音中のシークの知らせ（ドラッグで出続けないように）
    bool discarding = false;     // discardRecording の間だけ：止めたテイクを捨てる
    juce::File shadowFile;
    int shadowSerial = 0;
    std::vector<audio::PitchFrame> pitchFrames;   // 取り出し用（毎回確保しない）
    // お手本の声・声域（2026-10-02）
    void syncBackingLevel();
    void syncGuideGain();
    bool anyMonitorSolo() const { return s.guideSolo || s.guideHarmSolo || s.backingSolo || s.selfSolo; }
    void finishUpdateCheck (const update::CheckResult&, bool userAsked);
    update::Checker updateChecker;   // 新しいバージョンの確認（裏のスレッド。消える時に通信を切る）
    void syncGuideToEngine();
    juce::uint32 guideGeneration = 0;
    bool rangeMeasuring = false;
    bool gameVoice = false;
    float gameMidi = 0.0f;
    double gameMidiMs = 0.0;
    void noteGameFrame (const audio::PitchFrame&);
    std::vector<float> rangeNotes;
    mutable juce::int64 songRangeKey = -1;
    mutable analysis::SongRange cachedSongRange;

    // 保存（B14）
    bool dirty = false, restoring = false;
    juce::uint32 dirtySince = 0, lastBackupMs = 0;
    project::RecMode recordingMode = project::RecMode::delivery;   // いまのテイクを録り始めた時の本番 / リハーサル
    juce::String songHash;          // 開いている曲の音の中身のハッシュ（.vbooth に保存し、同じ名前・長さの別の曲と見分ける）
    int guideSerial = 0;            // お手本を読み込むたびに増える（前のお手本のリード分離の結果を使わない）
    bool saveFailed = false;        // 前の保存が書けなかった（試し直している。知らせは 1 回だけ）
    juce::uint32 saveRetryAt = 0;   // 書けなかった時、次に試す時刻（getMillisecondCounter）
    bool awaitingRestore = false;
    bool timelineLocked = false;    // 開いたプロジェクトにテイクがある：時間軸の SR を保存した時のまま（機器の SR に合わせない）   // 続きから開くプロジェクトの中身をまだ戻していない（戻すまで保存しない）
    int64 savedPlayhead = -1;   // 最後に保存した時の再生位置（曲を替える・終わる時に動いていれば保存する）
    std::unique_ptr<project::LoadedProject> pendingProject;   // 開く途中のプロジェクト（伴奏の SR をそろえてから戻す）
    juce::File pendingProjectFile;

    // 直前のテイクを採用する前の採用区間（Ctrl / ⌘+Z で戻す。B10）
    std::vector<project::CompSegment> compBeforeTake;
    std::vector<project::CompSegment> compAfterTake;   // 採用した直後の採用区間（違っていたら、その後に変えている：戻さない）
    project::TrackType undoTrack = project::TrackType::main;   // どのトラックの採用区間を戻すか
    bool undoIsCompare = false;     // 戻すのがテイク比較の選び直し（知らせの言葉を変える。B18c）

    // テイク比較（B18c）：試聴中の採用区間の差し替えと、比べる間だけ変えた範囲・ループ（終わったら戻す）
    project::TakeAudition audition;
    int64 compareRangeIn = 0, compareRangeOut = 0;
    bool compareLoopOn = false, compareChangedRange = false, compareStartedPlay = false;
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
/** Ctrl / ⌘ と 1 文字（Mac は「⌘E」、ほかは「Ctrl+E」）。ツールチップのショートカット（#28） */
juce::String commandKeyName (char key);

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
