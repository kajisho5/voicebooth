#include "session/FakeEngine.h"
#include "session/SessionTestUtil.h"

/*  録音の流れ（B5 通し・B10 範囲の録り直し・6.1 本番 / リハーサル）：REC → 止める で、テイクのファイル・一覧・採用区間・.vbooth が
    そろうか。Esc の「破棄する」はファイルも消す。Ctrl / ⌘+Z は採用だけ戻してファイルは残す。リハーサルは採用に入れず、
    「本番に入れる」で Audio/Takes へ移して採用する。録れないときは理由を知らせて何もしない。
    偽のエンジン（tests/session/FakeEngine.h）の録音は、止めたときに録った長さの無音の WAV を書く。往復の遅延は測っていない（0） */

namespace vb::test
{
/** 遡及録音（B7）の試し用：録っている間の 10 ms ごとのピークを返す。エンジンの位置で voiceAt から先は声（-10 dB）、その前は無音 */
class RetroEngine final : public FakeEngine
{
public:
    int recordingEnvelope (std::vector<float>& env) const override
    {
        constexpr int hop = 480;
        env.clear();
        if (! recording)
            return 0;
        for (auto at = recordStart; at < juce::jmax (playhead, recordedTo); at += hop)
            env.push_back (voiceAt >= 0 && at >= voiceAt ? 0.3f : 0.0001f);
        return hop;
    }
    audio::int64 voiceAt = -1;
};

class SessionRecordTests : public juce::UnitTest
{
public:
    SessionRecordTests() : juce::UnitTest ("Session recording", "VoiceBoothSession") {}

    static constexpr juce::int64 rate = 48000;   // 曲（writeTone）の SR。ラムダから捕まえずに使うのでクラスに置く（MSVC）

