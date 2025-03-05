#pragma once

#include <string>
#include <vector>

#include <juce_core/juce_core.h>

struct Config
{
    static inline const std::vector<std::pair<std::string, std::string>> models = {
        { "small", "Small" },
        { "medium", "Medium" },
        { "large-v3", "Large" },
        { "large-v3-turbo", "Turbo" }
    };

    static const juce::URL getModelURL (std::string modelNameIn)
    {
        return juce::URL ("https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-" + modelNameIn + ".bin");
    }

    static const std::string getModelsDir()
    {
        const auto tempDir = juce::File::getSpecialLocation (juce::File::SpecialLocationType::tempDirectory);
        return tempDir.getFullPathName().toStdString() + "/models/";
    }
};
