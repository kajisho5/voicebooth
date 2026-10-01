#include "project/Comp.h"
#include "audio/TakeRecorder.h"
#include "export/ExportService.h"

namespace vb
{
namespace
{
    using project::int64;

    juce::File tempFolder (const juce::String& name)
    {
        auto f = juce::File::getSpecialLocation (juce::File::tempDirectory)
                     .getChildFile ("VoiceBoothTests-" + name + "-" + juce::String (juce::Random::getSystemRandom().nextInt64()));
        f.createDirectory();
        return f;
    }

    /** モノラル 24bit の WAV を書く（テイクの代わり） */
    void writeWav (const juce::File& f, double rate, const std::vector<float>& samples)
    {
        f.getParentDirectory().createDirectory();
        f.deleteFile();
        std::unique_ptr<juce::OutputStream> stream (new juce::FileOutputStream (f));
        juce::WavAudioFormat wav;
        auto w = wav.createWriterFor (stream, juce::AudioFormatWriterOptions{}.withSampleRate (rate)
                                                                              .withNumChannels (1)
                                                                              .withBitsPerSample (24));
        const float* ch[] = { samples.data() };
        w->writeFromFloatArrays (ch, 1, (int) samples.size());
    }

    struct Wav
    {
        double rate = 0.0;
        int bits = 0, channels = 0;
        std::vector<float> samples;
    };

    Wav readWav (const juce::File& f)
    {
        juce::AudioFormatManager m;
        m.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> r (m.createReaderFor (f));
        Wav w;
        if (r == nullptr)
            return w;
        w.rate = r->sampleRate;
        w.bits = (int) r->bitsPerSample;
        w.channels = (int) r->numChannels;
        juce::AudioBuffer<float> b ((int) r->numChannels, (int) r->lengthInSamples);
        r->read (&b, 0, (int) r->lengthInSamples, 0, true, true);
        w.samples.assign (b.getReadPointer (0), b.getReadPointer (0) + b.getNumSamples());
        return w;
    }

    project::Take take (const juce::String& id, int64 start, int64 end)
    {
        project::Take t;
        t.id = id;
        t.path = "Audio/Takes/main_" + id + ".wav";
        t.startSample = start;
        t.endSample = end;
        return t;
    }
}

class RecordTests : public juce::UnitTest
{
public:
    RecordTests() : juce::UnitTest ("Record", "VoiceBooth") {}

    void runTest() override
    {
        testComp();
        testRecorder();
        testExport();
    }

    //==========================================================================
    void testComp()
    {
        beginTest ("comp: a new take replaces its range, the rest of the older take stays");
        {
            project::Track t;
            expectEquals (project::nextTakeId (t), juce::String ("take1"));
            project::applyTake (t, take ("take1", 0, 1000));
            project::applyTake (t, take ("take2", 300, 600));
            expect (project::compIsValid (t));
            expectEquals ((int) t.comp.size(), 3);
            expect (t.comp[0].takeId == "take1" && t.comp[0].startSample == 0 && t.comp[0].endSample == 300);
            expect (t.comp[1].takeId == "take2" && t.comp[1].startSample == 300 && t.comp[1].endSample == 600);
            expect (t.comp[2].takeId == "take1" && t.comp[2].startSample == 600 && t.comp[2].endSample == 1000);
            expectEquals (project::nextTakeId (t), juce::String ("take3"));
            expect (project::takeAt (t, 299)->id == "take1");
            expect (project::takeAt (t, 300)->id == "take2");
            expect (project::takeAt (t, 1000) == nullptr);
        }

        beginTest ("comp: covering takes, gaps, merging and zero-length takes");
        {
            project::Track t;
            project::applyTake (t, take ("take1", 100, 200));
            project::applyTake (t, take ("take2", 500, 700));     // 間は未録音のまま
            expectEquals ((int) t.comp.size(), 2);
            expect (project::takeAt (t, 300) == nullptr);

            project::applyTake (t, take ("take3", 0, 1000));      // 全部を覆う
            expectEquals ((int) t.comp.size(), 1);
            expect (t.comp[0].takeId == "take3");
            expectEquals ((int) t.takes.size(), 3);               // 古いテイクは消さない

            project::applyTake (t, take ("take4", 50, 50));       // 長さ 0 は足さない
            expectEquals ((int) t.takes.size(), 3);
            expect (project::compIsValid (t));

            project::Track broken;
            broken.comp.push_back ({ 0, 10, "nope" });
            expect (! project::compIsValid (broken));
        }
    }

