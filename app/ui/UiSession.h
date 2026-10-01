#pragma once

#include "DummySession.h"
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
        takes     = 1 << 20,  // テイク・採用区間・テイクの波形が変わった（B5）
        notice    = 1 << 21,  // 知らせ（トースト）を出す（noticeText / noticeSerial）
        recordFormat = 1 << 22,  // 録音形式（SR・ビット数）が変わった
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
    void setOctaveUp (bool);
    void setFullRange (bool);

    // --- トラック -----------------------------------------------------------
    void selectTrack (int index);
    void armTrack (int index);          // アームは同時に 1 本
    void setMute (int index, bool);
    void setSolo (int index, bool);

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

    // --- 録音・書き出し（B5） -------------------------------------------------
    /** 録音を始められない理由の翻訳キー（空なら録れる）。曲・入力・アーム・SR を見る */
    juce::String recordProblem() const;

    /** トラックの採用区間をフル尺の WAV に書き出す（裏のスレッドで。終わったら知らせる）。
        書き出し先は曲のプロジェクトフォルダの export_YYYYMMDD/。曲が無ければ何もしない */
    void exportTracks (const std::vector<project::TrackType>&);

    /** 曲ごとの作業フォルダ（テイク・書き出し）。.vbooth の保存（B14）までの仮の置き場：
        書類フォルダ/VoiceBooth/Projects/{曲名}/ */
    static juce::File projectFolderFor (const juce::String& songName);

    // --- 練習 / モード ------------------------------------------------------
    void setTempo (int percent);
    void setKey (int semitones);
    void setRecMode (project::RecMode);
    void setMode (project::Mode);
    void setPitchTolerance (float cents);

    /** 納品 REC 中か（テンポ / キーがロックされる） */
    bool deliveryLocked() const { return s.isRecording && s.recMode == project::RecMode::delivery; }

    /** そのモードで見せるトラックか（DESIGN 2） */
    bool isTrackVisible (project::TrackType) const;

private:
    void notify (juce::uint32 changes);
    void keepPlayheadInView();
    void followPlayhead (double seconds);
    void syncLoopToEngine();
    void refreshOutputStatus();
    void checkSpeakerOutput();
    void pushMonitorToEngine();
    void finishRecording();
    void conformSong();
    bool hasTakes() const;
    void loadTakeWave (project::TrackType, const project::Take&);
    void postNotice (const juce::String& text);
    void refreshInputStatus();
    void deviceChanged (bool lost);
    juce::String afterDeviceSelect (juce::String error);

    dummy::Session s;
    audio::AudioEngine* engine = nullptr;
    double sinceStatus = 0.0;
    std::shared_ptr<bool> alive = std::make_shared<bool> (true);   // 裏のスレッドから戻ってきた時に、まだ生きているか
    bool loopBeforeRecording = false;
    juce::ListenerList<Listener> listeners;
};

//==============================================================================
/** 入力チャンネルの表示（2 ch は L / R、それ以上は番号。1 ch は空） */
juce::String inputChannelLabel (int channel, int numChannels);

/** 入力デバイスの表示名（機器名 — チャンネル）。UI_MOCK はダミー、入力が無ければ「入力なし」 */
juce::String inputDisplayName (const dummy::Session&);

/** 入力が使えない理由（短い文。ステータスバー用）。使えていれば空 */
juce::String inputProblemShort (const dummy::Session&);

/** 表示するレイテンシ。実デバイスならデバイスの申告値（実測は B6）、UI_MOCK はダミー */
struct LatencyDisplay
{
    bool known = false;       // 入力が無いと出せない
    bool reported = false;    // デバイスの申告値（実測ではない）
    bool estimated = false;   // 申告が無く、バッファから推定
    int64 samples = 0;
    double ms = 0.0;
};
LatencyDisplay latencyDisplay (const dummy::Session&);

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
