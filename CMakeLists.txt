cmake_minimum_required(VERSION 3.13)
project(Squeeze CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

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
