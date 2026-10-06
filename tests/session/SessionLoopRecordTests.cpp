#include "session/FakeEngine.h"
#include "session/SessionTestUtil.h"

/*  範囲をループして何回も録る（#29。DESIGN B10）：範囲とループを点けて録ると、範囲の終わり（＋余韻 0.5 秒）で今のテイクを閉じ、
    範囲の少し前へ戻って、止めずに次のテイクを録る。止めた時の途中のテイクは残すが、採用は前の周回のまま。
    ループを消していれば（録音中に消しても）範囲の終わりで止まる。範囲に届く前に止めたテイクは採用を変えない。
    偽のエンジン（tests/session/FakeEngine.h）の録音は、止めた時に録った長さの無音の WAV を書く */

namespace vb::test
{
/** n 回目の録音を始められない（周回の途中で機器が外れた など）・seek の回数を数えるエンジン */
struct LoopProbeEngine : FakeEngine
{
    int failOnStart = -1;
    int seeks = 0;
    juce::String startRecording (const juce::File& f, bool asFloat, audio::int64 latency) override
    {
        if (recordingsStarted + 1 == failOnStart)
        {
            ++recordingsStarted;
            return "no device";
        }
        return FakeEngine::startRecording (f, asFloat, latency);
    }
    void seek (audio::int64 sample) override { ++seeks; FakeEngine::seek (sample); }
};

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

            const auto enablesBefore = engine.loopEnables;
            pass (ui, engine, out + rate / 2 + ui.get().recordingLatency + 10);   // 範囲の終わり＋余韻
            expect (ui.get().isRecording && engine.recording && engine.playing, "it goes on without stopping");
            expectEquals (engine.loopEnables, enablesBefore, "the engine loop is not turned on between passes (the range head would sound)");
            expect (! engine.loopEnabled);
            expectEquals (engine.recordingsStarted, 2, "the next take starts");
            expectEquals (engine.playhead, preroll, "back to a little before the range");
            expectEquals ((int) mainTakes (ui).size(), 1);

            pass (ui, engine, out + rate / 2 + ui.get().recordingLatency + 10);
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

