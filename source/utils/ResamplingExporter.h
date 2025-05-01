#pragma once

#include <algorithm>
#include <memory>
#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>

struct ResamplingExporter
{
    static constexpr int blockSize = 4096;

    /**
     * Reads audio data from the given audio source, resamples it to the
     * specified destination sample rate, and stores the resampled audio data
     * in the provided buffer.
     *
     * @param audioSource The audio source to read audio data from.
     * @param destSampleRate The sample rate to which the audio data should be resampled.
     * @param channel The channel index to read from the audio source.
     * @param buffer A vector to store the resampled audio data.
     * @param isAborted Optional callback that returns true if the operation should be aborted.
     */
    static void exportAudio (juce::ARAAudioSource* audioSource,
        ARA::ARASampleRate destSampleRate,
        int channel,
        std::vector<float>& buffer,
        std::function<bool()> isAborted = nullptr)
    {
        const auto sourceChannelCount = audioSource->getChannelCount();
        jassert (channel >= 0 && channel < sourceChannelCount);

        const auto sourceSampleRate = audioSource->getSampleRate();
        const auto sourceSampleCount = audioSource->getSampleCount();

        // Create an audio reader source
        auto readerSource = std::make_unique<juce::AudioFormatReaderSource> (
            new juce::ARAAudioSourceReader (audioSource), true);

        // Create a resampling source
        auto resamplingSource = std::make_unique<juce::ResamplingAudioSource> (
            readerSource.get(), false, sourceChannelCount);

        // Calculate the resampling ratio
        const double sourceSamplesPerDestSample = sourceSampleRate / destSampleRate;
        const double destSamplesPerSourceSample = destSampleRate / sourceSampleRate;

        // Set the resampling ratio
        resamplingSource->setResamplingRatio (sourceSamplesPerDestSample);
        resamplingSource->prepareToPlay (blockSize, destSampleRate);

        // Calculate destination buffer size based on resampling ratio
        const auto destSampleCount = juce::roundToInt (sourceSampleCount * destSamplesPerSourceSample);
        buffer.resize (static_cast<size_t>(destSampleCount));

        // Process in blocks
        juce::AudioBuffer<float> tempBuffer(sourceChannelCount, blockSize);
        juce::AudioSourceChannelInfo channelInfo(tempBuffer);

        int destSamplePos = 0;
        while (destSamplePos < destSampleCount)
        {
            if (isAborted && isAborted())
                return;

            resamplingSource->getNextAudioBlock(channelInfo);

            // Copy the resampled block to the destination buffer
            const int samplesToProcess = juce::jmin (blockSize, destSampleCount - destSamplePos);
            const float* sourceData = tempBuffer.getReadPointer(channel);
            std::copy(sourceData, sourceData + samplesToProcess, buffer.begin() + destSamplePos);

            destSamplePos += samplesToProcess;
        }
    }
};
