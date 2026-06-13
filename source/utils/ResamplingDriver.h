#pragma once

#include <cstring>
#include <memory>
#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>

class ResamplingDriver
{
public:
    ResamplingDriver(
        std::unique_ptr<juce::AudioFormatReaderSource> sourceIn,
        int sourceNumChannelsIn,
        double sourceSamplesPerDestSampleIn,
        int maximumSamplesPerBlockIn,
        double destSampleRateIn
    ) : source(std::move (sourceIn)),
        sourceNumChannels (sourceNumChannelsIn),
        sourceSamplesPerDestSample (sourceSamplesPerDestSampleIn),
        unityRatio (juce::approximatelyEqual (sourceSamplesPerDestSampleIn, 1.0))
    {
        if (! unityRatio)
            interpolators.resize ((size_t) juce::jmax (1, sourceNumChannels));
        prepareToPlay (maximumSamplesPerBlockIn, destSampleRateIn);
    }

    void seek (int sourceStartSample)
    {
        if (! hasExpectedSeekPosition)
        {
            source->setNextReadPosition (sourceStartSample);
            expectedSeekPosition = sourceStartSample;
            hasExpectedSeekPosition = true;
            clearResamplingState();
            return;
        }

        // Playback renderers repeatedly call seek() with positions derived from rounding.
        // Keep the interpolator state for tiny drift to avoid zippering artifacts.
        const auto delta = sourceStartSample - expectedSeekPosition;
        if (std::abs (delta) <= seekToleranceSamples)
            return;

        source->setNextReadPosition (sourceStartSample);
        expectedSeekPosition = sourceStartSample;
        clearResamplingState();
    }

    void read (const juce::AudioSourceChannelInfo& buffer)
    {
        if (unityRatio)
        {
            source->getNextAudioBlock (buffer);
            expectedSeekPosition += buffer.numSamples;
            return;
        }

        const auto requiredInputSamples = juce::roundToInt (std::ceil (buffer.numSamples * sourceSamplesPerDestSample))
                                        + interpolationPadding;

        ensureBufferedInput (requiredInputSamples);

        int consumedSamples = 0;
        const auto channelsToProcess = juce::jmin (sourceNumChannels, buffer.buffer->getNumChannels());

        for (int channel = 0; channel < channelsToProcess; ++channel)
        {
            auto* const output = buffer.buffer->getWritePointer (channel, buffer.startSample);
            const auto* const input = sourceBuffer.getReadPointer (channel);

            const auto used = interpolators[(size_t) channel].process (
                sourceSamplesPerDestSample,
                input,
                output,
                buffer.numSamples);

            if (channel == 0)
                consumedSamples = used;
            else
                jassert (consumedSamples == used);
        }

        discardConsumedInput (consumedSamples);
        expectedSeekPosition += consumedSamples;
    }

private:
    void prepareToPlay (int maximumSamplesPerBlockIn, double destSampleRateIn)
    {
        maximumSamplesPerBlock = maximumSamplesPerBlockIn;
        destSampleRate = destSampleRateIn;

        const auto sourceBlockSize = unityRatio
            ? maximumSamplesPerBlock
            : juce::roundToInt (std::ceil (maximumSamplesPerBlock * sourceSamplesPerDestSample)) + interpolationPadding;
        const auto sourceSampleRate = destSampleRateIn * sourceSamplesPerDestSample;

        source->prepareToPlay (sourceBlockSize, sourceSampleRate);
        if (! unityRatio)
            sourceBuffer.setSize (juce::jmax (1, sourceNumChannels), maximumSamplesPerBlock * 4);
        clearResamplingState();
    }

    void clearResamplingState()
    {
        for (auto& interpolator : interpolators)
            interpolator.reset();

        bufferedInputSamples = 0;
    }

    void ensureBufferedInput (int minimumSamplesNeeded)
    {
        if (sourceBuffer.getNumSamples() < minimumSamplesNeeded)
            sourceBuffer.setSize (sourceBuffer.getNumChannels(), minimumSamplesNeeded, true, false, true);

        while (bufferedInputSamples < minimumSamplesNeeded)
        {
            const auto samplesToRead = minimumSamplesNeeded - bufferedInputSamples;
            juce::AudioSourceChannelInfo readInfo (&sourceBuffer, bufferedInputSamples, samplesToRead);
            source->getNextAudioBlock (readInfo);
            bufferedInputSamples += samplesToRead;
        }
    }

    void discardConsumedInput (int consumedSamples)
    {
        jassert (consumedSamples >= 0);
        jassert (consumedSamples <= bufferedInputSamples);

        const auto remainingSamples = bufferedInputSamples - consumedSamples;

        if (remainingSamples > 0)
        {
            for (int channel = 0; channel < sourceBuffer.getNumChannels(); ++channel)
            {
                auto* const dest = sourceBuffer.getWritePointer (channel);
                const auto* const src = sourceBuffer.getReadPointer (channel, consumedSamples);
                std::memmove (dest, src, (size_t) remainingSamples * sizeof (float));
            }
        }

        bufferedInputSamples = remainingSamples;
    }

    std::unique_ptr<juce::AudioFormatReaderSource> source;
    std::vector<juce::WindowedSincInterpolator> interpolators;

    const int sourceNumChannels = 1;
    const double sourceSamplesPerDestSample = 1.0;
    const bool unityRatio = true;

    double destSampleRate = 48000.0;
    int maximumSamplesPerBlock = 128;

    juce::AudioBuffer<float> sourceBuffer;
    int bufferedInputSamples = 0;
    int expectedSeekPosition = 0;
    bool hasExpectedSeekPosition = false;

    static constexpr int interpolationPadding = 16;
    static constexpr int seekToleranceSamples = 2;
};
