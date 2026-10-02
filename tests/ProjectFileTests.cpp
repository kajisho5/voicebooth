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
            p.lyrics.headings.push_back ({ "Chorus", 1 });
            p.lyrics.chorusBlocks = { 1 };

            ProjectExtras ex;
            ex.guidePath = "Audio/guide.wav";
            ex.recordRate = 96000.0;
            ex.recordFloat = true;

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
            expectEquals (back.project.tempo.signature.numerator, 6);
        }

        beginTest ("not a project, broken JSON and a newer format are refused with a reason");
        {
            expectEquals (fromJson ("{ nope").error, juce::String ("project.error.notProject"));
            expectEquals (fromJson ("{\"format\":\"other\"}").error, juce::String ("project.error.notProject"));
            expectEquals (fromJson ("{\"format\":\"voicebooth.project\",\"format_version\":99}").error, juce::String ("project.error.newer"));
            const auto minimal = fromJson ("{\"format\":\"voicebooth.project\"}");
            expect (minimal.ok);
            expect (minimal.project.tracks.empty());
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
    }
};

static ProjectFileTests projectFileTests;
} // namespace vb::project
