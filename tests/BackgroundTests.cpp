#include "system/Background.h"
#include <atomic>

/*  バックグラウンドの作業（#26）：同時に動くのは limit() 本まで・全部の作業が動く */

namespace vb::background
{
class BackgroundTests : public juce::UnitTest
{
public:
    BackgroundTests() : juce::UnitTest ("Background jobs", "VoiceBooth") {}

    void runTest() override
    {
        beginTest ("no more than limit() jobs run at once, and every job runs");
        {
            expect (limit() >= 2 && limit() <= 4, juce::String (limit()));
            std::atomic<int> now { 0 }, peak { 0 }, finished { 0 };
            constexpr int jobs = 12;
            for (int i = 0; i < jobs; ++i)
                run ([&]
                {
                    const auto n = ++now;
                    for (auto p = peak.load(); n > p && ! peak.compare_exchange_weak (p, n);) {}
                    juce::Thread::sleep (40);
                    --now;
                    ++finished;
                });
            const auto until = juce::Time::getMillisecondCounter() + 10000;
            while (finished.load() < jobs && juce::Time::getMillisecondCounter() < until)
                juce::Thread::sleep (10);
            expectEquals (finished.load(), jobs);
            expect (peak.load() <= limit(), "peak " + juce::String (peak.load()) + " > limit " + juce::String (limit()));
            expect (peak.load() >= 2, "jobs still run side by side: peak " + juce::String (peak.load()));
            juce::Thread::sleep (50);
            expectEquals (running(), 0);
        }
    }
};

static BackgroundTests backgroundTests;
} // namespace vb::background
