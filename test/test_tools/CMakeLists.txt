add_subdirectory(src)

set(TARGET_NAME test_tools)
add_library(${TARGET_NAME} INTERFACE)
target_include_directories(${TARGET_NAME} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include/)
target_link_libraries(${TARGET_NAME} INTERFACE test_tools_src)
