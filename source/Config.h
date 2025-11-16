#pragma once

#include <string>
#include <vector>

#include <juce_core/juce_core.h>

struct Config
{
    static inline const std::vector<std::pair<std::string, std::string>> models = {
        { "parakeet-tdt-0.6b-v3", "Parakeet" },
        { "small", "Whisper Small" },
        { "medium", "Whisper Medium" },
        { "large-v3", "Whisper Large" },
        { "large-v3-turbo", "Whisper Turbo" }
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

    static bool isParakeetModel (const std::string& modelName)
    {
        return modelName.find ("parakeet") != std::string::npos;
    }

    static bool isWhisperModel (const std::string& modelName)
    {
        return !isParakeetModel (modelName);
    }
};
