#include "export/DeliveryPack.h"

namespace vb::exporter
{
namespace
{
    using project::int64;
    using project::TrackType;

    juce::File tempFolder()
    {
        auto f = juce::File::getSpecialLocation (juce::File::tempDirectory)
                     .getChildFile ("VoiceBoothTests-pack-" + juce::String (juce::Random::getSystemRandom().nextInt64()));
        f.createDirectory();
        return f;
    }

    void writeTake (const juce::File& f, int length, float value)
    {
        f.getParentDirectory().createDirectory();
        std::unique_ptr<juce::OutputStream> stream (new juce::FileOutputStream (f));
        juce::WavAudioFormat wav;
        auto w = wav.createWriterFor (stream, juce::AudioFormatWriterOptions{}.withSampleRate (48000.0).withNumChannels (1).withBitsPerSample (24));
        std::vector<float> s ((size_t) length, value);
        const float* ch[] = { s.data() };
        w->writeFromFloatArrays (ch, 1, length);
    }

    struct Info { int channels = 0; int64 length = 0; float peak = 0.0f; };
    Info readInfo (const juce::File& f)
    {
        juce::AudioFormatManager m;
        m.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> r (m.createReaderFor (f));
        Info i;
        if (r == nullptr) return i;
        i.channels = (int) r->numChannels;
        i.length = r->lengthInSamples;
        juce::AudioBuffer<float> b (i.channels, (int) i.length);
        r->read (&b, 0, (int) i.length, 0, true, true);
        i.peak = b.getMagnitude (0, (int) i.length);
        return i;
    }

    project::Take take (const char* id, const char* path, int64 start, int64 end)
    {
        project::Take k;
        k.id = id;
        k.path = path;
        k.startSample = start;
        k.endSample = end;
        return k;
    }

    project::Project makeProject (const juce::File& folder)
    {
        project::Project p;
        p.sampleRate = 48000;
        p.lengthSamples = 96000;   // 2 秒
        writeTake (folder.getChildFile ("Audio/Takes/main_take1.wav"), 96000, 0.25f);
        writeTake (folder.getChildFile ("Audio/Takes/main_take2.wav"), 48000, 0.5f);
        writeTake (folder.getChildFile ("Audio/Takes/harm1_take1.wav"), 48000, 0.125f);

        project::Track main;
        main.type = TrackType::main;
        main.takes.push_back (take ("take1", "Audio/Takes/main_take1.wav", 0, 96000));
        main.takes.push_back (take ("take2", "Audio/Takes/main_take2.wav", 48000, 96000));
        main.comp = { { 0, 48000, "take1" }, { 48000, 96000, "take2" } };
        project::Track harm;
        harm.type = TrackType::harm1;
        harm.takes.push_back (take ("take1", "Audio/Takes/harm1_take1.wav", 24000, 72000));
        harm.comp = { { 24000, 72000, "take1" } };
        p.tracks = { main, harm };
        return p;
    }
}

class DeliveryPackTests : public juce::UnitTest
{
public:
    DeliveryPackTests() : juce::UnitTest ("DeliveryPack", "VoiceBooth") {}

