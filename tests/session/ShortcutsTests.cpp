#include "ui/screens/ShortcutsDialog.h"
#include "ui/screens/SongInfoDialog.h"
#include "session/SessionTestUtil.h"

/*  1 文字のショートカットを変える（#28）：割り当て表・保存の文字列・設定の画面でのキー入力 */

namespace vb::test
{
class ShortcutsTests : public juce::UnitTest
{
public:
    ShortcutsTests() : juce::UnitTest ("Shortcuts", "VoiceBoothSession") {}

    void runTest() override
    {
        using shortcuts::Action;
        using shortcuts::Map;

        beginTest ("defaults are the keys the app has always used");
        {
            const auto m = Map::defaults();
            expect (m.actionFor ('r') == Action::record);
            expect (m.actionFor ('R') == Action::record, "upper case works too");
            expect (m.actionFor ('l') == Action::loop);
            expect (m.actionFor ('[') == Action::rangeIn);
            expect (m.actionFor (']') == Action::rangeOut);
            expect (m.actionFor ('t') == Action::tapTempo);
            expect (m.actionFor ('m') == Action::addSection);
            expect (m.actionFor ('1') == Action::track1);
            expect (m.actionFor ('4') == Action::track4);
            expect (! m.actionFor ('x').has_value());
            expect (! m.actionFor (' ').has_value(), "Space stays play");
            expectEquals (m.keyName (Action::record), juce::String ("R"));
            expect (m.isDefault());
        }

        beginTest ("a key moved to another action leaves the old one off");
        {
            auto m = Map::defaults();
            const auto displaced = m.assign (Action::loop, 'R');
            expect (displaced == Action::record);
            expect (m.actionFor ('r') == Action::loop);
            expectEquals ((int) m.key (Action::record), 0, "record has no key now");
            expect (! m.actionFor ('l').has_value(), "the old loop key does nothing");
            expectEquals (m.keyName (Action::record), juce::String());

            expect (! m.assign (Action::record, 'q').has_value(), "a free key displaces nothing");
            expect (m.actionFor ('Q') == Action::record);

            expect (! m.assign (Action::record, ' ').has_value());
            expect (! m.assign (Action::record, 0x3042).has_value(), "non-ASCII is not taken");
            expect (! m.assign (Action::record, '\t').has_value());
            expect (m.actionFor ('q') == Action::record, "a refused key changes nothing");

            m.clear (Action::record);
            expect (! m.actionFor ('q').has_value());
            expect (! m.isDefault());
        }

        beginTest ("saved text round-trips, including keys that look like separators");
        {
            auto m = Map::defaults();
            m.assign (Action::record, ';');
            m.assign (Action::loop, '=');
            m.clear (Action::tapTempo);
            const auto back = Map::fromString (m.toString());
            expect (back == m, m.toString());
            expect (back.actionFor (';') == Action::record);
            expect (back.actionFor ('=') == Action::loop);
            expectEquals ((int) back.key (Action::tapTempo), 0);

            expect (Map::fromString ({}) == Map::defaults(), "nothing saved: defaults");
            expect (Map::fromString ("record=abc;nonsense=5") == Map::defaults(), "unreadable values keep the defaults");
            const auto partial = Map::fromString ("loop=113");
            expect (partial.actionFor ('q') == Action::loop);
            expect (partial.actionFor ('r') == Action::record, "actions not written keep their default");
            const auto dup = Map::fromString ("record=120;loop=120");
            expect (dup.actionFor ('x') == Action::record);
            expectEquals ((int) dup.key (Action::loop), 0, "a duplicated key is kept only once");
        }

        beginTest ("the settings screen: press the action's key, then the new key");
        {
            UiSession ui;
            ShortcutsDialog dlg (ui);
            auto press = [&] (juce::juce_wchar c, int code = 0)
            {
                return dlg.keyPressed (juce::KeyPress (code != 0 ? code : (int) c, {}, c));
            };

            expect (! press ('x'), "not waiting: keys go on (Esc closes the screen)");

            dlg.startCapture ((int) Action::record);
            expect (press ('x'));
            expect (ui.get().shortcuts.actionFor ('x') == Action::record);
            expectEquals (dlg.capturingIndex(), -1);

            dlg.startCapture ((int) Action::loop);
            expect (press ('X'));
            expect (ui.get().shortcuts.actionFor ('x') == Action::loop, "upper case is the same key");
            expectEquals ((int) ui.get().shortcuts.key (Action::record), 0, "record lost the key");

            dlg.startCapture ((int) Action::tapTempo);
            expect (press (' ', juce::KeyPress::spaceKey));
            expectEquals (dlg.capturingIndex(), (int) Action::tapTempo, "Space is refused and it keeps waiting");
            expect (press (0, juce::KeyPress::escapeKey));
            expectEquals (dlg.capturingIndex(), -1);
            expect (ui.get().shortcuts.actionFor ('t') == Action::tapTempo, "Esc changes nothing");

            dlg.startCapture ((int) Action::addSection);
            expect (press (0, juce::KeyPress::deleteKey));
            expectEquals ((int) ui.get().shortcuts.key (Action::addSection), 0, "Delete turns it off");

            ui.resetShortcuts();
            expect (ui.get().shortcuts.isDefault());
        }

        // 曲の情報のパネルを開いている間も、割り当てたキーで動く（前は T / M の決め打ち。バグチェック 2026-10-05）
        beginTest ("the song info panel follows the assigned keys");
        {
            UiSession ui;
            Actions actions;
            ui.setShortcut (Action::tapTempo, 'y');
            SongInfoDialog dlg (ui, actions);
            const auto before = ui.tapCount();
            expect (dlg.keyPressed (juce::KeyPress ('y', 0, 'y')), "the new key taps");
            expectEquals (ui.tapCount(), before + 1);
            expect (! dlg.keyPressed (juce::KeyPress ('t', 0, 't')), "the old key does nothing");
            expectEquals (ui.tapCount(), before + 1);
            ui.resetShortcuts();
        }

        // 曲を開き直しても、お手本の線の番号は戻さない（音符・おすすめのキーのキャッシュの鍵。バグチェック 2026-10-05）
        beginTest ("opening another song keeps counting the guide line number");
        {
            dummy::Session prev;
            prev.refPitchSerial = 5;
            const auto next = dummy::makeSongSession (prev, "song", "/song.wav", 48000, 48000, nullptr);
            expect (next.refPitchSerial > prev.refPitchSerial);
        }
    }
};

static ShortcutsTests shortcutsTests;
} // namespace vb::test
