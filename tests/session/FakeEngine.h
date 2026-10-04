#pragma once

#include "audio/AudioEngine.h"
#include "audio/SongLoader.h"

/*  UiSession のテスト用の偽のエンジン（音声の機器なし）。曲を持ち、再生位置を覚えるだけ。
    出力はいつも「開いている・曲の SR のまま鳴らせる」 */

namespace vb::test
{
class FakeEngine final : public audio::AudioEngine
{
public:
    void setSong (std::shared_ptr<const audio::SongAudio> a) override
    {
        song = std::move (a);
        playhead = 0;
        playing = false;
    }
    bool hasSong() const override { return song != nullptr; }

    void  play() override                          { playing = true; }
    void  stop() override                          { playing = false; }
    bool  isPlaying() const override               { return playing; }
    void  seek (audio::int64 sample) override      { playhead = sample; }
    audio::int64 getPlayheadSample() const override { return playhead; }
    bool  consumeReachedEnd() override             { return false; }
    void  setLoop (audio::int64, audio::int64, bool) override {}

    void setBackingLevel (float, bool) override {}

    audio::OutputStatus getOutputStatus() const override
    {
        audio::OutputStatus o;
        o.open = true;
        o.deviceName = "Fake";
        o.sampleRate = song != nullptr ? song->sampleRate : 48000.0;
        o.bufferSize = 256;
        return o;
    }

    // 入力はいつも開いている（録音のテスト用。音は無い）
    audio::InputStatus getInputStatus() const override
    {
        audio::InputStatus i;
        i.open = true;
        i.problem = audio::InputProblem::none;
        i.deviceName = "Fake input";
        i.sampleRate = song != nullptr ? song->sampleRate : 48000.0;
        i.bufferSize = 256;
        return i;
    }

    // 録音の偽物：始めた位置を覚え、止めたら（今の位置 − 始めた位置）の長さの無音の WAV を書く
    juce::String startRecording (const juce::File& f, bool, audio::int64) override
    {
        recordFile = f;
        recordStart = playhead;
        recording = true;
        ++recordingsStarted;
        return {};
    }
    audio::RecordedTake stopRecording() override
    {
        audio::RecordedTake r;
        if (! recording)
            return r;
        recording = false;
        r.file = recordFile;
        r.startSample = recordStart;
        r.length = juce::jmax ((audio::int64) 0, playhead - recordStart);
        recordFile.getParentDirectory().createDirectory();
        juce::AudioBuffer<float> b (1, (int) r.length);
        b.clear();
        std::unique_ptr<juce::OutputStream> out (recordFile.createOutputStream().release());
        juce::WavAudioFormat wav;
        if (auto w = wav.createWriterFor (out, juce::AudioFormatWriterOptions{}.withSampleRate (song != nullptr ? song->sampleRate : 48000.0)
                                                                              .withNumChannels (1).withBitsPerSample (24)))
            w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
        return r;
    }
    bool isRecording() const override                    { return recording; }
    audio::int64 recordingStartSample() const override   { return recording ? recordStart : -1; }

    std::shared_ptr<const audio::SongAudio> song;
    audio::int64 playhead = 0;
    bool playing = false;
    bool recording = false;
    audio::int64 recordStart = 0;
    juce::File recordFile;
    int recordingsStarted = 0;
};
} // namespace vb::test
