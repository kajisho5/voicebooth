#include "ui/screens/HelpDialog.h"
#include "session/FakeEngine.h"
#include "session/SessionTestUtil.h"
#include <set>

/*  困ったときのヘルプ（2026-10-04）：全部の項目を 1 回ずつ並べる。いまの状態に関係する項目を先に並べて開いておく。
    項目ごとの文言と、移る先のキーの文言が翻訳表にある */

namespace vb::test
{
/** 出力の機器が開いていないエンジン（音が出ない状態） */
class NoOutputEngine final : public FakeEngine
{
public:
    audio::OutputStatus getOutputStatus() const override { return {}; }
};

class HelpTests : public juce::UnitTest
{
public:
    HelpTests() : juce::UnitTest ("Help", "VoiceBoothSession") {}

    void runTest() override
    {
        beginTest ("every topic is listed once, with its title and text translated");
        {
            UiSession ui;
            const auto items = help::items (ui);
            expectEquals ((int) items.size(), help::numTopics);
            std::set<int> seen;
            for (auto& i : items)
            {
                seen.insert ((int) i.topic);
                expect (! i.relevant, "nothing is known without an audio engine");
                expect (i18n::has (i18n::current(), help::titleKey (i.topic)), help::titleKey (i.topic));
                expect (i18n::has (i18n::current(), help::bodyKey (i.topic)), help::bodyKey (i.topic));
                if (const auto* k = help::actionKey (help::action (i.topic)))
                    expect (i18n::has (i18n::current(), k), k);
            }
            expectEquals ((int) seen.size(), help::numTopics);
        }

        beginTest ("a closed output puts \"no sound\" first, marked and opened");
        {
            NoOutputEngine engine;
            UiSession ui;
            ui.attachEngine (&engine);
            ui.tick (1.1);   // 機器の状態を読む
            const auto items = help::items (ui);
            expect (! items.empty() && items.front().topic == help::Topic::noSound && items.front().relevant);

            HelpDialog dlg (ui);
            expectEquals (dlg.openIndex(), 0, "the first relevant topic is open");
            dlg.openItem (3);
            expectEquals (dlg.openIndex(), 3);
            dlg.openItem (-1);
            expectEquals (dlg.openIndex(), -1);
            ui.attachEngine (nullptr);
        }

        beginTest ("with a working device nothing about sound is marked, and nothing is opened");
        {
            FakeEngine engine;
            UiSession ui;
            ui.attachEngine (&engine);
            ui.tick (1.1);
            for (auto& i : help::items (ui))
                expect (! (i.relevant && (i.topic == help::Topic::noSound || i.topic == help::Topic::noMic)));
            HelpDialog dlg (ui);
            expectEquals (dlg.openIndex(), -1);
            ui.attachEngine (nullptr);
        }
    }
};

static HelpTests helpTests;
} // namespace vb::test
