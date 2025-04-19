# Option to build web assets
option(BUILD_WEB_ASSETS "Build web assets using npm" OFF)

# Check for JS file or build it if BUILD_WEB_ASSETS is ON
set(REQUIRED_JS_FILE "${CMAKE_CURRENT_SOURCE_DIR}/assets/js/main.js")

if(BUILD_WEB_ASSETS)
    # Find npm executable
    find_program(NPM_EXECUTABLE npm)
    if(NOT NPM_EXECUTABLE)
        message(FATAL_ERROR "npm not found but BUILD_WEB_ASSETS is ON")
    endif()

    # Run npm install first
    if(WIN32)
        execute_process(
            COMMAND cmd /c ${NPM_EXECUTABLE} install
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/source/ts"
            RESULT_VARIABLE NPM_INSTALL_RESULT
        )
    else()
        execute_process(
            COMMAND ${NPM_EXECUTABLE} install
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/source/ts"
            RESULT_VARIABLE NPM_INSTALL_RESULT
        )
    endif()

    if(NOT NPM_INSTALL_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to install npm dependencies")
    endif()

    # Then run npm build
    if(WIN32)
        execute_process(
            COMMAND cmd /c ${NPM_EXECUTABLE} run build
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/source/ts"
            RESULT_VARIABLE NPM_BUILD_RESULT
        )
    else()
        execute_process(
            COMMAND ${NPM_EXECUTABLE} run build
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/source/ts"
            RESULT_VARIABLE NPM_BUILD_RESULT
        )
    endif()

    if(NOT NPM_BUILD_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to build web assets")
    endif()
endif()

# Now include Assets after we've potentially created main.js
include(Assets)

if(BUILD_WEB_ASSETS)
    # Create rebuild target for subsequent builds
    if(WIN32)
        add_custom_target(WebAssets
            COMMAND cmd /c ${NPM_EXECUTABLE} run build
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/source/ts"
            COMMENT "Building web assets..."
        )
    else()
        add_custom_target(WebAssets
            COMMAND ${NPM_EXECUTABLE} run build
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/source/ts"
            COMMENT "Building web assets..."
        )
    endif()

    # Make Assets depend on the rebuild target
    add_dependencies(Assets WebAssets)
else()
    # Check if main.js exists when not building
    list(FIND AssetFiles "${REQUIRED_JS_FILE}" JS_FILE_INDEX)
    if(JS_FILE_INDEX EQUAL -1)
        message(FATAL_ERROR "Missing required file: ${REQUIRED_JS_FILE}\nPlease run 'npm run build' from the 'source/ts' directory before building.")
    endif()
endif()
