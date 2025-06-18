#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>

#include "../types/ProcessingLockInterface.h"
#include "ReaSpeechLiteAudioSource.h"
#include "ReaSpeechLitePlaybackRenderer.h"

class ReaSpeechLiteDocumentController final :
    public juce::ARADocumentControllerSpecialisation,
    private ProcessingLockInterface
{
public:
    using ARADocumentControllerSpecialisation::ARADocumentControllerSpecialisation;

    static ReaSpeechLiteDocumentController* get (ARA::PlugIn::DocumentController* documentController)
    {
        return getSpecialisedDocumentController<ReaSpeechLiteDocumentController> (documentController);
    }

    static ReaSpeechLiteDocumentController* get (const juce::ARAEditorView& editorView)
    {
        return get (editorView.getDocumentController());
    }

    ARA::PlugIn::HostPlaybackController* getPlaybackController()
    {
        return getDocumentController()->getHostPlaybackController();
    }

protected:
    void willBeginEditing (juce::ARADocument*) noexcept override
    {
        processBlockLock.enterWrite();
    }

    void didEndEditing (juce::ARADocument*) noexcept override
    {
        processBlockLock.exitWrite();
    }

    juce::ARAAudioSource* doCreateAudioSource (juce::ARADocument* document, ARA::ARAAudioSourceHostRef hostRef) noexcept override
    {
        return new ReaSpeechLiteAudioSource (document, hostRef);
    }

    juce::ARAPlaybackRenderer* doCreatePlaybackRenderer() noexcept override
    {
        return new ReaSpeechLitePlaybackRenderer (getDocumentController(), *this);
    }

    bool doRestoreObjectsFromStream (juce::ARAInputStream& input, const juce::ARARestoreObjectsFilter* filter) noexcept override
    {
        // Start reading data from the archive, starting with the number of audio sources in the archive
        const auto numAudioSources = input.readInt64();

        // Loop over stored audio source data
        for (juce::int64 i = 0; i < numAudioSources; ++i)
        {
            auto audioSourceID = input.readString();
            auto transcriptJSON = input.readString();

            auto audioSource = filter->getAudioSourceToRestoreStateWithID<ReaSpeechLiteAudioSource> (audioSourceID.getCharPointer());

            if (audioSource == nullptr)
                continue;

            // Parse JSON string back to juce::var
            juce::var transcript;
            auto result = juce::JSON::parse (transcriptJSON, transcript);

            if (result.wasOk())
            {
                audioSource->setTranscript (transcript);
            }
            else
            {
                DBG ("Failed to parse transcript JSON for audio source ID: " + audioSourceID);
                return false;
            }
        }

        return ! input.failed();
    }

    bool doStoreObjectsToStream (juce::ARAOutputStream& output, const juce::ARAStoreObjectsFilter* filter) noexcept override
    {
        const auto& audioSourcesToPersist { filter->getAudioSourcesToStore<ReaSpeechLiteAudioSource>() };

        // Write the number of audio sources we are persisting
        const auto numAudioSources = audioSourcesToPersist.size();

        if (! output.writeInt64 ((juce::int64) numAudioSources))
            return false;

        // For each audio source to persist, persist its ID followed by its transcript
        for (size_t i = 0; i < numAudioSources; ++i)
        {
            // Write audio source ID and transcript as JSON
            if (! output.writeString (audioSourcesToPersist[i]->getPersistentID()))
                return false;

            // Convert juce::var to JSON string for storage
            juce::String transcriptJSON = juce::JSON::toString (audioSourcesToPersist[i]->getTranscript());
            if (! output.writeString (transcriptJSON))
                return false;
        }

        return true;
    }

private:
    juce::ScopedTryReadLock getProcessingLock() override
    {
        return juce::ScopedTryReadLock { processBlockLock };
    }

    juce::ReadWriteLock processBlockLock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReaSpeechLiteDocumentController)
};
