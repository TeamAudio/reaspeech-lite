# Embed the Rust C API; native backend/system libraries are linked separately.
set(REASPEECH_ROOT "${CMAKE_SOURCE_DIR}/reaspeech-lib" CACHE PATH "ReaSpeech Lib source checkout")
set(REASPEECH_LIBRARY "" CACHE FILEPATH "Prebuilt ReaSpeech static archive (.lib or .a)")
set(REASPEECH_FEATURES "" CACHE STRING "Rust features, e.g. cuda or metal")
set(REASPEECH_NATIVE_LIBRARIES "" CACHE STRING "Additional native libraries required by a prebuilt ReaSpeech static archive")
if(NOT EXISTS "${REASPEECH_ROOT}/include/reaspeech.h")
    message(FATAL_ERROR "ReaSpeech Lib headers missing. Run git submodule update --init --recursive, or set REASPEECH_ROOT to a checkout containing include/reaspeech.h")
endif()
if(APPLE AND NOT REASPEECH_LIBRARY AND CMAKE_OSX_ARCHITECTURES MATCHES ";")
    message(FATAL_ERROR "Universal macOS builds require a universal static REASPEECH_LIBRARY; see the CI workflow for the two-target Rust build.")
endif()
add_library(ReaSpeech STATIC IMPORTED GLOBAL)
if(NOT REASPEECH_LIBRARY)
    find_program(CARGO_EXECUTABLE cargo REQUIRED)
    set(_cargo_dir "${CMAKE_BINARY_DIR}/reaspeech-target")
    if(WIN32)
        set(_library "${_cargo_dir}/release/reaspeech.lib")
    else()
        set(_library "${_cargo_dir}/release/libreaspeech.a")
    endif()
    set(_native_response "${_cargo_dir}/native-libs.rsp")
    set(_native_object "${_cargo_dir}/native-libs.obj")
    add_custom_target(ReaSpeechBuild
        COMMAND "${CMAKE_COMMAND}" "-DCARGO=${CARGO_EXECUTABLE}"
            "-DROOT=${REASPEECH_ROOT}" "-DTARGET_DIR=${_cargo_dir}"
            "-DFEATURES=${REASPEECH_FEATURES}" "-DMSVC=${MSVC}"
            "-DC_COMPILER=${CMAKE_C_COMPILER}" "-DNATIVE_OBJECT=${_native_object}"
            "-DRESPONSE=${_native_response}"
            "-DMACOS_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}"
            -P "${CMAKE_CURRENT_LIST_DIR}/BuildReaSpeech.cmake"
        BYPRODUCTS "${_library}" "${_native_response}" "${_native_object}"
        COMMENT "Building ReaSpeech Lib static C API" VERBATIM)
    add_dependencies(ReaSpeech ReaSpeechBuild)
    if(MSVC)
        target_link_libraries(ReaSpeech INTERFACE "${_native_object}")
    else()
        # Keep native libraries after the Rust archive for Unix linkers.
        target_link_libraries(ReaSpeech INTERFACE "-Wl,@${_native_response}")
    endif()
else()
    get_filename_component(_library "${REASPEECH_LIBRARY}" ABSOLUTE BASE_DIR "${CMAKE_SOURCE_DIR}")
    if(NOT EXISTS "${_library}" OR _library MATCHES "\\.(dll\\.lib|so|dylib)$")
        message(FATAL_ERROR "REASPEECH_LIBRARY must name an existing static archive (.lib or .a), not a shared library/import library")
    endif()
    if(APPLE)
        target_link_libraries(ReaSpeech INTERFACE
            "-framework Accelerate" "-framework CoreFoundation" "-framework Security"
            "-framework SystemConfiguration" "-framework Foundation" "-framework Metal"
            c++ objc iconv resolv)
    endif()
endif()
set_target_properties(ReaSpeech PROPERTIES IMPORTED_LOCATION "${_library}"
    INTERFACE_INCLUDE_DIRECTORIES "${REASPEECH_ROOT}/include")
target_link_libraries(ReaSpeech INTERFACE ${REASPEECH_NATIVE_LIBRARIES})
