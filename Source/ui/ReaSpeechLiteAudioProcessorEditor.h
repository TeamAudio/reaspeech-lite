#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <whisper.h>

#include "../Config.h"
#include "../asr/ASREngine.h"
#include "../asr/ASROptions.h"
#include "../asr/ASRThreadPoolJob.h"
#include "../asr/WhisperLanguages.h"
#include "../plugin/ReaSpeechLiteAudioProcessorImpl.h"
#include "../reaper/ReaperProxy.h"
#include "../utils/SafeUTF8.h"

class ReaSpeechLiteAudioProcessorEditor final :
    public juce::AudioProcessorEditor,
    public juce::AudioProcessorEditorARAExtension
{
public:
    ReaSpeechLiteAudioProcessorEditor (ReaSpeechLiteAudioProcessorImpl& p) :
        AudioProcessorEditor (&p),
        AudioProcessorEditorARAExtension (&p),
        audioProcessor (p)
    {
        asrEngine = std::make_unique<ASREngine> (Config::getModelsDir());

        addAndMakeVisible (webComponent);

        // Navigate to index page
        webComponent.goToURL (juce::WebBrowserComponent::getResourceProviderRoot());

        // ARA plugins must be resizable for proper view embedding
        setResizable (true, false);

        // Make sure that before the constructor has finished, you've set the
        // editor's size to whatever you need it to be.
        setSize (900, 600);
    }

    ~ReaSpeechLiteAudioProcessorEditor() override {}

    void paint (juce::Graphics& g) override
    {
        // (Our component is opaque, so we must completely fill the background with a solid colour)
        g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
    }

    void resized() override
    {
        webComponent.setBounds (getLocalBounds());
    }

private:
    static std::vector<std::byte> getBinaryData (const char* data, size_t size)
    {
        return std::vector<std::byte>(
            reinterpret_cast<const std::byte*>(data),
            reinterpret_cast<const std::byte*>(data + size)
        );
    }

    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url)
    {
        const auto urlToRetrieve = url == "/" ? juce::String { "index.html" }
                                              : url.fromFirstOccurrenceOf ("/", false, false);

        if (urlToRetrieve == "index.html")
        {
            return juce::WebBrowserComponent::Resource {
                getBinaryData (BinaryData::index_html, BinaryData::index_htmlSize),
                "text/html"
            };
        }

        if (urlToRetrieve == "css/bootstrap.min.css")
        {
            return juce::WebBrowserComponent::Resource {
                getBinaryData (BinaryData::bootstrap_min_css, BinaryData::bootstrap_min_cssSize),
                "text/css"
            };
        }

        if (urlToRetrieve == "css/bootstrap.min.css.map")
        {
            return juce::WebBrowserComponent::Resource {
                getBinaryData (BinaryData::bootstrap_min_css_map, BinaryData::bootstrap_min_css_mapSize),
                "application/json"
            };
        }

        if (urlToRetrieve == "js/bootstrap.bundle.min.js")
        {
            return juce::WebBrowserComponent::Resource {
                getBinaryData (BinaryData::bootstrap_bundle_min_js, BinaryData::bootstrap_bundle_min_jsSize),
                "application/javascript"
            };
        }

        if (urlToRetrieve == "js/bootstrap.bundle.min.js.map")
        {
            return juce::WebBrowserComponent::Resource {
                getBinaryData (BinaryData::bootstrap_bundle_min_js_map, BinaryData::bootstrap_bundle_min_js_mapSize),
                "application/json"
            };
        }

        if (urlToRetrieve == "js/main.js")
        {
            return juce::WebBrowserComponent::Resource {
                getBinaryData (BinaryData::main_js, BinaryData::main_jsSize),
                "application/javascript"
            };
        }

        return {};
    }

    ReaSpeechLiteDocumentController* getDocumentController()
    {
        if (auto* editorView = getARAEditorView())
            return ReaSpeechLiteDocumentController::get (*editorView);
        return nullptr;
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

    void canCreateMarkers (const juce::var&, std::function<void (const juce::var&)> complete)
    {
        if (rpr.hasAddProjectMarker2)
            complete (juce::var (true));
        else
            complete (juce::var (false));
    }

    void createMarkers (const juce::var& args, std::function<void (const juce::var&)> complete)
    {
        if (! args.isArray() || args.size() < 1)
        {
            complete (makeError ("Invalid arguments"));
            return;
        }

        const auto markers = args[0].getArray();

        if (! rpr.hasAddProjectMarker2)
        {
            complete (makeError ("Function not available"));
            return;
        }

        withReaperUndo ("Create markers from transcript", [&] {
            for (auto i = 0; i < (*markers).size(); ++i)
            {
                const auto marker = (*markers)[i].getDynamicObject();
                const auto position = marker->getProperty ("position");
                const auto name = marker->getProperty ("name");
                rpr.AddProjectMarker2 (ReaperProxy::activeProject, false, position, position, name.toString().toRawUTF8(), i + 1, 0);
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
                audioSource->setProperty ("sampleCount", as->getSampleCount());
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
                            segments.add (segment.toDynamicObject().get());
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

    juce::WebBrowserComponent webComponent {
        juce::WebBrowserComponent::Options{}
            .withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
            .withWinWebView2Options (juce::WebBrowserComponent::Options::WinWebView2{}
                .withUserDataFolder (juce::File::getSpecialLocation (juce::File::SpecialLocationType::tempDirectory)))
            .withNativeIntegrationEnabled()
            .withNativeFunction ("canCreateMarkers", [this] (const auto& args, const auto& complete) { canCreateMarkers (args, complete); })
            .withNativeFunction ("createMarkers", [this] (const auto& args, const auto& complete) { createMarkers (args, complete); })
            .withNativeFunction ("getAudioSources", [this] (const auto& args, const auto& complete) { getAudioSources (args, complete); })
            .withNativeFunction ("getRegionSequences", [this] (const auto& args, const auto& complete) { getRegionSequences (args, complete); })
            .withNativeFunction ("getTranscriptionStatus", [this] (const auto& args, const auto& complete) { getTranscriptionStatus (args, complete); })
            .withNativeFunction ("getWhisperLanguages", [this] (const auto& args, const auto& complete) { getWhisperLanguages (args, complete); })
            .withNativeFunction ("play", [this] (const auto& args, const auto& complete) { play (args, complete); })
            .withNativeFunction ("stop", [this] (const auto& args, const auto& complete) { stop (args, complete); })
            .withNativeFunction ("setPlaybackPosition", [this] (const auto& args, const auto& complete) { setPlaybackPosition (args, complete); })
            .withNativeFunction ("transcribeAudioSource", [this] (const auto& args, const auto& complete) { transcribeAudioSource (args, complete); })
            .withResourceProvider ([this] (const auto& url) { return getResource (url); })
    };

    std::unique_ptr<ASREngine> asrEngine;
    std::atomic<ASRThreadPoolJobStatus> asrStatus;
    juce::ThreadPool threadPool { 1 };

    ReaSpeechLiteAudioProcessorImpl& audioProcessor;
    ReaperProxy& rpr { audioProcessor.reaperProxy };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReaSpeechLiteAudioProcessorEditor)
};
