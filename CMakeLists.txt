cmake_minimum_required(VERSION 3.25)

# This tells cmake we have goodies in the /cmake folder
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(PamplejuceVersion)

# Modern concise way to add dependencies to your project
include(CPM)

# Configures universal binaries and decides which version of macOS to support
include(PamplejuceMacOS)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "" FORCE)

# Couple tweaks that IMO should be JUCE defaults
include(JUCEDefaults)

# Change me!
# This is the internal name of the project and the name of JUCE's shared code target
# Note: This cannot have spaces (it may be 2024, but you can't have it all!)
# Worry not, JUCE's PRODUCT_NAME can have spaces (and is what DAWs display)
set(PROJECT_NAME "ReaSpeechLite")

# Worry not, JUCE's PRODUCT_NAME can have spaces (and is what DAWs will display)
# You can also just have it be the same thing as PROJECT_NAME
# You may want to append the major version on the end of this (and PROJECT_NAME) ala:
#   set(PROJECT_NAME "MyPlugin_v${MAJOR_VERSION}")
# Doing so enables major versions to show up in IDEs and DAWs as separate plugins
# allowing you to change parameters and behavior without breaking existing user projects
set(PRODUCT_NAME "ReaSpeechLite")

# Change me! Used for the MacOS bundle name and Installers
set(COMPANY_NAME "TechAudio")

# Change me! Used for the MacOS bundle identifier (and signing)
set(BUNDLE_ID "com.TechAudio.ReaSpeechLite")

# Change me! Set the plugin formats you want built
# Valid choices: AAX Unity VST VST3 AU AUv3 Standalone
set(FORMATS VST3)

# For simplicity, the name of the CMake project is also the name of the target
project(${PROJECT_NAME} VERSION ${CURRENT_VERSION})

# JUCE is setup as a submodule in the /JUCE folder
# Locally, you must run `git submodule update --init --recursive` once
# and later `git submodule update --remote --merge` to keep it up to date
# On Github Actions, this is done as a part of actions/checkout
add_subdirectory(JUCE)

# Add CLAP format
# add_subdirectory(modules/clap-juce-extensions EXCLUDE_FROM_ALL)

# Add any other modules you want modules here, before the juce_add_plugin call
# juce_add_module(modules/my_module)

# This adds the melatonin inspector module
# add_subdirectory(modules/melatonin_inspector)

# Add ARA SDK
juce_set_ara_sdk_path("${CMAKE_SOURCE_DIR}/ARA_SDK")

# Add WebView2 for Windows
if(WIN32)
    # Check for nuget and install it if not found
    find_program(NUGET_EXE NAMES nuget)
    if(NOT NUGET_EXE)
        message("NUGET.EXE not found. Attempting to download...")
        file(DOWNLOAD "https://dist.nuget.org/win-x86-commandline/latest/nuget.exe" "${CMAKE_BINARY_DIR}/nuget.exe" SHOW_PROGRESS)
        set(NUGET_EXE "${CMAKE_BINARY_DIR}/nuget.exe")
        if(NOT EXISTS "${NUGET_EXE}")
            message(FATAL_ERROR "Failed to download nuget.exe")
        endif()
        message("Downloaded nuget.exe to ${NUGET_EXE}")
    endif()

    execute_process(COMMAND ${NUGET_EXE} install "Microsoft.Web.WebView2" -Version 1.0.1901.177 -OutputDirectory ${CMAKE_BINARY_DIR}/packages)
    set(WebView2_DIR ${CMAKE_BINARY_DIR}/packages/Microsoft.Web.WebView2)
    set(JUCE_WEBVIEW2_PACKAGE_LOCATION "${CMAKE_BINARY_DIR}/packages")
endif()

# Enable position independent code for whisper.cpp
set(BUILD_SHARED_LIBS OFF)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Add whisper.cpp
add_subdirectory(whisper.cpp)

