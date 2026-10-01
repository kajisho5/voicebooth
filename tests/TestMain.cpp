#include <juce_events/juce_events.h>

/*  VoiceBoothTests：UnitTest（カテゴリ "VoiceBooth"）をすべて回し、
    失敗が 1 つでもあれば 1 を返す（ctest / CI 用） */

namespace
{
    /** テストのログを標準出力へ（Windows の既定はデバッガ出力で、CI のログに残らない） */
    struct StdoutLogger final : juce::Logger
    {
        void logMessage (const juce::String& message) override
        {
            std::fputs ((message + "\n").toRawUTF8(), stdout);
            std::fflush (stdout);
        }
    };
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;   // MessageManager（非同期の通知を受けるテスト用）
    StdoutLogger logger;
    juce::Logger::setCurrentLogger (&logger);

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
    juce::Logger::setCurrentLogger (nullptr);
    return failures == 0 ? 0 : 1;
}
