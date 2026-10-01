#include <juce_events/juce_events.h>

/*  VoiceBoothTests：UnitTest（カテゴリ "VoiceBooth"）をすべて回し、
    失敗が 1 つでもあれば 1 を返す（ctest / CI 用） */

int main()
{
    juce::ScopedJuceInitialiser_GUI init;   // MessageManager（非同期の通知を受けるテスト用）

    juce::UnitTestRunner runner;
    runner.setAssertOnFailure (false);
    runner.runTestsInCategory ("VoiceBooth");

    int failures = 0, passes = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
    {
        failures += runner.getResult (i)->failures;
        passes += runner.getResult (i)->passes;
    }

    std::printf ("\n%s: %d passed, %d failed (%d groups)\n", failures == 0 ? "OK" : "FAILED",
                 passes, failures, runner.getNumResults());
    return failures == 0 ? 0 : 1;
}
