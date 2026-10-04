#include "session/FakeEngine.h"
#include "session/FakeSeparator.h"
#include "session/SessionTestUtil.h"
#include "i18n/I18n.h"

/*  分離の流れ（2026-10-04、UiSession を分ける 3 段目）：偽の分離（tests/session/FakeSeparator.h）で、原曲からオフボを作る流れ
    （makeOffVocal）の成功・準備中の中止・分離中の中止・失敗・引き算中の中止・同じ原曲のときの使い回しを確かめる。
    #50 で直した「準備中・引き算中の［キャンセル］が効かない」「作業ファイルが残る」が戻らないための見張り */

namespace vb::test
{
class SessionSeparationTests : public juce::UnitTest
{
public:
    SessionSeparationTests() : juce::UnitTest ("Session separation", "VoiceBoothSession") {}

    void runTest() override
    {
        const auto tag = juce::String::toHexString (juce::Random::getSystemRandom().nextInt64()).substring (0, 8);
        const auto work = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("vbsep-" + tag);
        const auto cache = work.getChildFile ("cache");
        cache.createDirectory();
        const auto original = writeTone (work.getChildFile ("original-" + tag + ".wav"), 440.0, 3.0);

        struct Result { bool called = false; juce::File made; juce::String error; };
        auto run = [&] (UiSession& ui, Result& r)
        {
            ui.makeOffVocal (original, [&r] (juce::File f, juce::String e) { r.called = true; r.made = f; r.error = e; });
        };
        auto workFiles = [&]
        {
            juce::StringArray names;
            for (auto& f : cache.findChildFiles (juce::File::findFiles, true, "mix.wav;vocals.wav;backing.wav;*.part"))
                names.add (f.getFileName());
            return names;
        };

        beginTest ("a backing track is made from the original: the input goes to the separator at 44.1 kHz, work files are removed");
        {
            auto st = std::make_shared<FakeSeparation>();
            FakeEngine engine;
            UiSession ui;
            ui.attachEngine (&engine);
            ui.setSeparationService (std::make_unique<FakeSeparationService> (st));
            ui.setCacheFolder (cache);
            Result r;
            run (ui, r);
            expect (ui.get().separating);
            expect (pumpUntil ([&] { return st->starts == 1; }, 10000), "the separator starts after the input is prepared");
            expect (st->input.existsAsFile(), "the input copy is written");
            {
                juce::AudioFormatManager fm;
                fm.registerBasicFormats();
                std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (st->input));
                expect (reader != nullptr && std::abs (reader->sampleRate - 44100.0) < 0.5, "the input is 44.1 kHz");
            }
            st->finish (true);
            expect (pumpUntil ([&] { return r.called; }, 10000));
            expect (r.made.existsAsFile(), r.made.getFullPathName());
            expect (r.error.isEmpty(), r.error);
            expect (! ui.get().separating);
            expect (workFiles().isEmpty(), workFiles().joinIntoString (","));
            ui.attachEngine (nullptr);
        }

        beginTest ("the same original again: the made file is used, nothing is separated");
        {
            auto st = std::make_shared<FakeSeparation>();
            FakeEngine engine;
            UiSession ui;
            ui.attachEngine (&engine);
            ui.setSeparationService (std::make_unique<FakeSeparationService> (st));
            ui.setCacheFolder (cache);
            Result r;
            run (ui, r);
            expect (r.called && r.made.existsAsFile() && r.error.isEmpty());
            expectEquals (st->starts, 0);
            ui.attachEngine (nullptr);
        }
        for (auto& d : cache.getChildFile ("offvocal").findChildFiles (juce::File::findDirectories, false))
            d.deleteRecursively();   // 以降は作り直す

        beginTest ("cancel while the input is being prepared: the separator never starts");
        {
            auto st = std::make_shared<FakeSeparation>();
            FakeEngine engine;
            UiSession ui;
            ui.attachEngine (&engine);
            ui.setSeparationService (std::make_unique<FakeSeparationService> (st));
            ui.setCacheFolder (cache);
            Result r;
            run (ui, r);
            ui.stopSeparation();   // 準備（読み込み・44.1 kHz・mix.wav）の最中
            expect (pumpUntil ([&] { return r.called; }, 10000));
            expectEquals (st->starts, 0);
            expect (r.made == juce::File());
            expectEquals (r.error, tr ("separation.stopped"));
            expect (! ui.get().separating);
            pump (300);
            expect (workFiles().isEmpty(), workFiles().joinIntoString (","));
            ui.attachEngine (nullptr);
        }

