#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>

class AudioSourceEventEmitter : private juce::ARADocument::Listener, private juce::ARAAudioSource::Listener
{
public:
    AudioSourceEventEmitter (
        juce::ARAEditorView& editorViewIn,
        juce::WebBrowserComponent& webComponentIn
    ) : editorView (editorViewIn),
        webComponent (webComponentIn)
    {
        if (auto* document = getDocument())
        {
            document->addListener (this);

            for (auto* audioSource : document->getAudioSources())
            {
                audioSource->addListener (this);
            }
        }
    }

    ~AudioSourceEventEmitter() override
    {
        if (auto* document = getDocument())
        {
            for (auto* audioSource : document->getAudioSources())
            {
                audioSource->removeListener (this);
            }

            document->removeListener (this);
        }
    }

private:
    void didAddAudioSourceToDocument (juce::ARADocument*, juce::ARAAudioSource* audioSource) override
    {
        audioSource->addListener (this);
        emitAudioSourceEvent ("audioSourceAdded", audioSource);
    }

    void willRemoveAudioSourceFromDocument (juce::ARADocument*, juce::ARAAudioSource* audioSource) override
    {
        audioSource->removeListener (this);
        emitAudioSourceEvent ("audioSourceRemoved", audioSource);
    }

    void doUpdateAudioSourceContent (juce::ARAAudioSource* audioSource, juce::ARAContentUpdateScopes) override
    {
        emitAudioSourceEvent ("audioSourceContentUpdated", audioSource);
    }

    void emitAudioSourceEvent (const juce::String& eventName, juce::ARAAudioSource* audioSource)
    {
        juce::DynamicObject::Ptr eventObj = new juce::DynamicObject();
        eventObj->setProperty ("persistentID", juce::String (audioSource->getPersistentID()));
        webComponent.emitEventIfBrowserIsVisible (eventName, juce::var (eventObj.get()));
    }

    juce::ARADocument* getDocument()
    {
        if (auto* documentController = editorView.getDocumentController())
        {
            return documentController->getDocument<juce::ARADocument>();
        }
        return nullptr;
    }

    juce::ARAEditorView& editorView;
    juce::WebBrowserComponent& webComponent;
};
