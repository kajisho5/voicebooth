#include "session/FakeEngine.h"
#include "session/SessionTestUtil.h"

/*  ハモリ録りの補助（#30。DESIGN A5・B12）：ハモリのトラックをアームしている間は Main を薄く（約 -8 dB）鳴らす。
    フェーダーの値は変えない。Main に戻すと元の音量 */

namespace vb::test
{
class SessionHarmonyTests : public juce::UnitTest
{
public:
    SessionHarmonyTests() : juce::UnitTest ("Session harmony recording aid", "VoiceBoothSession") {}

    void runTest() override
    {
        const auto tag = juce::String::toHexString (juce::Random::getSystemRandom().nextInt64()).substring (0, 8);
        const auto work = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("vbharm-" + tag);
        const auto songName = "vbharm-" + tag;
        const auto song = writeTone (work.getChildFile (songName + ".wav"), 440.0, 4.0);

        beginTest ("arming a harmony track plays Main quieter; arming Main again brings it back");
        {
            FakeEngine engine;
            UiSession ui;
            ui.attachEngine (&engine);
            expect (openSong (ui, song));
            ui.setMode (project::Mode::standard);
            auto indexOf = [&] (project::TrackType type)
            {
                for (int i = 0; i < (int) ui.get().trackUi.size(); ++i)
                    if (ui.get().trackUi[(size_t) i].type == type)
                        return i;
                return -1;
            };
            const auto main = indexOf (project::TrackType::main), harm = indexOf (project::TrackType::harm1);
            expect (main >= 0 && harm >= 0);
            if (! ui.get().trackUi[(size_t) main].armed)
                ui.armTrack (main);
            const auto fader = ui.get().trackUi[(size_t) main].monitorGain;
            const auto full = engine.vocalGains[0];
            expect (full > 0.0f, "Main is audible: " + juce::String (full));

            ui.armTrack (harm);
            expectWithinAbsoluteError (engine.vocalGains[0], full * 0.4f, 1.0e-6f, "Main is about -8 dB while a harmony is armed");
            expectEquals (ui.get().trackUi[(size_t) main].monitorGain, fader, "the fader does not move");

            ui.armTrack (main);
            expectWithinAbsoluteError (engine.vocalGains[0], full, 1.0e-6f, "back to the fader's level");
            ui.attachEngine (nullptr);
        }

        UiSession::projectFolderFor (songName).deleteRecursively();
        work.deleteRecursively();
    }
};

static SessionHarmonyTests sessionHarmonyTests;
} // namespace vb::test
