#include "audio/SongLoader.h"
#include "audio/MediaFoundationFormat.h"

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

    /** OS の読み手でこの環境が mp3 / m4a を読めるはずか（DESIGN 19 の対応表）
        Windows の mp3 は Windows Media（wmvcore.dll）。Server や N エディションでは無いことがある */
    bool expectCompressedSupport (const juce::String& ext)
    {
       #if JUCE_MAC
        juce::ignoreUnused (ext);
        return true;
       #elif JUCE_WINDOWS
        // mp3：Windows Media（wmvcore.dll）、m4a：Media Foundation と AAC デコーダ（msauddecmft.dll）
        juce::DynamicLibrary lib;
        if (ext == "mp3") return lib.open ("wmvcore.dll");
        if (ext == "m4a") return lib.open ("msauddecmft.dll") && juce::DynamicLibrary().open ("mfreadwrite.dll");
        return false;
       #else
        juce::ignoreUnused (ext);
        return false;
       #endif
    }

    /** 最初に |x| > threshold になるサンプル（無ければ -1） */
    int64 firstAbove (juce::AudioFormatReader& reader, float threshold)
    {
        juce::AudioBuffer<float> b ((int) reader.numChannels, (int) reader.lengthInSamples);
        reader.read (&b, 0, (int) reader.lengthInSamples, 0, true, true);
        for (int i = 0; i < b.getNumSamples(); ++i)
            for (int c = 0; c < b.getNumChannels(); ++c)
                if (std::abs (b.getSample (c, i)) > threshold)
                    return i;
        return -1;
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
        registerSongFormats (formats);

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

        beginTest ("mp3 / m4a through the OS decoder");
        {
            // tests/data/make_fixtures.sh：88200 サンプル、22050 サンプル目から 1 kHz のバースト
            //   ffmpeg で 22051 サンプル目に |x| > 0.05（基準）。差がデコーダの頭のずれ
            const juce::File data (VOICEBOOTH_TEST_DATA_DIR);
            for (auto fixture : { "burst.mp3", "burst.m4a" })
            {
                const auto f = data.getChildFile (fixture);
                expect (f.existsAsFile(), f.getFullPathName());
                const auto ext = f.getFileExtension().substring (1);
                const auto r = loadSong (f, formats);

                if (! expectCompressedSupport (ext))
                {
                    logMessage (juce::String ("  ") + fixture + ": not supported on this system (expected) -> " + errorKey (r.error));
                    expect (r.error == LoadResult::Error::unsupported, fixture);
                    continue;
                }

                expect (r.ok(), juce::String (fixture) + ": " + errorKey (r.error));
                if (! r.ok()) continue;

                expectEquals (r.info.sampleRate, 44100.0);
                expectEquals (r.info.numChannels, 2);
                // エンコーダの遅延・詰め物の扱いはデコーダしだい。長さは ±4096 サンプルの範囲で確認し、値は記録する
                expect (std::abs (r.info.lengthSamples - 88200) <= 4096, juce::String (r.info.lengthSamples));
                expectWithinAbsoluteError (r.overview->getOverallMagnitude(), 0.5f, 0.08f);

                std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (f));
                const auto onset = firstAbove (*reader, 0.05f);
                logMessage (juce::String ("  ") + fixture + " via " + r.info.formatName
                            + ": length " + juce::String (r.info.lengthSamples) + " (wav 88200)"
                            + ", onset " + juce::String (onset) + " (ffmpeg 22051, offset "
                            + juce::String (onset - 22051) + " samples)");
                expect (onset >= 0 && std::abs (onset - 22051) <= 2400, juce::String (onset));

                // 途中から読む・後ろに戻って読む（再生のシークで使う経路）。頭から読んだときと同じ位置にバーストが出ること
                {
                    std::unique_ptr<juce::AudioFormatReader> r2 (formats.createReaderFor (f));
                    juce::AudioBuffer<float> later (2, 4096), back (2, 4096);
                    r2->read (&later, 0, 4096, 60000, true, true);            // 先へ（無音のはず）
                    r2->read (&back, 0, 4096, onset - 1000, true, true);      // 戻る：1000 サンプル目にバースト
                    expectLessThan (later.getMagnitude (0, 4096), 0.02f);
                    int found = -1;
                    for (int i = 0; i < 4096 && found < 0; ++i)
                        if (std::abs (back.getSample (0, i)) > 0.05f || std::abs (back.getSample (1, i)) > 0.05f)
                            found = i;
                    logMessage (juce::String ("  ") + fixture + " seek back: burst at +" + juce::String (found) + " (expected +1000)");
                    expect (std::abs (found - 1000) <= 2, juce::String (found));
                }
            }
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
