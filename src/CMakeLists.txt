set(TARGET_NAME squeeze)
add_library(${TARGET_NAME} SHARED
    squeeze.cpp reader.cpp writer.cpp reader_iterator.cpp
    entry_common.cpp entry_header.cpp entry_input.cpp entry_output.cpp
    wrap/file_appender.cpp wrap/file_remover.cpp wrap/file_extracter.cpp wrap/file_squeeze.cpp
    utils/io.cpp utils/fs.cpp)
target_include_directories(${TARGET_NAME} PUBLIC ${MAIN_INCLUDE_DIR})
set_target_properties(${TARGET_NAME} PROPERTIES OUTPUT_NAME sqz)
install(TARGETS squeeze DESTINATION lib)

if (USE_LOGGING)
    find_package(spdlog REQUIRED)
    target_compile_options(${TARGET_NAME} PUBLIC -DSQUEEZE_USE_LOGGING)
    target_link_libraries(${TARGET_NAME} PUBLIC spdlog::spdlog)
endif ()
