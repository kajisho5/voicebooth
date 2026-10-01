#include "audio/SongLoader.h"

namespace vb::audio
{
namespace
{
    bool writeFile (juce::AudioFormat& format, const juce::File& f, const juce::AudioBuffer<float>& buffer,
                    double sampleRate, int bits)
    {
        f.deleteFile();
        std::unique_ptr<juce::OutputStream> out = std::make_unique<juce::FileOutputStream> (f);
        auto writer = format.createWriterFor (out, juce::AudioFormatWriterOptions{}
                                                       .withSampleRate (sampleRate)
                                                       .withNumChannels (buffer.getNumChannels())
                                                       .withBitsPerSample (bits));
        return writer != nullptr && writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
    }

    juce::AudioBuffer<float> makeSine (int channels, int samples, double sampleRate, float gain)
    {
        juce::AudioBuffer<float> b (channels, samples);
        for (int c = 0; c < channels; ++c)
            for (int i = 0; i < samples; ++i)
                b.setSample (c, i, gain * (float) std::sin (juce::MathConstants<double>::twoPi * 440.0 * i / sampleRate));
        return b;
    }

    bool waitFor (const bool& flag, int ms)
    {
        for (int t = 0; t < ms && ! flag; t += 10)
            juce::MessageManager::getInstance()->runDispatchLoopUntil (10);
        return flag;
    }
}

class SongLoaderTests : public juce::UnitTest
{
public:
    SongLoaderTests() : juce::UnitTest ("SongLoader", "VoiceBooth") {}

    void runTest() override
    {
        // 日本語と空白を含むパスでも開けること（DESIGN 13）
        const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                             .getChildFile ("VoiceBoothTests").getNonexistentSibling();
        dir.createDirectory();

        juce::AudioFormatManager formats;
        formats.registerBasicFormats();

        beginTest ("WAV: exact length / rate / channels / bits, peak position");
        {
            constexpr double sr = 44100.0;
            constexpr int len = 44100 * 2 + 123;   // ブロック境界・ビン境界に揃わない長さ
            auto buf = makeSine (2, len, sr, 0.1f);
            buf.setSample (1, 50000, 0.5f);        // 右だけに山

            const auto f = dir.getChildFile (juce::String::fromUTF8 ("\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88 song.wav"));   // 「テスト song.wav」
            expect (writeFile (*formats.findFormatForFileExtension ("wav"), f, buf, sr, 24));

            const auto r = loadSong (f, formats);
            expect (r.ok(), errorKey (r.error));
            expectEquals (r.info.lengthSamples, (int64) len);
            expectEquals (r.info.sampleRate, sr);
            expectEquals (r.info.numChannels, 2);
            expectEquals (r.info.bitsPerSample, 24);
            expectEquals (r.info.extension(), juce::String ("wav"));
            expect (r.overview != nullptr && r.overview->isComplete());
            expectWithinAbsoluteError (r.overview->getPeak (50000, 50001).max, 0.5f, 1.0e-4f);
            expectWithinAbsoluteError (r.overview->getPeak (0, 40000).max, 0.1f, 1.0e-3f);
            expectWithinAbsoluteError (r.overview->getOverallMagnitude(), 0.5f, 1.0e-4f);
        }

        beginTest ("FLAC: sample-exact length");
        {
            constexpr double sr = 48000.0;
            constexpr int len = 48000 * 3 + 7;
            const auto f = dir.getChildFile ("mono.flac");
            expect (writeFile (*formats.findFormatForFileExtension ("flac"), f, makeSine (1, len, sr, 0.3f), sr, 16));

            const auto r = loadSong (f, formats);
            expect (r.ok(), errorKey (r.error));
            expectEquals (r.info.lengthSamples, (int64) len);
            expectEquals (r.info.numChannels, 1);
            expectWithinAbsoluteError (r.overview->getOverallMagnitude(), 0.3f, 1.0e-3f);
        }

        beginTest ("errors: missing / not audio / empty");
        {
            expect (loadSong (dir.getChildFile ("none.wav"), formats).error == LoadResult::Error::notFound);

            const auto junk = dir.getChildFile ("junk.wav");
            junk.replaceWithText ("this is not audio");
            expect (loadSong (junk, formats).error == LoadResult::Error::unsupported);

            const auto empty = dir.getChildFile ("empty.wav");
            expect (writeFile (*formats.findFormatForFileExtension ("wav"), empty, juce::AudioBuffer<float> (2, 0), 48000.0, 16));
            expect (loadSong (empty, formats).error == LoadResult::Error::empty);
        }

        beginTest ("cancel while loading");
        {
            const auto f = dir.getChildFile ("long.wav");
            expect (writeFile (*formats.findFormatForFileExtension ("wav"), f, makeSine (2, 48000 * 10, 48000.0, 0.2f), 48000.0, 16));

            int calls = 0;
            const auto r = loadSong (f, formats, [&] (float) { return ++calls < 2; });
            expect (r.error == LoadResult::Error::cancelled);
            expect (r.overview == nullptr);
        }

        beginTest ("song extensions");
        {
            expect (hasSongExtension (juce::File::getCurrentWorkingDirectory().getChildFile ("A.WAV")));
            expect (hasSongExtension (juce::File::getCurrentWorkingDirectory().getChildFile ("b.aif")));
            expect (hasSongExtension (juce::File::getCurrentWorkingDirectory().getChildFile ("c.m4a")));
            expect (! hasSongExtension (juce::File::getCurrentWorkingDirectory().getChildFile ("d.txt")));
            expect (songWildcard().contains ("*.flac"));
        }

        beginTest ("SongLoader delivers the result on the message thread");
        {
            const auto f = dir.getChildFile ("long.wav");
            SongLoader loader;
            bool done = false;
            LoadResult got;
            loader.start (f, [&] (LoadResult r) { got = std::move (r); done = true; });
            expect (waitFor (done, 10000));
            expect (got.ok());
            expectEquals (got.info.lengthSamples, (int64) 48000 * 10);
            expect (! loader.isLoading());
        }

        beginTest ("SongLoader destroyed while loading: no callback, no crash");
        {
            bool called = false;
            {
                SongLoader loader;
                loader.start (dir.getChildFile ("long.wav"), [&] (LoadResult) { called = true; });
                loader.start (dir.getChildFile ("long.wav"), [&] (LoadResult) { called = true; });   // 上書き
            }
            waitFor (called, 300);
            expect (! called);
        }

        dir.deleteRecursively();
    }
};

static SongLoaderTests songLoaderTests;
} // namespace vb::audio
