#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>

#include "../utils/ResamplingExporter.h"
#include "ASREngine.h"
#include "ASROptions.h"
#include "ASRSegment.h"

enum class ASRThreadPoolJobStatus
{
    ready,
    exporting,
    downloadingModel,
    downloadingVadModel,
    loadingModel,
    transcribing,
    aborted,
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
        juce::ARAAudioSource* audioSourceIn,
        std::unique_ptr<ASROptions> optionsIn,
        std::function<void (ASRThreadPoolJobStatus)> onStatus,
        std::function<void (const ASRThreadPoolJobResult&)> onComplete
    ) : ThreadPoolJob ("ASR Threadpool Job"),
        asrEngine (asrEngineIn),
        audioSource (audioSourceIn),
        options (std::move (optionsIn)),
        onStatusCallback (onStatus),
        onCompleteCallback (onComplete)
    {
    }

    ThreadPoolJob::JobStatus runJob() override
    {
        DBG ("ASRThreadPoolJob::runJob");

        auto isAborted = [this] { return shouldExit(); };

        DBG ("Exporting audio data");
        onStatusCallback (ASRThreadPoolJobStatus::exporting);

        std::vector<float> audioData;
        ResamplingExporter::exportAudio (audioSource, 16000, 0, audioData, isAborted);

        if (aborting())
            return jobHasFinished;

        DBG ("Audio data size: " + juce::String (audioData.size()));

        DBG ("Transcribing audio data");
        onStatusCallback (ASRThreadPoolJobStatus::transcribing);

        DBG ("ASR options: " + options->toJSON());

        std::vector<ASRSegment> segments;
        bool result = asrEngine.transcribe (audioData, *options, segments, isAborted,
            [this] (const juce::String& message)
            {
                if (message.containsIgnoreCase ("download") || message.containsIgnoreCase ("checking model"))
                    onStatusCallback (ASRThreadPoolJobStatus::downloadingModel);
                else if (message.containsIgnoreCase ("loading model"))
                    onStatusCallback (ASRThreadPoolJobStatus::loadingModel);
                else
                    onStatusCallback (ASRThreadPoolJobStatus::transcribing);
            });

        if (aborting())
            return jobHasFinished;

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
            onCompleteCallback ({ true, asrEngine.getError().toStdString(), {} });
        }

        return jobHasFinished;
    }

    void removedFromQueue()
    {
        DBG ("ASRThreadPoolJob::removedFromQueue");
    }

private:
    bool aborting() const
    {
        if (shouldExit())
        {
            DBG ("Transcription aborted");
            onStatusCallback (ASRThreadPoolJobStatus::aborted);
            onCompleteCallback ({ false, "", {} });
            return true;
        }
        return false;
    }

    ASREngine& asrEngine;
    juce::ARAAudioSource* audioSource;
    std::unique_ptr<ASROptions> options;
    std::function<void (ASRThreadPoolJobStatus)> onStatusCallback;
    std::function<void (const ASRThreadPoolJobResult&)> onCompleteCallback;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ASRThreadPoolJob)
};
