#pragma once

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <chrono>

#include <juce_core/juce_core.h>
#include <onnxruntime_cxx_api.h>

#include "../utils/SafeUTF8.h"
#include "ASROptions.h"
#include "ASRSegment.h"

class ParakeetEngine
{
public:
    ParakeetEngine(const std::string &modelsDirIn) : modelsDir(modelsDirIn)
    {
        // Don't initialize ONNX Runtime here - do it lazily when first needed
    }

    ~ParakeetEngine()
    {
        DBG("ParakeetEngine destructor");
        preprocessor.reset();
        encoder.reset();
        decoderJoint.reset();
        downloadTask.reset();
    }

    // Get processing time in seconds from last transcription
    double getProcessingTime() const
    {
        return processingTimeSeconds.load();
    }

    // Download the model if needed. Returns true if successful or already downloaded.
    bool downloadModel(const std::string &modelName, std::function<bool()> isAborted)
    {
        std::string modelDir = getModelDir(modelName);
        juce::File modelDirFile(modelDir);

        // Check if all required model files exist
        std::vector<juce::String> requiredFiles = {
            "nemo128.onnx",
            "encoder-model.int8.onnx",
            "decoder_joint-model.int8.onnx",
            "vocab.txt"};

        bool allFilesExist = true;
        for (const auto &filename : requiredFiles)
        {
            juce::String filePath = juce::String(modelDir) + "/" + filename;
            if (!juce::File(filePath).exists())
            {
                allFilesExist = false;
                break;
            }
        }

        if (allFilesExist)
        {
            DBG(juce::String("Model already downloaded: ") + juce::String(modelDir));
            progress.store(100);
            return true;
        }

        modelDirFile.createDirectory();
        progress.store(0);

        DBG("Downloading parakeet model files");

        // Download each file (v3 int8 quantized model for multilingual support)
        std::map<juce::String, juce::String> fileUrls = {
            {"nemo128.onnx", "https://huggingface.co/istupakov/parakeet-tdt-0.6b-v3-onnx/resolve/main/nemo128.onnx"},
            {"encoder-model.int8.onnx", "https://huggingface.co/istupakov/parakeet-tdt-0.6b-v3-onnx/resolve/main/encoder-model.int8.onnx"},
            {"decoder_joint-model.int8.onnx", "https://huggingface.co/istupakov/parakeet-tdt-0.6b-v3-onnx/resolve/main/decoder_joint-model.int8.onnx"},
            {"vocab.txt", "https://huggingface.co/istupakov/parakeet-tdt-0.6b-v3-onnx/resolve/main/vocab.txt"}};

        int fileIndex = 0;
        for (const auto &[filename, url] : fileUrls)
        {
            if (isAborted())
                return false;

            juce::String filePath = juce::String(modelDir) + "/" + filename;
            if (juce::File(filePath).exists())
            {
                fileIndex++;
                progress.store(static_cast<int>(fileIndex * 100 / static_cast<int>(fileUrls.size())));
                continue;
            }

            DBG(juce::String("Downloading: ") + filename);
            auto file = juce::File(filePath);

            downloadTask = juce::URL(url).downloadToFile(file, juce::URL::DownloadTaskOptions());

            while (downloadTask != nullptr && !downloadTask->isFinished())
            {
                if (isAborted())
                {
                    DBG("Download aborted");
                    downloadTask.reset();
                    return false;
                }
                juce::Thread::sleep(100);
            }

            if (downloadTask == nullptr || downloadTask->hadError())
            {
                DBG(juce::String("Failed to download: ") + filename);
                downloadTask.reset();
                return false;
            }

            downloadTask.reset();
            fileIndex++;
            progress.store(static_cast<int>(fileIndex * 100 / static_cast<int>(fileUrls.size())));
        }

        progress.store(100);
        return true;
    }

