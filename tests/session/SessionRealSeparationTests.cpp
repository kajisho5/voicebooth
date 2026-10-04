#include "session/FakeEngine.h"
#include "session/SessionTestUtil.h"
#include "i18n/I18n.h"

/*  本物のモデルで分離の 3 つの流れを通す（手元だけ。VB_REAL_SONG に曲のパスを入れたときだけ動く）。
    VB_REAL_STEPS：a = 原曲からオフボ、b = 分離中の［キャンセル］、c = お手本（引き算）→ リード分け、d = お手本の分離（separateGuide）、
    e = d の途中（声と伴奏を書いた後、リードの途中）でアプリが落ちたプロジェクトを開き直し、リードだけを分け直す（VB_REAL_PROJECT に .vbooth） */

namespace vb::test
{
class SessionRealSeparationTests : public juce::UnitTest
{
public:
    SessionRealSeparationTests() : juce::UnitTest ("Session real separation", "VoiceBoothSession") {}

    /** 動いている分離のプロセスの数（シェルを通さない。pgrep 自身のコマンド行には [/] の正規表現が一致しない） */
    static int separatorProcesses()
    {
        juce::ChildProcess p;
       #if JUCE_WINDOWS
        if (! p.start (juce::StringArray { "tasklist", "/FI", "IMAGENAME eq VoiceBoothSeparator.exe", "/NH" }))
            return -1;
       #else
        if (! p.start (juce::StringArray { "pgrep", "-f", "[/]VoiceBoothSeparator" }))
            return -1;
       #endif
        int n = 0;
        for (auto& line : juce::StringArray::fromLines (p.readAllProcessOutput()))
            n += line.contains ("VoiceBoothSeparator") || (line.trim().containsOnly ("0123456789") && line.trim().isNotEmpty()) ? 1 : 0;
        return n;
    }

    static double seconds (double startMs) { return (juce::Time::getMillisecondCounterHiRes() - startMs) / 1000.0; }

