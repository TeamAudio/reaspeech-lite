#pragma once

#include <JuceHeader.h>
#include <whisper.h>

#include "../utils/ResamplingExporter.h"
#include "ASREngine.h"
#include "ASROptions.h"
#include "ASRSegment.h"

enum class ASRThreadPoolJobStatus
{
    ready,
    exporting,
    loadingModel,
    transcribing,
    finished,
    failed
};

struct ASRThreadPoolJobResult
{
    bool isError;
    std::string errorMessage;
    std::vector<ASRSegment> segments;
};

class ASRThreadPoolJob final : public juce::ThreadPoolJob
{
public:
    ASRThreadPoolJob(
        ASREngine& asrEngineIn,
        std::string modelNameIn,
        juce::ARAAudioSource* audioSourceIn,
        std::unique_ptr<ASROptions> optionsIn,
        std::function<void (ASRThreadPoolJobStatus)> onStatus,
        std::function<void (const ASRThreadPoolJobResult&)> onComplete
    ) : ThreadPoolJob ("ASR Threadpool Job"),
        asrEngine (asrEngineIn),
        modelName (modelNameIn),
        audioSource (audioSourceIn),
        options (std::move (optionsIn)),
        onStatusCallback (onStatus),
        onCompleteCallback (onComplete)
    {
    }

    ThreadPoolJob::JobStatus runJob() override
    {
        DBG ("ASRThreadPoolJob::runJob");

        DBG ("Exporting audio data");
        onStatusCallback (ASRThreadPoolJobStatus::exporting);

        std::vector<float> audioData;
        ResamplingExporter::exportAudio (audioSource, WHISPER_SAMPLE_RATE, 0, audioData);
        DBG ("Audio data size: " + juce::String (audioData.size()));

        DBG ("Loading model");
        onStatusCallback (ASRThreadPoolJobStatus::loadingModel);

        if (! asrEngine.loadModel (modelName))
        {
            onStatusCallback (ASRThreadPoolJobStatus::failed);
            onCompleteCallback ({ true, "Failed to load model", {} });
            return jobHasFinished;
        }

        DBG ("Transcribing audio data");
        onStatusCallback (ASRThreadPoolJobStatus::transcribing);

        DBG ("ASR options: " + options->toJSON());

        std::vector<ASRSegment> segments;
        bool result = asrEngine.transcribe (audioData, *options, segments);

        if (result)
        {
            DBG ("Transcription successful");
            onStatusCallback (ASRThreadPoolJobStatus::finished);
            onCompleteCallback ({ false, "", segments });
        }
        else
        {
            DBG ("Transcription failed");
            onStatusCallback (ASRThreadPoolJobStatus::failed);
            onCompleteCallback ({ true, "Transcription failed", {} });
        }

        return jobHasFinished;
    }

    void removedFromQueue()
    {
        DBG ("ASRThreadPoolJob::removedFromQueue");
    }

private:
    ASREngine& asrEngine;
    std::string modelName;
    juce::ARAAudioSource* audioSource;
    std::unique_ptr<ASROptions> options;
    std::function<void (ASRThreadPoolJobStatus)> onStatusCallback;
    std::function<void (const ASRThreadPoolJobResult&)> onCompleteCallback;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ASRThreadPoolJob)
};
