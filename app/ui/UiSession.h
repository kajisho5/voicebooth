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
        device    = 1 << 9,   // 出力デバイスの状態
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

    // --- モニター（B2：オフボだけ音が出る） ---------------------------------
    void setBackingLevel (float fader);   // 0..1（0.75 = 0 dB）
    void setBackingMuted (bool);

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
    void syncLoopToEngine();
    void refreshOutputStatus();

    dummy::Session s;
    audio::AudioEngine* engine = nullptr;
    double sinceStatus = 0.0;
    juce::ListenerList<Listener> listeners;
};

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
