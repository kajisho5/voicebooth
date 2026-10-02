#include "Animator.h"

namespace vb::motion
{
namespace
{
    std::optional<bool> reducedOverride;
}

void setReducedMotionOverride (std::optional<bool> o) { reducedOverride = o; }

bool prefersReducedMotion()
{
    if (reducedOverride.has_value())
        return *reducedOverride;

    // OS への問い合わせは軽いが、毎フレームは聞かない（2 秒ごと）
    static bool cached = false;
    static juce::uint32 checkedAt = 0;
    static bool checked = false;
    const auto t = juce::Time::getMillisecondCounter();
    if (! checked || t - checkedAt > 2000)
    {
        cached = systemPrefersReducedMotion();
        checkedAt = t;
        checked = true;
    }
    return cached;
}

double now()
{
    return juce::Time::getMillisecondCounterHiRes() / 1000.0;
}

//==============================================================================
/** 共通の時計（1 つだけ） */
class Driver : private juce::Timer,
               public juce::DeletedAtShutdown
{
public:
    Driver() { clients.ensureStorageAllocated (64); }
    ~Driver() override
    {
        stopTimer();
        for (auto* c : clients) c->animating = false;
        clearSingletonInstance();
    }

    void add (Animated* a)
    {
        if (a->animating) return;
        a->animating = true;
        clients.add (a);
        if (! isTimerRunning())
        {
            last = juce::Time::getMillisecondCounterHiRes();
            startTimerHz (60);
        }
    }

    void remove (Animated* a)
    {
        if (! a->animating) return;
        a->animating = false;
        clients.removeFirstMatchingValue (a);
        if (clients.isEmpty())
            stopTimer();
    }

    JUCE_DECLARE_SINGLETON_SINGLETHREADED_MINIMAL_INLINE (Driver)

private:
    void timerCallback() override
    {
        const auto t = juce::Time::getMillisecondCounterHiRes();
        const auto dt = (float) juce::jlimit (0.0, 0.05, (t - last) / 1000.0);   // 止まっていた後に飛ばない
        last = t;

        // advance の中で他の部品が降りる・消えることがあるので、毎回範囲を確かめる
        for (int i = 0; i < clients.size(); ++i)
        {
            auto* c = clients.getUnchecked (i);
            if (! c->advanceAnimation (dt))
            {
                // advance の中で自分から降りていなければ降ろす
                const auto at = clients.indexOf (c);
                if (at >= 0)
                {
                    c->animating = false;
                    clients.remove (at);
                    if (at <= i) --i;
                }
            }
        }

        if (clients.isEmpty())
            stopTimer();
    }

    juce::Array<Animated*> clients;
    double last = 0.0;
};

//==============================================================================
Animated::~Animated()
{
    if (animating)
        if (auto* d = Driver::getInstanceWithoutCreating())
            d->remove (this);
}

void Animated::startAnimating()
{
    if (! animating)
        Driver::getInstance()->add (this);
}

void Animated::stopAnimating()
{
    if (animating)
        if (auto* d = Driver::getInstanceWithoutCreating())
            d->remove (this);
}
} // namespace vb::motion
