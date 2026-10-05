#include "SongLoader.h"
#include "MediaFoundationFormat.h"
#include "Mp3Format.h"
#include <limits>

namespace vb::audio
{
void downmixToStereo (const juce::AudioBuffer<float>& in, juce::AudioBuffer<float>& out)
{
    const auto n = in.getNumSamples(), channels = in.getNumChannels();
    out.setSize (2, n, false, false, true);
    out.clear();
    if (channels == 6)
    {
        // 5.1ch（L R C LFE Ls Rs）：ITU-R BS.775 のまとめ方（LFE は入れない）。重なっても割れないよう全体を下げる
        constexpr float k = 0.70710678f, scale = 1.0f / (1.0f + 2.0f * k);
        for (int side = 0; side < 2; ++side)
        {
            out.addFrom (side, 0, in, side, 0, n, scale);
            out.addFrom (side, 0, in, 2, 0, n, k * scale);
            out.addFrom (side, 0, in, 4 + side, 0, n, k * scale);
        }
        return;
    }
    // そのほか：偶数番目を左、奇数番目を右に平均（並びが分からないので、どのチャンネルも落とさない）
    int counts[2] = { 0, 0 };
    for (int c = 0; c < channels; ++c)
    {
        out.addFrom (c % 2, 0, in, c, 0, n);
        ++counts[c % 2];
    }
    for (int side = 0; side < 2; ++side)
        if (counts[side] > 1)
            out.applyGain (side, 0, n, 1.0f / (float) counts[side]);
    if (counts[1] == 0)
        out.copyFrom (1, 0, out, 0, 0, n);
}

const char* errorKey (LoadResult::Error e)
{
    switch (e)
    {
        case LoadResult::Error::none:        return "";
        case LoadResult::Error::notFound:    return "load.error.notFound";
        case LoadResult::Error::unsupported: return "load.error.unsupported";
        case LoadResult::Error::empty:       return "load.error.empty";
        case LoadResult::Error::readFailed:  return "load.error.readFailed";
        case LoadResult::Error::cancelled:   return "load.error.cancelled";
        case LoadResult::Error::tooLong:     return "load.error.tooLong";
        case LoadResult::Error::outOfMemory: return "load.error.outOfMemory";
    }
    return "";
}

const juce::StringArray& songExtensions()
{
    static const juce::StringArray exts { "wav", "flac", "aiff", "aif", "mp3", "m4a", "ogg" };
    return exts;
}

bool hasSongExtension (const juce::File& f)
{
    return songExtensions().contains (f.getFileExtension().trimCharactersAtStart ("."), true);
}

juce::String songWildcard()
{
    juce::StringArray w;
    for (auto& e : songExtensions())
        w.add ("*." + e);
    return w.joinIntoString (";");
}

void registerSongFormats (juce::AudioFormatManager& formats)
{
    formats.registerFormat (new Mp3AudioFormat(), false);
   #if JUCE_WINDOWS
    formats.registerFormat (new MediaFoundationAudioFormat(), false);
   #endif
    formats.registerBasicFormats();
}

juce::int64 estimatedMemoryBytes (juce::int64 lengthSamples, int numChannels)
{
    return juce::jmax ((juce::int64) 0, lengthSamples) * juce::jmax (1, numChannels) * (juce::int64) sizeof (float) * 5;
}

bool memoryTight (juce::int64 estimatedBytes, juce::int64 memoryMB)
{
    return memoryMB > 0 && (double) estimatedBytes > (double) memoryMB * 1024.0 * 1024.0 * 0.6;
}

LoadResult loadSong (const juce::File& file, juce::AudioFormatManager& formats,
                     const std::function<bool (float)>& progress)
{
    LoadResult r;
    r.info.file = file;

    if (! file.existsAsFile())
    {
        r.error = LoadResult::Error::notFound;
        return r;
    }

    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr)
    {
        r.error = LoadResult::Error::unsupported;
        return r;
    }

    auto& info = r.info;
    info.formatName    = reader->getFormatName();
    info.sampleRate    = reader->sampleRate;
    info.numChannels   = (int) reader->numChannels;
    info.bitsPerSample = (int) reader->bitsPerSample;
    info.floatingPoint = reader->usesFloatingPointData;
    info.lengthSamples = reader->lengthInSamples;

