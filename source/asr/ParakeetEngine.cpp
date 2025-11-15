#include "ParakeetEngine.h"
#include "ParakeetEngineAPI.h"

#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__) || defined(__linux__)
#include <dlfcn.h>
#endif

#ifdef __linux__
#include <unistd.h>
#endif

// ParakeetEngineImpl - wrapper that dynamically loads ParakeetEngine library
struct ParakeetEngineImpl
{
    std::string loadError; // Error message if library failed to load

    ParakeetEngineImpl(const std::string &modelsDirIn)
    {
#ifdef _WIN32
        // Windows: Load ParakeetEngine.dll from the VST3 bundle directory
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
#elif __APPLE__
        // macOS: Load ParakeetEngine.dylib from VST3 bundle Frameworks directory
        // Use dladdr to get the path of this loaded library (the VST3), not the host application
        Dl_info info;
        if (dladdr((void*)this, &info) && info.dli_fname)
        {
            std::string pathStr(info.dli_fname);
            DBG("VST3 library path: " + juce::String(pathStr));

            // Get the directory containing the VST3 binary
            size_t lastSlash = pathStr.find_last_of("/");
            if (lastSlash != std::string::npos)
            {
                // VST3 bundle structure: ReaSpeechLite.vst3/Contents/MacOS/ReaSpeechLite
                // We need to go to: ReaSpeechLite.vst3/Contents/Frameworks/libParakeetEngine.dylib
                pathStr = pathStr.substr(0, lastSlash + 1);
                pathStr += "../Frameworks/libParakeetEngine.dylib";

                DBG("Attempting to load ParakeetEngine.dylib from: " + juce::String(pathStr));
                dllHandle = dlopen(pathStr.c_str(), RTLD_NOW | RTLD_LOCAL);
            }
        }

        if (!dllHandle)
        {
            const char* error = dlerror();
            DBG("Failed to load ParakeetEngine.dylib - Parakeet models will not work");
            if (error)
            {
                DBG("dlerror: " + juce::String(error));
                loadError = std::string("Parakeet is not available: ") + error + ". Whisper models will still work normally.";
            }
            else
            {
                loadError = "Parakeet is not available (failed to load dylib). Whisper models will still work normally.";
            }
            return;
        }

        // Load function pointers
        createFunc = (CreateFunc)dlsym(dllHandle, "ParakeetEngine_Create");
        destroyFunc = (DestroyFunc)dlsym(dllHandle, "ParakeetEngine_Destroy");
        getLastTranscriptionTimeFunc = (GetLastTranscriptionTimeFunc)dlsym(dllHandle, "ParakeetEngine_GetLastTranscriptionTime");
        downloadModelFunc = (DownloadModelFunc)dlsym(dllHandle, "ParakeetEngine_DownloadModel");
        loadModelFunc = (LoadModelFunc)dlsym(dllHandle, "ParakeetEngine_LoadModel");
        transcribeFunc = (TranscribeFunc)dlsym(dllHandle, "ParakeetEngine_Transcribe");
        getProgressFunc = (GetProgressFunc)dlsym(dllHandle, "ParakeetEngine_GetProgress");

        if (!createFunc || !destroyFunc || !loadModelFunc || !transcribeFunc)
        {
            DBG("Failed to load functions from ParakeetEngine.dylib");
            loadError = "Parakeet is not available (dylib initialization error). Whisper models will still work normally.";
            dlclose(dllHandle);
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
            DBG("ParakeetEngine.dylib loaded successfully");
        }
#elif __linux__
        // Linux: Load libParakeetEngine.so from VST3 directory
        char path[4096];
        ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
        if (len != -1)
        {
            path[len] = '\0';
            std::string pathStr(path);
            size_t lastSlash = pathStr.find_last_of("/");
            if (lastSlash != std::string::npos)
            {
                pathStr = pathStr.substr(0, lastSlash + 1);
                pathStr += "libParakeetEngine.so";

                DBG("Attempting to load libParakeetEngine.so from: " + juce::String(pathStr));
                dllHandle = dlopen(pathStr.c_str(), RTLD_NOW | RTLD_LOCAL);
            }
        }

        if (!dllHandle)
        {
            const char* error = dlerror();
            DBG("Failed to load libParakeetEngine.so - Parakeet models will not work");
            if (error)
            {
                DBG("dlerror: " + juce::String(error));
                loadError = std::string("Parakeet is not available: ") + error + ". Whisper models will still work normally.";
            }
            else
            {
                loadError = "Parakeet is not available (failed to load library). Whisper models will still work normally.";
            }
            return;
        }

        // Load function pointers
        createFunc = (CreateFunc)dlsym(dllHandle, "ParakeetEngine_Create");
        destroyFunc = (DestroyFunc)dlsym(dllHandle, "ParakeetEngine_Destroy");
        getLastTranscriptionTimeFunc = (GetLastTranscriptionTimeFunc)dlsym(dllHandle, "ParakeetEngine_GetLastTranscriptionTime");
        downloadModelFunc = (DownloadModelFunc)dlsym(dllHandle, "ParakeetEngine_DownloadModel");
        loadModelFunc = (LoadModelFunc)dlsym(dllHandle, "ParakeetEngine_LoadModel");
        transcribeFunc = (TranscribeFunc)dlsym(dllHandle, "ParakeetEngine_Transcribe");
        getProgressFunc = (GetProgressFunc)dlsym(dllHandle, "ParakeetEngine_GetProgress");

        if (!createFunc || !destroyFunc || !loadModelFunc || !transcribeFunc)
        {
            DBG("Failed to load functions from libParakeetEngine.so");
            loadError = "Parakeet is not available (library initialization error). Whisper models will still work normally.";
            dlclose(dllHandle);
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
            DBG("libParakeetEngine.so loaded successfully");
        }
#else
        DBG("ParakeetEngine only supported on Windows, macOS, and Linux");
        loadError = "Parakeet is not available on this platform. Whisper models will still work normally.";
        (void)modelsDirIn;
#endif
    }

