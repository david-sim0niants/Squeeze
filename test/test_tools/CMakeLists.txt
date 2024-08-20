set(TARGET_NAME test_tools)
add_library(${TARGET_NAME} INTERFACE)
target_sources(${TARGET_NAME} INTERFACE
    mock_entry_input.cpp mock_entry_output.cpp random.cpp mockfs.cpp
    generate_mockfs.cpp generate_mock_contents.cpp)
target_include_directories(${TARGET_NAME} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
