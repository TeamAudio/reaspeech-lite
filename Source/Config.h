#pragma once

#include <JuceHeader.h>
#include <whisper.h>

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

    static const whisper_context_params getWhisperContextParams()
    {
        whisper_context_params params = whisper_context_default_params();

        // Enable flash attention for faster processing on supported GPUs
        params.flash_attn = true;

        return params;
    }

    static const whisper_full_params getWhisperFullParams()
    {
        whisper_full_params params = whisper_full_default_params (WHISPER_SAMPLING_GREEDY);

        // Slower but more accurate
        // whisper_full_params params = whisper_full_default_params (WHISPER_SAMPLING_BEAM_SEARCH);
        // params.beam_search.beam_size = 5;

        // Tuning this value can sometimes yield better performance
        // params.n_threads = 8;

        // Smaller context size can be faster but less accurate
        // params.n_max_text_ctx = 128;

        // Use smaller audio chunk size for faster processing
        // params.audio_ctx = 512;

        return params;
    }
};
