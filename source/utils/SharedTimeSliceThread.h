#pragma once

#include <juce_core/juce_core.h>

class SharedTimeSliceThread final : public juce::TimeSliceThread
{
public:
    SharedTimeSliceThread()
        : TimeSliceThread (juce::String (JucePlugin_Name) + " ARA Sample Reading Thread")
    {
        startThread (Priority::high);  // Above default priority so playback is fluent, but below realtime
    }
};