# See `docs/CMake API.md` in the JUCE repo for all config options
juce_add_plugin("${PROJECT_NAME}"
    # Icons for the standalone app
    # ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/packaging/icon.png"

    # Change me!
    COMPANY_NAME "${COMPANY_NAME}"
    COMPANY_WEBSITE https://techaud.io
    BUNDLE_ID "${BUNDLE_ID}"

    # On MacOS, plugin is copied to /Users/yourname/Library/Audio/Plug-Ins/
    # COPY_PLUGIN_AFTER_BUILD TRUE

    # Change me!
    # A four-character plugin id
    # First character MUST be uppercase for AU formats
    PLUGIN_MANUFACTURER_CODE TATA

    # Change me!
    # A unique four-character plugin id
    # Note: this must have at least one upper-case character
    PLUGIN_CODE Trsl
    FORMATS "${FORMATS}"

    # The name of your final executable
    # This is how it's listed in the DAW
    # This can be different from PROJECT_NAME and can have spaces!
    # You might want to use v${MAJOR_VERSION} here once you go to v2...
    PRODUCT_NAME "${PRODUCT_NAME}"

    IS_ARA_EFFECT TRUE
    NEEDS_CURL TRUE
    NEEDS_WEBVIEW2 TRUE
    NEEDS_WEB_BROWSER TRUE
    VST3_AUTO_MANIFEST FALSE
)

# This lets us use our code in both the JUCE targets and our Test target
# Without running into ODR violations
add_library(SharedCode INTERFACE)

# clap_juce_extensions_plugin(TARGET "${PROJECT_NAME}"
#     CLAP_ID "${BUNDLE_ID}"
#     CLAP_FEATURES audio-effect)

# Enable fast math, C++20 and a few other target defaults
include(SharedCodeDefaults)

# Manually list all .h and .cpp files for the plugin
# If you are like me, you'll use globs for your sanity.
# Just ensure you employ CONFIGURE_DEPENDS so the build system picks up changes
# If you want to appease the CMake gods and avoid globs, manually add files like so:
set(SourceFiles source/reaper/IReaperHostApplication.cpp source/Main.cpp)
# file(GLOB_RECURSE SourceFiles CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/source/*.h")
target_sources(SharedCode INTERFACE ${SourceFiles})

# Adds a BinaryData target for embedding assets into the binary
include(Assets)

# Check if the required JS file exists in the asset files
set(REQUIRED_JS_FILE "${CMAKE_CURRENT_SOURCE_DIR}/assets/js/main.js")
list(FIND AssetFiles "${REQUIRED_JS_FILE}" JS_FILE_INDEX)
if(JS_FILE_INDEX EQUAL -1)
    message(FATAL_ERROR "Missing required file: ${REQUIRED_JS_FILE}\nPlease run 'npm run build' from the 'source/ts' directory before building.")
endif()

# MacOS only: Cleans up folder and target organization on Xcode.
include(XcodePrettify)

# This is where you can set preprocessor definitions for JUCE and your plugin
target_compile_definitions(SharedCode
    INTERFACE

    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_WEB_BROWSER=1  # If you set this to 1, add `NEEDS_WEB_BROWSER TRUE` to the `juce_add_plugin` call
    JUCE_USE_CURL=1     # If you set this to 1, add `NEEDS_CURL TRUE` to the `juce_add_plugin` call
    JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
    JUCE_VST3_CAN_REPLACE_VST2=0

    # lets the app known if we're Debug or Release
    CMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}"
    VERSION="${CURRENT_VERSION}"

    # JucePlugin_Name is for some reason doesn't use the nicer PRODUCT_NAME
    PRODUCT_NAME_WITHOUT_VERSION="ReaSpeechLite"
)

# Link to any other modules you added (with juce_add_module) here!
# Usually JUCE modules must have PRIVATE visibility
# See https://github.com/juce-framework/JUCE/blob/master/docs/CMake%20API.md#juce_add_module
# However, with Pamplejuce, you'll link modules to SharedCode with INTERFACE visibility
# This allows the JUCE plugin targets and the Tests target to link against it
target_link_libraries(SharedCode
    INTERFACE
    Assets
    # melatonin_inspector
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

# Link the JUCE plugin targets our SharedCode target
target_link_libraries("${PROJECT_NAME}" PRIVATE SharedCode)

# IPP support, comment out to disable
# include(PamplejuceIPP)

# Everything related to the tests target
# include(Tests)

# A separate target for Benchmarks (keeps the Tests target fast)
# include(Benchmarks)

# Output some config for CI (like our PRODUCT_NAME)
include(GitHubENV)
