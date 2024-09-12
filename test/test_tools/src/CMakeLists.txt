set(TARGET_NAME test_tools_src)
add_library(${TARGET_NAME} INTERFACE)
target_sources(${TARGET_NAME} INTERFACE printing.cpp
    mock/entry_input.cpp mock/entry_output.cpp mock/fs.cpp
    generators/data_gen.cpp generators/mockfs_gen.cpp)
