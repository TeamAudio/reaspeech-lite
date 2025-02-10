cmake_minimum_required(VERSION 3.22)

project(ReaSpeechLite VERSION 0.0.1)

add_subdirectory(JUCE)
add_subdirectory(whisper.cpp)

juce_set_ara_sdk_path("${CMAKE_SOURCE_DIR}/ARA_SDK")

juce_add_plugin(${PROJECT_NAME}
    COMPANY_NAME TechAudio
    COMPANY_WEBSITE https://techaud.io
    PLUGIN_MANUFACTURER_CODE TATA
    PLUGIN_CODE Trsl
    FORMATS VST3
    IS_ARA_EFFECT TRUE
    NEEDS_WEBVIEW2 TRUE
)

juce_generate_juce_header(${PROJECT_NAME})

target_sources(${PROJECT_NAME}
    PRIVATE
        Source/reaper/IReaperHostApplication.cpp
        Source/Main.cpp
)

target_compile_definitions(${PROJECT_NAME}
    PUBLIC
        DONT_SET_USING_JUCE_NAMESPACE=1
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_WEB_BROWSER=1
        JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
)

juce_add_binary_data("${PROJECT_NAME}Data" SOURCES
    Source/binary/css/bootstrap.min.css
    Source/binary/css/bootstrap.min.css.map
    Source/binary/html/index.html
    Source/binary/js/bootstrap.bundle.min.js
    Source/binary/js/bootstrap.bundle.min.js.map
    Source/binary/js/main.js
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        "${PROJECT_NAME}Data"
        juce::juce_audio_utils
        whisper
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
