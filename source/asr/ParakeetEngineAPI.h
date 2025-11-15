#pragma once

// C API for ParakeetEngine.dll
// This allows the main VST3 to load ParakeetEngine dynamically without
// linking ONNX Runtime directly into the VST3 binary

#ifdef _WIN32
    #ifdef PARAKEET_ENGINE_DLL_EXPORT
        #define PARAKEET_API __declspec(dllexport)
    #else
        #define PARAKEET_API __declspec(dllimport)
    #endif
#else
    #define PARAKEET_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Opaque handle to ParakeetEngine instance
typedef void* ParakeetEngineHandle;

// Callback for checking if operation should abort
typedef bool (*IsAbortedCallback)();

// Create a new ParakeetEngine instance
PARAKEET_API ParakeetEngineHandle ParakeetEngine_Create(const char* modelsDir);

// Destroy a ParakeetEngine instance
PARAKEET_API void ParakeetEngine_Destroy(ParakeetEngineHandle handle);

// Get last transcription time in seconds
PARAKEET_API float ParakeetEngine_GetLastTranscriptionTime(ParakeetEngineHandle handle);

// Download model - returns 1 on success, 0 on failure
PARAKEET_API int ParakeetEngine_DownloadModel(
    ParakeetEngineHandle handle,
    const char* modelName,
    IsAbortedCallback isAborted);

// Load model - returns 1 on success, 0 on failure
PARAKEET_API int ParakeetEngine_LoadModel(
    ParakeetEngineHandle handle,
    const char* modelName);

// Transcribe audio
// audioData: pointer to float array of audio samples
// audioDataSize: number of samples in audioData
// optionsJson: JSON string with ASR options (language, translate, etc.)
// resultJson: buffer to write result JSON (segments) - caller must allocate
// resultJsonSize: size of resultJson buffer
// isAborted: callback to check if operation should abort
// Returns 1 on success, 0 on failure
PARAKEET_API int ParakeetEngine_Transcribe(
    ParakeetEngineHandle handle,
    const float* audioData,
    size_t audioDataSize,
    const char* optionsJson,
    char* resultJson,
    size_t resultJsonSize,
    IsAbortedCallback isAborted);

// Get current progress (0-100)
PARAKEET_API int ParakeetEngine_GetProgress(ParakeetEngineHandle handle);

#ifdef __cplusplus
}
#endif
