#pragma once
#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <functional>
#include <mutex>
#include <vector>
#include <juce_audio_formats/juce_audio_formats.h>
#include <reaspeech.h>
#include "ASROptions.h"
#include "ASRSegment.h"

class ASREngine
{
public:
    explicit ASREngine (const std::string& modelsDir)
    {
        std::call_once (cacheInitialized, [&modelsDir]
        {
            if (juce::SystemStats::getEnvironmentVariable ("MODELS_PATH", {}).isEmpty())
            {
                juce::File (modelsDir).createDirectory();
#ifdef _WIN32
                _putenv_s ("MODELS_PATH", modelsDir.c_str());
#else
                setenv ("MODELS_PATH", modelsDir.c_str(), 0);
#endif
            }
        });
    }
    bool transcribe (const std::vector<float>& audioData, ASROptions& options,
                     std::vector<ASRSegment>& segments, std::function<bool()> isAborted,
                     std::function<void(const juce::String&)> onProgress = {})
    {
        errorMessage.clear();
        progress.store (0);
        if (isAborted()) return false;
        auto* properties = new juce::DynamicObject();
        juce::var jobOptions (properties);
        properties->setProperty ("model", options.modelName);
        if (options.language.isNotEmpty() && options.language != "auto")
            properties->setProperty ("language", options.language);
        properties->setProperty ("translate", options.translate);
        properties->setProperty ("vad", options.vad);
        properties->setProperty ("words", true);
        const auto optionsJson = juce::JSON::toString (jobOptions).toStdString();
        char jobId[256] {};
        // The C API copies the mono 16 kHz PCM before returning.
        if (! reaspeech_start_from_memory (audioData.data(), audioData.size(), 16000, 1,
                                          optionsJson.c_str(), jobId, sizeof (jobId)))
            return fail (juce::String::fromUTF8 (jobId));
        bool cancelling = false;
        std::vector<ASRSegment> result;
        for (;;)
        {
            if (! cancelling && isAborted())
            {
                reaspeech_cancel (jobId);
                cancelling = true;
            }
            juce::String json;
            {
                // The C API return buffer is shared. Copy it while locked.
                std::lock_guard<std::mutex> lock (pollMutex);
                json = juce::String::fromUTF8 (reaspeech_poll (jobId));
            }
            if (json.isEmpty()) { juce::Thread::sleep (50); continue; }
            const auto event = juce::JSON::parse (json);
            const auto type = event["type"].toString();
            if (type == "progress")
            {
                if (onProgress) onProgress (event["message"].toString());
                const auto total = static_cast<double> (event["total"]);
                if (total > 0)
                    progress.store (juce::jlimit (0, 100, static_cast<int> (100.0 * static_cast<double> (event["completed"]) / total)));
            }
            else if (type == "segment" && ! cancelling)
            {
                const auto data = event["segment"];
                ASRSegment segment;
                segment.text = data["text"].toString().trim();
                segment.start = static_cast<float> (static_cast<double> (data["startMs"]) / 1000.0);
                segment.end = static_cast<float> (static_cast<double> (data["endMs"]) / 1000.0);
                const auto wordsValue = data["words"];
                if (const auto* words = wordsValue.getArray())
                    for (const auto& word : *words)
                        segment.words.add ({ word["word"].toString().trim(), static_cast<float> (word["start"]),
                                             static_cast<float> (word["end"]), static_cast<float> (word["probability"]) });
                result.push_back (std::move (segment));
            }
            else if (type == "completed")
            {
                if (cancelling || isAborted()) return false;
                segments.insert (segments.end(), result.begin(), result.end());
                progress.store (100);
                return true;
            }
            else if (type == "cancelled") return false;
            else if (type == "error") return fail (event["error"].toString());
        }
    }
    int getProgress() const { return progress.load(); }
    juce::String getError() const { return errorMessage; }
private:
    bool fail (const juce::String& message) { errorMessage = message; return false; }
    inline static std::mutex pollMutex;
    inline static std::once_flag cacheInitialized;
    std::atomic<int> progress { 0 };
    juce::String errorMessage;
};
