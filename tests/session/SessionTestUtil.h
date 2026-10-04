#pragma once

#include "ui/UiSession.h"
#include "project/ProjectFile.h"
#include "audio/SongLoader.h"

/*  UiSession のテストの共通の道具：正弦波の WAV を書く・知らせを受ける・起動画面と同じ手順で曲や .vbooth を開く */

namespace vb::test
{
/** 正弦波の WAV（48 kHz・24bit。既定はステレオ） */
inline juce::File writeTone (const juce::File& f, double hz, double seconds, int channels = 2)
{
    f.getParentDirectory().createDirectory();
    f.deleteFile();
    constexpr double rate = 48000.0;
    const auto n = (int) (seconds * rate);
    juce::AudioBuffer<float> b (channels, n);
    for (int i = 0; i < n; ++i)
    {
        const auto v = 0.3f * (float) std::sin (juce::MathConstants<double>::twoPi * hz * i / rate);
        for (int c = 0; c < channels; ++c)
            b.setSample (c, i, v);
    }
    std::unique_ptr<juce::OutputStream> out (f.createOutputStream().release());
    juce::WavAudioFormat wav;
    if (auto w = wav.createWriterFor (out, juce::AudioFormatWriterOptions{}.withSampleRate (rate).withNumChannels (channels).withBitsPerSample (24)))
        w->writeFromAudioSampleBuffer (b, 0, n);
    return f;
}

/** バックグラウンドの作業（曲のコピー・解析・分離の準備）の知らせを受ける */
inline void pump (int ms)
{
    const auto until = juce::Time::getMillisecondCounter() + (juce::uint32) ms;
    while (juce::Time::getMillisecondCounter() < until)
        juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
}

/** 条件がそろうまで知らせを受ける（最大 ms）。そろえば true */
template <typename Pred>
bool pumpUntil (Pred&& done, int ms)
{
    const auto until = juce::Time::getMillisecondCounter() + (juce::uint32) ms;
    while (! done())
    {
        if (juce::Time::getMillisecondCounter() >= until)
            return false;
        juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
    }
    return true;
}

/** 起動画面で曲を開くのと同じ（読み終わったら loadSong） */
inline bool openSong (UiSession& ui, const juce::File& song)
{
    juce::AudioFormatManager formats;
    audio::registerSongFormats (formats);
    auto r = audio::loadSong (song, formats);
    if (! r.ok())
        return false;
    ui.loadSong (r.info.file, juce::roundToInt (r.info.sampleRate), r.info.lengthSamples, r.overview, r.audio);
    pump (600);
    return true;
}

/** 起動画面で .vbooth を開くのと同じ（StartScreen::openProject → 曲のコピーを読む） */
inline bool reopenProject (UiSession& ui, const juce::File& vboothFile)
{
    const auto l = project::fromJson (vboothFile.loadFileAsString());
    if (! l.ok)
        return false;
    const auto song = project::findMedia (vboothFile.getParentDirectory(), l.project.songPath, "Audio");
    if (! song.existsAsFile())
        return false;
    ui.setPendingProject (vboothFile, l);
    return openSong (ui, song);
}
} // namespace vb::test