    //==========================================================================
    void testRecorder()
    {
        const double rate = 48000.0;

        beginTest ("recorder: starts with the first playing block, sample-exact position and content, 24-bit mono");
        {
            auto dir = tempFolder ("rec");
            audio::TakeRecorder r;
            const auto file = dir.getChildFile ("Audio/Takes/main_take1.wav");
            expect (r.begin (file, rate).isEmpty());

            std::vector<float> block (256);
            int64 song = 1000;
            int counter = 0;
            auto next = [&] { for (auto& v : block) v = (float) ((counter++ % 200) - 100) / 400.0f; };

            next();
            r.process (block.data(), 256, song, 0, false);        // まだ再生していない
            expectEquals (r.getRecordedSamples(), (int64) 0);

            counter = 0;
            std::vector<float> expected;
            for (int b = 0; b < 40; ++b)
            {
                next();
                expected.insert (expected.end(), block.begin(), block.end());
                r.process (block.data(), 256, song, 256, false);
                song += 256;
            }
            expect (! r.hasEnded());
            r.process (block.data(), 256, song, 0, false);        // 止まった
            expect (r.hasEnded());

            const auto res = r.finish();
            expectEquals (res.startSample, (int64) 1000);
            expectEquals (res.length, (int64) expected.size());
            expect (! res.clipped && ! res.dropped);

            const auto w = readWav (file);
            expectEquals (w.rate, rate);
            expectEquals (w.bits, 24);
            expectEquals (w.channels, 1);
            expectEquals ((int) w.samples.size(), (int) expected.size());
            float worst = 0.0f;
            for (size_t i = 0; i < expected.size(); ++i)
                worst = std::max (worst, std::abs (w.samples[i] - expected[i]));
            expectLessThan (worst, 1.0e-6f);
            dir.deleteRecursively();
        }

        beginTest ("recorder: stops at the end of the song, flags clipping, fills a vanished input with silence");
        {
            auto dir = tempFolder ("rec2");
            audio::TakeRecorder r;
            const auto file = dir.getChildFile ("t.wav");
            expect (r.begin (file, rate).isEmpty());

            std::vector<float> loud (128, 0.995f);
            r.process (loud.data(), 128, 0, 128, false);
            r.process (nullptr, 128, 128, 128, false);              // 入力が消えた
            r.process (loud.data(), 128, 256, 50, false);           // 曲の終わり（50 サンプルだけ）
            expect (r.hasEnded());
            r.process (loud.data(), 128, 306, 0, false);            // もう書かない

            const auto res = r.finish();
            expectEquals (res.length, (int64) (128 + 128 + 50));
            expect (res.clipped);
            const auto w = readWav (file);
            expectEquals ((int) w.samples.size(), 306);
            expectWithinAbsoluteError (w.samples[200], 0.0f, 1.0e-7f);
            expectWithinAbsoluteError (w.samples[300], 0.995f, 1.0e-6f);
            dir.deleteRecursively();
        }

        beginTest ("recorder: a loop wrap ends the take; nothing is written if playback never started");
        {
            auto dir = tempFolder ("rec3");
            audio::TakeRecorder r;
            expect (r.begin (dir.getChildFile ("a.wav"), rate).isEmpty());
            std::vector<float> x (64, 0.1f);
            r.process (x.data(), 64, 0, 64, false);
            r.process (x.data(), 64, 0, 64, true);
            expect (r.hasEnded());
            expectEquals (r.finish().length, (int64) 64);

            expect (r.begin (dir.getChildFile ("b.wav"), rate).isEmpty());
            r.process (x.data(), 64, 0, 0, false);
            expectEquals (r.finish().length, (int64) 0);
            dir.deleteRecursively();
        }
    }

