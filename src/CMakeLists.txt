set(TARGET_NAME squeeze)
add_library(${TARGET_NAME} SHARED
    reader.cpp writer.cpp reader_iterator.cpp
    entry_common.cpp entry_header.cpp entry_input.cpp entry_output.cpp
    utils/io.cpp utils/fs.cpp)
target_include_directories(${TARGET_NAME} PUBLIC ${MAIN_INCLUDE_DIR})
set_target_properties(${TARGET_NAME} PROPERTIES OUTPUT_NAME sqz)
install(TARGETS squeeze DESTINATION lib)
