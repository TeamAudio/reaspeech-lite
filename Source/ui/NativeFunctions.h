#pragma once

#include <JuceHeader.h>
#include <atomic>

#include "../Config.h"
#include "../asr/ASREngine.h"
#include "../asr/ASROptions.h"
#include "../asr/ASRThreadPoolJob.h"
#include "../asr/WhisperLanguages.h"
#include "../plugin/ReaSpeechLiteAudioProcessorImpl.h"
#include "../reaper/ReaperProxy.h"
#include "../types/MarkerType.h"
#include "../utils/SafeUTF8.h"

class NativeFunctions : public OptionsBuilder<juce::WebBrowserComponent::Options>
{
public:
    NativeFunctions (
        juce::ARAEditorView& editorViewIn,
        ReaSpeechLiteAudioProcessorImpl& audioProcessorIn
    ) : editorView (editorViewIn),
        audioProcessor (audioProcessorIn)
    {
        asrEngine = std::make_unique<ASREngine> (Config::getModelsDir());
    }

    juce::WebBrowserComponent::Options buildOptions (const juce::WebBrowserComponent::Options& initialOptions)
    {
        auto bindFn = [this] (auto memberFn)
        {
            return [this, memberFn] (const auto& args, const auto& complete)
            {
                (this->*memberFn) (args, complete);
            };
        };

        return initialOptions
            .withNativeFunction ("canCreateMarkers", bindFn (&NativeFunctions::canCreateMarkers))
            .withNativeFunction ("createMarkers", bindFn (&NativeFunctions::createMarkers))
            .withNativeFunction ("getAudioSources", bindFn (&NativeFunctions::getAudioSources))
            .withNativeFunction ("getRegionSequences", bindFn (&NativeFunctions::getRegionSequences))
            .withNativeFunction ("getTranscriptionStatus", bindFn (&NativeFunctions::getTranscriptionStatus))
            .withNativeFunction ("getWebState", bindFn (&NativeFunctions::getWebState))
            .withNativeFunction ("getWhisperLanguages", bindFn (&NativeFunctions::getWhisperLanguages))
            .withNativeFunction ("play", bindFn (&NativeFunctions::play))
            .withNativeFunction ("stop", bindFn (&NativeFunctions::stop))
            .withNativeFunction ("setPlaybackPosition", bindFn (&NativeFunctions::setPlaybackPosition))
            .withNativeFunction ("setWebState", bindFn (&NativeFunctions::setWebState))
            .withNativeFunction ("transcribeAudioSource", bindFn (&NativeFunctions::transcribeAudioSource));
    }

    void canCreateMarkers (const juce::var&, std::function<void (const juce::var&)> complete)
    {
        if (rpr.hasAddProjectMarker2)
            complete (juce::var (true));
        else
            complete (juce::var (false));
    }

    void createMarkers (const juce::var& args, std::function<void (const juce::var&)> complete)
    {
        if (! args.isArray() || args.size() < 2)
        {
            complete (makeError ("Invalid arguments"));
            return;
        }

        const auto markers = args[0].getArray();

        const auto markerTypeOpt = MarkerType::fromString (args[1].toString().toStdString());
        if (! markerTypeOpt)
        {
            complete (makeError ("Invalid marker type"));
            return;
        }
        const auto markerType = *markerTypeOpt;

        if (! rpr.hasAddProjectMarker2)
        {
            complete (makeError ("Function not available"));
            return;
        }

        withReaperUndo ("Create " + MarkerType::toString (markerType) + " from transcript", [&] {
            for (auto i = 0; i < (*markers).size(); ++i)
            {
                const auto marker = (*markers)[i].getDynamicObject();
                const auto regions = markerType == MarkerType::regions;
                const auto start = marker->getProperty ("start");
                const auto end = marker->getProperty ("end");
                const auto name = marker->getProperty ("name");
                rpr.AddProjectMarker2 (ReaperProxy::activeProject, regions, start, end, name.toString().toRawUTF8(), i + 1, 0);
            }
        });

        complete (juce::var());
    }

