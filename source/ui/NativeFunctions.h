#pragma once

#include <atomic>
#include <functional>
#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include "../Config.h"
#include "../asr/ASREngine.h"
#include "../asr/ParakeetEngine.h"
#include "../asr/ASROptions.h"
#include "../asr/ASRThreadPoolJob.h"
#include "../asr/GenericASRThreadPoolJob.h"
#include "../asr/WhisperLanguages.h"
#include "../plugin/ReaSpeechLiteAudioProcessorImpl.h"
#include "../reaper/ReaperProxy.h"
#include "../types/MarkerType.h"
#include "../utils/AbortHandler.h"
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
        // Don't create ParakeetEngine here - it will be created lazily when first needed
        // This prevents ONNX Runtime from being loaded at plugin startup
    }

    // Timeout in milliseconds for aborting transcription jobs
    static constexpr int abortTimeout = 5000;

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
            .withNativeFunction ("abortTranscription", bindFn (&NativeFunctions::abortTranscription))
            .withNativeFunction ("canCreateMarkers", bindFn (&NativeFunctions::canCreateMarkers))
            .withNativeFunction ("createMarkers", bindFn (&NativeFunctions::createMarkers))
            .withNativeFunction ("getAudioSources", bindFn (&NativeFunctions::getAudioSources))
            .withNativeFunction ("getAudioSourceTranscript", bindFn (&NativeFunctions::getAudioSourceTranscript))
            .withNativeFunction ("getModels", bindFn (&NativeFunctions::getModels))
            .withNativeFunction ("getPlayHeadState", bindFn (&NativeFunctions::getPlayHeadState))
            .withNativeFunction ("getProcessingTime", bindFn (&NativeFunctions::getProcessingTime))
            .withNativeFunction ("getRegionSequences", bindFn (&NativeFunctions::getRegionSequences))
            .withNativeFunction ("getTranscriptionStatus", bindFn (&NativeFunctions::getTranscriptionStatus))
            .withNativeFunction ("getWhisperLanguages", bindFn (&NativeFunctions::getWhisperLanguages))
            .withNativeFunction ("play", bindFn (&NativeFunctions::play))
            .withNativeFunction ("stop", bindFn (&NativeFunctions::stop))
            .withNativeFunction ("saveFile", bindFn (&NativeFunctions::saveFile))
            .withNativeFunction ("setAudioSourceTranscript", bindFn (&NativeFunctions::setAudioSourceTranscript))
            .withNativeFunction ("setDebugMode", bindFn (&NativeFunctions::setDebugMode))
            .withNativeFunction ("setPlaybackPosition", bindFn (&NativeFunctions::setPlaybackPosition))
            .withNativeFunction ("setWebState", bindFn (&NativeFunctions::setWebState))
            .withNativeFunction ("transcribeAudioSource", bindFn (&NativeFunctions::transcribeAudioSource));
    }

    void abortTranscription (const juce::var&, std::function<void (const juce::var&)> complete)
    {
        threadPool.removeAllJobs (true, 0); // Non-blocking call to initiate job removal
        new AbortHandler (threadPool, complete, abortTimeout);
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
        if (! args.isArray() || args.size() < 2 || ! args[0].isArray() || ! args[1].isString())
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

        // Check for required REAPER API functions based on marker type
        if (! rpr.hasAddProjectMarker2)
        {
            complete (makeError ("AddProjectMarker2 function not available"));
            return;
        }

        withReaperUndo ("Create " + MarkerType::toString (markerType) + " from transcript", [&] {
            try
            {
                if (markerType == MarkerType::notes)
                    addReaperNotesTrack (markers);
                else
                    addReaperMarkers (markers, markerType);
            }
            catch (const ReaperProxy::Missing& e)
            {
                DBG ("Missing REAPER API function: " + juce::String (e.what()));
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

    void getAudioSourceTranscript (const juce::var& args, std::function<void (const juce::var&)> complete)
    {
        if (! args.isArray() || args.size() < 1 || ! args[0].isString())
        {
            complete (makeError ("Invalid arguments"));
            return;
        }

        const auto audioSourceID = args[0].toString();
        if (auto* document = getDocument())
        {
            const auto& audioSources = document->getAudioSources<ReaSpeechLiteAudioSource>();
            for (const auto& audioSource : audioSources)
            {
                if (audioSource->getPersistentID() == audioSourceID)
                {
                    complete (audioSource->getTranscript());
                    return;
                }
            }
            complete (makeError ("Audio source not found"));
            return;
        }
        complete (makeError ("Document not found"));
    }

    void getModels (const juce::var&, std::function<void (const juce::var&)> complete)
    {
        juce::Array<juce::var> models;
        for (const auto& model : Config::models)
        {
            juce::DynamicObject::Ptr modelObj = new juce::DynamicObject();
            modelObj->setProperty ("name", juce::String(model.first));
            modelObj->setProperty ("label", juce::String(model.second));
            models.add (modelObj.get());
        }
        complete (juce::var (models));
    }

    void getPlayHeadState (const juce::var&, std::function<void (const juce::var&)> complete)
    {
        auto playHeadStateObj = audioProcessor.playHeadState.toDynamicObject();
        complete (juce::var (playHeadStateObj.get()));
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
        int progress = 0;
        float transcriptionTime = 0.0f;
        bool useParakeet = usingParakeetEngine.load();

        switch (asrStatus.load())
        {
            case ASRThreadPoolJobStatus::exporting:
                status = "Exporting";
                break;
            case ASRThreadPoolJobStatus::downloadingModel:
                status = "Downloading";
                if (useParakeet && parakeetEngine != nullptr)
                    progress = parakeetEngine->getProgress();
                else if (!useParakeet && asrEngine != nullptr)
                    progress = asrEngine->getProgress();
                break;
            case ASRThreadPoolJobStatus::loadingModel:
                status = "Loading Model";
                break;
            case ASRThreadPoolJobStatus::transcribing:
                status = "Transcribing";
                if (useParakeet && parakeetEngine != nullptr)
                    progress = parakeetEngine->getProgress();
                else if (!useParakeet && asrEngine != nullptr)
                    progress = asrEngine->getProgress();
                break;
            case ASRThreadPoolJobStatus::finished:
                // Get transcription time from both engines
                if (useParakeet && parakeetEngine != nullptr)
                    transcriptionTime = parakeetEngine->getLastTranscriptionTime();
                else if (!useParakeet && asrEngine != nullptr)
                    transcriptionTime = asrEngine->getLastTranscriptionTime();
                break;
            case ASRThreadPoolJobStatus::ready:
            case ASRThreadPoolJobStatus::aborted:
            case ASRThreadPoolJobStatus::failed:
                break;
        }
        juce::DynamicObject::Ptr result = new juce::DynamicObject();
        result->setProperty ("status", status);
        result->setProperty ("progress", progress);
        result->setProperty ("transcriptionTime", transcriptionTime);
        complete (juce::var (result.get()));
    }

    void getWhisperLanguages (const juce::var&, std::function<void (const juce::var&)> complete)
    {
        complete (juce::var (WhisperLanguages::get()));
    }

    void getProcessingTime (const juce::var&, std::function<void (const juce::var&)> complete)
    {
        // Return the last transcription time from whichever engine was used
        float transcriptionTime = 0.0f;
        bool useParakeet = usingParakeetEngine.load();

        if (useParakeet && parakeetEngine != nullptr)
            transcriptionTime = parakeetEngine->getLastTranscriptionTime();
        else if (!useParakeet && asrEngine != nullptr)
            transcriptionTime = asrEngine->getLastTranscriptionTime();

        complete (juce::var (transcriptionTime));
    }

    void setDebugMode (const juce::var& args, std::function<void (const juce::var&)> complete)
    {
        if (!args.isBool())
        {
            complete (makeError ("Invalid arguments"));
            return;
        }

        debugMode.store (args);
        complete (juce::var());
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

    void saveFile (const juce::var& args, std::function<void (const juce::var&)> complete)
    {
        if (! args.isArray() || args.size() < 4 || ! args[0].isString() || ! args[1].isString() || ! args[2].isString() || ! args[3].isString())
        {
            complete (makeError ("Invalid arguments"));
            return;
        }

        const auto title = args[0].toString();
        const auto initialFilename = args[1].toString();
        const auto patterns = args[2].toString();
        const auto content = args[3].toString();

        const auto initialFile = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
            .getChildFile (initialFilename);

        const auto flags = juce::FileBrowserComponent::saveMode
            | juce::FileBrowserComponent::canSelectFiles
            | juce::FileBrowserComponent::warnAboutOverwriting;

        fileChooser = std::make_unique<juce::FileChooser> (title, initialFile, patterns);
        fileChooser->launchAsync (flags, [this, content, complete] (const juce::FileChooser& chooser)
        {
            const auto file = chooser.getResult();
            if (file != juce::File())
            {
                if (file.replaceWithText (content, false))
                {
                    juce::DynamicObject::Ptr result = new juce::DynamicObject();
                    result->setProperty ("filePath", file.getFullPathName());
                    complete (juce::var (result.get()));
                    return;
                }
                complete (makeError ("Failed to save file"));
                return;
            }
            juce::DynamicObject::Ptr result = new juce::DynamicObject();
            result->setProperty ("filePath", "");
            complete (juce::var (result.get()));
        });
    }

    void setAudioSourceTranscript (const juce::var& args, std::function<void (const juce::var&)> complete)
    {
        if (! args.isArray() || args.size() < 2 || ! args[0].isString() || ! args[1].isObject())
        {
            complete (makeError ("Invalid arguments"));
            return;
        }

        const auto audioSourceID = args[0].toString();
        const auto transcript = args[1].getDynamicObject();

        if (auto* document = getDocument())
        {
            const auto& audioSources = document->getAudioSources<ReaSpeechLiteAudioSource>();
            for (const auto& audioSource : audioSources)
            {
                if (audioSource->getPersistentID() == audioSourceID)
                {
                    audioSource->setTranscript (transcript);
                    complete (juce::var());
                    return;
                }
            }
            complete (makeError ("Audio source not found"));
            return;
        }
        complete (makeError ("Document not found"));
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
                if (optionsObj->hasProperty ("modelName"))
                    options->modelName = optionsObj->getProperty ("modelName");
                if (optionsObj->hasProperty ("language"))
                    options->language = optionsObj->getProperty ("language");
                if (optionsObj->hasProperty ("translate"))
                    options->translate = optionsObj->getProperty ("translate");
            }
        }

        const auto audioSourcePersistentID = args[0].toString();
        if (auto* audioSource = getAudioSourceByPersistentID (audioSourcePersistentID))
        {
            // Determine which engine to use based on model name
            bool useParakeet = Config::isParakeetModel (options->modelName.toStdString());

            // Lazy initialization of ParakeetEngine - only create when first needed
            if (useParakeet && parakeetEngine == nullptr)
            {
                parakeetEngine = std::make_unique<ParakeetEngine> (Config::getModelsDir());
            }

            juce::ThreadPoolJob* job = nullptr;

            auto statusCallback = [this] (ASRThreadPoolJobStatus status) {
                asrStatus = status;
            };

            auto completionCallback = [this, complete] (const ASRThreadPoolJobResult& result) {
                if (result.isError)
                    complete (makeError (result.errorMessage));
                else
                {
                    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
                    juce::Array<juce::var> segments;
                    for (const auto& segment : result.segments)
                        segments.add (segment.toDynamicObject(false).get());
                    obj->setProperty ("segments", segments);
                    complete (juce::var (obj.get()));
                }
            };

            if (useParakeet)
            {
                usingParakeetEngine.store (true);
                job = new GenericASRThreadPoolJob<ParakeetEngine> (
                    *parakeetEngine,
                    audioSource,
                    std::move(options),
                    statusCallback,
                    completionCallback
                );
            }
            else
            {
                usingParakeetEngine.store (false);
                job = new GenericASRThreadPoolJob<ASREngine> (
                    *asrEngine,
                    audioSource,
                    std::move(options),
                    statusCallback,
                    completionCallback
                );
            }

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

    void logToConsole (const juce::String& message)
    {
        if (rpr.hasShowConsoleMsg)
        {
            rpr.ShowConsoleMsg((message + "\n").toRawUTF8());
        }
        // Also log with DBG for debugging outside REAPER
        DBG(message);
    }

    void addReaperMarkers (const juce::Array<juce::var>* markers, const MarkerType::Enum markerType)
    {
        int markerNum = 1;
        for (const auto& markerVar : *markers)
        {
            const auto marker = markerVar.getDynamicObject();
            const auto regions = markerType == MarkerType::regions;
            const auto start = marker->getProperty ("start");
            const auto end = marker->getProperty ("end");
            const auto name = marker->getProperty ("name");

            rpr.AddProjectMarker2 (ReaperProxy::activeProject, regions, start, end, name.toString().toRawUTF8(), markerNum, 0);
            markerNum++;
        }
    }

    void addReaperNotesTrack (const juce::Array<juce::var>* markers, const char* trackName = "Transcript")
    {
        const auto index = 0;
        const auto originalPosition = rpr.GetCursorPositionEx (ReaperProxy::activeProject);

        rpr.InsertTrackInProject (ReaperProxy::activeProject, index, 0);
        const auto track = rpr.GetTrack (ReaperProxy::activeProject, index);
        rpr.SetOnlyTrackSelected (track);
        rpr.GetSetMediaTrackInfo_String (track, "P_NAME", const_cast<char*> (trackName), true);

        for (const auto& markerVar : *markers)
        {
            const auto marker = markerVar.getDynamicObject();
            const auto start = marker->getProperty ("start");
            const auto end = marker->getProperty ("end");
            const auto name = marker->getProperty ("name");

            const auto item = createEmptyReaperItem (start, end);
            setReaperNoteText (item, name.toString());
        }

        rpr.SetEditCurPos2 (ReaperProxy::activeProject, originalPosition, true, true);
    }

    ReaperProxy::MediaItem* createEmptyReaperItem (const double start, const double end)
    {
        rpr.Main_OnCommandEx(40142, 0, ReaperProxy::activeProject); // Insert empty item
        auto* item = rpr.GetSelectedMediaItem(ReaperProxy::activeProject, 0);
        rpr.SelectAllMediaItems (ReaperProxy::activeProject, false);
        rpr.SetMediaItemPosition (item, start, true);
        rpr.SetMediaItemLength (item, end - start, true);
        return item;
    }

    void setReaperNoteText (ReaperProxy::MediaItem* item, const juce::String& text, bool stretch = false)
    {
        char buffer[4096];
        rpr.GetItemStateChunk (item, buffer, sizeof (buffer), false);
        const auto chunk = juce::String (buffer);

        // This function is currently only used with new items, and the chunk size
        // in that case is currently around 200 bytes. If this changes, the buffer
        // size may need to be increased. The current size is a bit arbitrary.
        jassert (static_cast<size_t> (chunk.length()) < sizeof (buffer) - 1);

        juce::String notesChunk;
        notesChunk << "<NOTES\n|" << text.trim() << "\n>\n";
        const juce::String flagsChunk { stretch ? "IMGRESOURCEFLAGS 11\n" : "" };

        const auto newChunk = chunk.replace(">", notesChunk.replace ("%", "%%") + flagsChunk + ">");
        rpr.SetItemStateChunk (item, newChunk.toRawUTF8(), false);
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
    std::unique_ptr<ParakeetEngine> parakeetEngine;
    std::atomic<ASRThreadPoolJobStatus> asrStatus;
    std::atomic<bool> usingParakeetEngine { false };
    std::atomic<bool> debugMode { false };
    juce::ThreadPool threadPool { 1 };

    std::unique_ptr<juce::FileChooser> fileChooser;
};
