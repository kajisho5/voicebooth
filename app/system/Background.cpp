#include "Background.h"
#include <condition_variable>
#include <mutex>

namespace vb::background
{
namespace
{
    std::mutex lock;
    std::condition_variable freed;
    int active = 0;
}

int limit()
{
    static const int n = juce::jlimit (2, 4, juce::SystemStats::getNumCpus() - 1);
    return n;
}

int running()
{
    std::lock_guard<std::mutex> g (lock);
    return active;
}

void run (std::function<void()> job)
{
    juce::Thread::launch (juce::Thread::Priority::low, [job = std::move (job)]
    {
        {
            std::unique_lock<std::mutex> g (lock);
            freed.wait (g, [] { return active < limit(); });
            ++active;
        }
        job();
        {
            std::lock_guard<std::mutex> g (lock);
            --active;
        }
        freed.notify_one();
    });
}
} // namespace vb::background
