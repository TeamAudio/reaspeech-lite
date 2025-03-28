#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "../ui/ReaSpeechLiteAudioProcessorEditor.h"
#include "ReaSpeechLiteAudioProcessorImpl.h"

class ReaSpeechLiteAudioProcessor final : public ReaSpeechLiteAudioProcessorImpl
{
public:
    bool hasEditor() const override { return true; }

    juce::AudioProcessorEditor* createEditor() override
    {
        return new ReaSpeechLiteAudioProcessorEditor (*this);
    }
};
