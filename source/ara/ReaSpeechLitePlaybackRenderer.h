#pragma once

#include <map>
#include <memory>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>

#include "../types/ProcessingLockInterface.h"
#include "../utils/ResamplingDriver.h"
#include "../utils/SharedTimeSliceThread.h"

class ReaSpeechLitePlaybackRenderer final : public juce::ARAPlaybackRenderer
{
public:
    ReaSpeechLitePlaybackRenderer (
        ARA::PlugIn::DocumentController* documentControllerIn,
        ProcessingLockInterface& lockInterfaceIn,
        bool useBufferedAudioSourceReaderIn = false,
        int readTimeoutIn = 100
    ) : ARAPlaybackRenderer(documentControllerIn),
        lockInterface (lockInterfaceIn),
        useBufferedAudioSourceReader (useBufferedAudioSourceReaderIn),
        readTimeout (readTimeoutIn)
    {
    }

    void prepareToPlay (
        double sampleRateIn,
        int maximumSamplesPerBlockIn,
        int numChannelsIn,
        juce::AudioProcessor::ProcessingPrecision,
        AlwaysNonRealtime) override
    {
        destSampleRate = sampleRateIn;
        destNumChannels = numChannelsIn;
        maximumSamplesPerBlock = maximumSamplesPerBlockIn;

        tempBuffer.reset (new juce::AudioBuffer<float> (destNumChannels, maximumSamplesPerBlock));

        for (const auto playbackRegion : getPlaybackRegions())
            buildReader (playbackRegion);
    }

    void releaseResources() override
    {
        resamplers.clear();
        tempBuffer.reset();
    }

    bool processBlock (
        juce::AudioBuffer<float>& buffer,
        juce::AudioProcessor::Realtime,
        const juce::AudioPlayHead::PositionInfo& positionInfo) noexcept override
    {
        const auto lock = lockInterface.getProcessingLock();

        if (! lock.isLocked())
            return true;

        if (! positionInfo.getIsPlaying())
        {
            buffer.clear();
            return true;
        }

        const auto numSamples = buffer.getNumSamples();
        jassert (numSamples <= maximumSamplesPerBlock);
        jassert (destNumChannels == buffer.getNumChannels());

        const auto timeInSamples = positionInfo.getTimeInSamples().orFallback (0);
        const auto blockRange = juce::Range<juce::int64>::withStartAndLength (timeInSamples, numSamples);

        return render (buffer, blockRange);
    }

    using ARAPlaybackRenderer::processBlock;

private:
    void buildReader (juce::ARAPlaybackRegion* playbackRegion)
    {
        auto audioSource = playbackRegion->getAudioModification()->getAudioSource();
        if (resamplers.find (audioSource) != resamplers.end())
            return;

        std::unique_ptr<juce::AudioFormatReaderSource> readerSource;

        if (useBufferedAudioSourceReader)
        {
            readerSource = std::make_unique<juce::AudioFormatReaderSource> (
                buildBufferedReader(audioSource), true);
        }
        else
        {
            readerSource = std::make_unique<juce::AudioFormatReaderSource> (
                new juce::ARAAudioSourceReader (audioSource), true);
        }

        auto resamplingSource = std::make_unique<juce::ResamplingAudioSource> (
            readerSource.get(), false, audioSource->getChannelCount());

        resamplingSource->setResamplingRatio (audioSource->getSampleRate() / destSampleRate);

        resamplers.emplace (audioSource, std::make_unique<ResamplingDriver> (
            std::move (readerSource),
            std::move (resamplingSource)));
    }

    juce::BufferingAudioReader* buildBufferedReader(juce::ARAAudioSource* audioSource)
    {
        const auto readAheadSize =
            juce::jmax (4 * maximumSamplesPerBlock, juce::roundToInt (2.0 * destSampleRate));

        auto bufferingReader = new juce::BufferingAudioReader (
            new juce::ARAAudioSourceReader (audioSource),
            *sharedTimesliceThread,
            readAheadSize);

        bufferingReader->setReadTimeout (readTimeout);

        return bufferingReader;
    }

    bool render (juce::AudioBuffer<float>& buffer, const juce::Range<juce::int64>& blockRange)
    {
        bool success = true;
        bool didRenderAnyRegion = false;

        for (const auto& playbackRegion : getPlaybackRegions())
        {
            success = success &&
                renderRegion (buffer, playbackRegion, blockRange, didRenderAnyRegion);
        }

        // If no playback or no region did intersect, clear buffer now.
        if (! didRenderAnyRegion)
            buffer.clear();

        return success;
    }

