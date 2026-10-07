#include "project/ProjectFile.h"

/*  プロジェクトファイル `.vbooth`（B14）：例のファイルが読めること、書いて読むと同じになること、読まない時の理由 */

namespace vb::project
{
class ProjectFileTests : public juce::UnitTest
{
public:
    ProjectFileTests() : juce::UnitTest ("ProjectFile", "VoiceBooth") {}

    void runTest() override
    {
        beginTest ("the example project (app/project/project.example.json) loads with every field");
        {
            const auto file = juce::File (VOICEBOOTH_TEST_DATA_DIR).getChildFile ("../../app/project/project.example.json");
            expect (file.existsAsFile());
            const auto r = fromJson (file.loadFileAsString());
            expect (r.ok, r.error);
            const auto& p = r.project;
            expectEquals (p.sampleRate, 48000);
            expectEquals (p.lengthSamples, (juce::int64) 6528000);
            expectEquals (p.key.tonic, 9);
            expect (p.key.minor && p.key.source == song::Source::confirmed);
            expectEquals (p.tempo.bpm, 128.0);
            expectEquals (p.tempo.downbeatSample, (juce::int64) 59520);
            const auto* main = p.findTrack (TrackType::main);
            expect (main != nullptr);
            expectEquals ((int) main->takes.size(), 2);
            expectEquals ((int) main->comp.size(), 3);
            expect (main->takes[1].clip);
            expectEquals (main->comp[1].takeId, juce::String ("take4"));
            expectEquals ((int) p.sections.size(), 5);
            expectEquals (p.sections[3].name, juce::String::fromUTF8 ("\xe3\x83\xa9\xe3\x83\x83\xe3\x83\x97"));   // ラップ
            expect (p.sections[4].source == song::Source::estimated);
            expectEquals ((int) p.lyrics.lines.size(), 4);
            expect (p.lyrics.lines[0].timed());
            expect (! p.lyrics.lines[3].timed());
            expectEquals (p.lyrics.encoding, juce::String ("Shift_JIS"));
        }

        beginTest ("save then load gives the same project (positions stay integers)");
        {
            Project p;
            p.songPath = "Audio/song.wav";
            p.sampleRate = 44100;
            p.bitDepthExport = 32;
            p.lengthSamples = 9876543210LL;   // 32bit を超える位置
            p.key = { 2, false, song::Source::estimated, 0.4f };
            p.tempo.bpm = 127.35;
            p.tempo.signature = { 6, 8 };
            p.tempo.downbeatSample = 12345;
            p.tempo.source = song::Source::confirmed;
            p.tempo.confidence = 1.0f;
            p.modeLast = Mode::pro;
            Track t;
            t.type = TrackType::harm1;
            Take k;
            k.id = "take3";
            k.path = "Audio/Takes/harm1_take3.wav";
            k.startSample = -538;
            k.endSample = 400000;
            k.created = juce::Time (2026, 9, 2, 1, 2, 3);
            k.peak = 0.5f;
            k.recMode = RecMode::practice;
            k.latencySamples = 538;
            k.useFrom = 1000;       // リハーサルのテイクを本番に入れる時の範囲
            k.useTo = 399000;
            k.tempoPercent = 85;    // 練習録音のテンポ・キー（B11）
            k.keyShift = -2;
            t.takes.push_back (k);
            t.comp.push_back ({ 0, 400000, "take3" });
            p.tracks.push_back (t);
            song::Section sec;
            sec.startSample = 777;
            sec.kind = song::kind::custom;
            sec.name = "Bridge";
            p.sections.push_back (sec);
            p.lyrics.sourceFileName = "a.lrc";
            p.lyrics.lines.push_back ({ "line one", 4800, -1, 0, -1, song::Source::confirmed });
            p.lyrics.lines.push_back ({ "line two", -1, -1, 1, 0, song::Source::confirmed });
            p.lyrics.headings.push_back ({ "Chorus", 1, true });   // 区間を消した見出し（2026-10-05）
            p.lyrics.chorusBlocks = { 1 };

            ProjectExtras ex;
            ex.guidePath = "Audio/guide.wav";
            ex.guideNudgeMs = -12.0;
            ex.recordRate = 96000.0;
            ex.recordFloat = true;
            ex.trackMix = { { TrackType::main, 0.5f, true, false }, { TrackType::harm1, 0.75f, false, true } };
            ex.practiceTempo = 85;
            ex.practiceKey = -3;

            const auto json = toJson (p, ex);
            const auto back = fromJson (json);
            expect (back.ok, back.error);
            expectEquals (toJson (back.project, back.extras), json);
            expectEquals (back.project.lengthSamples, 9876543210LL);
            expectEquals (back.project.tracks[0].takes[0].startSample, (juce::int64) -538);
            expect (back.project.tracks[0].takes[0].created == k.created);
            expectEquals (back.project.tracks[0].takes[0].tempoPercent, 85);
            expectEquals (back.project.tracks[0].takes[0].keyShift, -2);
            expectEquals (back.project.tracks[0].takes[0].useFrom, (juce::int64) 1000);
            expectEquals (back.project.tracks[0].takes[0].useTo, (juce::int64) 399000);
            expect (! json.contains ("\"tempo_percent\": 100"));
            expectEquals (back.extras.guidePath, ex.guidePath);
            expectEquals (back.extras.guideNudgeMs, -12.0);
            expectEquals ((int) back.extras.trackMix.size(), 2);
            expect (back.extras.trackMix[0].mute && ! back.extras.trackMix[0].solo);
            expect (back.extras.trackMix[1].type == TrackType::harm1 && back.extras.trackMix[1].solo);
            expectWithinAbsoluteError (back.extras.trackMix[0].gain, 0.5f, 1e-6f);
            expectEquals (back.extras.practiceTempo, 85);
            expectEquals (back.extras.practiceKey, -3);
            expectEquals (back.project.tempo.signature.numerator, 6);
            expect (back.project.lyrics.headings.size() == 1 && back.project.lyrics.headings[0].noSection, "a removed heading section stays removed");
        }

        beginTest ("monitor levels and mutes, range, loop, selected track, octave up and playhead come back (2026-10-04)");
        {
            Project p;
            p.sampleRate = 48000;
            p.lengthSamples = 48000 * 200;
            ProjectExtras ex;
            ex.monitor.has = true;
            ex.monitor.backing = 0.6f;
            ex.monitor.backingMute = true;
            ex.monitor.guide = 0.3f;
            ex.monitor.harmony = 0.9f;
            ex.monitor.harmonyMute = true;
            ex.monitor.self = 0.55f;
            ex.monitor.reverb = 0.1f;
            ex.work.has = true;
            ex.work.rangeIn = 480000;
            ex.work.rangeOut = 960000;
            ex.work.loop = true;
            ex.work.track = "harm1";
            ex.work.octaveUp = true;
            ex.work.thirdGuide = 2;
            ex.work.playhead = 1234567;
            const auto back = fromJson (toJson (p, ex));
            expect (back.ok);
            const auto& m = back.extras.monitor;
            expect (m.has && m.backingMute && ! m.guideMute && m.harmonyMute);
            expectWithinAbsoluteError (m.backing, 0.6f, 1e-6f);
            expectWithinAbsoluteError (m.guide, 0.3f, 1e-6f);
            expectWithinAbsoluteError (m.harmony, 0.9f, 1e-6f);
            expectWithinAbsoluteError (m.self, 0.55f, 1e-6f);
            expectWithinAbsoluteError (m.reverb, 0.1f, 1e-6f);
            const auto& w = back.extras.work;
            expect (w.has && w.loop && w.octaveUp);
            expectEquals (w.thirdGuide, 2);
            expect (w.rangeIn == 480000 && w.rangeOut == 960000 && w.playhead == 1234567);
            expectEquals (w.track, juce::String ("harm1"));

            // 範囲なしは書かない（-1 のまま戻る）。古いファイル（項目なし）は has = false で既定のまま
            ex.work.rangeIn = ex.work.rangeOut = -1;
            const auto noRange = fromJson (toJson (p, ex));
            expect (noRange.extras.work.rangeIn == -1 && noRange.extras.work.rangeOut == -1);
            const auto old = fromJson ("{\"format\":\"voicebooth.project\"}");
            expect (! old.extras.monitor.has && ! old.extras.work.has);
            // 範囲の外の音量は 0..1 に収める
            const auto wild = fromJson ("{\"format\":\"voicebooth.project\",\"monitor\":{\"backing\":7,\"self\":-2}}");
            expectWithinAbsoluteError (wild.extras.monitor.backing, 1.0f, 1e-6f);
            expectWithinAbsoluteError (wild.extras.monitor.self, 0.0f, 1e-6f);
        }

        beginTest ("not a project, broken JSON and a newer format are refused with a reason");
        {
            expectEquals (fromJson ("{ nope").error, juce::String ("project.error.notProject"));
            expectEquals (fromJson ("{\"format\":\"other\"}").error, juce::String ("project.error.notProject"));
            expectEquals (fromJson ("{\"format\":\"voicebooth.project\",\"format_version\":99}").error, juce::String ("project.error.newer"));
            const auto minimal = fromJson ("{\"format\":\"voicebooth.project\"}");
            expect (minimal.ok);
            expect (minimal.project.tracks.empty());
            expect (minimal.extras.trackMix.empty());
            expectEquals (minimal.extras.practiceTempo, 100);
        }

        beginTest ("writing replaces the file in one step");
        {
            auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                           .getChildFile ("VoiceBoothTests-proj-" + juce::String (juce::Random::getSystemRandom().nextInt64()));
            const auto f = dir.getChildFile ("song/song.vbooth");
            expect (writeAtomically (f, "first"));
            expect (writeAtomically (f, "second"));
            expectEquals (f.loadFileAsString(), juce::String ("second"));
            expectEquals (f.getParentDirectory().getNumberOfChildFiles (juce::File::findFiles), 1);   // 一時ファイルは残らない
            dir.deleteRecursively();
        }

        beginTest ("unused takes on open: used ones stay, others (retro leftovers, a take cut by a crash) move to Audio/Recovered, never deleted");
        {
            auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                           .getChildFile ("VoiceBoothTests-retro-" + juce::String (juce::Random::getSystemRandom().nextInt64()));
            const auto takes = dir.getChildFile ("Audio/Takes");
            expect (takes.createDirectory().wasOk());
            const auto usedRetro = takes.getChildFile (".retro-1-100.wav");   // 名前を付け直せずに採用したテイク
            const auto sung = takes.getChildFile (".retro-2-200.wav");        // REC の後に落ちた：歌った声
            const auto used = takes.getChildFile ("Main_take1.wav");
            const auto crashed = takes.getChildFile ("Main_take2.wav");       // 録音中に落ちて .vbooth に入らなかった
            for (auto& f : { usedRetro, sung, used, crashed })
                expect (f.replaceWithText ("x"));

            const juce::StringArray inUse { "Audio/Takes/.retro-1-100.wav", "Audio/Takes/Main_take1.wav" };
            expectEquals (recoverUnusedTakes (dir, inUse), 2);
            expect (usedRetro.existsAsFile() && used.existsAsFile());
            expect (! sung.exists() && ! crashed.exists());
            expect (dir.getChildFile ("Audio/Recovered/retro-2-200.wav").existsAsFile());
            expect (dir.getChildFile ("Audio/Recovered/Main_take2.wav").existsAsFile());

            expectEquals (recoverUnusedTakes (dir, inUse), 0);   // 2 回目は何もしない

            // リハーサル（Practice/）：使っているものは残し、落ちて .vbooth に入らなかったものは Recovered へ（監査 2026-10-04）
            const auto practice = dir.getChildFile ("Practice");
            expect (practice.createDirectory().wasOk());
            const auto usedPractice = practice.getChildFile ("Main_take3.wav");
            const auto lostPractice = practice.getChildFile ("Main_take4.wav");
            for (auto& f : { usedPractice, lostPractice })
                expect (f.replaceWithText ("x"));
            const juce::StringArray inUse2 { "Audio/Takes/.retro-1-100.wav", "Audio/Takes/Main_take1.wav", "Practice/Main_take3.wav" };
            expectEquals (recoverUnusedTakes (dir, inUse2), 1);
            expect (usedPractice.existsAsFile() && ! lostPractice.exists());
            expect (dir.getChildFile ("Audio/Recovered/Main_take4.wav").existsAsFile());
            dir.deleteRecursively();
        }

        beginTest ("findMedia: the written path first, then the copy inside the project, else the written path");
        {
            auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                           .getChildFile ("VoiceBoothTests-media-" + juce::String (juce::Random::getSystemRandom().nextInt64()));
            const auto copy = dir.getChildFile ("Audio/song.wav");
            expect (copy.getParentDirectory().createDirectory().wasOk());
            expect (copy.replaceWithText ("x"));
            expect (findMedia (dir, "Audio/song.wav", "Audio") == copy);
            // コピーし終える前に保存された元の場所（今は無い）→ プロジェクトの中のコピー
            const auto gone = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("VoiceBoothTests-gone/song.wav");
            expect (findMedia (dir, gone.getFullPathName(), "Audio") == copy);
            // Windows で保存された元の場所（\ 区切り）・/ 区切りのどちらでも、名前でコピーを見つける
            expect (findMedia (dir, "C:\\Users\\someone\\Music\\song.wav", "Audio") == copy);
            expect (findMedia (dir, "C:/Users/someone/Music/song.wav", "Audio") == copy);
            // どちらにも無い：書いてある場所のまま（呼ぶ側が「見つからない」と知らせる）
            expect (! findMedia (dir, "Audio/none.wav", "Audio").existsAsFile());
            dir.deleteRecursively();
        }

        beginTest ("isInsideProject: takes may only point inside the project folder (#17)");
        {
            for (auto ok : { "Audio/Takes/Main_take1.wav", "Audio/Takes/.retro-1-100.wav", "Audio\\Takes\\Main_take2.wav", "Audio/Takes/a..b.wav" })
                expect (isInsideProject (ok), ok);
            for (auto bad : { "", "   ", "../outside.wav", "Audio/../../outside.wav", "Audio\\..\\..\\x.wav", "/etc/passwd",
                              "\\\\server\\share\\x.wav", "C:\\Users\\someone\\x.wav", "C:/x.wav", "~/x.wav", "Audio/Takes/x.wav:stream" })
                expect (! isInsideProject (bad), bad);
        }
    }
};

static ProjectFileTests projectFileTests;
} // namespace vb::project
