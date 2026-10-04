#include "session/FakeEngine.h"
#include "ui/UiSession.h"
#include "project/ProjectFile.h"
#include "audio/SongLoader.h"

/*  UiSession の保存・開き直し（2026-10-04。分ける前の 1 段目）：偽のエンジンをつなぎ、本物の流れ
    （曲を開く → 変える → 保存 → 起動画面と同じ手順で .vbooth から開き直す）で、中身が戻るかを確かめる。
    #47・#48 で直した保存漏れが戻ってこないための見張り。プロジェクトは書類フォルダの VoiceBooth/Projects に作り、終わったら削除する */

namespace vb::test
{
class SessionSaveTests : public juce::UnitTest
{
public:
    SessionSaveTests() : juce::UnitTest ("Session save and reopen", "VoiceBoothSession") {}

    void runTest() override
    {
        const auto tag = juce::String::toHexString (juce::Random::getSystemRandom().nextInt64()).substring (0, 8);
        const auto work = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("vbsession-" + tag);
        work.createDirectory();
        const auto songName = "vbtest-" + tag;
        const auto song = writeTone (work.getChildFile (songName + ".wav"), 440.0, 6.0);
        const auto projectFolder = UiSession::projectFolderFor (songName);
        const auto vbooth = projectFolder.getChildFile (projectFolder.getFileName() + project::fileExtension);

        beginTest ("a new song makes a project with a copy of the song");
        {
            FakeEngine engine;
            UiSession ui;
            ui.attachEngine (&engine);
            expect (open (ui, song));
            ui.flushSave();
            expect (vbooth.existsAsFile(), vbooth.getFullPathName());
            expect (projectFolder.getChildFile ("Audio/" + song.getFileName()).existsAsFile(), "the song is copied into Audio/");
            ui.attachEngine (nullptr);
        }

        beginTest ("monitor, range, loop, track, practice and playhead come back after reopening");
        {
            {
                FakeEngine engine;
                UiSession ui;
                ui.attachEngine (&engine);
                expect (reopen (ui, vbooth));
                ui.setBackingLevel (0.31f);
                ui.setBackingMuted (true);
                ui.setGuideLevel (0.42f);
                ui.setRange (48000, 144000);
                ui.setLoop (false);
                ui.selectTrack (1);
                ui.setOctaveUp (true);
                ui.setPractice (90, -2);
                ui.seek (120000);
                ui.flushSave();
                ui.attachEngine (nullptr);
            }
            FakeEngine engine;
            UiSession ui;
            ui.attachEngine (&engine);
            expect (reopen (ui, vbooth));
            const auto& s = ui.get();
            expectWithinAbsoluteError (s.offVocalGain, 0.31f, 0.001f);
            expect (s.backingMuted);
            expectWithinAbsoluteError (s.mainGain, 0.42f, 0.001f);
            expectEquals ((int) s.rangeIn, 48000);
            expectEquals ((int) s.rangeOut, 144000);
            expect (! s.loopOn);
            expectEquals (s.selectedTrack, 1);
            expect (s.octaveUp);
            expectEquals (s.tempoPercent, 90);
            expectEquals (s.keyShift, -2);
            expectEquals ((int) s.playhead, 120000);
            ui.attachEngine (nullptr);
        }

        beginTest ("reopening without changes does not rewrite the project");
        {
            const auto before = vbooth.loadFileAsString();
            FakeEngine engine;
            UiSession ui;
            ui.attachEngine (&engine);
            expect (reopen (ui, vbooth));
            ui.flushSave();
            ui.attachEngine (nullptr);
            expectEquals (vbooth.loadFileAsString(), before);
        }

        beginTest ("another song with the same name and length gets its own project; the first is untouched");
        {
            const auto before = vbooth.loadFileAsString();
            const auto other = writeTone (work.getChildFile ("other").getChildFile (songName + ".wav"), 523.25, 6.0);
            FakeEngine engine;
            UiSession ui;
            ui.attachEngine (&engine);
            expect (open (ui, other));
            ui.flushSave();
            ui.attachEngine (nullptr);
            expectEquals (vbooth.loadFileAsString(), before);
            const auto second = UiSession::projectFolderFor (songName + " (2)");
            expect (second.getChildFile (second.getFileName() + project::fileExtension).existsAsFile(), second.getFullPathName());
            second.deleteRecursively();
        }

        projectFolder.deleteRecursively();
        work.deleteRecursively();
    }

private:
    /** 正弦波の WAV（48 kHz ステレオ 24bit） */
    static juce::File writeTone (const juce::File& f, double hz, double seconds)
    {
        f.getParentDirectory().createDirectory();
        f.deleteFile();
        constexpr double rate = 48000.0;
        const auto n = (int) (seconds * rate);
        juce::AudioBuffer<float> b (2, n);
        for (int i = 0; i < n; ++i)
        {
            const auto v = 0.3f * (float) std::sin (juce::MathConstants<double>::twoPi * hz * i / rate);
            b.setSample (0, i, v);
            b.setSample (1, i, v);
        }
        std::unique_ptr<juce::OutputStream> out (f.createOutputStream().release());
        juce::WavAudioFormat wav;
        if (auto w = wav.createWriterFor (out, juce::AudioFormatWriterOptions{}.withSampleRate (rate).withNumChannels (2).withBitsPerSample (24)))
            w->writeFromAudioSampleBuffer (b, 0, n);
        return f;
    }

    /** バックグラウンドの作業（曲のコピー・解析）の知らせを受ける */
    static void pump (int ms)
    {
        const auto until = juce::Time::getMillisecondCounter() + (juce::uint32) ms;
        while (juce::Time::getMillisecondCounter() < until)
            juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
    }

    /** 起動画面で曲を開くのと同じ（読み終わったら loadSong） */
    static bool open (UiSession& ui, const juce::File& song)
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
    static bool reopen (UiSession& ui, const juce::File& vboothFile)
    {
        const auto l = project::fromJson (vboothFile.loadFileAsString());
        if (! l.ok)
            return false;
        const auto song = project::findMedia (vboothFile.getParentDirectory(), l.project.songPath, "Audio");
        if (! song.existsAsFile())
            return false;
        ui.setPendingProject (vboothFile, l);
        return open (ui, song);
    }
};

static SessionSaveTests sessionSaveTests;
} // namespace vb::test
