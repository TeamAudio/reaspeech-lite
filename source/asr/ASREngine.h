#pragma once

#include <memory>
#include <string>
#include <vector>

#include <juce_core/juce_core.h>
#include <whisper.h>

#include "../utils/SafeUTF8.h"
#include "ASROptions.h"
#include "ASRSegment.h"

class ASREngine
{
public:
    ASREngine (const std::string& modelsDirIn) : modelsDir (modelsDirIn) {}

    ~ASREngine()
    {
        DBG ("ASREngine destructor");
        if (ctx != nullptr)
        {
            DBG ("Freeing whisper context");
            whisper_free (ctx);
            ctx = nullptr;
        }

        downloadTask.reset();
    }

    // Load the model by name. Returns true if successful.
    bool loadModel (const std::string& modelName)
    {
        DBG ("ASREngine::loadModel: " + modelName);

        if (modelName == lastModelName)
        {
            DBG ("Model already loaded");
            return true;
        }

        if (ctx != nullptr)
        {
            DBG ("Freeing whisper context");
            whisper_free (ctx);
        }

        std::string modelPath = modelsDir + "ggml-" + modelName + ".bin";
        DBG ("Loading model from: " + modelPath);

        if (! juce::File (modelPath).exists())
        {
            juce::File (modelsDir).createDirectory();

            DBG ("Downloading model");
            juce::URL url = Config::getModelURL (modelName);
            auto file = juce::File (modelPath);
            downloadTask = url.downloadToFile (file, juce::URL::DownloadTaskOptions());

            while (downloadTask != nullptr && ! downloadTask->isFinished())
                juce::Thread::sleep (100);

            if (downloadTask == nullptr || downloadTask->hadError())
            {
                DBG ("Failed to download model");
                downloadTask.reset();

                if (juce::File (modelPath).deleteFile())
                    DBG ("Deleted model file");

                return false;
            }

            downloadTask.reset();
        }

        whisper_context_params params;
        params.use_gpu = true;

        ctx = whisper_init_from_file_with_params (modelPath.c_str(), params);
        if (ctx == nullptr)
        {
            DBG ("Failed to load model");

            if (juce::File (modelPath).deleteFile())
                DBG ("Deleted model file");

            return false;
        }

        DBG ("Model loaded successfully");
        lastModelName = modelName;
        return true;
    }

    // Transcribe the audio data. Returns true if successful.
    bool transcribe (const std::vector<float>& audioData, ASROptions& options, std::vector<ASRSegment>& segments)
    {
        DBG ("ASREngine::transcribe");
        if (ctx == nullptr)
        {
            DBG ("No model loaded");
            return false;
        }

        whisper_full_params params = whisper_full_default_params (WHISPER_SAMPLING_GREEDY);
        params.token_timestamps = true;
        params.language = options.language.toStdString().c_str();
        params.translate = options.translate;

        // Note: setting this to true causes 0 segments to be returned
        // params.detect_language = true;

        if (whisper_full (ctx, params, audioData.data(), static_cast<int> (audioData.size())) != 0)
        {
            DBG ("Transcription failed");
            return false;
        }

        int nSegments = whisper_full_n_segments (ctx);
        DBG ("Number of segments: " + juce::String (nSegments));

        for (int i = 0; i < nSegments; ++i)
        {
            ASRSegment segment;

            segment.text = SafeUTF8::encode (whisper_full_get_segment_text (ctx, i)).trim();
            segment.start = ((float) whisper_full_get_segment_t0 (ctx, i)) / 100.0f;
            segment.end = ((float) whisper_full_get_segment_t1 (ctx, i)) / 100.0f;

            int nTokens = whisper_full_n_tokens (ctx, i);
            for (int j = 0; j < nTokens; ++j)
            {
                if (whisper_full_get_token_id (ctx, i, j) >= whisper_token_eot (ctx))
                    continue;

                ASRWord word;

                word.text = SafeUTF8::encode (whisper_full_get_token_text (ctx, i, j));
                word.start = ((float) whisper_full_get_token_data (ctx, i, j).t0) / 100.0f;
                word.end = ((float) whisper_full_get_token_data (ctx, i, j).t1) / 100.0f;
                word.probability = whisper_full_get_token_p (ctx, i, j);

                if (! segment.words.isEmpty() && ! word.text.isEmpty() && word.text[0] != ' ')
                {
                    auto& lastWord = segment.words.getReference (segment.words.size() - 1);
                    lastWord.end = word.end;
                    lastWord.text += word.text.trim();
                }
                else
                {
                    word.text = word.text.trim();
                    segment.words.add (word);
                }
            }

            segments.push_back (segment);
        }

        return true;
    }

private:
    std::string modelsDir;
    std::string lastModelName;
    whisper_context* ctx = nullptr;
    std::unique_ptr<juce::URL::DownloadTask> downloadTask;
};
