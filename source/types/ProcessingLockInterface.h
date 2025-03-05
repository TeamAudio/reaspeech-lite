#pragma once

#include <juce_core/juce_core.h>

struct ProcessingLockInterface
{
    virtual ~ProcessingLockInterface() = default;
    virtual juce::ScopedTryReadLock getProcessingLock() = 0;
};
