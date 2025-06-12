#pragma once

#include <ARA_API/ARAInterface.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>

class ReaSpeechLiteAudioSource final : public juce::ARAAudioSource
{
public:
    ReaSpeechLiteAudioSource (juce::ARADocument* document, ARA::ARAAudioSourceHostRef hostRef)
        : ARAAudioSource (document, hostRef)
    {
        transcript = new juce::DynamicObject();
    }

    const juce::var& getTranscript() const noexcept
    {
        return transcript;
    }

    void setTranscript (const juce::var& newTranscript) noexcept
    {
        if (! newTranscript.isObject())
        {
            jassertfalse; // Transcript must be an object
            return;
        }

        if (transcript != newTranscript)
        {
            transcript = newTranscript;
            notifyContentChanged (juce::ARAContentUpdateScopes::nothingIsAffected(), false);
        }
    }

private:
    juce::var transcript;
};
