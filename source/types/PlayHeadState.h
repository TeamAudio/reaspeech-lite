#pragma once

#include <atomic>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

struct PlayHeadState
{
    juce::DynamicObject::Ptr toDynamicObject() const
    {
        juce::DynamicObject::Ptr obj = new juce::DynamicObject();

        obj->setProperty ("isPlaying", isPlaying.load (std::memory_order_relaxed));
        obj->setProperty ("timeInSeconds", timeInSeconds.load (std::memory_order_relaxed));

        return obj;
    }

    void update (const juce::Optional<juce::AudioPlayHead::PositionInfo>& info)
    {
        if (! info.hasValue())
        {
            isPlaying.store (false, std::memory_order_relaxed);
            return;
        }

        isPlaying.store (info->getIsPlaying(), std::memory_order_relaxed);
        timeInSeconds.store (info->getTimeInSeconds().orFallback (0), std::memory_order_relaxed);
    }

    std::atomic<bool> isPlaying { false };
    std::atomic<double> timeInSeconds { 0.0 };
};
