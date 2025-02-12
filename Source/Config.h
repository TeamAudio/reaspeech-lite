#pragma once

#include <JuceHeader.h>

struct Config
{
    static inline const std::string modelName = "small";

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