    void runTest() override
    {
        const auto songPath = juce::SystemStats::getEnvironmentVariable ("VB_REAL_SONG", {});
        const auto steps = juce::SystemStats::getEnvironmentVariable ("VB_REAL_STEPS", "abcd");
        const auto outDir = juce::File (juce::SystemStats::getEnvironmentVariable ("VB_REAL_OUT", "/tmp/vbreal"));
        if (songPath.isEmpty())
        {
            beginTest ("skipped (VB_REAL_SONG is not set)");
            expect (true);
            return;
        }
        const juce::File song (songPath);
        const auto cache = outDir.getChildFile ("cache");
        cache.createDirectory();
        auto workFiles = [&]
        {
            juce::StringArray names;
            for (auto& f : cache.findChildFiles (juce::File::findFiles, true, "mix.wav;vocals.wav;backing.wav;rest.wav;*.part"))
                names.add (f.getFullPathName());
            return names;
        };
        juce::File offVocal;

        if (steps.contains ("a"))
        {
            beginTest ("real: a backing track is made from the original");
            FakeEngine engine;
            UiSession ui;
            ui.attachEngine (&engine);
            ui.setCacheFolder (cache);
            expect (ui.separationAvailable(), "the separator and the model are installed");
            bool called = false;
            juce::String error;
            const auto t0 = juce::Time::getMillisecondCounterHiRes();
            float lastProgress = 0.0f;
            int progressUpdates = 0;
            ui.makeOffVocal (song, [&] (juce::File f, juce::String e) { called = true; offVocal = f; error = e; });
            const bool finished = pumpUntil ([&]
            {
                if (std::abs (ui.get().separationProgress - lastProgress) > 1.0e-6f) { lastProgress = ui.get().separationProgress; ++progressUpdates; }
                return called;
            }, 60 * 60 * 1000);
            expect (finished, "finished within an hour");
            logMessage ("  off vocal: " + juce::String (seconds (t0), 1) + " s, progress updates " + juce::String (progressUpdates)
                        + ", error '" + error + "', file " + offVocal.getFullPathName());
            expect (error.isEmpty(), error);
            expect (offVocal.existsAsFile());
            expect (progressUpdates > 3, "progress is reported");
            expect (! ui.get().separating);
            expect (workFiles().isEmpty(), workFiles().joinIntoString (", "));
            pump (500);
            expectEquals (separatorProcesses(), 0);

            juce::AudioFormatManager fm;
            audio::registerSongFormats (fm);
            std::unique_ptr<juce::AudioFormatReader> a (fm.createReaderFor (song)), b (fm.createReaderFor (offVocal));
            expect (a != nullptr && b != nullptr);
            if (a != nullptr && b != nullptr)
            {
                logMessage ("  original " + juce::String (a->sampleRate) + " Hz " + juce::String (a->lengthInSamples) + " samples / off vocal "
                            + juce::String (b->sampleRate) + " Hz " + juce::String (b->lengthInSamples) + " samples");
                expect (std::abs (a->sampleRate - b->sampleRate) < 0.5);
                expect (std::abs (a->lengthInSamples - b->lengthInSamples) < 2048, "same length");
            }
            ui.attachEngine (nullptr);
        }

        if (steps.contains ("b"))
        {
            beginTest ("real: cancel while separating stops the process and leaves no work files");
            const auto copy = juce::File (juce::SystemStats::getEnvironmentVariable ("VB_REAL_SONG_CANCEL", songPath));
            FakeEngine engine;
            UiSession ui;
            ui.attachEngine (&engine);
            ui.setCacheFolder (cache.getChildFile ("cancel"));
            bool called = false;
            juce::File made;
            juce::String error;
            ui.makeOffVocal (copy, [&] (juce::File f, juce::String e) { called = true; made = f; error = e; });
            expect (pumpUntil ([&] { return separatorProcesses() > 0; }, 120000), "the separator process starts");
            pump (15000);   // 分離が進んでいる最中
            logMessage ("  progress before cancel " + juce::String (ui.get().separationProgress, 3));
            const auto t0 = juce::Time::getMillisecondCounterHiRes();
            ui.stopSeparation();
            expect (pumpUntil ([&] { return called; }, 30000), "stopped within 30 s");
            logMessage ("  stopped in " + juce::String (seconds (t0), 2) + " s, error '" + error + "'");
            expectEquals (error, tr ("separation.stopped"));
            expect (made == juce::File());
            pump (1000);
            expectEquals (separatorProcesses(), 0);
            juce::StringArray left;
            for (auto& f : cache.getChildFile ("cancel").findChildFiles (juce::File::findFiles, true, "*"))
                left.add (f.getFullPathName());
            expect (left.isEmpty(), left.joinIntoString (", "));
            ui.attachEngine (nullptr);
        }

        if (steps.containsAnyOf ("cd"))
        {
            if (offVocal == juce::File())
                for (auto& f : cache.findChildFiles (juce::File::findFiles, true, "*(off vocal).wav"))
                    offVocal = f;
            beginTest ("real: guide by subtraction, then lead and harmony with the karaoke model");
            expect (offVocal.existsAsFile(), "run step a first");
            if (! offVocal.existsAsFile())
                return;
            FakeEngine engine;
            UiSession ui;
            ui.attachEngine (&engine);
            ui.setCacheFolder (cache);
            expect (openSong (ui, offVocal));
            expect (pumpUntil ([&] { return ui.get().projectFolder.isDirectory(); }, 20000));
            const auto t0 = juce::Time::getMillisecondCounterHiRes();
            ui.loadGuide (song);
            expect (pumpUntil ([&] { return ! ui.get().guideBusy; }, 10 * 60 * 1000), "the guide is analysed");
            logMessage ("  guide by subtraction: " + juce::String (seconds (t0), 1) + " s, points " + juce::String ((int) ui.get().refPitch.size())
                        + ", separating (lead) " + juce::String (ui.get().separating ? "yes" : "no"));
            expect (! ui.get().refPitch.empty(), "the guide line is made");

            auto onlyDir = [&] (const juce::String& sub)
            {
                const auto dirs = ui.get().projectFolder.getChildFile ("Cache/" + sub).findChildFiles (juce::File::findDirectories, false);
                return dirs.size() == 1 ? dirs[0] : juce::File();
            };
            if (steps.contains ("c"))
            {
                const auto t1 = juce::Time::getMillisecondCounterHiRes();
                expect (ui.get().separating && ui.get().separationKind == 2, "the lead separation starts by itself");
                expect (pumpUntil ([&] { return ! ui.get().separating && ! ui.get().leadAnalysing; }, 60 * 60 * 1000), "lead finished");
                pump (500);
                logMessage ("  lead: " + juce::String (seconds (t1), 1) + " s, lead points " + juce::String ((int) ui.get().refPitch.size())
                            + ", harmony points " + juce::String ((int) ui.get().refPitchHarm.size())
                            + ", harmony audio " + juce::String (ui.get().guideHarmVocals != nullptr ? "yes" : "no"));
                const auto leadDir = onlyDir ("lead");
                expect (leadDir.getChildFile ("lead.wav").existsAsFile(), "lead.wav is kept in the cache");
                expect (! leadDir.getChildFile ("rest.wav").existsAsFile(), "rest.wav is removed");
                expect (! leadDir.getChildFile ("mix.wav").existsAsFile(), "mix.wav is removed");
                expectEquals (separatorProcesses(), 0);
                leadDir.getChildFile ("lead.wav").copyFileTo (outDir.getChildFile ("lead.wav"));
            }

            if (steps.contains ("d"))
            {
                beginTest ("real: separate the guide (vocals, backing and lead in one run)");
                expect (pumpUntil ([&] { return ! ui.get().separating && ! ui.get().leadAnalysing; }, 60 * 60 * 1000));
                const auto t2 = juce::Time::getMillisecondCounterHiRes();
                ui.separateGuide();
                expect (ui.get().separating && ui.get().separationKind == 0, "the guide separation starts");
                expect (pumpUntil ([&] { return ! ui.get().separating && ! ui.get().guideBusy && ! ui.get().leadAnalysing; }, 90 * 60 * 1000),
                        "finished");
                pump (1000);
                const auto dir = onlyDir ("separation");
                logMessage ("  separate guide: " + juce::String (seconds (t2), 1) + " s, points " + juce::String ((int) ui.get().refPitch.size())
                            + ", harmony points " + juce::String ((int) ui.get().refPitchHarm.size()));
                expect (dir.getChildFile ("vocals.wav").existsAsFile() && dir.getChildFile ("backing.wav").existsAsFile(), "vocals and backing are cached");
                expect (dir.getChildFile ("lead.wav").existsAsFile(), "lead is written in the same run");
                expect (! dir.getChildFile ("mix.wav").existsAsFile(), "mix.wav is removed");
                expectEquals (separatorProcesses(), 0);
                dir.getChildFile ("vocals.wav").copyFileTo (outDir.getChildFile ("vocals.wav"));
            }
            ui.attachEngine (nullptr);
        }

        if (steps.contains ("e"))
        {
            beginTest ("real: reopen a project that stopped during the lead step; only the lead is separated again");
            const juce::File vbooth (juce::SystemStats::getEnvironmentVariable ("VB_REAL_PROJECT", {}));
            expect (vbooth.existsAsFile(), "VB_REAL_PROJECT: " + vbooth.getFullPathName());
            if (! vbooth.existsAsFile())
                return;
            FakeEngine engine;
            UiSession ui;
            ui.attachEngine (&engine);
            ui.setCacheFolder (cache);
            expect (reopenProject (ui, vbooth));
            expect (pumpUntil ([&] { return ! ui.get().guideBusy && ! ui.get().refPitch.empty(); }, 10 * 60 * 1000), "the guide comes back");
            // 開き直すとリード分けが自動で始まる（引き算のお手本）。ここでは分離（separateGuide）の続きを確かめたいので、止めてから呼ぶ
            if (ui.get().separating)
            {
                ui.stopSeparation();
                expect (pumpUntil ([&] { return ! ui.get().separating; }, 30000));
            }
            const auto dirs = ui.get().projectFolder.getChildFile ("Cache/separation").findChildFiles (juce::File::findDirectories, false);
            expectEquals (dirs.size(), 1);
            if (dirs.size() != 1)
                return;
            const auto dir = dirs[0];
            expect (dir.getChildFile ("vocals.wav").existsAsFile() && dir.getChildFile ("backing.wav").existsAsFile(), "vocals and backing are left from before");
            expect (! dir.getChildFile ("lead.wav").existsAsFile(), "the lead was not finished");
            const auto vocalsTime = dir.getChildFile ("vocals.wav").getLastModificationTime();
            const auto t0 = juce::Time::getMillisecondCounterHiRes();
            ui.separateGuide();
            expect (ui.get().separating && ui.get().separationKind == 2, "only the lead step runs (kind " + juce::String (ui.get().separationKind) + ")");
            expect (pumpUntil ([&] { return ! ui.get().separating && ! ui.get().guideBusy && ! ui.get().leadAnalysing; }, 90 * 60 * 1000), "finished");
            pump (1000);
            logMessage ("  lead only: " + juce::String (seconds (t0), 1) + " s, harmony points " + juce::String ((int) ui.get().refPitchHarm.size()));
            expect (dir.getChildFile ("lead.wav").existsAsFile(), "lead.wav is written");
            expect (dir.getChildFile ("vocals.wav").getLastModificationTime() == vocalsTime, "vocals.wav is not made again");
            expect (! dir.getChildFile ("mix.wav").existsAsFile() && ! dir.getChildFile ("rest.wav").existsAsFile(), "work files are removed");
            expect (ui.get().guideHarmVocals != nullptr, "the harmony guide is made");
            expectEquals (separatorProcesses(), 0);
            ui.attachEngine (nullptr);
        }
    }
};

static SessionRealSeparationTests sessionRealSeparationTests;
} // namespace vb::test