    ~ParakeetEngineImpl()
    {
        if (engineHandle && destroyFunc)
        {
            destroyFunc(engineHandle);
            engineHandle = nullptr;
        }
#ifdef _WIN32
        if (dllHandle)
        {
            FreeLibrary(dllHandle);
            dllHandle = nullptr;
        }
#elif defined(__APPLE__) || defined(__linux__)
        if (dllHandle)
        {
            dlclose(dllHandle);
            dllHandle = nullptr;
        }
#endif
    }

    bool isLoaded() const
    {
#if defined(_WIN32) || defined(__APPLE__) || defined(__linux__)
        return dllHandle != nullptr && engineHandle != nullptr;
#else
        return false;
#endif
    }

    float getLastTranscriptionTime() const
    {
#if defined(_WIN32) || defined(__APPLE__) || defined(__linux__)
        if (isLoaded() && getLastTranscriptionTimeFunc)
        {
            return getLastTranscriptionTimeFunc(engineHandle);
        }
#endif
        return 0.0f;
    }

    bool downloadModel(const std::string &modelName, std::function<bool()> isAborted)
    {
#if defined(_WIN32) || defined(__APPLE__) || defined(__linux__)
        (void)isAborted; // TODO: implement abort callback
        if (isLoaded() && downloadModelFunc)
        {
            // Note: We can't pass C++ lambda directly to C API
            // For now, just pass nullptr - abort checking happens in DLL
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
#if defined(_WIN32) || defined(__APPLE__) || defined(__linux__)
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
#if defined(_WIN32) || defined(__APPLE__) || defined(__linux__)
        (void)isAborted; // TODO: implement abort callback
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
#if defined(_WIN32) || defined(__APPLE__) || defined(__linux__)
        if (isLoaded() && getProgressFunc)
        {
            return getProgressFunc(engineHandle);
        }
#endif
        return 0;
    }

private:
#if defined(_WIN32) || defined(__APPLE__) || defined(__linux__)
    #ifdef _WIN32
    HMODULE dllHandle = nullptr;
    #else
    void* dllHandle = nullptr;
    #endif

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
    juce::ignoreUnused(options); // Options passed to DLL via JSON in impl

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
