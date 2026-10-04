#pragma once

#include "separation/Separator.h"
#include <juce_events/juce_events.h>
#include <juce_audio_formats/juce_audio_formats.h>

/*  UiSession のテスト用の偽の分離（別プロセスもモデルも要らない）。始めたことを覚え、テストが finish で終わらせる。
    止めると、本物と同じく "stopped" で終わる（メッセージスレッドへ回してから） */

namespace vb::test
{
struct FakeSeparation
{
    int starts = 0;
    bool running = false;
    juce::File input, vocals, backing, lead, model;
    separation::Callbacks callbacks;
    bool karaoke = false;

    /** 分離を終わらせる。ok なら声と伴奏（44.1 kHz ステレオの小さな正弦波）を書いてから done を呼ぶ */
    void finish (bool ok, const juce::String& error = {})
    {
        if (! running)
            return;
        running = false;
        if (ok)
        {
            writeWav (vocals, 0.1f);
            writeWav (backing, 0.2f);
        }
        if (callbacks.done)
            callbacks.done (ok, error);
    }

    static void writeWav (const juce::File& f, float level)
    {
        if (f == juce::File())
            return;
        f.getParentDirectory().createDirectory();
        f.deleteFile();
        constexpr double rate = 44100.0;
        const int n = (int) (rate * 3.0);
        juce::AudioBuffer<float> b (2, n);
        for (int i = 0; i < n; ++i)
            for (int c = 0; c < 2; ++c)
                b.setSample (c, i, level * (float) std::sin (juce::MathConstants<double>::twoPi * 330.0 * i / rate));
        std::unique_ptr<juce::OutputStream> out (f.createOutputStream().release());
        juce::WavAudioFormat wav;
        if (auto w = wav.createWriterFor (out, juce::AudioFormatWriterOptions{}.withSampleRate (rate).withNumChannels (2).withBitsPerSample (32)
                                                    .withSampleFormat (juce::AudioFormatWriterOptions::SampleFormat::floatingPoint)))
            w->writeFromAudioSampleBuffer (b, 0, n);
    }
};

class FakeSeparator final : public separation::Separator
{
public:
    explicit FakeSeparator (std::shared_ptr<FakeSeparation> s) : st (std::move (s)) {}

    bool start (const juce::File& input, const juce::File& vocals, const juce::File& backing, separation::Callbacks cb,
                const juce::File& lead, const juce::File& model) override
    {
        if (st->running)
            return false;
        st->running = true;
        ++st->starts;
        st->input = input; st->vocals = vocals; st->backing = backing; st->lead = lead; st->model = model;
        st->callbacks = std::move (cb);
        return true;
    }

    void stop() override
    {
        if (! st->running)
            return;
        st->running = false;
        auto done = st->callbacks.done;
        juce::MessageManager::callAsync ([done] { if (done) done (false, "stopped"); });
    }

    bool isBusy() const override { return st->running; }

private:
    std::shared_ptr<FakeSeparation> st;
};

class FakeSeparationService final : public separation::Service
{
public:
    explicit FakeSeparationService (std::shared_ptr<FakeSeparation> s) : st (std::move (s)) {}
    bool available() const override        { return true; }
    bool karaokeInstalled() const override { return st->karaoke; }
    bool executableExists() const override { return true; }
    std::unique_ptr<separation::Separator> create() override { return std::make_unique<FakeSeparator> (st); }

private:
    std::shared_ptr<FakeSeparation> st;
};
} // namespace vb::test
