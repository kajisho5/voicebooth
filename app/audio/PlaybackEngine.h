#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include "AudioEngine.h"
#include "PlaybackCore.h"
#include "InputMeter.h"
#include "DeviceRules.h"

/*  デバイスを開いて PlaybackCore を鳴らし、入力をメーターに通す（Phase B2 / B3）
    - 入力は 1 ch（モノラル、既定 L）。メーターに使うだけで、出力には流さない（モニターは B4、録音は B5）
    - Mac はマイクの許可を先に確かめる。許可が無ければ出力だけ開く（DESIGN 13）
    - 入力が開けなくても出力だけで開き直す（オフボの再生は止めない）
    - 曲を開いたら、デバイスの SR を曲に合わせる。合わせられなければ試聴用に変換して鳴らす（DESIGN 13）
    - デバイスが外から変わったら（抜けた等）再生を止めて知らせる（DESIGN 13「デバイス抜け：停止して再選択」）
    - UI_MOCK ビルドでは作らない */

namespace vb::audio
{
class PlaybackEngine final : public AudioEngine,
                             private juce::AudioIODeviceCallback,
                             private juce::ChangeListener,
                             private juce::Timer
{
public:
    PlaybackEngine();
    ~PlaybackEngine() override;

    /** デバイスを開く。saved は前回の設定（createDeviceStateXml。nullptr なら既定）。
        戻せなければ既定のデバイスで開く。出力も開けなければ理由を返す（空なら成功） */
    juce::String openDevices (const juce::XmlElement* saved);

    /** いまの設定（アプリ設定に保存する） */
    std::unique_ptr<juce::XmlElement> createDeviceStateXml() const { return manager.createStateXml(); }

    void setSong (std::shared_ptr<const SongAudio>) override;
    bool hasSong() const override { return core.hasSong(); }

    void  play() override                         { core.play(); }
    void  stop() override                         { core.stop(); }
    bool  isPlaying() const override              { return core.isPlaying(); }
    void  seek (int64 s) override                 { core.seek (s); }
    int64 getPlayheadSample() const override      { return core.getPosition(); }
    bool  consumeReachedEnd() override            { return core.consumeReachedEnd(); }
    void  setLoop (int64 a, int64 b, bool on) override { core.setLoop (a, b, on); }

    void setBackingLevel (float fader, bool muted) override;
    OutputStatus getOutputStatus() const override;

    DeviceList getDeviceList() const override;
    void rescanDevices() override;
    juce::String setDeviceType (const juce::String&) override;
    juce::String setInputDevice (const juce::String&) override;
    juce::String setOutputDevice (const juce::String&) override;
    juce::String setInputChannel (int) override;
    juce::String setBufferSize (int) override;
    void setDeviceChangeCallback (std::function<void (bool)> cb) override { onDeviceChange = std::move (cb); }

    InputStatus getInputStatus() const override;
    InputLevel getInputLevel() const override { return meter.read(); }
    void resetInputClip() override            { meter.resetClip(); }

private:
    void audioDeviceIOCallbackWithContext (const float* const* inputs, int numInputs,
                                           float* const* outputs, int numOutputs, int numSamples,
                                           const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart (juce::AudioIODevice*) override;
    void audioDeviceStopped() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    /** 音の処理が止まっていないか見張る（抜けても知らせないドライバがあるため。2 Hz） */
    void timerCallback() override;

    bool inputAllowed() const { return permission == MicPermission::notNeeded || permission == MicPermission::granted; }

    /** 入力を 1 ch だけ開いた状態にそろえる（無ければ既定の入力・L）。失敗したら出力だけに戻す */
    void ensureMonoInput();

    /** 設定を変える。失敗したら元の設定に戻して理由を返す */
    juce::String applySetup (juce::AudioDeviceManager::AudioDeviceSetup);

    /** 閉じて、いまの設定（だめなら既定）で開き直す */
    void reopen();

    void matchDeviceRateToSong (double songRate);
    DeviceSnapshot takeSnapshot() const;
    bool hasSeparateInputsAndOutputs() const;

    juce::AudioDeviceManager manager;
    PlaybackCore core;
    InputMeter meter;
    juce::String openError, inputError;
    double songRate = 0.0;
    int wantedChannel = 0;                                  // 選んだ入力チャンネル（0 = L）
    MicPermission permission = MicPermission::notNeeded;
    DeviceSnapshot lastSnapshot;                            // 自分で変えた後の状態（外からの変化と区別する）
    std::atomic<juce::int64> callbacks { 0 };               // オーディオスレッドが数える（止まったら増えない）
    juce::int64 lastCallbacks = 0;
    juce::uint32 lastProgressMs = 0;
    bool stalled = false;
    std::function<void (bool)> onDeviceChange;

    JUCE_DECLARE_WEAK_REFERENCEABLE (PlaybackEngine)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlaybackEngine)
};
} // namespace vb::audio
