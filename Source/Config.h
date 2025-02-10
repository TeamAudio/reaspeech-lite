#pragma once

#include <JuceHeader.h>

struct Config
{
    static inline const std::string modelName = "small";

    static const std::string getModelsDir()
    {
        const auto tempDir = juce::File::getSpecialLocation (juce::File::SpecialLocationType::tempDirectory);
        return tempDir.getFullPathName().toStdString() + "/models/";
    }
};