    void runTest() override
    {
        beginTest ("time text and file names");
        {
            expectEquals (DeliveryPack::timeText (0, 48000), juce::String ("0:00.000"));
            expectEquals (DeliveryPack::timeText (48000 * 48 + 12000, 48000), juce::String ("0:48.250"));
            expectEquals (DeliveryPack::timeText ((int64) 48000 * 3723, 48000), juce::String ("1:02:03.000"));
            expectEquals (DeliveryPack::packFileName (TrackType::main), juce::String ("vocal_dry.wav"));
            expectEquals (DeliveryPack::packFileName (TrackType::harm1), juce::String ("harmony1_dry.wav"));
            expect (DeliveryPack::packFileName (TrackType::backing).isEmpty());
        }

        beginTest ("zip only while it fits in 32-bit sizes (no Zip64): bigger packs stay a folder (#22)");
        {
            expect (DeliveryPack::fitsInZip (0, 0));
            expect (DeliveryPack::fitsInZip ((int64) 3 * 1024 * 1024 * 1024, 6));            // 3 GiB
            expect (! DeliveryPack::fitsInZip (0xFFFFFFFFLL, 1));                            // ちょうど 4 GiB
            expect (! DeliveryPack::fitsInZip (0xFFFFFFFFLL - 1000, 6));                      // 中身は収まっても見出しで超える
            expect (! DeliveryPack::fitsInZip ((int64) 5500 * 1000 * 1000, 6));              // 192 kHz・32bit float・20 分
            expect (! DeliveryPack::fitsInZip (-1, 1));
        }

        beginTest ("pack: vocals, refmix, notes, take map and zip; a second pack never overwrites the first");
        {
            const auto folder = tempFolder();
            const auto p = makeProject (folder);

            PackOptions o;
            o.songName = "Test Song";
            o.tracks = { TrackType::main, TrackType::harm1 };
            o.takeMap = true;
            o.songKey = "Am";
            o.bpm = 128.0;
            auto backing = std::make_shared<juce::AudioBuffer<float>> (2, 96000);
            for (int i = 0; i < 96000; ++i) { backing->setSample (0, i, 0.75f); backing->setSample (1, i, -0.75f); }
            o.backing = backing;
            o.backingGain = 1.0f;
            o.vocalGains[TrackType::harm1] = 0.0f;   // ハモリはモニターで切っていた
            o.sectionAt = [] (int64 s) { return s >= 48000 ? juce::String ("Chorus") : juce::String(); };

            const auto r = DeliveryPack::write (p, folder, o);
            expect (r.ok, r.message);
            expect (r.folder.getFileName().startsWith ("export_"));
            for (auto file : { "vocal_dry.wav", "harmony1_dry.wav", "refmix.wav", "notes.txt", "take_map.txt" })
                expect (r.folder.getChildFile (file).existsAsFile(), file);

            // ボーカルは曲の長さ・モノラル・ノーマライズなし（値そのまま）。最大は継ぎ目の等パワーのクロスフェードで
            // 0.25 と 0.5 が重なる所：√(0.25² + 0.5²) ≒ 0.559（書き出しと同じ計算）
            const auto seamPeak = std::sqrt (0.25f * 0.25f + 0.5f * 0.5f);
            const auto v = readInfo (r.folder.getChildFile ("vocal_dry.wav"));
            expectEquals (v.channels, 1);
            expectEquals (v.length, (int64) 96000);
            expectWithinAbsoluteError (v.peak, seamPeak, 0.002f);
            expectWithinAbsoluteError (r.peaks.at (TrackType::main), seamPeak, 0.002f);

            // refmix はステレオ。伴奏 0.75 + Main（最大 0.559）は -1 dBFS まで下げる（ハモリは音量 0 なので入らない）
            const auto m = readInfo (r.folder.getChildFile ("refmix.wav"));
            expectEquals (m.channels, 2);
            expectEquals (m.length, (int64) 96000);
            expectWithinAbsoluteError (m.peak, juce::Decibels::decibelsToGain (-1.0f), 0.002f);
            expectWithinAbsoluteError (r.refmixGainDb, -1.0f - juce::Decibels::gainToDecibels (0.75f + seamPeak), 0.05f);

            const auto notes = r.folder.getChildFile ("notes.txt").loadFileAsString();
            expect (notes.contains ("title: Test Song"));
            expect (notes.contains ("sr: 48000"));
            expect (notes.contains ("normalized: no"));
            expect (notes.contains ("key: 0"));
            expect (notes.contains ("song_key: Am"));
            expect (notes.contains ("peak_vocal_dbfs: -5.1"), notes);
            expect (notes.contains ("peak_harmony1_dbfs: -18.1"), notes);

            const auto map = r.folder.getChildFile ("take_map.txt").loadFileAsString();
            expect (map.contains ("Main\n0:00.000-0:01.000 take1\n0:01.000-end take2  [Chorus]"), map);
            expect (map.contains ("Harmony 1\n0:00.000-0:00.500 (none)\n0:00.500-0:01.500 take1  [Chorus]") == false);   // 0:00.500 は Chorus の前
            expect (map.contains ("0:00.500-0:01.500 take1\n0:01.500-end (none)  [Chorus]"), map);

            // zip に同じファイルが入っている
            expect (r.zipFile.existsAsFile());
            juce::ZipFile zip (r.zipFile);
            expectEquals (zip.getNumEntries(), r.files.size());
            expect (zip.getIndexOfFileName (r.folder.getFileName() + "/vocal_dry.wav") >= 0);

            // 2 回目は別のフォルダ
            const auto r2 = DeliveryPack::write (p, folder, o);
            expect (r2.ok);
            expect (r2.folder != r.folder);
            expect (r2.folder.getFileName().endsWith ("_2"), r2.folder.getFileName());
            folder.deleteRecursively();
        }

        beginTest ("a pack that fails leaves nothing behind");
        {
            const auto folder = tempFolder();
            auto p = makeProject (folder);
            p.tracks[0].comp.push_back ({ 90000, 96000, "missing" });
            PackOptions o;
            o.tracks = { TrackType::main };
            const auto r = DeliveryPack::write (p, folder, o);
            expect (! r.ok);
            expect (folder.findChildFiles (juce::File::findDirectories, false, "export_*").isEmpty());
            folder.deleteRecursively();
        }
    }
};

static DeliveryPackTests deliveryPackTests;
} // namespace vb::exporter
