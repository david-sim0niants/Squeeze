include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

set(TEST_TARGETS squeeze entry_header encode_decode
    compression/huffman compression/deflate_huffman compression/huffman_15 compression/lz77)
set(TEST_SRC_FILES ${TEST_TARGETS})
list(TRANSFORM TEST_SRC_FILES APPEND .cpp)
list(APPEND TEST_SRC_FILES main.cpp test_common/test_data.cpp test_common/test_on_data_input.cpp)

set(TARGET_NAME test_all)
add_executable(${TARGET_NAME} ${TEST_SRC_FILES})
target_include_directories(${TARGET_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(${TARGET_NAME} squeeze test_tools gtest gmock)

# include(GoogleTest)
# gtest_discover_tests(${TARGET_NAME})

set(INSTALL_GTEST OFF CACHE BOOL "Disable installation of GTest" FORCE)

add_subdirectory(test_tools)
