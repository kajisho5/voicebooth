#include "SongLoader.h"
#include "MediaFoundationFormat.h"

namespace vb::audio
{
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

    auto overview = std::make_shared<WaveformOverview> (info.lengthSamples);

    constexpr int blockSize = 1 << 16;
    juce::AudioBuffer<float> buffer (info.numChannels, blockSize);

    for (int64 pos = 0; pos < info.lengthSamples;)
    {
        const auto n = (int) juce::jmin ((int64) blockSize, info.lengthSamples - pos);
        if (! reader->read (buffer.getArrayOfWritePointers(), info.numChannels, pos, n))
        {
            r.error = LoadResult::Error::readFailed;
            return r;
        }

        overview->append (buffer.getArrayOfReadPointers(), info.numChannels, n);
        pos += n;

        if (progress && ! progress ((float) ((double) pos / (double) info.lengthSamples)))
        {
            r.error = LoadResult::Error::cancelled;
            return r;
        }
    }

    r.overview = std::move (overview);
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
