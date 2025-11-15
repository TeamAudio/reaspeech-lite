#pragma once

#include <memory>
#include <string>
#include <atomic>

#include <juce_core/juce_core.h>

#include "ASROptions.h"
#include "ASRSegment.h"

// Forward declaration to avoid including ONNX Runtime headers
// This uses PIMPL pattern to hide ONNX Runtime from the header
struct ParakeetEngineImpl;

class ParakeetEngine
{
public:
    ParakeetEngine(const std::string &modelsDirIn);
    ~ParakeetEngine();

    // Get last transcription time in seconds
    float getLastTranscriptionTime() const;

    // Download the model if needed
    bool downloadModel(const std::string &modelName, std::function<bool()> isAborted);

    // Load the model
    bool loadModel(const std::string &modelName);

    // Transcribe audio
    bool transcribe(
        const std::vector<float> &audioData,
        ASROptions &options,
        std::vector<ASRSegment> &segments,
        std::function<bool()> isAborted);

    int getProgress() const;

    // Check if ParakeetEngine.dll is loaded and available
    bool isAvailable() const;

    // Get error message if DLL failed to load
    std::string getLoadError() const;

private:
    std::string modelsDir;
    std::atomic<int> progress{0};
    float lastTranscriptionTimeSecs = 0.0f;

    // PIMPL: implementation details hidden in .cpp file
    std::unique_ptr<ParakeetEngineImpl> impl;
};
