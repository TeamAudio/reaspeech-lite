#include "ParakeetEngine.h"
#include "ParakeetEngineAPI.h"

#ifdef _WIN32
#include <windows.h>
#endif

// ParakeetEngineImpl - wrapper that dynamically loads ParakeetEngine.dll
struct ParakeetEngineImpl
{
    std::string loadError; // Error message if DLL failed to load

    ParakeetEngineImpl(const std::string &modelsDirIn)
    {
#ifdef _WIN32
        // Load ParakeetEngine.dll from the VST3 bundle directory
        // The DLL should be in the same directory as the VST3
        HMODULE hModule = GetModuleHandleW(nullptr);
        if (hModule)
        {
            wchar_t path[MAX_PATH];
            GetModuleFileNameW(hModule, path, MAX_PATH);

            // Get directory of VST3
            std::wstring wPath(path);
            size_t lastSlash = wPath.find_last_of(L"\\/");
            if (lastSlash != std::wstring::npos)
            {
                wPath = wPath.substr(0, lastSlash + 1);
                wPath += L"ParakeetEngine.dll";

                DBG("Attempting to load ParakeetEngine.dll from: " + juce::String(wPath.c_str()));
                dllHandle = LoadLibraryW(wPath.c_str());
            }
        }

        if (!dllHandle)
        {
            DWORD error = GetLastError();
            DBG("Failed to load ParakeetEngine.dll - Parakeet models will not work");
            DBG("GetLastError: " + juce::String((int)error));

            // Store user-friendly error message
            if (error == 126 || error == 127) {
                // ERROR_MOD_NOT_FOUND or ERROR_PROC_NOT_FOUND
                loadError = "Parakeet is not available on this system (missing system dependencies). Whisper models will still work normally.";
            } else {
                loadError = "Parakeet is not available on this system. Whisper models will still work normally.";
            }
            return;
        }

        // Load function pointers
        createFunc = (CreateFunc)GetProcAddress(dllHandle, "ParakeetEngine_Create");
        destroyFunc = (DestroyFunc)GetProcAddress(dllHandle, "ParakeetEngine_Destroy");
        getLastTranscriptionTimeFunc = (GetLastTranscriptionTimeFunc)GetProcAddress(dllHandle, "ParakeetEngine_GetLastTranscriptionTime");
        downloadModelFunc = (DownloadModelFunc)GetProcAddress(dllHandle, "ParakeetEngine_DownloadModel");
        loadModelFunc = (LoadModelFunc)GetProcAddress(dllHandle, "ParakeetEngine_LoadModel");
        transcribeFunc = (TranscribeFunc)GetProcAddress(dllHandle, "ParakeetEngine_Transcribe");
        getProgressFunc = (GetProgressFunc)GetProcAddress(dllHandle, "ParakeetEngine_GetProgress");

        if (!createFunc || !destroyFunc || !loadModelFunc || !transcribeFunc)
        {
            DBG("Failed to load functions from ParakeetEngine.dll");
            loadError = "Parakeet is not available (DLL initialization error). Whisper models will still work normally.";
            FreeLibrary(dllHandle);
            dllHandle = nullptr;
            return;
        }

        // Create engine instance
        engineHandle = createFunc(modelsDirIn.c_str());
        if (!engineHandle)
        {
            DBG("Failed to create ParakeetEngine instance");
            loadError = "Parakeet is not available (engine creation failed). Whisper models will still work normally.";
        }
        else
        {
            DBG("ParakeetEngine.dll loaded successfully");
        }
#else
        DBG("ParakeetEngine.dll only supported on Windows for now");
        loadError = "Parakeet is only available on Windows. Whisper models will still work normally.";
        (void)modelsDirIn;
#endif
    }

    ~ParakeetEngineImpl()
    {
#ifdef _WIN32
        if (engineHandle && destroyFunc)
        {
            destroyFunc(engineHandle);
            engineHandle = nullptr;
        }
        if (dllHandle)
        {
            FreeLibrary(dllHandle);
            dllHandle = nullptr;
        }
#endif
    }

    bool isLoaded() const
    {
#ifdef _WIN32
        return dllHandle != nullptr && engineHandle != nullptr;
#else
        return false;
#endif
    }

    float getLastTranscriptionTime() const
    {
#ifdef _WIN32
        if (isLoaded() && getLastTranscriptionTimeFunc)
        {
            return getLastTranscriptionTimeFunc(engineHandle);
        }
#endif
        return 0.0f;
    }

    bool downloadModel(const std::string &modelName, std::function<bool()> isAborted)
    {
#ifdef _WIN32
        if (isLoaded() && downloadModelFunc)
        {
            // Create callback wrapper
            auto callback = [](void* userData) -> bool {
                auto* abortFunc = static_cast<std::function<bool()>*>(userData);
                return (*abortFunc)();
            };

            // Note: We can't pass C++ lambda directly to C API
            // For now, just pass nullptr and implement download in main plugin
            return downloadModelFunc(engineHandle, modelName.c_str(), nullptr) != 0;
        }
#else
        (void)modelName;
        (void)isAborted;
#endif
        return false;
    }

