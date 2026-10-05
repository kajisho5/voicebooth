#include "ui/screens/HelpDialog.h"
#include "ui/screens/ReportDialog.h"
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

        // ヘルプのメニューと不具合の報告（2026-10-05）
        beginTest ("the help menu and the report are translated in every language");
        {
            const char* keys[] = { "topbar.help", "menu.help", "menu.help.trouble", "menu.help.shortcuts", "menu.help.guide", "menu.help.report",
                                   "menu.help.update", "menu.help.releases", "menu.help.about", "menu.app.settings",
                                   "report.title", "report.micro", "report.intro", "report.env", "report.note", "report.open", "report.copy",
                                   "report.copied", "report.opened", "report.issue.title", "report.issue.what", "report.issue.steps",
                                   "report.issue.env", "report.issue.paste", "help.action.report" };
            for (auto& l : i18n::available())
                for (auto* k : keys)
                    expect (i18n::has (l.id, k), juce::String (l.code) + ": " + k);
        }

        beginTest ("the report carries the app and device, but no song or project name");
        {
            const auto tag = juce::String::toHexString (juce::Random::getSystemRandom().nextInt64()).substring (0, 8);
            const auto work = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("vbreport-" + tag);
            const auto songName = "vbreport-" + tag;
            const auto song = writeTone (work.getChildFile (songName + ".wav"), 440.0, 2.0);

            FakeEngine engine;
            UiSession ui;
            ui.attachEngine (&engine);
            expect (openSong (ui, song));
            ui.tick (1.1);

            const auto report = help::environmentReport (ui);
            expect (report.startsWith ("VoiceBooth " + update::currentVersion()), report);
            expect (report.contains ("OS: ") && report.contains ("Audio driver: ") && report.contains ("Output: ")
                    && report.contains ("Input: ") && report.contains ("Latency: ") && report.contains ("Models: "), report);
            expect (report.contains ("Song: ") && ! report.contains ("Song: none"), report);
            expect (! report.contains (songName) && ! report.contains (work.getFullPathName()), "no song name or path: " + report);

            const auto body = help::issueBody (ui);
            expect (body.contains ("### " + tr ("report.issue.what")) && body.contains ("```\n" + report + "\n```"));

            // URL：題と本文がそのまま戻る（改行・空白・記号・日本語を逃がしている）
            const auto url = help::newIssueUrl (tr ("report.issue.title"), body);
            expect (url.startsWith (juce::String (help::issuesUrl) + "/new?title="));
            expect (! url.containsAnyOf (" \n#\"<>") && url.indexOf ("&") == url.indexOf ("&body="), url);
            const juce::URL parsed (url);
            expectEquals (parsed.getParameterValues()[parsed.getParameterNames().indexOf ("body")], body);
            expectEquals (parsed.getParameterValues()[parsed.getParameterNames().indexOf ("title")], tr ("report.issue.title"));

            const auto reportUrl = help::reportUrl (ui);
            expect (reportUrl.length() <= help::maxUrlLength, juce::String (reportUrl.length()));

            ReportDialog dlg (ui);
            expectEquals (dlg.shownText(), report);

            ui.attachEngine (nullptr);
            UiSession::projectFolderFor (songName).deleteRecursively();
            work.deleteRecursively();
        }

        beginTest ("a report too long for a URL leaves the information out and asks to paste it");
        {
            juce::String longInfo;
            for (int i = 0; i < 200; ++i)
                longInfo << "device name " << i << "\n";
            const auto url = help::reportUrl (longInfo);
            expect (url.length() <= help::maxUrlLength, juce::String (url.length()));
            const juce::URL parsed (url);
            const auto body = parsed.getParameterValues()[parsed.getParameterNames().indexOf ("body")];
            expect (body.contains (tr ("report.issue.paste")) && ! body.contains ("device name"), body);
            // 短ければ情報が本文に入る
            const auto shortBody = juce::URL (help::reportUrl ("OS: test")).getParameterValues()[1];
            expect (shortBody.contains ("```\nOS: test\n```"), shortBody);
        }

        beginTest ("the guide link follows the language");
        {
            const auto before = i18n::current();
            i18n::setLanguage (i18n::Language::ja);
            expect (help::guideUrl().endsWith ("/README.md"), help::guideUrl());
            i18n::setLanguage (i18n::Language::zhHans);
            expect (help::guideUrl().endsWith ("/README.zh-Hans.md"), help::guideUrl());
            i18n::setLanguage (before);
        }
    }
};

static HelpTests helpTests;
} // namespace vb::test
