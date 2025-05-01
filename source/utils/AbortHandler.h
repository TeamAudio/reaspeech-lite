#pragma once

#include <functional>

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

class AbortHandler : public juce::Timer
{
public:
    AbortHandler (juce::ThreadPool& pool, std::function<void (const juce::var&)> callback, int timeoutMs) :
        threadPool (pool),
        complete (callback),
        startTime (juce::Time::getMillisecondCounter()),
        timeout (static_cast<juce::uint32> (timeoutMs))
    {
        startTimer (50); // Check every 50ms
    }

    void timerCallback() override
    {
        auto now = juce::Time::getMillisecondCounter();
        if (now - startTime > timeout)
        {
            // Timed out
            complete (juce::var (false));
            delete this;
            return;
        }

        if (threadPool.getNumJobs() == 0)
        {
            // Successfully removed all jobs
            complete (juce::var (true));
            delete this;
            return;
        }
    }

private:
    juce::ThreadPool& threadPool;
    std::function<void (const juce::var&)> complete;
    juce::uint32 startTime;
    juce::uint32 timeout;
};