    void getAudioSources (const juce::var&, std::function<void (const juce::var&)> complete)
    {
        if (auto* document = getDocument())
        {
            juce::Array<juce::var> audioSources;
            for (const auto& as : document->getAudioSources())
            {
                juce::DynamicObject::Ptr audioSource = new juce::DynamicObject();
                audioSource->setProperty ("name", SafeUTF8::encode (as->getName()));
                audioSource->setProperty ("persistentID", juce::String (as->getPersistentID()));
                audioSource->setProperty ("sampleRate", as->getSampleRate());
                audioSource->setProperty ("sampleCount", (juce::int64) as->getSampleCount());
                audioSource->setProperty ("duration", as->getDuration());
                audioSource->setProperty ("channelCount", as->getChannelCount());
                audioSource->setProperty ("merits64BitSamples", as->merits64BitSamples());
                audioSources.add (audioSource.get());
            }
            complete (juce::var (audioSources));
            return;
        }
        complete (makeError ("Document not found"));
    }

    void getRegionSequences (const juce::var&, std::function<void (const juce::var&)> complete)
    {
        if (auto* document = getDocument())
        {
            juce::Array<juce::var> regionSequences;
            for (const auto& rs : document->getRegionSequences())
            {
                juce::DynamicObject::Ptr regionSequence = new juce::DynamicObject();
                regionSequence->setProperty ("name", SafeUTF8::encode (rs->getName()));
                regionSequence->setProperty ("orderIndex", rs->getOrderIndex());

                juce::Array<juce::var> playbackRegions;
                for (const auto& pr : rs->getPlaybackRegions())
                {
                    juce::DynamicObject::Ptr playbackRegion = new juce::DynamicObject();
                    playbackRegion->setProperty ("name", SafeUTF8::encode (pr->getName()));
                    playbackRegion->setProperty ("playbackStart", pr->getStartInPlaybackTime());
                    playbackRegion->setProperty ("playbackEnd", pr->getEndInPlaybackTime());
                    playbackRegion->setProperty ("modificationStart", pr->getStartInAudioModificationTime());
                    playbackRegion->setProperty ("modificationEnd", pr->getEndInAudioModificationTime());
                    auto* audioSource = pr->getAudioModification()->getAudioSource();
                    playbackRegion->setProperty ("audioSourcePersistentID", juce::String (audioSource->getPersistentID()));
                    playbackRegions.add (playbackRegion.get());
                }
                regionSequence->setProperty ("playbackRegions", playbackRegions);

                regionSequences.add (regionSequence.get());
            }

            complete (juce::var (regionSequences));
            return;
        }
        complete (makeError ("Document not found"));
    }

    void getTranscriptionStatus (const juce::var&, std::function<void (const juce::var&)> complete)
    {
        juce::String status;
        switch (asrStatus.load())
        {
            case ASRThreadPoolJobStatus::exporting:
                status = "Exporting";
                break;
            case ASRThreadPoolJobStatus::loadingModel:
                status = "Loading Model";
                break;
            case ASRThreadPoolJobStatus::transcribing:
                status = "Transcribing";
                break;
            case ASRThreadPoolJobStatus::ready:
            case ASRThreadPoolJobStatus::finished:
            case ASRThreadPoolJobStatus::failed:
                break;
        }
        complete (juce::var (status));
    }

    void getWebState (const juce::var&, std::function<void (const juce::var&)> complete)
    {
        complete (audioProcessor.state.getProperty ("webState"));
    }

    void getWhisperLanguages (const juce::var&, std::function<void (const juce::var&)> complete)
    {
        complete (juce::var (WhisperLanguages::get()));
    }

    void play (const juce::var&, std::function<void (const juce::var&)> complete)
    {
        if (auto* playbackController = getPlaybackController())
        {
            playbackController->requestStartPlayback();
            complete (juce::var());
            return;
        }
        complete (makeError ("Playback controller not found"));
    }

    void stop (const juce::var&, std::function<void (const juce::var&)> complete)
    {
        if (auto* playbackController = getPlaybackController())
        {
            playbackController->requestStopPlayback();
            complete (juce::var());
            return;
        }
        complete (makeError ("Playback controller not found"));
    }

    void setPlaybackPosition (const juce::var& args, std::function<void (const juce::var&)> complete)
    {
        if (! args.isArray() || args.size() < 1)
        {
            complete (makeError ("Invalid arguments"));
            return;
        }

        if (auto* playbackController = getPlaybackController())
        {
            const double position = args[0];
            playbackController->requestSetPlaybackPosition (position);
            complete (juce::var());
            return;
        }
        complete (makeError ("Playback controller not found"));
    }

