#include "PlaybackEngine.h"

namespace vb::audio
{
PlaybackEngine::PlaybackEngine() = default;

PlaybackEngine::~PlaybackEngine()
{
    manager.removeAudioCallback (this);
    manager.closeAudioDevice();
}

juce::String PlaybackEngine::openDefaultOutput()
{
    openError = manager.initialiseWithDefaultDevices (0, 2);   // 入力 0 ch・出力 2 ch
    if (openError.isEmpty() && manager.getCurrentAudioDevice() == nullptr)
        openError = "no output device";
    manager.addAudioCallback (this);
    return openError;
}

void PlaybackEngine::setSong (std::shared_ptr<const SongAudio> song)
{
    songRate = song != nullptr ? song->sampleRate : 0.0;
    core.setSong (std::move (song));
    if (songRate > 0.0)
        matchDeviceRateToSong (songRate);
}

void PlaybackEngine::matchDeviceRateToSong (double rate)
{
    auto* device = manager.getCurrentAudioDevice();
    if (device == nullptr || std::abs (device->getCurrentSampleRate() - rate) < 0.5)
        return;

    // DESIGN 13：原則リサンプルしない。デバイスが対応していれば曲の SR に切り替える
    if (! device->getAvailableSampleRates().contains (rate))
        return;

    auto setup = manager.getAudioDeviceSetup();
    setup.sampleRate = rate;
    manager.setAudioDeviceSetup (setup, true);
}

void PlaybackEngine::setBackingLevel (float fader, bool muted)
{
    core.setGain (PlaybackCore::faderToGain (fader));
    core.setMuted (muted);
}

OutputStatus PlaybackEngine::getOutputStatus() const
{
    OutputStatus st;
    st.error = openError;
    if (auto* device = manager.getCurrentAudioDevice())
    {
        st.open = true;
        st.deviceName = device->getName();
        st.typeName = device->getTypeName();
        st.sampleRate = device->getCurrentSampleRate();
        st.bufferSize = device->getCurrentBufferSizeSamples();
        st.converting = songRate > 0.0 && std::abs (st.sampleRate - songRate) >= 0.5;
    }
    return st;
}

void PlaybackEngine::audioDeviceIOCallbackWithContext (const float* const*, int, float* const* outputs, int numOutputs,
                                                       int numSamples, const juce::AudioIODeviceCallbackContext&)
{
    core.render (outputs, numOutputs, numSamples);
}

void PlaybackEngine::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    core.prepare (device->getCurrentSampleRate());
}

void PlaybackEngine::audioDeviceStopped()
{
}
} // namespace vb::audio