    //==========================================================================
    void testExport()
    {
        const int rate = 48000;
        const int64 length = 200001;          // 書き出しのブロック（65536）の境目をまたぐ

        auto makeProject = [&] (const juce::File& dir)
        {
            project::Project p;
            p.sampleRate = rate;
            p.lengthSamples = length;
            project::Track t;
            t.type = project::TrackType::main;

            // take1：1000〜150000 に 0.25、take2：65500〜70000 に -0.5（継ぎ目がブロックの境目をまたぐ）
            auto t1 = take ("take1", 1000, 150000);
            auto t2 = take ("take2", 65500, 70000);
            writeWav (dir.getChildFile (t1.path), rate, std::vector<float> ((size_t) (t1.endSample - t1.startSample), 0.25f));
            writeWav (dir.getChildFile (t2.path), rate, std::vector<float> ((size_t) (t2.endSample - t2.startSample), -0.5f));
            project::applyTake (t, t1);
            project::applyTake (t, t2);
            p.tracks.push_back (t);
            return p;
        };

        beginTest ("export: full length from sample 0, silence where unrecorded, hard edges to silence, 24-bit mono at song SR");
        {
            auto dir = tempFolder ("exp");
            const auto p = makeProject (dir);
            const auto out = dir.getChildFile ("Export/song_vocal_dry.wav");
            exporter::Options o;
            o.crossfadeMs = 0.0;
            const auto res = exporter::ExportService::exportTrackDry (p, project::TrackType::main, dir, out, o);
            expect (res.ok, res.message);
            expectEquals (res.length, length);

            const auto w = readWav (out);
            expectEquals (w.rate, (double) rate);
            expectEquals (w.bits, 24);
            expectEquals (w.channels, 1);
            expectEquals ((int64) w.samples.size(), length);       // 曲と同じ長さ（DESIGN 17 のテスト）
            expectEquals (w.samples[0], 0.0f);
            expectEquals (w.samples[999], 0.0f);                    // 録っていない所は無音
            expectEquals (w.samples[1000], 0.25f);                  // 自動フェードなし
            expectEquals (w.samples[65499], 0.25f);
            expectEquals (w.samples[65500], -0.5f);                 // クロスフェード 0 はちょうどで切り替わる
            expectEquals (w.samples[69999], -0.5f);
            expectEquals (w.samples[70000], 0.25f);
            expectEquals (w.samples[149999], 0.25f);
            expectEquals (w.samples[150000], 0.0f);
            expectEquals (w.samples.back(), 0.0f);
            expectWithinAbsoluteError (res.peak, 0.5f, 1.0e-6f);
            expect (! res.clipped);
            expect (! out.getSiblingFile (out.getFileName() + ".part").exists());
            dir.deleteRecursively();
        }

        beginTest ("export: 8 ms equal-power crossfade only between different takes, kept where both have audio");
        {
            auto dir = tempFolder ("exp2");
            const auto p = makeProject (dir);
            const auto out = dir.getChildFile ("v.wav");
            const auto res = exporter::ExportService::exportTrackDry (p, project::TrackType::main, dir, out);
            expect (res.ok, res.message);
            const auto w = readWav (out);
            const int64 h = 192;                                    // 8 ms の半分（48 kHz）
            const auto c = std::cos (juce::MathConstants<float>::halfPi * 0.5f);   // = sin(π/4)
            expectEquals (w.samples[1000], 0.25f);                  // 無音との境目にはフェードを掛けない

            // take2 は 65500 から録ったので、その前に音は無い → 窓は [65500, 65884) にずれる（落ち込まない）
            expectEquals (w.samples[65499], 0.25f);
            expectEquals (w.samples[65500], 0.25f);
            expectWithinAbsoluteError (w.samples[(size_t) (65500 + h)], 0.25f * c - 0.5f * c, 1.0e-5f);
            expectEquals (w.samples[(size_t) (65500 + 2 * h)], -0.5f);

            // take2 は 70000 で終わる → 窓は [69616, 70000)
            expectEquals (w.samples[(size_t) (70000 - 2 * h - 1)], -0.5f);
            expectWithinAbsoluteError (w.samples[(size_t) (70000 - h)], -0.5f * c + 0.25f * c, 1.0e-5f);
            expectEquals (w.samples[70000], 0.25f);
            expectEquals ((int64) w.samples.size(), length);
            dir.deleteRecursively();
        }

        beginTest ("export: a crossfade is centred on the seam when both takes have audio around it");
        {
            auto dir = tempFolder ("exp4");
            project::Project p;
            p.sampleRate = rate;
            p.lengthSamples = 10000;
            project::Track t;
            auto t1 = take ("take1", 0, 10000);
            auto t2 = take ("take2", 0, 10000);
            writeWav (dir.getChildFile (t1.path), rate, std::vector<float> (10000, 0.25f));
            writeWav (dir.getChildFile (t2.path), rate, std::vector<float> (10000, -0.5f));
            project::applyTake (t, t1);
            project::applyTake (t, t2);
            t.comp = { { 0, 5000, "take1" }, { 5000, 10000, "take2" } };   // 選び直した形（B10）
            p.tracks.push_back (t);
            const auto out = dir.getChildFile ("c.wav");
            const auto res = exporter::ExportService::exportTrackDry (p, project::TrackType::main, dir, out);
            expect (res.ok, res.message);
            const auto w = readWav (out);
            const auto c = std::cos (juce::MathConstants<float>::halfPi * 0.5f);
            expectEquals (w.samples[5000 - 193], 0.25f);
            expectWithinAbsoluteError (w.samples[5000], 0.25f * c - 0.5f * c, 1.0e-5f);
            expectEquals (w.samples[5000 + 192], -0.5f);
            dir.deleteRecursively();
        }

        beginTest ("export: refuses a take with a different sample rate, a missing take, or nothing recorded");
        {
            auto dir = tempFolder ("exp3");
            auto p = makeProject (dir);
            writeWav (dir.getChildFile (p.tracks[0].takes[1].path), 44100, std::vector<float> (4500, -0.5f));
            const auto out = dir.getChildFile ("x.wav");
            auto res = exporter::ExportService::exportTrackDry (p, project::TrackType::main, dir, out);
            expect (! res.ok);
            expect (! out.exists());

            dir.getChildFile (p.tracks[0].takes[1].path).deleteFile();
            res = exporter::ExportService::exportTrackDry (p, project::TrackType::main, dir, out);
            expect (! res.ok);

            res = exporter::ExportService::exportTrackDry (p, project::TrackType::harm1, dir, out);
            expect (! res.ok);
            expect (! out.exists());

            exporter::Options cancel;
            cancel.progress = [] (float) { return false; };
            auto ok = makeProject (dir);
            res = exporter::ExportService::exportTrackDry (ok, project::TrackType::main, dir, out, cancel);
            expect (! res.ok);
            expect (! out.exists());
            dir.deleteRecursively();
        }

        beginTest ("export: file names follow DESIGN 9 and never include backing / guide");
        {
            using exporter::ExportService;
            expectEquals (ExportService::dryFileName ("Song A", project::TrackType::main), juce::String ("Song A_vocal_dry.wav"));
            expectEquals (ExportService::dryFileName ("x", project::TrackType::harm1), juce::String ("x_harmony1_dry.wav"));
            expect (ExportService::dryFileName ("x", project::TrackType::backing).isEmpty());
            expect (ExportService::dryFileName ("x", project::TrackType::guide).isEmpty());
            expect (! ExportService::dryFileName ("a/b:c", project::TrackType::main).containsAnyOf ("/:"));
        }
    }
};

static RecordTests recordTests;
} // namespace vb