    // Load the model by name. Returns true if successful.
    bool loadModel(const std::string &modelName)
    {
        DBG(juce::String("ParakeetEngine::loadModel: ") + juce::String(modelName));

        if (modelName == lastModelName && preprocessor != nullptr)
        {
            DBG("Model already loaded");
            return true;
        }

        std::string modelDir = getModelDir(modelName);
        DBG(juce::String("Loading model from: ") + juce::String(modelDir));

        if (!juce::File(modelDir).exists())
        {
            DBG(juce::String("Model directory not found: ") + juce::String(modelDir));
            return false;
        }

        try
        {
            Ort::SessionOptions sessionOptions;
            sessionOptions.SetIntraOpNumThreads(2);
            sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

            env = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "ParakeetEngine");

            std::string preprocessorPath = modelDir + "/nemo128.onnx";
            std::string encoderPath = modelDir + "/encoder-model.int8.onnx";
            std::string decoderJointPath = modelDir + "/decoder_joint-model.int8.onnx";

#ifdef _WIN32
            // Windows requires wide char paths for ONNX Runtime
            std::wstring preprocessorPathW(preprocessorPath.begin(), preprocessorPath.end());
            std::wstring encoderPathW(encoderPath.begin(), encoderPath.end());
            std::wstring decoderJointPathW(decoderJointPath.begin(), decoderJointPath.end());

            preprocessor = std::make_unique<Ort::Session>(*env, preprocessorPathW.c_str(), sessionOptions);
            encoder = std::make_unique<Ort::Session>(*env, encoderPathW.c_str(), sessionOptions);
            decoderJoint = std::make_unique<Ort::Session>(*env, decoderJointPathW.c_str(), sessionOptions);
#else
            preprocessor = std::make_unique<Ort::Session>(*env, preprocessorPath.c_str(), sessionOptions);
            encoder = std::make_unique<Ort::Session>(*env, encoderPath.c_str(), sessionOptions);
            decoderJoint = std::make_unique<Ort::Session>(*env, decoderJointPath.c_str(), sessionOptions);
#endif

            // Load vocab
            vocab = loadVocab(modelDir + "/vocab.txt");
            vocabSize = static_cast<int64_t>(vocab.size());
            blankIdx = findBlankIdx("<blk>", vocab);

            if (blankIdx < 0)
            {
                DBG("Failed to find blank token in vocab");
                return false;
            }

            DBG("Model loaded successfully");
            lastModelName = modelName;
            return true;
        }
        catch (const Ort::Exception &e)
        {
            DBG("ONNX Runtime error: " + juce::String(e.what()));
            return false;
        }
    }

    // Transcribe the audio data. Returns true if successful.
    bool transcribe(
        const std::vector<float> &audioData,
        ASROptions & /* options */,
        std::vector<ASRSegment> &segments,
        std::function<bool()> isAborted)
    {
        DBG("ParakeetEngine::transcribe");

        // Lazy initialization of ONNX Runtime
        ensureMemoryInfoInitialized();

        if (!preprocessor || !encoder || !decoderJoint)
        {
            DBG("Models not loaded");
            return false;
        }

        progress.store(0);

        auto startTime = juce::Time::getMillisecondCounterHiRes();

        try
        {
            // Note: Parakeet models currently don't support language/translate options
            // They are English-only in this version

            // Chunking parameters for large files
            const int sampleRate = 16000;
            const float chunkLenSecs = 30.0f; // Process 30 seconds at a time
            const float overlapSecs = 1.0f;   // 1 second overlap between chunks
            const int chunkSamples = static_cast<int>(chunkLenSecs * sampleRate);
            const int overlapSamples = static_cast<int>(overlapSecs * sampleRate);

            float audioDuration = static_cast<float>(audioData.size()) / sampleRate;
            bool useChunking = audioData.size() > chunkSamples;

            if (useChunking)
            {
                DBG(juce::String::formatted("Processing %.1f seconds of audio in chunks", audioDuration));
            }

            std::vector<int> allTokens;
            std::vector<TokenInfo> tokenInfos; // For word-level timestamps
            int64_t totalEncoderFrames = 0;

            if (!useChunking)
            {
                // Process entire audio in one go (for short files) - use timing-aware decode
                auto [features, featuresLens] = preprocess(audioData);
                if (isAborted())
                    return false;
                progress.store(20);

                auto [encoderOut, encoderLens] = encode(std::move(features), std::move(featuresLens));
                if (isAborted())
                    return false;
                progress.store(50);

                // Use decodeWithTiming to get word-level timestamps
                tokenInfos = decodeWithTiming(encoderOut, encoderLens, isAborted);
                if (isAborted())
                    return false;
                progress.store(90);

                totalEncoderFrames = encoderLens.GetTensorData<int64_t>()[0];
            }
            else
            {
                // Process audio in chunks for large files
                int stride = chunkSamples - overlapSamples; // Effective stride between chunk starts
                int numChunks = static_cast<int>(std::ceil(static_cast<float>(audioData.size() - overlapSamples) / stride));
                DBG(juce::String::formatted("Processing %d chunks with %.1fs overlap", numChunks, overlapSecs));

                for (int chunkIdx = 0; chunkIdx < numChunks; ++chunkIdx)
                {
                    if (isAborted())
                        return false;

                    // Extract chunk with overlap
                    int startSample = chunkIdx * stride;
                    int endSample = std::min(static_cast<int>(audioData.size()), startSample + chunkSamples);

                    // Ensure we don't go past the end
                    if (startSample >= static_cast<int>(audioData.size()))
                        break;

                    std::vector<float> chunkData(audioData.begin() + startSample, audioData.begin() + endSample);

                    DBG(juce::String::formatted("Processing chunk %d/%d (%.1f-%.1f sec)",
                                                chunkIdx + 1, numChunks,
                                                startSample / static_cast<float>(sampleRate),
                                                endSample / static_cast<float>(sampleRate)));

                    // 1. Preprocess chunk
                    auto [features, featuresLens] = preprocess(chunkData);
                    if (isAborted())
                        return false;

                    // 2. Encode chunk
                    auto [encoderOut, encoderLens] = encode(std::move(features), std::move(featuresLens));
                    if (isAborted())
                        return false;

                    int64_t chunkEncoderFrames = encoderLens.GetTensorData<int64_t>()[0];

                    // 3. Decode chunk with timing info to preserve confidence scores
                    auto chunkTokenInfos = decodeWithTiming(encoderOut, encoderLens, isAborted);
                    if (isAborted())
                        return false;

                    // For overlapping chunks, skip duplicate tokens at the beginning (simple deduplication)
                    if (chunkIdx > 0 && !chunkTokenInfos.empty())
                    {
                        // Estimate how many tokens to skip based on overlap ratio
                        // This is a simple heuristic - we skip tokens from the overlap region
                        int tokensToSkip = std::min(5, static_cast<int>(chunkTokenInfos.size() / 10));
                        if (tokensToSkip > 0)
                        {
                            chunkTokenInfos.erase(chunkTokenInfos.begin(), chunkTokenInfos.begin() + tokensToSkip);
                        }
                    }

                    // Adjust timesteps to be relative to the full audio, not just this chunk
                    float chunkStartTime = startSample / static_cast<float>(sampleRate);
                    int64_t chunkStartFrames = totalEncoderFrames;

                    for (auto& tokenInfo : chunkTokenInfos)
                    {
                        tokenInfo.timestep += chunkStartFrames;
                    }

                    // Append token infos from this chunk
                    tokenInfos.insert(tokenInfos.end(), chunkTokenInfos.begin(), chunkTokenInfos.end());
                    totalEncoderFrames += chunkEncoderFrames;

                    // Update progress
                    int chunkProgress = 20 + (70 * (chunkIdx + 1) / numChunks);
                    progress.store(chunkProgress);
                }

                // Remove consecutive duplicate token infos (additional cleanup)
                deduplicateTokenInfo(tokenInfos);
            }

            auto endTime = juce::Time::getMillisecondCounterHiRes();
            processingTimeSeconds.store((endTime - startTime) / 1000.0);

            // Create segments with word-level timestamps if available
            if (!tokenInfos.empty())
            {
                // Use native TDT timestamps from decodeWithTiming
                createSegmentsFromTokens(tokenInfos, totalEncoderFrames, audioDuration, segments);
            }
            else if (!allTokens.empty())
            {
                // Fallback: Use simple text-based segmentation for chunked files
                juce::String text = tokensToText(allTokens);
                splitIntoSegments(text.trim(), audioDuration, segments);
            }

            progress.store(100);
            return true;
        }
        catch (const Ort::Exception & /* e */)
        {
            DBG("ONNX Runtime error during transcription");
            return false;
        }
    }

    // Get the full path to a model directory based on its name
    std::string getModelDir(const std::string &modelName) const
    {
        return modelsDir + modelName;
    }

    // Get current progress (0-100) of download or transcription
    int getProgress() const
    {
        return progress.load();
    }

