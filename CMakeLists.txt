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

# Add build timestamp to product name
string(TIMESTAMP BUILD_TIMESTAMP "%Y%m%d_%H%M" UTC)
set(PRODUCT_NAME "ReaSpeechLite_${BUILD_TIMESTAMP}")
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

# Add ONNX Runtime for parakeet model support
include(FetchContent)

# Determine platform and set ONNX Runtime URL
set(ONNXRUNTIME_VERSION "1.19.2")
if(APPLE)
    # For macOS, download both architectures to create universal binary
    set(ONNXRUNTIME_URL_ARM64 "https://github.com/microsoft/onnxruntime/releases/download/v${ONNXRUNTIME_VERSION}/onnxruntime-osx-arm64-${ONNXRUNTIME_VERSION}.tgz")
    set(ONNXRUNTIME_URL_X86_64 "https://github.com/microsoft/onnxruntime/releases/download/v${ONNXRUNTIME_VERSION}/onnxruntime-osx-x86_64-${ONNXRUNTIME_VERSION}.tgz")

    message(STATUS "ONNX Runtime ARM64 URL: ${ONNXRUNTIME_URL_ARM64}")
    message(STATUS "ONNX Runtime x86_64 URL: ${ONNXRUNTIME_URL_X86_64}")

    FetchContent_Declare(
        onnxruntime_arm64
        URL ${ONNXRUNTIME_URL_ARM64}
    )
    FetchContent_Declare(
        onnxruntime_x86_64
        URL ${ONNXRUNTIME_URL_X86_64}
    )
    FetchContent_MakeAvailable(onnxruntime_arm64 onnxruntime_x86_64)

    # Create universal binary directory
    set(ONNXRUNTIME_UNIVERSAL_DIR ${CMAKE_BINARY_DIR}/onnxruntime-universal)
    file(MAKE_DIRECTORY ${ONNXRUNTIME_UNIVERSAL_DIR}/lib)
    file(MAKE_DIRECTORY ${ONNXRUNTIME_UNIVERSAL_DIR}/include)

    # Copy headers from one architecture (they're the same)
    file(COPY ${onnxruntime_arm64_SOURCE_DIR}/include/ DESTINATION ${ONNXRUNTIME_UNIVERSAL_DIR}/include)

    # Create universal binary from both architectures using lipo
    set(DYLIB_NAME "libonnxruntime.${ONNXRUNTIME_VERSION}.dylib")
    execute_process(
        COMMAND lipo -create
            ${onnxruntime_arm64_SOURCE_DIR}/lib/${DYLIB_NAME}
            ${onnxruntime_x86_64_SOURCE_DIR}/lib/${DYLIB_NAME}
            -output ${ONNXRUNTIME_UNIVERSAL_DIR}/lib/${DYLIB_NAME}
        RESULT_VARIABLE LIPO_RESULT
    )

    if(NOT LIPO_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to create universal ONNX Runtime binary")
    endif()

    # Create symlink for libonnxruntime.dylib
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E create_symlink
            ${DYLIB_NAME}
            ${ONNXRUNTIME_UNIVERSAL_DIR}/lib/libonnxruntime.dylib
    )

    set(ONNXRUNTIME_INCLUDE_DIRS ${ONNXRUNTIME_UNIVERSAL_DIR}/include)
    set(ONNXRUNTIME_LIB_DIR ${ONNXRUNTIME_UNIVERSAL_DIR}/lib)

    message(STATUS "Created universal ONNX Runtime binary at ${ONNXRUNTIME_UNIVERSAL_DIR}")
elseif(WIN32)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(ONNXRUNTIME_URL "https://github.com/microsoft/onnxruntime/releases/download/v${ONNXRUNTIME_VERSION}/onnxruntime-win-x64-${ONNXRUNTIME_VERSION}.zip")
    else()
        set(ONNXRUNTIME_URL "https://github.com/microsoft/onnxruntime/releases/download/v${ONNXRUNTIME_VERSION}/onnxruntime-win-x86-${ONNXRUNTIME_VERSION}.zip")
    endif()

    message(STATUS "ONNX Runtime URL: ${ONNXRUNTIME_URL}")

    FetchContent_Declare(
        onnxruntime
        URL ${ONNXRUNTIME_URL}
    )
    FetchContent_MakeAvailable(onnxruntime)

    set(ONNXRUNTIME_INCLUDE_DIRS ${onnxruntime_SOURCE_DIR}/include)
    set(ONNXRUNTIME_LIB_DIR ${onnxruntime_SOURCE_DIR}/lib)