    void setWebState (const juce::var& args, std::function<void (const juce::var&)> complete)
    {
        if (! args.isArray() || args.size() < 1 || ! args[0].isString())
        {
            complete (makeError ("Invalid arguments"));
            return;
        }

        audioProcessor.state.setProperty ("webState", args[0], nullptr);
        complete (juce::var());
    }

    void transcribeAudioSource (const juce::var& args, std::function<void (const juce::var&)> complete)
    {
        if (! args.isArray() || args.size() < 1)
        {
            complete (makeError ("Invalid arguments"));
            return;
        }

        if (asrEngine == nullptr)
        {
            complete (makeError ("ASR engine not initialized"));
            return;
        }

        std::unique_ptr<ASROptions> options = std::make_unique<ASROptions>();
        if (args.size() > 1)
        {
            const auto optionsObj = args[1].getDynamicObject();
            if (optionsObj != nullptr)
            {
                if (optionsObj->hasProperty ("language"))
                    options->language = optionsObj->getProperty ("language");
                if (optionsObj->hasProperty ("translate"))
                    options->translate = optionsObj->getProperty ("translate");
            }
        }

        const auto audioSourcePersistentID = args[0].toString();
        if (auto* audioSource = getAudioSourceByPersistentID (audioSourcePersistentID))
        {
            auto* job = new ASRThreadPoolJob (
                *asrEngine,
                Config::modelName,
                audioSource,
                std::move(options),
                [this] (ASRThreadPoolJobStatus status) {
                    asrStatus = status;
                },
                [this, complete] (const ASRThreadPoolJobResult& result) {
                    if (result.isError)
                        complete (makeError (result.errorMessage));
                    else
                    {
                        juce::DynamicObject::Ptr obj = new juce::DynamicObject();
                        juce::Array<juce::var> segments;
                        for (const auto& segment : result.segments)
                            segments.add (segment.toDynamicObject(false).get());
                        obj->setProperty ("segments", segments);
                        complete (juce::var (obj));
                    }
                }
            );
            threadPool.addJob (job, true);
            return;
        }
        complete (makeError ("Audio source not found"));
    }

private:
    ReaSpeechLiteDocumentController* getDocumentController()
    {
        return ReaSpeechLiteDocumentController::get (editorView);
    }

    ARA::PlugIn::HostPlaybackController* getPlaybackController()
    {
        if (auto* documentController = getDocumentController())
            return documentController->getPlaybackController();
        return nullptr;
    }

    juce::ARADocument* getDocument()
    {
        if (auto* documentController = getDocumentController())
            return documentController->getDocument();
        return nullptr;
    }

    juce::ARAAudioSource* getAudioSourceByPersistentID (const juce::String& audioSourcePersistentID)
    {
        if (auto* document = getDocument())
        {
            for (const auto& as : document->getAudioSources())
                if (as->getPersistentID() == audioSourcePersistentID)
                    return as;

        }
        return nullptr;
    }

    juce::var makeError (const juce::String& message)
    {
        juce::DynamicObject::Ptr error = new juce::DynamicObject();
        error->setProperty ("error", message);
        return juce::var (error.get());
    }

    void withReaperUndo (const juce::String& label, std::function<void()> action)
    {
        if (rpr.hasPreventUIRefresh)
            rpr.PreventUIRefresh(1);

        if (rpr.hasUndo_BeginBlock2)
            rpr.Undo_BeginBlock2(ReaperProxy::activeProject);

        action();

        if (rpr.hasUndo_EndBlock2)
            rpr.Undo_EndBlock2(ReaperProxy::activeProject, label.toRawUTF8(), -1);

        if (rpr.hasPreventUIRefresh)
            rpr.PreventUIRefresh(-1);
    }

    juce::ARAEditorView& editorView;
    ReaSpeechLiteAudioProcessorImpl& audioProcessor;
    ReaperProxy& rpr { audioProcessor.reaperProxy };

    std::unique_ptr<ASREngine> asrEngine;
    std::atomic<ASRThreadPoolJobStatus> asrStatus;
    juce::ThreadPool threadPool { 1 };
};