    bool renderRegion (
        juce::AudioBuffer<float>& buffer,
        juce::ARAPlaybackRegion* playbackRegion,
        const juce::Range<juce::int64>& blockRange,
        bool& didRenderAnyRegion)
    {
        // Get the audio source for the region and find the reader and resampling sources.
        const auto audioSource = playbackRegion->getAudioModification()->getAudioSource();
        const auto resamplerIt = resamplers.find (audioSource);

        if (resamplerIt == resamplers.end())
            return false;

        auto& resampler = resamplerIt->second;

        auto sourceSampleRate = audioSource->getSampleRate();
        auto sourceNumChannels = audioSource->getChannelCount();

        auto destSamplesPerSourceSample = destSampleRate / sourceSampleRate;
        auto sourceSamplesPerDestSample = sourceSampleRate / destSampleRate;

        // Evaluate region borders in song time, calculate sample range to render in song time.
        // Note that this example does not use head- or tailtime, so the includeHeadAndTail
        // parameter is set to false here - this might need to be adjusted in actual plug-ins.
        const auto destPlaybackSampleRange =
            playbackRegion->getSampleRange (destSampleRate, juce::ARAPlaybackRegion::IncludeHeadAndTail::no);

        auto destRenderRange = blockRange.getIntersectionWith (destPlaybackSampleRange);
        if (destRenderRange.isEmpty())
            return true;

        // Evaluate region borders in modification/source time and calculate offset between
        // song and source samples, then clip song samples accordingly
        // (if an actual plug-in supports time stretching, this must be taken into account here).
        juce::Range<juce::int64> destModificationSampleRange {
            juce::roundToInt (playbackRegion->getStartInAudioModificationSamples() * destSamplesPerSourceSample),
            juce::roundToInt (playbackRegion->getEndInAudioModificationSamples() * destSamplesPerSourceSample)
        };

        const auto destModificationSampleOffset =
            destModificationSampleRange.getStart() - destPlaybackSampleRange.getStart();

        destRenderRange = destRenderRange.getIntersectionWith (
            destModificationSampleRange.movedToStartAt (
                destPlaybackSampleRange.getStart()));

        if (destRenderRange.isEmpty())
            return true;

        // Calculate buffer offsets.
        const int destNumSamples = (int) (destRenderRange.getLength());
        const int destStartSample = (int) (destRenderRange.getStart() - blockRange.getStart());
        const int sourceStartSample = juce::roundToInt (
            (destRenderRange.getStart() + destModificationSampleOffset)
            * sourceSamplesPerDestSample);

        resampler->seek (sourceStartSample);
        resampler->read (juce::AudioSourceChannelInfo (*tempBuffer));

        // Mix local buffer into the output buffer.
        for (int destChannel = 0; destChannel < destNumChannels; ++destChannel)
        {
            auto sourceChannel = juce::jmin (destChannel, sourceNumChannels - 1);
            buffer.addFrom (
                destChannel,
                destStartSample,
                *tempBuffer,
                sourceChannel,
                0,
                destNumSamples);
        }

        if (! didRenderAnyRegion)
        {
            // Clear any excess at start or end of the region.
            if (destStartSample != 0)
                buffer.clear (0, destStartSample);

            const int destEndSample = destStartSample + destNumSamples;
            const int destRemainingSamples = (int) blockRange.getLength() - destEndSample;

            if (destRemainingSamples != 0)
                buffer.clear (destEndSample, destRemainingSamples);

            didRenderAnyRegion = true;
        }

        return true;
    }

    ProcessingLockInterface& lockInterface;
    juce::SharedResourcePointer<SharedTimeSliceThread> sharedTimesliceThread;

    double destSampleRate = 48000.0;
    int destNumChannels = 2;
    int maximumSamplesPerBlock = 128;
    bool useBufferedAudioSourceReader = false;
    int readTimeout = 0;

    std::map<juce::ARAAudioSource*, std::unique_ptr<ResamplingDriver>> resamplers;
    std::unique_ptr<juce::AudioBuffer<float>> tempBuffer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReaSpeechLitePlaybackRenderer)
};
