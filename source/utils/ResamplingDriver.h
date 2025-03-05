#pragma once

#include <memory>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>

class ResamplingDriver
{
public:
    ResamplingDriver(
        std::unique_ptr<juce::AudioFormatReaderSource> sourceIn,
        std::unique_ptr<juce::ResamplingAudioSource> resamplerIn
    ) : source(std::move (sourceIn)),
        resampler(std::move (resamplerIn))
    {
        prepareToPlay (maximumSamplesPerBlock, destSampleRate);
    }

    void seek(int sourceStartSample)
    {
        if (source->getNextReadPosition() != sourceStartSample)
        {
            // Clear resampling state if read position is moving significantly
            if (std::abs (source->getNextReadPosition() - sourceStartSample) > 10)
                resampler->flushBuffers();

            source->setNextReadPosition (sourceStartSample);
        }
    }

    void read(const juce::AudioSourceChannelInfo& buffer)
    {
        resampler->getNextAudioBlock (buffer);
    }

private:
    void prepareToPlay (int maximumSamplesPerBlockIn, double destSampleRateIn)
    {
        maximumSamplesPerBlock = maximumSamplesPerBlockIn;
        destSampleRate = destSampleRateIn;

        source->prepareToPlay (maximumSamplesPerBlock, destSampleRate);
        resampler->prepareToPlay (maximumSamplesPerBlock, destSampleRate);
    }

    std::unique_ptr<juce::AudioFormatReaderSource> source;
    std::unique_ptr<juce::ResamplingAudioSource> resampler;

    double destSampleRate = 48000.0;
    int maximumSamplesPerBlock = 128;

};
