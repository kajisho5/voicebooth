#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include "AudioEngine.h"
#include "PlaybackCore.h"

/*  出力デバイスを開いて PlaybackCore を鳴らす（Phase B2）
    - 入力は開かない（0 ch）。マイクの許可も求めない（B3 / B4 で）
    - 曲を開いたら、出力デバイスの SR を曲に合わせる。合わせられなければ試聴用に変換して鳴らす（DESIGN 13）
    - UI_MOCK ビルドでは作らない */

namespace vb::audio
{
class PlaybackEngine final : public AudioEngine,
                             private juce::AudioIODeviceCallback
{
public:
    PlaybackEngine();
    ~PlaybackEngine() override;

    /** 既定の出力デバイスを開く。失敗したら理由（空なら成功） */
    juce::String openDefaultOutput();

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

    juce::AudioDeviceManager& deviceManager() { return manager; }

private:
    void audioDeviceIOCallbackWithContext (const float* const* inputs, int numInputs,
                                           float* const* outputs, int numOutputs, int numSamples,
                                           const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart (juce::AudioIODevice*) override;
    void audioDeviceStopped() override;

    void matchDeviceRateToSong (double songRate);

    juce::AudioDeviceManager manager;
    PlaybackCore core;
    juce::String openError;
    double songRate = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlaybackEngine)
};
} // namespace vb::audio