else()
    # Linux
    set(ONNXRUNTIME_URL "https://github.com/microsoft/onnxruntime/releases/download/v${ONNXRUNTIME_VERSION}/onnxruntime-linux-x64-${ONNXRUNTIME_VERSION}.tgz")

    message(STATUS "ONNX Runtime URL: ${ONNXRUNTIME_URL}")

    FetchContent_Declare(
        onnxruntime
        URL ${ONNXRUNTIME_URL}
    )
    FetchContent_MakeAvailable(onnxruntime)

    set(ONNXRUNTIME_INCLUDE_DIRS ${onnxruntime_SOURCE_DIR}/include)
    set(ONNXRUNTIME_LIB_DIR ${onnxruntime_SOURCE_DIR}/lib)
endif()

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

target_include_directories(SharedCode
    INTERFACE
    ${ONNXRUNTIME_INCLUDE_DIRS}
)

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

target_link_directories(SharedCode
    INTERFACE
    ${ONNXRUNTIME_LIB_DIR}
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
    onnxruntime
)

# Link the JUCE plugin targets with our SharedCode target
target_link_libraries("${PROJECT_NAME}" PRIVATE SharedCode)

# Windows: Delay-load ONNX Runtime DLL to prevent loading at plugin startup
# Apply directly to VST3 target to ensure the flags are properly applied to the final binary
if(WIN32)
    # Apply delay-load to the VST3 target specifically
    target_link_options("${PROJECT_NAME}_VST3" PRIVATE
        /DELAYLOAD:onnxruntime.dll
    )
    target_link_libraries("${PROJECT_NAME}_VST3" PRIVATE delayimp)
endif()

# macOS: Copy ONNX Runtime library into VST3 bundle and fix paths
if(APPLE)
    add_custom_command(TARGET ${PROJECT_NAME}_VST3 POST_BUILD
        # Copy the universal ONNX Runtime library into the VST3 bundle
        COMMAND ${CMAKE_COMMAND} -E copy
            ${ONNXRUNTIME_LIB_DIR}/libonnxruntime.${ONNXRUNTIME_VERSION}.dylib
            $<TARGET_BUNDLE_DIR:${PROJECT_NAME}_VST3>/Contents/MacOS/libonnxruntime.${ONNXRUNTIME_VERSION}.dylib

        # Change the library's install_name to use @loader_path
        COMMAND install_name_tool -id "@loader_path/libonnxruntime.${ONNXRUNTIME_VERSION}.dylib"
            $<TARGET_BUNDLE_DIR:${PROJECT_NAME}_VST3>/Contents/MacOS/libonnxruntime.${ONNXRUNTIME_VERSION}.dylib

        # Update the plugin binary to look for the library at @loader_path
        COMMAND install_name_tool -change
            "@rpath/libonnxruntime.${ONNXRUNTIME_VERSION}.dylib"
            "@loader_path/libonnxruntime.${ONNXRUNTIME_VERSION}.dylib"
            $<TARGET_BUNDLE_DIR:${PROJECT_NAME}_VST3>/Contents/MacOS/${PRODUCT_NAME}

        # Re-sign the bundle after modifications (ad-hoc signing)
        COMMAND codesign --force --deep --sign - $<TARGET_BUNDLE_DIR:${PROJECT_NAME}_VST3>

        COMMENT "Copying ONNX Runtime library into VST3 bundle, fixing paths, and re-signing"
    )
endif()

# Windows: Copy ONNX Runtime DLL into VST3 bundle
if(WIN32)
    add_custom_command(TARGET ${PROJECT_NAME}_VST3 POST_BUILD
        # Copy the ONNX Runtime DLL into the VST3 bundle next to the plugin binary
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${ONNXRUNTIME_LIB_DIR}/onnxruntime.dll
            $<TARGET_FILE_DIR:${PROJECT_NAME}_VST3>/onnxruntime.dll

        COMMENT "Copying ONNX Runtime DLL into VST3 bundle"
    )
endif()

# Output some config for CI (like our PRODUCT_NAME)
include(GitHubENV)