            // 採用：途中で止めた周回（take3）は残すだけで、採用は前の周回（take2）のまま（範囲の頭を切れ端で上書きしない）
            const auto& comp = ui.get().project.findTrack (project::TrackType::main)->comp;
            juce::String compText;
            for (auto& c : comp)
                compText << c.takeId << "[" << juce::String (c.startSample / (double) rate, 1) << "-" << juce::String (c.endSample / (double) rate, 1) << "] ";
            expect (comp.size() == 1 && comp[0].takeId == "take2", compText);
            if (comp.size() == 1)
            {
                expectEquals (comp[0].startSample, in);
                expectEquals (comp[0].endSample, out);
            }
            expectEquals (ui.get().noticeText, tr ("record.loopPartial", "take3"));

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
            pass (ui, engine, out + rate / 2 + ui.get().recordingLatency + 10);
            expect (! ui.get().isRecording && ! engine.recording && ! engine.playing, "it stops at the end of the range");
            expectEquals (engine.recordingsStarted, 1);
            expectEquals ((int) mainTakes (ui).size(), before + 1);
            ui.attachEngine (nullptr);
        }

        beginTest ("turning the loop off while recording stops at the end of this pass, and that pass is used");
        {
            FakeEngine engine;
            UiSession ui;
            ui.attachEngine (&engine);
            prepare (ui, true);
            const auto before = (int) mainTakes (ui).size();
            ui.setRecording (true);
            pass (ui, engine, out + rate / 2 + ui.get().recordingLatency + 10);
            expectEquals (engine.recordingsStarted, 2);
            ui.setLoop (false);
            pass (ui, engine, out + rate / 2 + ui.get().recordingLatency + 10);
            expect (! ui.get().isRecording && ! engine.playing, "it stops at the end of the range");
            expectEquals (engine.recordingsStarted, 2, "no third pass");
            expectEquals ((int) mainTakes (ui).size(), before + 2);
            const auto& comp = ui.get().project.findTrack (project::TrackType::main)->comp;
            expect (! comp.empty() && comp.back().takeId == mainTakes (ui).back().id, "the last full pass is used");
            ui.attachEngine (nullptr);
        }

        beginTest ("stopping before the range keeps the take but does not change what is used, and Ctrl+Z still works");
        {
            FakeEngine engine;
            UiSession ui;
            ui.attachEngine (&engine);
            prepare (ui, false);
            ui.setRecording (true);
            pass (ui, engine, out + rate / 2 + ui.get().recordingLatency + 10);   // 1 本録って採用
            const auto compAfter = ui.get().project.findTrack (project::TrackType::main)->comp;
            expect (ui.get().canUndoTake);
            ui.setRecording (true);
            pass (ui, engine, in - rate / 4);          // 範囲の手前で止める
            ui.setPlaying (false);
            const auto& comp = ui.get().project.findTrack (project::TrackType::main)->comp;
            expect (comp.size() == compAfter.size() && (comp.empty() || comp.back().takeId == compAfter.back().takeId), "what is used does not change");
            expectEquals (ui.get().noticeText, tr ("record.notReached", mainTakes (ui).back().id));
            expect (ui.get().canUndoTake, "Ctrl+Z still undoes the take before");
            const auto usedId = compAfter.empty() ? juce::String() : compAfter.back().takeId;
            ui.undoTake();
            bool stillUsed = false;
            for (auto& c : ui.get().project.findTrack (project::TrackType::main)->comp)
                stillUsed = stillUsed || c.takeId == usedId;
            expect (! stillUsed, "undo takes the previous take out of what is used");
            ui.attachEngine (nullptr);
        }

        beginTest ("a range that ends at the end of the song still goes on to the next pass");
        {
            FakeEngine engine;
            UiSession ui;
            ui.attachEngine (&engine);
            prepare (ui, true);
            const auto length = ui.get().project.lengthSamples;
            ui.setRange (length - 2 * rate, length);
            ui.setRecording (true);
            pass (ui, engine, length);   // 位置は曲の終わりで止まる
            expect (ui.get().isRecording, "still recording");
            expectEquals (engine.recordingsStarted, 2, "the next pass starts");
            ui.setPlaying (false);
            ui.attachEngine (nullptr);
        }

        // バグチェック（2026-10-05）で見つかった 4 件
        auto compOf = [] (UiSession& ui) { return ui.get().project.findTrack (project::TrackType::main)->comp; };

        beginTest ("with the loop on, stopping in the middle of the first pass uses what was recorded (there is no earlier pass)");
        {
            FakeEngine engine;
            UiSession ui;
            ui.attachEngine (&engine);
            prepare (ui, true);
            ui.setRecording (true);
            const auto id = ui.get().recordingTake;
            pass (ui, engine, in + rate);
            ui.setPlaying (false);
            bool used = false;
            for (auto& c : compOf (ui))   // 採用区間は時刻順
                used = used || (c.takeId == id && c.startSample == in && c.endSample > in && c.endSample < out);
            expect (used, "the first pass is used from the range start up to where it stopped: " + ui.get().noticeText);
            ui.attachEngine (nullptr);
        }

        beginTest ("when the next pass cannot start, the practice loop comes back");
        {
            LoopProbeEngine engine;
            UiSession ui;
            ui.attachEngine (&engine);
            prepare (ui, true);
            engine.failOnStart = 2;
            ui.setRecording (true);
            pass (ui, engine, out + rate / 2 + ui.get().recordingLatency + 10);
            expect (! ui.get().isRecording, "it stops");
            ui.setPlaying (true);
            expect (engine.loopEnabled, "the engine loops the range again");
            ui.setPlaying (false);
            ui.attachEngine (nullptr);
        }

        beginTest ("with the loop off, a range near the end of the song still waits for the late tail");
        {
            FakeEngine engine;
            UiSession ui;
            ui.attachEngine (&engine);
            prepare (ui, false);
            const auto length = ui.get().project.lengthSamples;
            ui.setRange (length - 2 * rate, length - rate / 10);
            ui.setRecording (true);
            engine.playhead = length;
            engine.playing = false;   // 本物のエンジンは曲の終わりで止まり、遅延の分を録り足す
            ui.tick (0.02);
            expect (ui.get().isRecording, "it waits for the tail instead of closing at once");
            ui.setPlaying (false);
            ui.attachEngine (nullptr);
        }

        beginTest ("moving to the next pass seeks the engine once");
        {
            LoopProbeEngine engine;
            UiSession ui;
            ui.attachEngine (&engine);
            prepare (ui, true);
            ui.setRecording (true);
            const auto before = engine.seeks;
            pass (ui, engine, out + rate / 2 + ui.get().recordingLatency + 10);
            expectEquals (engine.seeks - before, 1, "the pre-roll plays from its start once");
            ui.setPlaying (false);
            ui.attachEngine (nullptr);
        }

        UiSession::projectFolderFor (songName).deleteRecursively();
        for (int n = 2; n < 9; ++n)
            UiSession::projectFolderFor (songName + " (" + juce::String (n) + ")").deleteRecursively();
        work.deleteRecursively();
    }
};

static SessionLoopRecordTests sessionLoopRecordTests;
} // namespace vb::test
