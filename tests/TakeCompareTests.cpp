#include "project/TakeCompare.h"
#include "project/Comp.h"
#include "project/ProjectFile.h"

namespace vb::project
{
namespace
{
    Take take (const juce::String& id, int64 start, int64 end, int minutesAgo = 0, RecMode mode = RecMode::delivery)
    {
        Take t;
        t.id = id;
        t.path = "Audio/Takes/main_" + id + ".wav";
        t.startSample = start;
        t.endSample = end;
        t.recMode = mode;
        t.created = juce::Time (2026, 9, 2, 12, 0) - juce::RelativeTime::minutes (minutesAgo);
        return t;
    }

    juce::String describe (const std::vector<CompSegment>& comp)
    {
        juce::String s;
        for (auto& c : comp)
            s << c.takeId << ':' << c.startSample << '-' << c.endSample << ' ';
        return s.trim();
    }

    /** take1 で通し（0..1000）、take2 で 300..600 を録り直した Main。take3 は 200..800 だけ、take4 はリハーサル */
    Track recorded()
    {
        Track t;
        t.type = TrackType::main;
        applyTake (t, take ("take1", 0, 1000, 30));
        applyTake (t, take ("take2", 300, 600, 20));
        applyTake (t, take ("take3", 200, 800, 10), 200, 250);   // 採用は頭の 50 だけ（残りはファイルにある）
        t.takes.push_back (take ("take4", 0, 1000, 5, RecMode::practice));
        return t;
    }
}

class TakeCompareTests : public juce::UnitTest
{
public:
    TakeCompareTests() : juce::UnitTest ("TakeCompare", "VoiceBooth") {}

