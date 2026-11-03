cmake_minimum_required(VERSION 3.25)

# Pamplejuce includes
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Project-specific includes
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake-local")

include(PamplejuceVersion)

# Configures the macOS architecture and minimum supported version
include(PamplejuceMacOS)
# Override the upstream CI default of universal binaries before project().
if((CMAKE_HOST_APPLE OR CMAKE_SYSTEM_NAME STREQUAL "Darwin") AND NOT (CMAKE_SYSTEM_NAME STREQUAL "iOS"))
    get_property(_macos_architectures CACHE CMAKE_OSX_ARCHITECTURES PROPERTY VALUE)
    if(_macos_architectures AND NOT _macos_architectures STREQUAL "arm64")
        message(FATAL_ERROR "ReaSpeech Lite supports only arm64 on macOS. Configure with -DCMAKE_OSX_ARCHITECTURES=arm64.")
    endif()
    set(CMAKE_OSX_ARCHITECTURES arm64 CACHE STRING "macOS architecture" FORCE)
    set(CMAKE_OSX_ARCHITECTURES arm64)
endif()
# The prebuilt ONNX Runtime objects embedded in ReaSpeech require macOS 13.4.
set(CMAKE_OSX_DEPLOYMENT_TARGET "13.4" CACHE STRING "" FORCE)

include(JUCEDefaults)

# Rust and the prebuilt ONNX/CUDA native libraries use the dynamic MSVC CRT.
if(MSVC OR WIN32)
    set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreadedDLL CACHE INTERNAL "" FORCE)
endif()

set(BUNDLE_ID "com.TechAudio.ReaSpeechLite")
set(COMPANY_NAME "TechAudio")
set(COMPANY_WEBSITE "https://techaud.io")
set(FORMATS VST3)
set(PRODUCT_NAME "ReaSpeechLite")
set(PROJECT_NAME "ReaSpeechLite")

project(${PROJECT_NAME} VERSION ${CURRENT_VERSION})

if(MSVC)
    # ONNX's static archive uses the release STL ABI, including in Debug builds.
    # Keep JUCE's DEBUG assertions, but avoid selecting the debug CRT/STL headers.
    add_compile_options(/U_DEBUG)
    add_compile_definitions(_ITERATOR_DEBUG_LEVEL=0)
endif()

add_subdirectory(JUCE)
juce_set_ara_sdk_path("${CMAKE_SOURCE_DIR}/ARA_SDK")

# Add WebView2 for Windows
include(WebView2)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
include(ReaSpeech)

# See `docs/CMake API.md` in the JUCE repo for all config options
juce_add_plugin("${PROJECT_NAME}"
    BUNDLE_ID "${BUNDLE_ID}"
    COMPANY_NAME "${COMPANY_NAME}"
    COMPANY_WEBSITE "${COMPANY_WEBSITE}"
    FORMATS "${FORMATS}"
    PRODUCT_NAME "${PRODUCT_NAME}"

    # A four-character plugin id
    # First character MUST be uppercase for AU formats
    PLUGIN_MANUFACTURER_CODE TATA

    # A unique four-character plugin id
    # Note: this must have at least one upper-case character
    PLUGIN_CODE Trsl

    IS_ARA_EFFECT TRUE
    NEEDS_CURL TRUE
    NEEDS_WEBVIEW2 TRUE
    NEEDS_WEB_BROWSER TRUE
    VST3_AUTO_MANIFEST FALSE
)

add_library(SharedCode INTERFACE)
include(SharedCodeDefaults)

set(SourceFiles
    source/Main.cpp
    source/reaper/IReaperHostApplication.cpp
)
target_sources(SharedCode INTERFACE ${SourceFiles})

# Creates Assets target and optionally builds web assets.
include(WebAssets)

# MacOS only: Cleans up folder and target organization on Xcode.
include(XcodePrettify)

target_compile_definitions(SharedCode
    INTERFACE

    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_WEB_BROWSER=1
    JUCE_USE_CURL=1
    JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
    JUCE_VST3_CAN_REPLACE_VST2=0

    CMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}"
    VERSION="${CURRENT_VERSION}"
)

target_link_libraries(SharedCode
    INTERFACE
    Assets
    juce_audio_utils
    juce_audio_processors
    juce_dsp
    juce_gui_basics
    juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
    ReaSpeech
)

# Link the JUCE plugin targets with our SharedCode target
target_link_libraries("${PROJECT_NAME}" PRIVATE SharedCode)

# Output some config for CI (like our PRODUCT_NAME)
include(GitHubENV)

option(BUILD_ASR_TESTS "Build C API adapter tests with a mock backend" OFF)
if(BUILD_ASR_TESTS)
    enable_testing()
    add_executable(ASREngineTests tests/ASREngineTests.cpp)
    target_include_directories(ASREngineTests PRIVATE "${REASPEECH_ROOT}/include")
    target_compile_features(ASREngineTests PRIVATE cxx_std_20)
    target_compile_definitions(ASREngineTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(ASREngineTests PRIVATE juce::juce_audio_formats juce::juce_recommended_config_flags)
    add_test(NAME ASREngineTests COMMAND ASREngineTests)
    add_executable(ReaSpeechRuntimeTests tests/ReaSpeechRuntimeTests.cpp)
    target_compile_features(ReaSpeechRuntimeTests PRIVATE cxx_std_17)
    target_link_libraries(ReaSpeechRuntimeTests PRIVATE ReaSpeech)
    add_test(NAME ReaSpeechRuntimeTests COMMAND ReaSpeechRuntimeTests)
endif()
