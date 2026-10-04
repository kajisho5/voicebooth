#include "ui/parts/KeyButton.h"
#include "ui/parts/Dropdown.h"
#include "ui/parts/Encoder.h"
#include "ui/parts/ConsoleFader.h"
#include "ui/Overlay.h"
#include "session/SessionTestUtil.h"

/*  キーボードと画面読み上げ（#28）：部品は Tab で移れて、マウスで押してもフォーカスを取らない。
    Space・文字のキーは部品で止めず、メイン画面のショートカット（再生・録音）に届く。アイコンだけのキーにも読み上げの名前がある */

namespace vb::test
{
class PartsKeyboardTests : public juce::UnitTest
{
public:
    PartsKeyboardTests() : juce::UnitTest ("Parts keyboard and screen reader", "VoiceBoothSession") {}

    static void expectTabOnly (juce::UnitTest& t, juce::Component& c, const juce::String& what)
    {
        t.expect (c.getWantsKeyboardFocus(), what + " can be reached with Tab");
        t.expect (! c.getMouseClickGrabsKeyboardFocus(), what + " does not take the focus when clicked");
    }

    void runTest() override
    {
        beginTest ("keys: Tab and Enter, Space and letters go on to the shortcuts, icon keys have a name");
        {
            KeyButton icon;
            icon.withIcon (Icon::play);
            icon.setTooltip ("Play");
            expectEquals (icon.getTitle(), juce::String ("Play"), "an icon-only key is read by its tooltip");
            expectTabOnly (*this, icon, "a key");

            KeyButton toggle ("On");
            toggle.setTooltip ("Check for updates");
            expect (toggle.getTitle().isEmpty(), "a key with a short word (On / Off) is read by its word, which changes with the state");

            KeyButton text ("Export");
            text.setTooltip ("Write the files");
            expect (text.getTitle().isEmpty(), "a key with words is read by its words (the tooltip is the help)");

            int clicks = 0;
            text.onClick = [&] { ++clicks; };
            expect (static_cast<juce::Component&> (text).keyPressed (juce::KeyPress (juce::KeyPress::returnKey)));
            pump (100);   // JUCE はクリックをメッセージで届ける
            expectEquals (clicks, 1, "Enter presses the focused key");
            expect (! static_cast<juce::Component&> (text).keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)), "Space goes on to play / stop");
            expect (! static_cast<juce::Component&> (text).keyPressed (juce::KeyPress ('r', {}, 'r')), "R goes on to record");
            pump (100);
            expectEquals (clicks, 1);
        }

        beginTest ("segmented keys: arrows choose, the value is read and can be set by its words");
        {
            SegmentedKeys seg ({ "Easy", "Standard", "Pro" }, 0);
            expectTabOnly (*this, seg, "segmented keys");
            int changed = -1;
            seg.onChange = [&] (int i) { changed = i; };
            expect (seg.keyPressed (juce::KeyPress (juce::KeyPress::rightKey)));
            expectEquals (seg.getSelected(), 1);
            expectEquals (changed, 1);
            expect (seg.keyPressed (juce::KeyPress (juce::KeyPress::rightKey)));
            expect (seg.keyPressed (juce::KeyPress (juce::KeyPress::rightKey)));
            expectEquals (seg.getSelected(), 2, "stops at the last one");
            expect (seg.keyPressed (juce::KeyPress (juce::KeyPress::leftKey)));
            expectEquals (seg.getSelected(), 1);
            expect (! seg.keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)), "Space goes on to play / stop");
            expect (! seg.keyPressed (juce::KeyPress (juce::KeyPress::rightKey, juce::ModifierKeys::commandModifier, 0)), "Ctrl / Cmd + arrow goes on");
            expectEquals (seg.getSelected(), 1);

            auto handler = seg.createAccessibilityHandler();
            expect (handler != nullptr && handler->getRole() == juce::AccessibilityRole::comboBox);
            if (auto* value = handler != nullptr ? handler->getValueInterface() : nullptr)
            {
                expectEquals (value->getCurrentValueAsString(), juce::String ("Standard"));
                value->setValueAsString ("Pro");
                expectEquals (seg.getSelected(), 2);
                value->setValueAsString ("nothing like this");
                expectEquals (seg.getSelected(), 2, "an unknown word changes nothing");
            }
            else
            {
                expect (false, "a value interface");
            }
        }

        beginTest ("dropdown: reachable with Tab, read as a combo box with its value");
        {
            Dropdown dd ({ "English", "Japanese" }, 1);
            expectTabOnly (*this, dd, "a dropdown");
            auto handler = dd.createAccessibilityHandler();
            expect (handler != nullptr && handler->getRole() == juce::AccessibilityRole::comboBox);
            expect (handler != nullptr && handler->getActions().contains (juce::AccessibilityActionType::press), "it can be opened");
            if (auto* value = handler != nullptr ? handler->getValueInterface() : nullptr)
            {
                expectEquals (value->getCurrentValueAsString(), juce::String ("Japanese"));
                value->setValueAsString ("English");
                expectEquals (dd.getSelected(), 0);
            }
            expect (! dd.keyPressed (juce::KeyPress ('r', {}, 'r')), "letters go on to the shortcuts");
        }

        beginTest ("a dialog keeps Tab inside itself and takes the keyboard (keys behind it cannot be pressed with Enter)");
        {
            OverlayHost overlay;
            overlay.setSize (800, 600);
            auto panel = std::make_unique<juce::Component>();
            panel->setSize (300, 200);
            auto* raw = panel.get();
            overlay.show (std::move (panel), false);
            expect (raw->isKeyboardFocusContainer(), "Tab goes round inside the dialog");
            expect (raw->getWantsKeyboardFocus(), "the dialog takes the keyboard even if it did not ask for it");
            overlay.close();
            pump (50);
        }

        beginTest ("faders and knobs: reachable with Tab, arrows move them, they have names");
        {
            ConsoleFader fader (0.5);
            expectTabOnly (*this, fader, "a fader");
            const auto before = fader.getValue();
            expect (fader.keyPressed (juce::KeyPress (juce::KeyPress::upKey)));
            expectGreaterThan (fader.getValue(), before, "up raises the fader");

            Encoder knob (50.0, 150.0, 100.0, 1.0);
            expectTabOnly (*this, knob, "a knob");
            expect (knob.keyPressed (juce::KeyPress (juce::KeyPress::downKey)));
            expectEquals (knob.getValue(), 99.0, "down turns the knob one step");

            EncoderBlock block ("Tempo", 50.0, 150.0, 100.0, 1.0, [] (double v) { return juce::String (v); });
            bool named = false;
            for (auto* c : block.getChildren())
                named = named || c->getTitle() == "Tempo";
            expect (named, "the knob is read by its heading");

            ChannelStrip strip ("Guide\nvoice", 0.75, -1.0f, colours::signal, true);
            juce::StringArray titles;
            for (auto* c : strip.getChildren())
                titles.add (c->getTitle());
            expect (titles.contains ("Guide voice"), "the fader is read by the channel name: " + titles.joinIntoString (" | "));
            expect (titles.contains ("Guide voice " + tr ("monitor.mute")), "M is read as the channel and mute");
            expect (titles.contains ("Guide voice " + tr ("monitor.solo")), "S is read as the channel and solo");
        }
    }
};

static PartsKeyboardTests partsKeyboardTests;
} // namespace vb::test
