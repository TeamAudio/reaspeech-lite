#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>

#include "../types/ProcessingLockInterface.h"
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

    juce::ARAPlaybackRenderer* doCreatePlaybackRenderer() noexcept override
    {
        return new ReaSpeechLitePlaybackRenderer (getDocumentController(), *this);
    }

    bool doRestoreObjectsFromStream (juce::ARAInputStream&, const juce::ARARestoreObjectsFilter*) noexcept override
    {
        // You should use this method to read any persistent data associated with
        // your ARA model graph stored in an archive using the supplied ARAInputStream.
        // Be sure to check the ARARestoreObjectsFilter to determine which objects to restore.
        return true;
    }

    bool doStoreObjectsToStream (juce::ARAOutputStream&, const juce::ARAStoreObjectsFilter*) noexcept override
    {
        // You should use this method to write any persistent data associated with
        // your ARA model graph into the an archive using the supplied ARAOutputStream.
        // Be sure to check the ARAStoreObjectsFilter to determine which objects to store.
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