        beginTest ("cancel while separating: stopped, nothing is made, work files are removed");
        {
            auto st = std::make_shared<FakeSeparation>();
            FakeEngine engine;
            UiSession ui;
            ui.attachEngine (&engine);
            ui.setSeparationService (std::make_unique<FakeSeparationService> (st));
            ui.setCacheFolder (cache);
            Result r;
            run (ui, r);
            expect (pumpUntil ([&] { return st->starts == 1; }, 10000));
            ui.stopSeparation();
            expect (pumpUntil ([&] { return r.called; }, 10000));
            expect (r.made == juce::File());
            expectEquals (r.error, tr ("separation.stopped"));
            expect (! ui.get().separating);
            expect (workFiles().isEmpty(), workFiles().joinIntoString (","));
            ui.attachEngine (nullptr);
        }

        beginTest ("the separator fails: the reason is passed on, work files are removed");
        {
            auto st = std::make_shared<FakeSeparation>();
            FakeEngine engine;
            UiSession ui;
            ui.attachEngine (&engine);
            ui.setSeparationService (std::make_unique<FakeSeparationService> (st));
            ui.setCacheFolder (cache);
            Result r;
            run (ui, r);
            expect (pumpUntil ([&] { return st->starts == 1; }, 10000));
            st->finish (false, "model error");
            expect (pumpUntil ([&] { return r.called; }, 10000));
            expect (r.made == juce::File());
            expect (r.error.isNotEmpty() && r.error != tr ("separation.stopped"), r.error);
            expect (! ui.get().separating);
            expect (workFiles().isEmpty(), workFiles().joinIntoString (","));
            ui.attachEngine (nullptr);
        }

        beginTest ("cancel while subtracting the vocals: the backing track is not opened and not kept");
        {
            auto st = std::make_shared<FakeSeparation>();
            FakeEngine engine;
            UiSession ui;
            ui.attachEngine (&engine);
            ui.setSeparationService (std::make_unique<FakeSeparationService> (st));
            ui.setCacheFolder (cache);
            Result r;
            run (ui, r);
            expect (pumpUntil ([&] { return st->starts == 1; }, 10000));
            st->finish (true);      // 分離が終わり、引き算がバックグラウンドで始まる
            ui.stopSeparation();    // その最中
            expect (pumpUntil ([&] { return r.called; }, 10000));
            expect (r.made == juce::File());
            expectEquals (r.error, tr ("separation.stopped"));
            expect (! ui.get().separating);
            int kept = 0;
            for (auto& f : cache.getChildFile ("offvocal").findChildFiles (juce::File::findFiles, true, "*.wav"))
                kept += f.getFileName().contains ("off vocal") ? 1 : 0;
            expectEquals (kept, 0);
            ui.attachEngine (nullptr);
        }

        beginTest ("the time estimate covers the measured times (4 cores: 4-minute song, one model, 1,750 s / 30-second song, 109 s)");
        {
            const auto four = UiSession::estimateSeparation (240.0, 1, 4);
            expect (four.lowSeconds < 1750.0 && 1750.0 < four.highSeconds, juce::String (four.lowSeconds) + " - " + juce::String (four.highSeconds));
            expectEquals (four.lowMinutes(), 14);
            expectEquals (four.highMinutes(), 32);
            const auto short30 = UiSession::estimateSeparation (30.0, 1, 4);
            expect (short30.lowSeconds < 109.0 && 109.0 < short30.highSeconds);
            // リードも分けると倍、コアが半分なら倍。コアが多くても短くは見積もらない（測っていない）
            expectWithinAbsoluteError (UiSession::estimateSeparation (240.0, 2, 4).highSeconds, four.highSeconds * 2.0, 0.01);
            expectWithinAbsoluteError (UiSession::estimateSeparation (240.0, 1, 2).highSeconds, four.highSeconds * 2.0, 0.01);
            expectWithinAbsoluteError (UiSession::estimateSeparation (240.0, 1, 16).highSeconds, four.highSeconds, 0.01);
            // 長さが分からない（読めない）ときも 1 分〜2 分と表示する（0 分と表示しない）
            const auto unknown = UiSession::estimateSeparation (0.0, 1, 4);
            expectEquals (unknown.lowMinutes(), 1);
            expectEquals (unknown.highMinutes(), 2);
        }

        work.deleteRecursively();
    }
};

static SessionSeparationTests sessionSeparationTests;
} // namespace vb::test
