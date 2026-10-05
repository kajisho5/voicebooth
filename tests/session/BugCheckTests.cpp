#include "session/FakeEngine.h"
#include "session/SessionTestUtil.h"

/*  2 回目のバグチェック（2026-10-05）で直したもののうち、UiSession で確認できるもの */

namespace vb::test
{
namespace
{
    /** 出力が開いていないエンジン（機器を外した・開けなかった） */
    class ClosedOutputEngine final : public FakeEngine
    {
    public:
        audio::OutputStatus getOutputStatus() const override { return {}; }
    };

    int trackIndex (UiSession& ui, project::TrackType type)
    {
        const auto& t = ui.get().trackUi;
        for (int i = 0; i < (int) t.size(); ++i)
            if (t[(size_t) i].type == type)
                return i;
        return -1;
    }
}

class BugCheckTests : public juce::UnitTest
{
public:
    BugCheckTests() : juce::UnitTest ("Bug check 2026-10-05", "VoiceBoothSession") {}

    void runTest() override
    {
        const auto tag = juce::String::toHexString (juce::Random::getSystemRandom().nextInt64()).substring (0, 8);
        const auto work = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("vbbug-" + tag);
        const auto songName = "vbbug-" + tag;
        const auto song = writeTone (work.getChildFile (songName + ".wav"), 440.0, 4.0);

        beginTest ("the assigned keys survive opening a song");
        {
            dummy::Session prev;
            prev.shortcuts.assign (shortcuts::Action::tapTempo, 'y');
            const auto next = dummy::makeSongSession (prev, "song", "/song.wav", 48000, 48000, nullptr);
            expect (next.shortcuts.actionFor ('y') == shortcuts::Action::tapTempo);
        }

        beginTest ("setting only one end of the voice range keeps the other end");
        {
            UiSession ui;
            ui.setVoiceRange (60, -1);   // 低い声から測る
            expectEquals (ui.get().voiceLow, 60, "the unset high end is not swapped in");
            expectEquals (ui.get().voiceHigh, -1);
            ui.setVoiceRange (72, 60);
            expectEquals (ui.get().voiceLow, 60, "both ends given the wrong way round are swapped");
            expectEquals (ui.get().voiceHigh, 72);
        }

        beginTest ("switching to easy mode unarms a harmony track that is no longer shown");
        {
            UiSession ui;
            ui.setMode (project::Mode::standard);
            const auto harm = trackIndex (ui, project::TrackType::harm1);
            expect (harm >= 0);
            ui.armTrack (harm);
            expect (ui.get().trackUi[(size_t) harm].armed);
            ui.setMode (project::Mode::easy);
            expect (! ui.get().trackUi[(size_t) harm].armed, "a hidden track is not recorded");
        }

        beginTest ("exporting with nothing chosen tells you instead of doing nothing");
        {
            UiSession ui;
            ui.exportTracks ({});
            expectEquals (ui.get().noticeText, tr ("export.nothing"));
            ui.exportPack ({});
            expectEquals (ui.get().noticeText, tr ("export.nothing"));
        }

        beginTest ("REC with the output closed does not pretend to record");
        {
            ClosedOutputEngine engine;
            UiSession ui;
            ui.attachEngine (&engine);
            expect (openSong (ui, song));
            ui.tick (1.1);
            ui.setRecording (true);
            expect (! ui.get().isRecording, "not recording");
            expectEquals (engine.recordingsStarted, 0);
            expect (ui.get().noticeText.isNotEmpty(), "the reason is shown");
            ui.attachEngine (nullptr);
        }

        beginTest ("project folder names: long names keep their number, symbol-only names are Untitled");
        {
            const auto longName = juce::String::repeatedString ("a", 200);
            const auto one = UiSession::projectFolderFor (longName), two = UiSession::projectFolderFor (longName, 2), three = UiSession::projectFolderFor (longName, 3);
            expect (two != three && one != two, "each number gets its own folder (it hung before)");
            expect (one.getFileName().length() <= 80);
            expect (two.getFileName().endsWith (" (2)"));
            const auto jp = juce::String::repeatedString (juce::String::fromUTF8 ("\xe3\x81\x82"), 120);   // 「あ」×120（360 バイト）
            expect (UiSession::projectFolderFor (jp).getFileName().getNumBytesAsUTF8() <= 160);
            const auto root = UiSession::projectFolderFor ({}).getParentDirectory();
            for (auto* bad : { "???", "...", "..", ".", "" })
                expect (UiSession::projectFolderFor (bad).getParentDirectory() == root
                        && UiSession::projectFolderFor (bad).getFileName() == "Untitled", juce::String ("name: ") + bad);
            expectEquals (UiSession::projectFolderFor ("My Song").getFileName(), juce::String ("My Song"), "short names do not change");
        }

        UiSession::projectFolderFor (songName).deleteRecursively();
        work.deleteRecursively();
    }
};

static BugCheckTests bugCheckTests;
} // namespace vb::test
