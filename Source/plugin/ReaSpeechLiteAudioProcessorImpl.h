#pragma once

#include <JuceHeader.h>

#include "../reaper/ReaperProxy.h"
#include "../reaper/VST3Extensions.h"

class ReaSpeechLiteAudioProcessorImpl :
    public juce::AudioProcessor,
    public juce::AudioProcessorARAExtension
{
public:
    ReaSpeechLiteAudioProcessorImpl() : AudioProcessor (getBusesProperties()) {}

    ~ReaSpeechLiteAudioProcessorImpl() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override
    {
        prepareToPlayForARA (sampleRate, samplesPerBlock, getMainBusNumOutputChannels(), getProcessingPrecision());
    }

    void releaseResources() override
    {
        releaseResourcesForARA();
    }

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override
    {
        // This is the place where you check if the layout is supported.
        // In this template code we only support mono or stereo.
        // Some plugin hosts, such as certain GarageBand versions, will only
        // load plugins that support stereo bus layouts.
        if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
            && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
            return false;

        // This checks if the input layout matches the output layout
        if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
            return false;

        return true;
    }

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override
    {
        juce::ScopedNoDenormals noDenormals;

        auto* audioPlayHead = getPlayHead();

        if (! processBlockForARA (buffer, isRealtime(), audioPlayHead))
            processBlockBypassed (buffer, midiMessages);
    }

    const juce::String getName() const override { return JucePlugin_Name; }

    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override
    {
        // Use this method as the place to do any pre-playback
        // initialisation that you need..
        juce::ignoreUnused (destData);
    }

    void setStateInformation (const void* data, int sizeInBytes) override
    {
        // When playback stops, you can use this as an opportunity to free up any
        // spare memory, etc.
        juce::ignoreUnused (data);
        juce::ignoreUnused (sizeInBytes);
    }

    juce::VST3ClientExtensions* getVST3ClientExtensions() override { return &vst3Extensions; }

    ReaperProxy reaperProxy;
    VST3Extensions vst3Extensions { reaperProxy };

private:
    static BusesProperties getBusesProperties()
    {
        return BusesProperties()
            .withInput ("Input", juce::AudioChannelSet::stereo(), true)
            .withOutput ("Output", juce::AudioChannelSet::stereo(), true);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReaSpeechLiteAudioProcessorImpl)
};