    if (info.lengthSamples <= 0 || info.sampleRate <= 0.0 || info.numChannels <= 0)
    {
        r.error = LoadResult::Error::empty;
        return r;
    }

    // 長さは時間だけでなくサンプル数でも見る。バッファは int のサンプル数なので、極端な SR を書いたファイル
    // （時間は短いのにサンプル数が int を超える）を通すと、確保した外へ読み込んでしまう（監査 2026-10-03）
    // 長すぎるとき以外（チャンネルが多すぎる・SR が高すぎる）に「長すぎます」と出していた（バグチェック 2026-10-05）
    if (info.numChannels > 8 || info.sampleRate > maxSampleRate)
    {
        r.error = LoadResult::Error::unsupported;
        return r;
    }
    if ((double) info.lengthSamples / info.sampleRate > maxSongMinutes * 60.0 || info.lengthSamples > (int64) std::numeric_limits<int>::max())
    {
        r.error = LoadResult::Error::tooLong;
        return r;
    }

    auto overview = std::make_shared<WaveformOverview> (info.lengthSamples);
    auto audio = std::make_shared<SongAudio>();
    audio->sampleRate = info.sampleRate;
    try
    {
        audio->buffer.setSize (info.numChannels, (int) info.lengthSamples);
    }
    catch (const std::bad_alloc&)
    {
        r.error = LoadResult::Error::outOfMemory;
        return r;
    }

    // デコードした音をそのまま曲のバッファへ。概形も同じ通過で作る
    constexpr int blockSize = 1 << 16;
    std::vector<float*> dest ((size_t) info.numChannels);

    for (int64 pos = 0; pos < info.lengthSamples;)
    {
        const auto n = (int) juce::jmin ((int64) blockSize, info.lengthSamples - pos);
        for (int c = 0; c < info.numChannels; ++c)
            dest[(size_t) c] = audio->buffer.getWritePointer (c, (int) pos);

        if (! reader->read (dest.data(), info.numChannels, pos, n))
        {
            r.error = LoadResult::Error::readFailed;
            return r;
        }

        overview->append (dest.data(), info.numChannels, n);
        pos += n;

        if (progress && ! progress ((float) ((double) pos / (double) info.lengthSamples)))
        {
            r.error = LoadResult::Error::cancelled;
            return r;
        }
    }

    // 3 チャンネル以上はステレオにまとめる（鳴らすのは 2ch までなので、5.1ch ではセンターの歌が鳴らず、
    // 解析は全チャンネルの平均で、聞こえる音と食い違っていた。バグチェック 2026-10-05）
    if (info.numChannels > 2)
    {
        auto stereo = std::make_shared<SongAudio>();
        stereo->sampleRate = audio->sampleRate;
        try { stereo->buffer.setSize (2, audio->buffer.getNumSamples()); }
        catch (const std::bad_alloc&) { r.error = LoadResult::Error::outOfMemory; return r; }
        downmixToStereo (audio->buffer, stereo->buffer);
        audio = std::move (stereo);
    }

    r.overview = std::move (overview);
    r.audio = std::move (audio);
    return r;
}

//==============================================================================
SongLoader::SongLoader() : juce::Thread ("VoiceBooth song loader")
{
    registerSongFormats (formats);
}

SongLoader::~SongLoader()
{
    cancel();
}

void SongLoader::start (const juce::File& f, std::function<void (LoadResult)> onDone)
{
    cancel();

    file = f;
    callback = std::move (onDone);
    progress = 0.0f;
    startThread (juce::Thread::Priority::low);
}

void SongLoader::cancel()
{
    signalThreadShouldExit();
    stopThread (4000);
    cancelPendingUpdate();
}

void SongLoader::run()
{
    auto r = loadSong (file, formats, [this] (float p)
    {
        progress = p;
        return ! threadShouldExit();
    });

    if (threadShouldExit())
        return;

    {
        const juce::ScopedLock sl (lock);
        result = std::move (r);
    }
    triggerAsyncUpdate();
}

void SongLoader::handleAsyncUpdate()
{
    LoadResult r;
    {
        const juce::ScopedLock sl (lock);
        r = std::move (result);
        result = {};
    }

    if (callback)
        callback (std::move (r));
}
} // namespace vb::audio
