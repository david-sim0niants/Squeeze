cmake_minimum_required(VERSION 3.13)
project(Squeeze CXX)

set(SQUEEZE_VERSION_MAJOR 0)
set(SQUEEZE_VERSION_MINOR 1)
set(SQUEEZE_VERSION_PATCH 1)
set(SQUEEZE_VERSION ${SQUEEZE_VERSION_MAJOR}.${SQUEEZE_VERSION_MINOR}.${SQUEEZE_VERSION_PATCH})

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

if (WIN32)
    # windows-specific
    # place all binaries under their respective directories
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
endif ()

option(BUILD_SHARED_LIBS "Build using shared libraries?" ON)
set(MAIN_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include)
add_subdirectory(src)

option(BUILD_TESTS "Build tests?" OFF)
if (BUILD_TESTS)
    add_subdirectory(test)
endif ()

option(BUILD_TOOLS "Build tools?" ON)
if (BUILD_TOOLS)
    add_subdirectory(tools)
endif ()

option(USE_LOGGING "Use logging?" OFF)

find_package(Doxygen)
if (DOXYGEN_FOUND)
    set(DOXYGEN_CONFIG_FILE ${PROJECT_SOURCE_DIR}/Doxyfile)

    add_custom_target(doc
        COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_CONFIG_FILE}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM
    )
else ()
    message(STATUS "Doxygen not found. Documentation target 'doc' will not be available.")
endif ()

install(FILES ${PROJECT_SOURCE_DIR}/LICENSE DESTINATION share/doc/squeeze)
