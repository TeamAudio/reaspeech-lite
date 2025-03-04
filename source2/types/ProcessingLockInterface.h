#pragma once

#include <JuceHeader.h>

struct ProcessingLockInterface
{
    virtual ~ProcessingLockInterface() = default;
    virtual juce::ScopedTryReadLock getProcessingLock() = 0;
};
