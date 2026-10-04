#include "session/FakeEngine.h"
#include "session/SessionTestUtil.h"

/*  範囲をループして何回も録る（#29。DESIGN B10）：範囲とループを点けて録ると、範囲の終わり（＋余韻 0.5 秒）で今のテイクを閉じ、
    範囲の少し前へ戻って、止めずに次のテイクを録る。止めた時の途中のテイクも残す。ループを消していれば今までどおり範囲の終わりで止まる。
    偽のエンジン（tests/session/FakeEngine.h）の録音は、止めた時に録った長さの無音の WAV を書く */

namespace vb::test
{
class SessionLoopRecordTests : public juce::UnitTest
{
public:
    SessionLoopRecordTests() : juce::UnitTest ("Session loop recording", "VoiceBoothSession") {}

    void runTest() override
    {
        const auto tag = juce::String::toHexString (juce::Random::getSystemRandom().nextInt64()).substring (0, 8);
        const auto work = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("vbloop-" + tag);
        const auto songName = "vbloop-" + tag;
        const auto song = writeTone (work.getChildFile (songName + ".wav"), 440.0, 12.0);
        constexpr juce::int64 rate = 48000;
        const juce::int64 in = 4 * rate, out = 6 * rate;

        auto prepare = [&] (UiSession& ui, bool loop)
        {
            expect (openSong (ui, song));
            ui.setMode (project::Mode::standard);   // 範囲の録り直しは標準・プロ（簡単は通しだけ）
            ui.setRecMode (project::RecMode::delivery);
            ui.setCountIn (0);
            if (! ui.get().trackUi[0].armed)
                ui.armTrack (0);   // 押すたびに切り替わる（Main は最初からアーム）
            ui.setRange (in, out);
            ui.setLoop (loop);
            ui.tick (1.1);   // 入力の状態を読む
        };
        auto pass = [&] (UiSession& ui, FakeEngine& engine, juce::int64 to)
        {
            engine.playhead = to;
            ui.tick (0.02);
        };
        auto mainTakes = [] (UiSession& ui) -> const std::vector<project::Take>&
        {
            return ui.get().project.findTrack (project::TrackType::main)->takes;
        };

        beginTest ("with the loop on, each pass of the range becomes a take until you stop");
        {
            FakeEngine engine;
            UiSession ui;
            ui.attachEngine (&engine);
            prepare (ui, true);
            ui.setRecording (true);
            expect (ui.get().isRecording && engine.recording, "recording starts: " + ui.get().noticeText);
            const auto preroll = engine.playhead;
            expect (preroll < in, "it starts a little before the range");

            pass (ui, engine, out + rate / 2 + 10);   // 範囲の終わり＋余韻
            expect (ui.get().isRecording && engine.recording && engine.playing, "it goes on without stopping");
            expectEquals (engine.recordingsStarted, 2, "the next take starts");
            expectEquals (engine.playhead, preroll, "back to a little before the range");
            expectEquals ((int) mainTakes (ui).size(), 1);

            pass (ui, engine, out + rate / 2 + 10);
            expectEquals (engine.recordingsStarted, 3);
            expectEquals ((int) mainTakes (ui).size(), 2);

            pass (ui, engine, in + rate);              // 3 回目の途中で止める
            ui.setPlaying (false);
            expect (! ui.get().isRecording && ! engine.recording && ! engine.playing);
            const auto& takes = mainTakes (ui);
            expectEquals ((int) takes.size(), 3, "the stopped pass is kept too");
            juce::StringArray ids;
            for (auto& t : takes)
                ids.add (t.id);
            expectEquals (ids.joinIntoString (","), juce::String ("take1,take2,take3"));
            for (auto& t : takes)
                expect (ui.get().projectFolder.getChildFile (t.path).existsAsFile(), t.path);

            // 採用：範囲の前半は最後（take3）、take3 が録れていない後半は take2
            const auto& comp = ui.get().project.findTrack (project::TrackType::main)->comp;
            juce::String compText;
            for (auto& c : comp)
                compText << c.takeId << "[" << juce::String (c.startSample / (double) rate, 1) << "-" << juce::String (c.endSample / (double) rate, 1) << "] ";
            expect (comp.size() == 2 && comp[0].takeId == "take3" && comp[1].takeId == "take2", compText);
            if (comp.size() == 2)
            {
                expectEquals (comp[0].startSample, in);
                expectEquals (comp[1].endSample, out);
            }

            // もう一度 REC：ループの続きではなく、ふつうに範囲の前から録る
            ui.setRecording (true);
            expect (ui.get().isRecording);
            ui.setPlaying (false);
            ui.attachEngine (nullptr);
        }

        beginTest ("with the loop off, recording the range stops at its end as before");
        {
            FakeEngine engine;
            UiSession ui;
            ui.attachEngine (&engine);
            prepare (ui, false);
            const auto before = (int) mainTakes (ui).size();
            ui.setRecording (true);
            pass (ui, engine, out + rate / 2 + 10);
            expect (! ui.get().isRecording && ! engine.recording && ! engine.playing, "it stops at the end of the range");
            expectEquals (engine.recordingsStarted, 1);
            expectEquals ((int) mainTakes (ui).size(), before + 1);
            ui.attachEngine (nullptr);
        }

        UiSession::projectFolderFor (songName).deleteRecursively();
        for (int n = 2; n < 5; ++n)
            UiSession::projectFolderFor (songName + " (" + juce::String (n) + ")").deleteRecursively();
        work.deleteRecursively();
    }
};

static SessionLoopRecordTests sessionLoopRecordTests;
} // namespace vb::test
