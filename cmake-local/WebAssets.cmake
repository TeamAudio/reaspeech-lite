# Option to build web assets
option(BUILD_WEB_ASSETS "Build web assets using npm" OFF)

if(BUILD_WEB_ASSETS)
    # Find npm executable
    find_program(NPM_EXECUTABLE npm)
    if(NOT NPM_EXECUTABLE)
        message(FATAL_ERROR "npm not found but BUILD_WEB_ASSETS is ON")
    endif()

    if(WIN32)
        set(OS_COMMAND_PREFIX cmd /c)
    else()
        set(OS_COMMAND_PREFIX "")
    endif()

    # Run npm install first
    execute_process(
        COMMAND ${OS_COMMAND_PREFIX} ${NPM_EXECUTABLE} install
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/source/ts"
        RESULT_VARIABLE NPM_INSTALL_RESULT
    )

    if(NOT NPM_INSTALL_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to install npm dependencies")
    endif()

    # Then run npm build
    execute_process(
        COMMAND ${OS_COMMAND_PREFIX} ${NPM_EXECUTABLE} run build
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/source/ts"
        RESULT_VARIABLE NPM_BUILD_RESULT
    )

    if(NOT NPM_BUILD_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to build web assets")
    endif()

    # Create rebuild target for subsequent builds
    add_custom_target(WebAssets
        COMMAND ${OS_COMMAND_PREFIX} ${NPM_EXECUTABLE} run build
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/source/ts"
        COMMENT "Building web assets..."
    )
endif()

# Now include Assets after we've potentially created main.js
include(Assets)

# Check if main.js exists
set(REQUIRED_JS_FILE "${CMAKE_CURRENT_SOURCE_DIR}/assets/js/main.js")
list(FIND AssetFiles "${REQUIRED_JS_FILE}" JS_FILE_INDEX)
if(JS_FILE_INDEX EQUAL -1)
    message(FATAL_ERROR "Missing required file: ${REQUIRED_JS_FILE}\nPlease run 'npm run build' from the 'source/ts' directory before building.")
endif()

if(BUILD_WEB_ASSETS)
    # Make Assets depend on the rebuild target
    add_dependencies(Assets WebAssets)
endif()