    void runTest() override
    {
        const auto tag = juce::String::toHexString (juce::Random::getSystemRandom().nextInt64()).substring (0, 8);
        const auto work = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("vbrec-" + tag);
        const auto songName = "vbrec-" + tag;
        const auto song = writeTone (work.getChildFile (songName + ".wav"), 440.0, 12.0);
        const auto projectFolder = UiSession::projectFolderFor (songName);
        const auto vbooth = projectFolder.getChildFile (projectFolder.getFileName() + project::fileExtension);

        auto prepare = [&] (UiSession& ui, project::Mode mode = project::Mode::standard)
        {
            expect (openSong (ui, song));
            ui.setMode (mode);
            ui.setRecMode (project::RecMode::delivery);
            ui.setCountIn (0);
            ui.clearRange();
            if (! ui.get().trackUi[0].armed)
                ui.armTrack (0);   // 押すたびに切り替わる（Main は最初からアーム）
            ui.tick (1.1);         // 入力の状態を読む
        };
        auto mainTrack = [] (UiSession& ui) -> const project::Track&
        {
            return *ui.get().project.findTrack (project::TrackType::main);
        };
        auto sec = [] (juce::int64 sample) { return juce::String (sample / (double) rate, 2); };
        auto compText = [] (const project::Track& t)
        {
            juce::String out;
            for (auto& c : t.comp)
                out << c.takeId << "[" << juce::String (c.startSample / (double) rate, 2) << "-" << juce::String (c.endSample / (double) rate, 2) << "] ";
            return out.trim();
        };
        auto savedTakeIds = [&]
        {
            juce::StringArray ids;
            const auto l = project::fromJson (vbooth.loadFileAsString());
            if (l.ok)
                if (auto* t = l.project.findTrack (project::TrackType::main))
                    for (auto& k : t->takes)
                        ids.add (k.id);
            return ids.joinIntoString (",");
        };
        // 録って止める（再生位置 from から to まで）
        auto record = [&] (UiSession& ui, FakeEngine& engine, juce::int64 from, juce::int64 to)
        {
            ui.seek (from);
            ui.setRecording (true);
            expect (ui.get().isRecording && engine.recording, "recording starts: " + ui.get().noticeText);
            engine.playhead = to;
            ui.setRecording (false);
            expect (! ui.get().isRecording && ! engine.recording);
        };

        beginTest ("a take from the playhead: file, list, comp and the saved project agree");
        {
            projectFolder.deleteRecursively();
            FakeEngine engine;
            UiSession ui;
            ui.attachEngine (&engine);
            prepare (ui);
            record (ui, engine, 2 * rate, 5 * rate);

            const auto& t = mainTrack (ui);
            expectEquals ((int) t.takes.size(), 1);
            if (t.takes.size() == 1)
            {
                const auto& k = t.takes[0];
                expectEquals (k.id, juce::String ("take1"));
                expectEquals (k.path, juce::String ("Audio/Takes/main_take1.wav"));
                expect (projectFolder.getChildFile (k.path).existsAsFile());
                // 往復の遅延（測っていなければバッファーからの見込み）の分だけ前にずらす：歌い手が聞いた伴奏の位置
                expectGreaterThan (k.latencySamples, (juce::int64) 0);
                expectEquals (k.startSample, 2 * rate - k.latencySamples);
                expectEquals (k.endSample, 5 * rate - k.latencySamples);
                expect (k.recMode == project::RecMode::delivery);
                expectEquals (compText (t), "take1[2.00-" + sec (5 * rate - k.latencySamples) + "]", "the take is used from where REC was pressed");
            }
            expect (ui.get().canUndoTake);
            expectEquals (savedTakeIds(), juce::String ("take1"), "the take is written into the .vbooth at once");
            ui.attachEngine (nullptr);
        }

        beginTest ("discarding (Esc) stops, deletes the file and changes nothing");
        {
            projectFolder.deleteRecursively();
            FakeEngine engine;
            UiSession ui;
            ui.attachEngine (&engine);
            prepare (ui);
            ui.seek (1 * rate);
            ui.setRecording (true);
            expect (engine.recording);
            engine.playhead = 4 * rate;
            const auto file = engine.recordFile;
            ui.discardRecording();
            expect (! ui.get().isRecording && ! engine.recording);
            expect (! file.existsAsFile(), "the recorded file is deleted: " + file.getFullPathName());
            expect (mainTrack (ui).takes.empty());
            expect (mainTrack (ui).comp.empty());
            expectEquals (ui.get().noticeText, tr ("record.discarded"));
            ui.attachEngine (nullptr);
        }

        beginTest ("undo puts back the comp before the take, and keeps the take and its file");
        {
            projectFolder.deleteRecursively();
            FakeEngine engine;
            UiSession ui;
            ui.attachEngine (&engine);
            prepare (ui);
            record (ui, engine, 0, 10 * rate);
            const auto latency = mainTrack (ui).takes.empty() ? (juce::int64) 0 : mainTrack (ui).takes[0].latencySamples;
            const auto whole = "take1[0.00-" + sec (10 * rate - latency) + "]";
            expectEquals (compText (mainTrack (ui)), whole);

            // 範囲の録り直し（B10）：採用は範囲の中だけ。範囲の少し前から鳴らす
            ui.setRange (4 * rate, 6 * rate);
            ui.setRecording (true);
            expect (engine.recording);
            expect (engine.playhead < 4 * rate, "it starts a little before the range");
            engine.playhead = 7 * rate;
            ui.setRecording (false);
            expectEquals (compText (mainTrack (ui)), juce::String ("take1[0.00-4.00] take2[4.00-6.00] take1[6.00-") + sec (10 * rate - latency) + "]",
                          "only the range is replaced");

            expect (ui.undoTake());
            expectEquals (compText (mainTrack (ui)), whole);
            expectEquals ((int) mainTrack (ui).takes.size(), 2, "the take stays in the list");
            expect (projectFolder.getChildFile ("Audio/Takes/main_take2.wav").existsAsFile(), "and its file stays");
            expect (! ui.get().canUndoTake);
            expect (! ui.undoTake(), "only one step back");
            ui.attachEngine (nullptr);
        }

        beginTest ("a rehearsal take is kept out of the comp, and can be moved into the takes later");
        {
            projectFolder.deleteRecursively();
            FakeEngine engine;
            UiSession ui;
            ui.attachEngine (&engine);
            prepare (ui);
            ui.setRecMode (project::RecMode::practice);
            record (ui, engine, 3 * rate, 6 * rate);

            const auto& t = mainTrack (ui);
            expectEquals ((int) t.takes.size(), 1);
            expect (t.comp.empty(), "a rehearsal does not change the comp: " + compText (t));
            if (t.takes.size() == 1)
            {
                expect (t.takes[0].recMode == project::RecMode::practice);
                expect (t.takes[0].path.startsWith ("Practice/"), t.takes[0].path);
            }
            expectEquals (ui.get().rescueTakeId, juce::String ("take1"), "the notice offers to use it (recorded at the original speed and key)");

            expect (ui.promoteRehearsalTake (project::TrackType::main, "take1"));
            const auto& after = mainTrack (ui);
            expectEquals ((int) after.takes.size(), 1);
            if (after.takes.size() == 1)
            {
                expect (after.takes[0].recMode == project::RecMode::delivery);
                expect (after.takes[0].path.startsWith ("Audio/Takes/"), after.takes[0].path);
                expect (projectFolder.getChildFile (after.takes[0].path).existsAsFile());
            }
            expect (! projectFolder.getChildFile ("Practice/main_take1.wav").exists(), "the file is moved, not copied");
            if (after.takes.size() == 1)
                expectEquals (compText (after), "take1[3.00-" + sec (6 * rate - after.takes[0].latencySamples) + "]", "used where it was recorded");
            ui.attachEngine (nullptr);
        }

        beginTest ("recording at a practice tempo is put back to the original speed and key first");
        {
            projectFolder.deleteRecursively();
            FakeEngine engine;
            UiSession ui;
            ui.attachEngine (&engine);
            prepare (ui);
            ui.setPractice (90, -2);
            record (ui, engine, 0, 2 * rate);
            expectEquals (ui.get().tempoPercent, 100);
            expectEquals (ui.get().keyShift, 0);
            const auto& t = mainTrack (ui);
            expectEquals ((int) t.takes.size(), 1);
            if (t.takes.size() == 1)
            {
                expectEquals (t.takes[0].tempoPercent, 100);
                expectEquals (t.takes[0].keyShift, 0);
            }
            ui.attachEngine (nullptr);
        }

        beginTest ("a file with the next name already in the folder is not overwritten");
        {
            projectFolder.deleteRecursively();
            FakeEngine engine;
            UiSession ui;
            ui.attachEngine (&engine);
            prepare (ui);
            const auto stray = projectFolder.getChildFile ("Audio/Takes/main_take1.wav");
            stray.getParentDirectory().createDirectory();
            expect (stray.replaceWithText ("an earlier voice"));
            record (ui, engine, 0, 2 * rate);
            expectEquals (stray.loadFileAsString(), juce::String ("an earlier voice"));
            const auto& t = mainTrack (ui);
            expectEquals ((int) t.takes.size(), 1);
            if (t.takes.size() == 1)
                expectEquals (t.takes[0].id, juce::String ("take2"));
            ui.attachEngine (nullptr);
        }

        beginTest ("nothing to record on: the reason is shown and nothing starts");
        {
            projectFolder.deleteRecursively();
            FakeEngine engine;
            UiSession ui;
            ui.attachEngine (&engine);
            prepare (ui);
            ui.armTrack (0);   // アームを外す
            expect (! ui.get().trackUi[0].armed);
            ui.setRecording (true);
            expect (! ui.get().isRecording && ! engine.recording);
            expectEquals (engine.recordingsStarted, 0);
            expectEquals (ui.get().noticeText, tr ("record.problem.noArm"));
            ui.attachEngine (nullptr);
        }

        beginTest ("easy mode records straight through even with IN / OUT set");
        {
            projectFolder.deleteRecursively();
            FakeEngine engine;
            UiSession ui;
            ui.attachEngine (&engine);
            prepare (ui, project::Mode::easy);
            ui.setRange (4 * rate, 6 * rate);
            ui.seek (1 * rate);
            ui.setRecording (true);
            expectEquals (engine.playhead, 1 * rate, "it starts where it is, not before the range");
            engine.playhead = 8 * rate;
            ui.setRecording (false);
            const auto& t = mainTrack (ui);
            if (t.takes.size() == 1)
                expectEquals (compText (t), "take1[1.00-" + sec (8 * rate - t.takes[0].latencySamples) + "]");
            ui.attachEngine (nullptr);
        }

        beginTest ("pressing REC late while playing takes the phrase from where the singing started (retro recording)");
        {
            projectFolder.deleteRecursively();
            RetroEngine engine;
            UiSession ui;
            ui.attachEngine (&engine);
            prepare (ui);
            ui.seek (1 * rate);
            ui.setPlaying (true);
            ui.tick (0.02);
            expect (engine.recording, "playing with an armed track records in the background");
            const auto shadow = engine.recordFile;
            expect (shadow.getFileName().startsWith (".retro-"), shadow.getFileName());

            // 3 秒から歌い始め（入力に届くのは往復の遅延の後）、4 秒で REC を押す
            const auto latency = ui.get().recordingLatency;
            engine.voiceAt = 3 * rate + latency;
            engine.playhead = 4 * rate;
            ui.tick (0.02);
            ui.setRecording (true);
            expect (ui.get().isRecording);
            expectEquals (engine.recordingsStarted, 1, "the background recording becomes the take (no new file)");
            const auto head = 3 * rate - rate / 20;   // フレーズの頭の 50 ms 手前（子音・息）
            expectWithinAbsoluteError ((double) ui.get().recordStart, (double) head, 2.0 * 480.0,
                                       "the take is used from the start of the phrase, not from the press");

            engine.playhead = 6 * rate;
            ui.setRecording (false);
            const auto& t = mainTrack (ui);
            expectEquals ((int) t.takes.size(), 1);
            if (t.takes.size() == 1)
            {
                expectEquals (t.takes[0].path, juce::String ("Audio/Takes/main_take1.wav"), "renamed to the take's name");
                expect (projectFolder.getChildFile (t.takes[0].path).existsAsFile());
                expectEquals (t.takes[0].startSample, 1 * rate - latency, "the file keeps what was recorded before the press");
            }
            expect (! shadow.exists(), "the background file was moved, not left behind");
            expect (t.comp.size() == 1 && std::abs (t.comp[0].startSample - head) <= 2 * 480, compText (t));
            ui.attachEngine (nullptr);
        }

        beginTest ("pressing REC late with nothing sung yet takes from the press");
        {
            projectFolder.deleteRecursively();
            RetroEngine engine;
            UiSession ui;
            ui.attachEngine (&engine);
            prepare (ui);
            ui.seek (1 * rate);
            ui.setPlaying (true);
            ui.tick (0.02);
            engine.playhead = 4 * rate;
            ui.tick (0.02);
            ui.setRecording (true);
            expectEquals (ui.get().recordStart, 4 * rate);
            engine.playhead = 6 * rate;
            ui.setRecording (false);
            const auto& t = mainTrack (ui);
            expect (t.comp.size() == 1 && t.comp[0].startSample == 4 * rate, compText (t));
            ui.attachEngine (nullptr);
        }

        beginTest ("stopping without pressing REC leaves no file from the background recording");
        {
            projectFolder.deleteRecursively();
            RetroEngine engine;
            UiSession ui;
            ui.attachEngine (&engine);
            prepare (ui);
            ui.setPlaying (true);
            ui.tick (0.02);
            const auto shadow = engine.recordFile;
            expect (engine.recording);
            engine.playhead = 3 * rate;
            ui.setPlaying (false);
            ui.tick (0.02);
            expect (! engine.recording);
            expect (! shadow.exists(), shadow.getFullPathName());
            expect (mainTrack (ui).takes.empty());
            ui.attachEngine (nullptr);
        }

        projectFolder.deleteRecursively();
        work.deleteRecursively();
    }
};

static SessionRecordTests sessionRecordTests;
} // namespace vb::test
