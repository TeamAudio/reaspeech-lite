cmake_minimum_required(VERSION 3.25)

# Pamplejuce includes
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Project-specific includes
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake-local")

include(PamplejuceVersion)

# Configures universal binaries and decides which version of macOS to support
include(PamplejuceMacOS)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "" FORCE)

include(JUCEDefaults)

set(BUNDLE_ID "com.TechAudio.ReaSpeechLite")
set(COMPANY_NAME "TechAudio")
set(COMPANY_WEBSITE "https://techaud.io")
set(FORMATS VST3)
set(PRODUCT_NAME "ReaSpeechLite")
set(PROJECT_NAME "ReaSpeechLite")

project(${PROJECT_NAME} VERSION ${CURRENT_VERSION})

add_subdirectory(JUCE)
juce_set_ara_sdk_path("${CMAKE_SOURCE_DIR}/ARA_SDK")

# Add WebView2 for Windows
include(WebView2)

# Enable position independent code for whisper.cpp
set(BUILD_SHARED_LIBS OFF)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Add whisper.cpp
add_subdirectory(whisper.cpp)

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
    whisper
)

# Link the JUCE plugin targets with our SharedCode target
target_link_libraries("${PROJECT_NAME}" PRIVATE SharedCode)

# Output some config for CI (like our PRODUCT_NAME)
include(GitHubENV)