    void runTest() override
    {
        beginTest ("candidates: delivery takes with sound in the range, newest first");
        {
            const auto t = recorded();
            expectEquals (describe (t.comp), juce::String ("take1:0-200 take3:200-250 take1:250-300 take2:300-600 take1:600-1000"));   // 前提

            auto ids = [] (const std::vector<const Take*>& v)
            {
                juce::StringArray a;
                for (auto* k : v) a.add (k->id);
                return a.joinIntoString (",");
            };
            // リハーサル（take4）は入らない。新しい順
            expectEquals (ids (compareCandidates (t, 0, 1000)), juce::String ("take3,take2,take1"));
            // 範囲に音が無いテイクは入らない（take2 は 300..600、take3 は 200..800）
            expectEquals (ids (compareCandidates (t, 850, 950)), juce::String ("take1"));
            expectEquals (ids (compareCandidates (t, 100, 200)), juce::String ("take1"));

            // 時刻が同じ（古い形式・見本）なら番号の大きい方を新しいとみなす
            Track same;
            for (auto id : { "take2", "take10", "take1" })
            {
                auto k = take (id, 0, 100);
                k.created = juce::Time();
                same.takes.push_back (k);
            }
            expectEquals (ids (compareCandidates (same, 0, 100)), juce::String ("take10,take2,take1"));

            // 曲の頭より前（遅れの補正で負）だけのテイクは比べられない
            Track early;
            early.takes.push_back (take ("take1", -500, -10));
            expect (compareCandidates (early, 0, 1000).empty());
        }

        beginTest ("coverage and share: how much of the range a take can fill / fills now");
        {
            const auto t = recorded();
            expectWithinAbsoluteError (takeCoverage (t.takes[2], 0, 1000), 0.6f, 1.0e-6f);    // take3 は 200..800
            expectWithinAbsoluteError (takeCoverage (t.takes[0], 0, 1000), 1.0f, 1.0e-6f);
            expectWithinAbsoluteError (takeCoverage (t.takes[1], 0, 300), 0.0f, 1.0e-6f);
            expectWithinAbsoluteError (compShare (t.comp, "take1", 0, 1000), 0.65f, 1.0e-6f);  // 0..200・250..300・600..1000
            expectWithinAbsoluteError (compShare (t.comp, "take2", 300, 600), 1.0f, 1.0e-6f);
            expectWithinAbsoluteError (compShare (t.comp, "take3", 0, 1000), 0.05f, 1.0e-6f);
            expectWithinAbsoluteError (compShare (t.comp, "take2", 10, 10), 0.0f, 1.0e-6f);    // 空の範囲で割らない
        }

        beginTest ("segment pick: clicking the comp bar gives that segment, or the gap between segments");
        {
            const auto t = recorded();
            auto span = [&] (int64 at) { auto s = compSpanAt (t, at, 1200); return juce::String (s.first) + "-" + juce::String (s.second); };
            expectEquals (span (0), juce::String ("0-200"));
            expectEquals (span (220), juce::String ("200-250"));
            expectEquals (span (270), juce::String ("250-300"));
            expectEquals (span (450), juce::String ("300-600"));
            expectEquals (span (999), juce::String ("600-1000"));
            expectEquals (span (1100), juce::String ("1000-1200"));  // 最後の区間の後ろは曲の終わりまで
            auto outside = compSpanAt (t, 1200, 1200);
            expect (outside.second <= outside.first);
            Track gaps;   // 100..200 と 500..700 だけ録った：間をクリックしたら前後の区間の間
            applyTake (gaps, take ("take1", 100, 200));
            applyTake (gaps, take ("take2", 500, 700));
            auto gap = compSpanAt (gaps, 300, 1000);
            expect (gap.first == 200 && gap.second == 500);
            gap = compSpanAt (gaps, 50, 1000);
            expect (gap.first == 0 && gap.second == 100);
            Track empty;
            expectEquals (juce::String (compSpanAt (empty, 5, 100).second), juce::String (100));
        }

        beginTest ("audition: preview replaces only the range, switching takes starts from the original each time");
        {
            auto t = recorded();
            const auto original = t.comp;
            TakeAudition a;
            expect (! a.begin (t, 500, 500));                         // 空の範囲では始めない
            expect (a.begin (t, 300, 600));

            expect (a.preview (t, "take3"));
            expectEquals (describe (t.comp), juce::String ("take1:0-200 take3:200-250 take1:250-300 take3:300-600 take1:600-1000"));
            expect (compIsValid (t));

            expect (a.preview (t, "take1"));                          // 選び直し：take3 の切れ端は残らない
            expectEquals (describe (t.comp), juce::String ("take1:0-200 take3:200-250 take1:250-1000"));
            expectEquals (a.previewing(), juce::String ("take1"));

            expect (! a.preview (t, "take4"));                        // リハーサルは入れない
            expect (! a.preview (t, "nope"));
            expectEquals (a.previewing(), juce::String ("take1"));    // 失敗しても今の試聴のまま

            expect (a.preview (t, {}));                               // 「いまの採用」に戻して聴く
            expectEquals (describe (t.comp), describe (original));
            expect (a.preview (t, "take3"));
            expectEquals ((int) t.takes.size(), 4);                   // テイクは足しも消しもしない
        }

        beginTest ("audition: cancel restores the comp exactly, also after several previews");
        {
            auto t = recorded();
            const auto original = describe (t.comp);
            TakeAudition a;
            a.begin (t, 0, 1000);
            a.preview (t, "take3");
            a.preview (t, "take2");
            a.preview (t, "take1");
            expectEquals (describe (t.comp), juce::String ("take1:0-1000"));
            a.cancel (t);
            expectEquals (describe (t.comp), original);
            expect (! a.isActive());
            a.cancel (t);                                             // 二度目は何もしない
            expectEquals (describe (t.comp), original);
        }

        beginTest ("audition: commit keeps the preview and hands back the original comp for undo");
        {
            auto t = recorded();
            const auto original = describe (t.comp);
            TakeAudition a;
            a.begin (t, 300, 600);
            a.preview (t, "take3");
            const auto undo = a.commit (t);
            expect (undo.has_value());
            expectEquals (describe (t.comp), juce::String ("take1:0-200 take3:200-250 take1:250-300 take3:300-600 take1:600-1000"));
            if (undo.has_value())
            {
                expectEquals (describe (*undo), original);
                t.comp = *undo;                                       // Ctrl / ⌘+Z と同じ
                expectEquals (describe (t.comp), original);
            }
            expect (! a.isActive());

            // 何も選ばずに確定・元と同じテイクを選んで確定は「変更なし」（取り消しの対象を上書きしない）
            TakeAudition b;
            b.begin (t, 0, 1000);
            expect (! b.commit (t).has_value());
            expectEquals (describe (t.comp), original);
            TakeAudition c;
            c.begin (t, 300, 600);
            c.preview (t, "take2");
            expect (! c.commit (t).has_value());
            expectEquals (describe (t.comp), original);
        }

        beginTest ("audition: a take shorter than the range keeps the comp outside the take");
        {
            auto t = recorded();
            TakeAudition a;
            a.begin (t, 100, 900);
            a.preview (t, "take3");                                   // take3 は 200..800 だけ
            expectEquals (describe (t.comp), juce::String ("take1:0-200 take3:200-800 take1:800-1000"));
            a.cancel (t);
        }

        beginTest ("audition: saving during a preview writes the committed comp, not the preview");
        {
            Project p;
            p.sampleRate = 48000;
            p.lengthSamples = 1000;
            p.tracks.push_back (recorded());
            const auto original = describe (p.tracks[0].comp);

            TakeAudition a;
            a.begin (p.tracks[0], 0, 1000);
            a.preview (p.tracks[0], "take3");
            auto copy = p;
            a.restoreCommitted (copy);
            const auto back = fromJson (toJson (copy, {}));
            expect (back.ok);
            if (back.ok)
                if (const auto* t = back.project.findTrack (TrackType::main))
                    expectEquals (describe (t->comp), original);
            expectEquals (describe (p.tracks[0].comp), juce::String ("take1:0-200 take3:200-800 take1:800-1000"));   // 画面は試聴のまま

            a.cancel (p.tracks[0]);
            auto after = p;
            a.restoreCommitted (after);                               // 終わった後は何もしない
            expectEquals (describe (after.tracks[0].comp), original);
        }
    }
};

static TakeCompareTests takeCompareTests;
} // namespace vb::project