private:
    // Lazy initialization of ONNX Runtime memory info
    void ensureMemoryInfoInitialized()
    {
        if (!memoryInfoInitialized)
        {
            memoryInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeCPU);
            memoryInfoInitialized = true;
        }
    }

    // Token with timing and confidence information
    struct TokenInfo
    {
        int tokenId;
        int64_t timestep; // Encoder timestep when token was emitted
        float confidence; // Max probability for this token
    };

    // Remove consecutive duplicate tokens
    void deduplicateTokens(std::vector<int> &tokens)
    {
        if (tokens.size() <= 1)
            return;

        auto it = std::unique(tokens.begin(), tokens.end());
        tokens.erase(it, tokens.end());
    }

    // Remove consecutive duplicate TokenInfo entries
    void deduplicateTokenInfo(std::vector<TokenInfo> &tokens)
    {
        if (tokens.size() <= 1)
            return;

        auto it = std::unique(tokens.begin(), tokens.end(),
                              [](const TokenInfo &a, const TokenInfo &b)
                              { return a.tokenId == b.tokenId; });
        tokens.erase(it, tokens.end());
    }

    // Check if string contains only punctuation and whitespace
    bool isOnlyPunctuation(const juce::String &text)
    {
        if (text.isEmpty())
            return true;

        for (int i = 0; i < text.length(); ++i)
        {
            juce::juce_wchar c = text[i];
            // Check if character is NOT punctuation or whitespace
            if (!juce::CharacterFunctions::isWhitespace(c) &&
                c != '.' && c != ',' && c != '!' && c != '?' &&
                c != ';' && c != ':' && c != '-' && c != '\'' && c != '"')
            {
                return false; // Found a non-punctuation character
            }
        }
        return true; // All characters are punctuation or whitespace
    }

    // Create segments with word-level timestamps from TokenInfo
    void createSegmentsFromTokens(const std::vector<TokenInfo> &tokenInfos, int64_t /*totalEncoderFrames*/, float audioDuration, std::vector<ASRSegment> &segments)
    {
        if (tokenInfos.empty())
            return;

        // Convert encoder timesteps to seconds (assuming 40ms per frame for NeMo models)
        const float msPerFrame = 40.0f;
        const float secondsPerFrame = msPerFrame / 1000.0f;

        // Build words with timestamps
        juce::Array<ASRWord> allWords;
        juce::String currentWord;
        int64_t wordStartTimestep = 0;
        float totalConfidence = 0.0f;
        int wordTokenCount = 0;

        for (size_t i = 0; i < tokenInfos.size(); ++i)
        {
            const auto &info = tokenInfos[i];
            auto it = vocab.find(info.tokenId);
            if (it == vocab.end())
                continue;

            std::string tokenText = it->second;

            // Check if this token starts a new word (has leading space)
            bool startsNewWord = tokenText.find(' ') == 0 || tokenText.find("\u2581") == 0;

            if (startsNewWord && !currentWord.isEmpty())
            {
                // Finish current word
                ASRWord word;
                word.text = currentWord.trim();
                word.start = wordStartTimestep * secondsPerFrame;
                word.end = info.timestep * secondsPerFrame;
                word.probability = wordTokenCount > 0 ? totalConfidence / wordTokenCount : 0.0f;

                if (word.text.isNotEmpty())
                    allWords.add(word);

                // Start new word
                currentWord = juce::String(tokenText).trim();
                wordStartTimestep = info.timestep;
                totalConfidence = info.confidence;
                wordTokenCount = 1;
            }
            else
            {
                // Continue current word
                currentWord += juce::String(tokenText);
                totalConfidence += info.confidence;
                wordTokenCount++;
            }
        }

        // Add last word
        if (!currentWord.isEmpty())
        {
            ASRWord word;
            word.text = currentWord.trim();
            word.start = wordStartTimestep * secondsPerFrame;
            word.end = audioDuration;
            word.probability = wordTokenCount > 0 ? totalConfidence / wordTokenCount : 0.0f;

            if (word.text.isNotEmpty())
                allWords.add(word);
        }

        // Group words into segments based on punctuation or time gaps
        if (allWords.isEmpty())
            return;

        const juce::String punctuation = ".!?";
        ASRSegment currentSegment;
        currentSegment.start = allWords[0].start;

        for (int wordIdx = 0; wordIdx < allWords.size(); ++wordIdx)
        {
            const auto &word = allWords[wordIdx];
            currentSegment.words.add(word);
            currentSegment.text += (currentSegment.text.isEmpty() ? "" : " ") + word.text;
            currentSegment.end = word.end;

            // Check if word ends with punctuation
            bool endsPunctuation = false;
            for (int i = 0; i < punctuation.length(); ++i)
            {
                if (word.text.endsWithChar(punctuation[i]))
                {
                    endsPunctuation = true;
                    break;
                }
            }

            if (endsPunctuation)
            {
                // Only add segment if it contains actual content (not just punctuation)
                if (!isOnlyPunctuation(currentSegment.text.trim()))
                {
                    segments.push_back(currentSegment);
                }
                currentSegment = ASRSegment();
                // If not the last word, set start to next word's start
                if (wordIdx < allWords.size() - 1)
                    currentSegment.start = allWords[wordIdx + 1].start;
            }
        }

        // Add final segment if it has words and actual content
        if (!currentSegment.words.isEmpty() && !isOnlyPunctuation(currentSegment.text.trim()))
        {
            segments.push_back(currentSegment);
        }
    }

    // Split text into segments at punctuation marks with estimated timestamps (fallback method)
    void splitIntoSegments(const juce::String &fullText, float totalDuration, std::vector<ASRSegment> &segments)
    {
        if (fullText.isEmpty())
            return;

        // Punctuation marks where we split
        const juce::String punctuation = ".!?;";

        std::vector<juce::String> sentenceParts;
        juce::String currentPart;
        int totalChars = 0;

        // Split at punctuation but keep the punctuation with the sentence
        for (int i = 0; i < fullText.length(); ++i)
        {
            juce::juce_wchar ch = fullText[i];
            currentPart += ch;

            if (punctuation.containsChar(ch))
            {
                // Found punctuation - add this part
                juce::String trimmed = currentPart.trim();
                if (trimmed.isNotEmpty())
                {
                    sentenceParts.push_back(trimmed);
                    totalChars += trimmed.length();
                }
                currentPart = "";
            }
        }

        // Add any remaining text
        juce::String trimmed = currentPart.trim();
        if (trimmed.isNotEmpty())
        {
            sentenceParts.push_back(trimmed);
            totalChars += trimmed.length();
        }

        // If no splits were found, create a single segment
        if (sentenceParts.empty())
        {
            ASRSegment segment;
            segment.text = fullText;
            segment.start = 0.0f;
            segment.end = totalDuration;
            segments.push_back(segment);
            return;
        }

        // Distribute timestamps proportionally based on character count
        float currentTime = 0.0f;
        for (const auto &part : sentenceParts)
        {
            ASRSegment segment;
            segment.text = part;
            segment.start = currentTime;

            // Estimate duration based on character proportion
            float proportion = static_cast<float>(part.length()) / static_cast<float>(totalChars);
            float segmentDuration = totalDuration * proportion;
            currentTime += segmentDuration;
            segment.end = currentTime;

            segments.push_back(segment);
        }

        // Ensure last segment ends exactly at total duration
        if (!segments.empty())
        {
            segments.back().end = totalDuration;
        }
    }

    std::map<int, std::string> loadVocab(const std::string &vocabPath)
    {
        std::map<int, std::string> result;
        std::ifstream infile(vocabPath);
        std::string line;

        while (std::getline(infile, line))
        {
            std::istringstream iss(line);
            std::string token;
            int id;
            if (iss >> token >> id)
            {
                // Replace Unicode character \u2581 with space
                size_t pos;
                while ((pos = token.find("\u2581")) != std::string::npos)
                {
                    token.replace(pos, 3, " ");
                }
                result[id] = token;
            }
        }
        return result;
    }

    int findBlankIdx(const std::string &token, const std::map<int, std::string> &vocabMap)
    {
        for (const auto &[id, tok] : vocabMap)
        {
            if (tok == token)
                return id;
        }
        return -1;
    }

    std::pair<Ort::Value, Ort::Value> preprocess(const std::vector<float> &audioWav)
    {
        int64_t waveformsSize = static_cast<int64_t>(audioWav.size());
        std::vector<int64_t> waveformsShape = {1, waveformsSize};
        std::vector<int64_t> waveformsLensShape = {1};
        std::vector<int64_t> waveformsLens = {waveformsSize};

        Ort::Value inputWaveforms = Ort::Value::CreateTensor<float>(
            memoryInfo, const_cast<float *>(audioWav.data()), audioWav.size(),
            waveformsShape.data(), waveformsShape.size());

        Ort::Value inputWaveformsLens = Ort::Value::CreateTensor<int64_t>(
            memoryInfo, waveformsLens.data(), waveformsLens.size(),
            waveformsLensShape.data(), waveformsLensShape.size());

        std::vector<const char *> inputNames = {"waveforms", "waveforms_lens"};
        std::vector<const char *> outputNames = {"features", "features_lens"};
        std::array<Ort::Value, 2> inputTensors = {std::move(inputWaveforms), std::move(inputWaveformsLens)};

        auto outputTensors = preprocessor->Run(Ort::RunOptions{nullptr},
                                               inputNames.data(), inputTensors.data(), inputNames.size(),
                                               outputNames.data(), outputNames.size());

        return {std::move(outputTensors[0]), std::move(outputTensors[1])};
    }

    std::pair<Ort::Value, Ort::Value> encode(Ort::Value features, Ort::Value featuresLens)
    {
        std::vector<const char *> inputNames = {"audio_signal", "length"};
        std::vector<const char *> outputNames = {"outputs", "encoded_lengths"};
        std::array<Ort::Value, 2> inputTensors = {std::move(features), std::move(featuresLens)};

        auto outputTensors = encoder->Run(Ort::RunOptions{nullptr},
                                          inputNames.data(), inputTensors.data(), inputNames.size(),
                                          outputNames.data(), outputNames.size());

        return {std::move(outputTensors[0]), std::move(outputTensors[1])};
    }

    // Create initial decoder state (2 x 1 x 640)
    std::pair<Ort::Value, Ort::Value> createState()
    {
        std::vector<int64_t> stateShape = {2, 1, 640};
        size_t stateSize = 2 * 1 * 640;

        auto state1Data = std::make_unique<float[]>(stateSize);
        auto state2Data = std::make_unique<float[]>(stateSize);
        std::fill_n(state1Data.get(), stateSize, 0.0f);
        std::fill_n(state2Data.get(), stateSize, 0.0f);

        Ort::Value state1 = Ort::Value::CreateTensor<float>(
            memoryInfo, state1Data.release(), stateSize,
            stateShape.data(), stateShape.size());
        Ort::Value state2 = Ort::Value::CreateTensor<float>(
            memoryInfo, state2Data.release(), stateSize,
            stateShape.data(), stateShape.size());

        return {std::move(state1), std::move(state2)};
    }

    // Clone state for inference
    std::pair<Ort::Value, Ort::Value> cloneState(std::pair<Ort::Value, Ort::Value> &state)
    {
        std::vector<int64_t> stateShape = {2, 1, 640};
        size_t stateSize = 2 * 1 * 640;

        auto state1Src = state.first.GetTensorData<float>();
        auto state2Src = state.second.GetTensorData<float>();

        auto state1Data = std::make_unique<float[]>(stateSize);
        auto state2Data = std::make_unique<float[]>(stateSize);
        std::copy_n(state1Src, stateSize, state1Data.get());
        std::copy_n(state2Src, stateSize, state2Data.get());

        Ort::Value state1 = Ort::Value::CreateTensor<float>(
            memoryInfo, state1Data.release(), stateSize,
            stateShape.data(), stateShape.size());
        Ort::Value state2 = Ort::Value::CreateTensor<float>(
            memoryInfo, state2Data.release(), stateSize,
            stateShape.data(), stateShape.size());

        return {std::move(state1), std::move(state2)};
    }

    // Find argmax of a vector
    int argmax(const std::vector<float> &v)
    {
        if (v.empty())
            return -1;
        return static_cast<int>(std::distance(v.begin(), std::max_element(v.begin(), v.end())));
    }

    // Apply softmax to convert logits to probabilities
    void softmax(std::vector<float> &logits)
    {
        if (logits.empty())
            return;

        // Find max for numerical stability
        float maxLogit = *std::max_element(logits.begin(), logits.end());

        // Compute exp(logit - max) and sum
        float sum = 0.0f;
        for (auto &logit : logits)
        {
            logit = std::exp(logit - maxLogit);
            sum += logit;
        }

        // Normalize
        for (auto &prob : logits)
        {
            prob /= sum;
        }
    }

    // Single decode step with decoder_joint model
    std::tuple<std::vector<float>, int, std::pair<Ort::Value, Ort::Value>>
    decodeStep(const std::vector<int> &prevTokens,
               std::pair<Ort::Value, Ort::Value> prevState,
               const std::vector<float> &encoderOut)
    {
        // Prepare encoder output tensor [1, 1024, 1]
        std::vector<int64_t> encoderOutShape = {1, static_cast<int64_t>(encoderOut.size()), 1};
        auto encoderOutData = std::make_unique<float[]>(encoderOut.size());
        std::copy(encoderOut.begin(), encoderOut.end(), encoderOutData.get());

        Ort::Value inputEncoderOutputs = Ort::Value::CreateTensor<float>(
            memoryInfo, encoderOutData.release(), encoderOut.size(),
            encoderOutShape.data(), encoderOutShape.size());

        // Prepare target tensor [1, 1]
        int target = blankIdx;
        if (!prevTokens.empty())
        {
            target = prevTokens.back();
        }
        std::vector<int> targets = {target};
        std::vector<int64_t> targetShape = {1, 1};
        auto targetsData = std::make_unique<int[]>(1);
        targetsData[0] = target;

        Ort::Value inputTargets = Ort::Value::CreateTensor<int>(
            memoryInfo, targetsData.release(), 1,
            targetShape.data(), targetShape.size());

        // Prepare target length tensor [1]
        std::vector<int32_t> targetLength = {1};
        std::vector<int64_t> targetLengthShape = {1};
        auto targetLengthData = std::make_unique<int32_t[]>(1);
        targetLengthData[0] = 1;

        Ort::Value inputTargetLength = Ort::Value::CreateTensor<int32_t>(
            memoryInfo, targetLengthData.release(), 1,
            targetLengthShape.data(), targetLengthShape.size());

        // Run decoder_joint
        std::vector<const char *> inputNames = {
            "encoder_outputs", "targets", "target_length",
            "input_states_1", "input_states_2"};
        std::vector<const char *> outputNames = {
            "outputs", "output_states_1", "output_states_2"};

        std::array<Ort::Value, 5> inputTensors = {
            std::move(inputEncoderOutputs),
            std::move(inputTargets),
            std::move(inputTargetLength),
            std::move(prevState.first),
            std::move(prevState.second)};

        auto outputTensors = decoderJoint->Run(Ort::RunOptions{nullptr},
                                               inputNames.data(), inputTensors.data(), inputNames.size(),
                                               outputNames.data(), outputNames.size());

        // Extract decoder output
        auto decoderOutShape = outputTensors[0].GetTensorTypeAndShapeInfo().GetShape();
        auto decoderOutPtr = outputTensors[0].GetTensorData<float>();
        int64_t decoderNumElements = 1;
        for (auto dim : decoderOutShape)
        {
            decoderNumElements *= dim;
        }

        std::vector<float> decoderOut(decoderOutPtr, decoderOutPtr + decoderNumElements);

        // Split into probs and step prediction
        std::vector<float> probs(decoderOut.begin(), decoderOut.begin() + vocabSize);
        std::vector<float> stepPred(decoderOut.begin() + vocabSize, decoderOut.end());

        int step = argmax(stepPred);

        // Extract new state
        std::pair<Ort::Value, Ort::Value> newState = {
            std::move(outputTensors[1]),
            std::move(outputTensors[2])};

        return {probs, step, std::move(newState)};
    }

    // Decode with timing and confidence information
    std::vector<TokenInfo> decodeWithTiming(Ort::Value &encoderOut, Ort::Value &encoderLens, std::function<bool()> isAborted)
    {
        // Greedy TDT decoding with timestamp tracking
        std::vector<TokenInfo> tokenInfos;

        auto encoderOutPtr = encoderOut.GetTensorMutableData<float>();
        auto encoderLensPtr = encoderLens.GetTensorData<int64_t>();
        int64_t encodingsLen = encoderLensPtr[0];

        const int encodingDim = 1024; // Encoder output dimension
        const int maxTokensPerStep = 10;

        auto prevState = createState();
        int64_t t = 0;
        int emittedTokens = 0;
        std::vector<int> tokenIds; // Needed for decodeStep

        while (t < encodingsLen)
        {
            if (isAborted())
                break;

            // Extract encoding at timestep t [1024]
            std::vector<float> encodingT(static_cast<size_t>(encodingDim));
            for (size_t i = 0; i < static_cast<size_t>(encodingDim); i++)
            {
                // Encoder output layout: [batch=1, dim=1024, time]
                encodingT[i] = encoderOutPtr[i * encodingsLen + t];
            }

            // Decode one step
            auto [probs, step, newState] = decodeStep(tokenIds, cloneState(prevState), encodingT);

            // Convert logits to probabilities using softmax
            softmax(probs);

            int token = argmax(probs);
            float confidence = probs[token]; // Get confidence (probability) for this token

            if (token != blankIdx)
            {
                prevState = std::move(newState);
                tokenIds.push_back(token);

                // Store token with timing info
                TokenInfo info;
                info.tokenId = token;
                info.timestep = t;
                info.confidence = confidence;
                tokenInfos.push_back(info);

                emittedTokens += 1;
            }

            if (step >= 0)
            {
                t += step;
                emittedTokens = 0;
            }
            else if (token == blankIdx || emittedTokens >= maxTokensPerStep)
            {
                t += 1;
                emittedTokens = 0;
            }
        }

        return tokenInfos;
    }

    // Simple decode without timing (for chunking)
    std::vector<int> decode(Ort::Value &encoderOut, Ort::Value &encoderLens, std::function<bool()> isAborted)
    {
        // Greedy TDT decoding
        std::vector<int> tokens;

        auto encoderOutPtr = encoderOut.GetTensorMutableData<float>();
        auto encoderLensPtr = encoderLens.GetTensorData<int64_t>();
        int64_t encodingsLen = encoderLensPtr[0];

        const int encodingDim = 1024; // Encoder output dimension
        const int maxTokensPerStep = 10;

        auto prevState = createState();
        int t = 0;
        int emittedTokens = 0;

        while (t < encodingsLen)
        {
            if (isAborted())
                break;

            // Extract encoding at timestep t [1024]
            std::vector<float> encodingT(static_cast<size_t>(encodingDim));
            for (size_t i = 0; i < static_cast<size_t>(encodingDim); i++)
            {
                // Encoder output layout: [batch=1, dim=1024, time]
                encodingT[i] = encoderOutPtr[i * encodingsLen + t];
            }

            // Decode one step
            auto [probs, step, newState] = decodeStep(tokens, cloneState(prevState), encodingT);

            int token = argmax(probs);

            if (token != blankIdx)
            {
                prevState = std::move(newState);
                tokens.push_back(token);
                emittedTokens += 1;
            }

            if (step >= 0)
            {
                t += step;
                emittedTokens = 0;
            }
            else if (token == blankIdx || emittedTokens >= maxTokensPerStep)
            {
                t += 1;
                emittedTokens = 0;
            }
        }

        return tokens;
    }

    juce::String tokensToText(const std::vector<int> &tokens)
    {
        juce::String joined;
        for (int id : tokens)
        {
            auto it = vocab.find(id);
            if (it != vocab.end())
            {
                joined += juce::String(it->second);
            }
        }

        // Decode space pattern (simplified version)
        juce::String result = joined.trim();

        // Remove leading spaces
        while (result.startsWithChar(' '))
            result = result.substring(1);

        return result;
    }

    std::string modelsDir;
    std::string lastModelName;
    Ort::MemoryInfo memoryInfo{nullptr};
    bool memoryInfoInitialized = false;
    std::unique_ptr<Ort::Env> env;
    std::unique_ptr<Ort::Session> preprocessor;
    std::unique_ptr<Ort::Session> encoder;
    std::unique_ptr<Ort::Session> decoderJoint;
    std::unique_ptr<juce::URL::DownloadTask> downloadTask;
    std::atomic<int> progress;

    std::map<int, std::string> vocab;
    int64_t vocabSize = 0;
    int blankIdx = -1;
    std::atomic<double> processingTimeSeconds{0.0};
};
