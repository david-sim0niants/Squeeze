set(TARGET_NAME squeeze)
add_library(${TARGET_NAME} SHARED
    squeeze.cpp reader.cpp writer.cpp reader_iterator.cpp
    entry_common.cpp entry_header.cpp entry_input.cpp entry_output.cpp
    wrap/file_appender.cpp wrap/file_remover.cpp wrap/file_extracter.cpp wrap/file_squeeze.cpp
    utils/io.cpp utils/fs.cpp)
target_include_directories(${TARGET_NAME} PUBLIC ${MAIN_INCLUDE_DIR})

target_compile_options(${TARGET_NAME} PUBLIC
    -DSQUEEZE_VERSION_MAJOR=${SQUEEZE_VERSION_MAJOR}
    -DSQUEEZE_VERSION_MINOR=${SQUEEZE_VERSION_MINOR}
    -DSQUEEZE_VERSION_PATCH=${SQUEEZE_VERSION_PATCH})

set_target_properties(${TARGET_NAME} PROPERTIES OUTPUT_NAME sqz
    VERSION ${SQUEEZE_VERSION} SOVERSION ${SQUEEZE_VERSION_MAJOR})

install(TARGETS squeeze DESTINATION lib)

if (USE_LOGGING)
    find_package(spdlog REQUIRED)
    target_compile_options(${TARGET_NAME} PUBLIC -DSQUEEZE_USE_LOGGING)
    target_link_libraries(${TARGET_NAME} PUBLIC spdlog::spdlog)
endif ()