    bool loadModel(const std::string &modelName)
    {
#ifdef _WIN32
        if (isLoaded() && loadModelFunc)
        {
            return loadModelFunc(engineHandle, modelName.c_str()) != 0;
        }
#else
        (void)modelName;
#endif
        return false;
    }

    bool transcribe(
        const std::vector<float> &audioData,
        std::vector<ASRSegment> &segments,
        std::function<bool()> isAborted)
    {
#ifdef _WIN32
        if (isLoaded() && transcribeFunc)
        {
            // Allocate buffer for result JSON
            std::vector<char> resultJson(1024 * 1024); // 1MB buffer

            int result = transcribeFunc(
                engineHandle,
                audioData.data(),
                audioData.size(),
                "{}",  // Empty options JSON for now
                resultJson.data(),
                resultJson.size(),
                nullptr // isAborted callback not implemented yet
            );

            if (result)
            {
                // Parse JSON and populate segments
                // For now, just clear segments as stub
                segments.clear();
                return true;
            }
        }
#else
        (void)audioData;
        (void)isAborted;
#endif
        segments.clear();
        return false;
    }

    int getProgress() const
    {
#ifdef _WIN32
        if (isLoaded() && getProgressFunc)
        {
            return getProgressFunc(engineHandle);
        }
#endif
        return 0;
    }

private:
#ifdef _WIN32
    HMODULE dllHandle = nullptr;
    ParakeetEngineHandle engineHandle = nullptr;

    // Function pointer types
    typedef ParakeetEngineHandle (*CreateFunc)(const char*);
    typedef void (*DestroyFunc)(ParakeetEngineHandle);
    typedef float (*GetLastTranscriptionTimeFunc)(ParakeetEngineHandle);
    typedef int (*DownloadModelFunc)(ParakeetEngineHandle, const char*, IsAbortedCallback);
    typedef int (*LoadModelFunc)(ParakeetEngineHandle, const char*);
    typedef int (*TranscribeFunc)(ParakeetEngineHandle, const float*, size_t, const char*, char*, size_t, IsAbortedCallback);
    typedef int (*GetProgressFunc)(ParakeetEngineHandle);

    // Function pointers
    CreateFunc createFunc = nullptr;
    DestroyFunc destroyFunc = nullptr;
    GetLastTranscriptionTimeFunc getLastTranscriptionTimeFunc = nullptr;
    DownloadModelFunc downloadModelFunc = nullptr;
    LoadModelFunc loadModelFunc = nullptr;
    TranscribeFunc transcribeFunc = nullptr;
    GetProgressFunc getProgressFunc = nullptr;
#endif
};

// ParakeetEngine wrapper implementation
ParakeetEngine::ParakeetEngine(const std::string &modelsDirIn)
    : modelsDir(modelsDirIn)
{
    DBG("ParakeetEngine constructor - will load DLL on demand");
    impl = std::make_unique<ParakeetEngineImpl>(modelsDirIn);
}

ParakeetEngine::~ParakeetEngine()
{
    DBG("ParakeetEngine destructor");
}

float ParakeetEngine::getLastTranscriptionTime() const
{
    if (impl)
    {
        return impl->getLastTranscriptionTime();
    }
    return lastTranscriptionTimeSecs;
}

bool ParakeetEngine::downloadModel(const std::string &modelName, std::function<bool()> isAborted)
{
    DBG("ParakeetEngine::downloadModel called for " + juce::String(modelName));

    if (impl && impl->isLoaded())
    {
        return impl->downloadModel(modelName, isAborted);
    }

    // DLL not loaded - return error
    progress.store(0);
    return false;
}

bool ParakeetEngine::loadModel(const std::string &modelName)
{
    DBG("ParakeetEngine::loadModel called for " + juce::String(modelName));

    if (impl && impl->isLoaded())
    {
        return impl->loadModel(modelName);
    }

    // DLL not loaded - return error
    return false;
}

bool ParakeetEngine::transcribe(
    const std::vector<float> &audioData,
    ASROptions &options,
    std::vector<ASRSegment> &segments,
    std::function<bool()> isAborted)
{
    DBG("ParakeetEngine::transcribe called");

    if (impl && impl->isLoaded())
    {
        return impl->transcribe(audioData, segments, isAborted);
    }

    // DLL not loaded - return error
    lastTranscriptionTimeSecs = 0.0f;
    segments.clear();
    return false;
}

int ParakeetEngine::getProgress() const
{
    if (impl && impl->isLoaded())
    {
        return impl->getProgress();
    }
    return progress.load();
}

bool ParakeetEngine::isAvailable() const
{
    return impl && impl->isLoaded();
}

std::string ParakeetEngine::getLoadError() const
{
    if (impl)
    {
        return impl->loadError;
    }
    return "Parakeet engine not initialized.";
}
