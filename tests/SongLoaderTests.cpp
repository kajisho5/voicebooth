#include "audio/SongLoader.h"
#include "audio/Mp4Gapless.h"

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

    /** この環境で読めるはずか（DESIGN 19）
        mp3：全 OS で minimp3。m4a：Mac は Core Audio、Windows は Media Foundation＋AAC デコーダ（msauddecmft.dll） */
    bool expectSupport (const juce::String& ext)
    {
        if (ext == "mp3")
            return true;
       #if JUCE_MAC
        return true;
       #elif JUCE_WINDOWS
        return juce::DynamicLibrary().open ("mfreadwrite.dll") && juce::DynamicLibrary().open ("msauddecmft.dll");
       #else
        return false;
       #endif
    }

    /** 頭の位置・長さが ffmpeg と完全一致するはずか
        mp3：minimp3（全 OS）、m4a：Mac は Core Audio、Windows は Media Foundation ＋ MP4 の edit list で頭を切る */
    bool expectExact (const juce::String&)
    {
        return true;
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
            for (auto fixture : { "burst.mp3", "burst.m4a", "burst_itunes.m4a" })
            {
                const auto f = data.getChildFile (fixture);
                expect (f.existsAsFile(), f.getFullPathName());
                const auto ext = f.getFileExtension().substring (1);
                const auto r = loadSong (f, formats);

                if (! expectSupport (ext))
                {
                    logMessage (juce::String ("  ") + fixture + ": not supported on this system (expected) -> " + errorKey (r.error));
                    expect (r.error == LoadResult::Error::unsupported, fixture);
                    continue;
                }

                expect (r.ok(), juce::String (fixture) + ": " + errorKey (r.error));
                if (! r.ok()) continue;

                expectEquals (r.info.sampleRate, 44100.0);
                expectEquals (r.info.numChannels, 2);
                // エンコーダの遅延・詰め物の扱いはデコーダしだい。完全一致のはずのものは厳しく、ほかは値を記録する
                if (expectExact (ext))
                    expectEquals (r.info.lengthSamples, (juce::int64) 88200);
                else
                    expect (std::abs (r.info.lengthSamples - 88200) <= 4096, juce::String (r.info.lengthSamples));
                expectWithinAbsoluteError (r.overview->getOverallMagnitude(), 0.5f, 0.08f);

                std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (f));
                const auto onset = firstAbove (*reader, 0.05f);
                logMessage (juce::String ("  ") + fixture + " via " + r.info.formatName
                            + ": length " + juce::String (r.info.lengthSamples) + " (wav 88200)"
                            + ", onset " + juce::String (onset) + " (ffmpeg 22051, offset "
                            + juce::String (onset - 22051) + " samples)");
                if (expectExact (ext))
                    expect (std::abs (onset - 22051) <= 1, "onset " + juce::String (onset));
                else
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

        beginTest ("MP4 gapless info (edit list / iTunSMPB)");
        {
            // ffmpeg の AAC：edit list で 1024 サンプル飛ばし、2.000 秒を使う
            juce::FileInputStream m4a (juce::File (VOICEBOOTH_TEST_DATA_DIR).getChildFile ("burst.m4a"));
            const auto g = readMp4Gapless (m4a, 44100.0);
            expect (g.found);
            expectEquals (g.source, juce::String ("elst"));
            expectEquals (g.priming, (juce::int64) 1024);
            expectEquals (g.validSamples, (juce::int64) 88200);

            // 同じ AAC を edit list なし＋ iTunSMPB にしたもの（tests/data/add_itunsmpb.py）
            juce::FileInputStream itunes (juce::File (VOICEBOOTH_TEST_DATA_DIR).getChildFile ("burst_itunes.m4a"));
            const auto gi = readMp4Gapless (itunes, 44100.0);
            expect (gi.found);
            expectEquals (gi.source, juce::String ("iTunSMPB"));
            expectEquals (gi.priming, (juce::int64) 1024);
            expectEquals (gi.validSamples, (juce::int64) 88200);

            // iTunes 形式：moov/udta/meta/ilst/----（mean / name / data）を組み立てて読む
            auto box = [] (const char* type, const juce::MemoryBlock& body)
            {
                juce::MemoryOutputStream out;
                out.writeIntBigEndian ((int) body.getSize() + 8);
                out.write (type, 4);
                out.write (body.getData(), body.getSize());
                return out.getMemoryBlock();
            };
            auto withHeader = [] (int headerBytes, const juce::String& text)
            {
                juce::MemoryOutputStream out;
                for (int i = 0; i < headerBytes; ++i) out.writeByte (0);
                out << text;
                return out.getMemoryBlock();
            };
            juce::MemoryBlock item;
            item.append (box ("mean", withHeader (4, "com.apple.iTunes")).getData(), box ("mean", withHeader (4, "com.apple.iTunes")).getSize());
            const auto nameBox = box ("name", withHeader (4, "iTunSMPB"));
            const auto data = box ("data", withHeader (8, " 00000000 00000840 0000037C 0000000000015888 00000000"));
            item.append (nameBox.getData(), nameBox.getSize());
            item.append (data.getData(), data.getSize());
            const auto ilst = box ("ilst", box ("----", item));
            juce::MemoryBlock metaBody (4, true);
            metaBody.append (ilst.getData(), ilst.getSize());
            const auto moov = box ("moov", box ("udta", box ("meta", metaBody)));
            juce::MemoryBlock file = box ("ftyp", withHeader (0, "M4A "));
            file.append (moov.getData(), moov.getSize());

            juce::MemoryInputStream in (file, false);
            const auto t = readMp4Gapless (in, 44100.0);
            expect (t.found);
            expectEquals (t.source, juce::String ("iTunSMPB"));
            expectEquals (t.priming, (juce::int64) 2112);          // 0x840
            expectEquals (t.validSamples, (juce::int64) 88200);    // 0x15888

            // MP4 でないもの
            juce::MemoryInputStream junk ("not an mp4 file at all", 22, false);
            expect (! readMp4Gapless (junk, 44100.0).found);
        }

        beginTest ("external songs vs ffmpeg (VOICEBOOTH_EXTRA_AUDIO_DIR)");
        {
            // 手元の実曲で確かめる（著作物はリポジトリに入れない）。<name>.mp3 / .m4a と、ffmpeg でデコードした <name>.wav を並べて置く
            //   ffmpeg -i "<name>.mp3" -map 0:a -c:a pcm_f32le "<name>.wav"
            //   float で書くこと（16bit だと 0 dBFS を超えたサンプルが WAV 側で切れて差に見える）
            const auto dirPath = juce::SystemStats::getEnvironmentVariable ("VOICEBOOTH_EXTRA_AUDIO_DIR", {});
            if (dirPath.isEmpty())
            {
                logMessage ("  skipped (VOICEBOOTH_EXTRA_AUDIO_DIR is not set)");
            }
            else
            {
                for (const auto& song : juce::File (dirPath).findChildFiles (juce::File::findFiles, false, "*.mp3;*.m4a"))
                {
                    const auto ref = song.withFileExtension ("wav");
                    if (! ref.existsAsFile()) continue;

                    std::unique_ptr<juce::AudioFormatReader> b (formats.createReaderFor (ref));
                    expect (b != nullptr, ref.getFileName());
                    if (b == nullptr) continue;

                    // m4a：頭の詰め物と本当の長さの読み取りを ffmpeg と比べる（デコーダが無い OS でも確かめられる）
                    if (song.hasFileExtension ("m4a"))
                    {
                        juce::FileInputStream in (song);
                        const auto g = readMp4Gapless (in, b->sampleRate);
                        logMessage ("  " + song.getFileName() + " gapless via " + g.source + ": priming " + juce::String (g.priming)
                                    + ", length " + juce::String (g.validSamples) + " (ffmpeg " + juce::String (b->lengthInSamples) + ")");
                        expect (g.found, song.getFileName());
                        // ffmpeg は頭の詰め物だけ切る。iTunSMPB の尻の詰め物（多くて 1 フレーム強）は残すので、
                        // こちら（エンコーダの指定どおり両方切る。Core Audio と同じ）が少し短くなる
                        const auto tail = b->lengthInSamples - g.validSamples;
                        logMessage ("    ffmpeg keeps " + juce::String (tail) + " tail padding samples");
                        expect (tail >= 0 && tail <= 2112, juce::String (tail));
                        if (! expectSupport ("m4a"))
                            continue;   // この OS では m4a を開けない（Linux）
                    }

                    std::unique_ptr<juce::AudioFormatReader> a (formats.createReaderFor (song));
                    expect (a != nullptr, song.getFileName());
                    if (a == nullptr) continue;

                    // 全体を比べる（デコーダの丸め差だけのはず）
                    constexpr int block = 1 << 16;
                    juce::AudioBuffer<float> x ((int) a->numChannels, block), y ((int) b->numChannels, block);
                    float maxDiff = 0.0f;
                    for (juce::int64 pos = 0; pos < b->lengthInSamples; pos += block)
                    {
                        const auto n = (int) juce::jmin ((juce::int64) block, b->lengthInSamples - pos);
                        a->read (&x, 0, n, pos, true, true);
                        b->read (&y, 0, n, pos, true, true);
                        for (int c = 0; c < juce::jmin (x.getNumChannels(), y.getNumChannels()); ++c)
                            for (int i = 0; i < n; ++i)
                                maxDiff = juce::jmax (maxDiff, std::abs (x.getSample (c, i) - y.getSample (c, i)));
                    }
                    logMessage ("  " + song.getFileName() + " via " + a->getFormatName() + ": length " + juce::String (a->lengthInSamples)
                                + " (ffmpeg " + juce::String (b->lengthInSamples) + "), max diff " + juce::String (maxDiff, 6));
                    expectEquals (a->lengthInSamples, b->lengthInSamples);
                    expectLessThan (maxDiff, 1.0e-4f);

                    // 途中へ飛んで読んでも同じサンプル
                    const auto at = b->lengthInSamples / 3;
                    a->read (&x, 0, 4096, at, true, true);
                    b->read (&y, 0, 4096, at, true, true);
                    float seekDiff = 0.0f;
                    for (int i = 0; i < 4096; ++i)
                        seekDiff = juce::jmax (seekDiff, std::abs (x.getSample (0, i) - y.getSample (0, i)));
                    expectLessThan (seekDiff, 1.0e-4f);
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
