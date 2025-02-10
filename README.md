# reaspeech-lite

Speech-to-text transcription VST3/ARA plugin

## Git submodule initialization

    git submodule update --init --recursive

## CMake initialization

    mkdir build
    cd build
    cmake .. -DCMAKE_BUILD_TYPE=Debug -DBUILD_SHARED_LIBS=0

For Windows/CUDA, use:

    cmake .. -DCMAKE_BUILD_TYPE=Debug -DBUILD_SHARED_LIBS=0 -DGGML_CUDA=1

## Building

    cmake --build build
