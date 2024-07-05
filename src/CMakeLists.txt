set(TARGET_NAME squeeze)
add_library(${TARGET_NAME} SHARED
    squeeze.cpp reader.cpp writer.cpp appender.cpp remover.cpp extracter.cpp lister.cpp
    append_scheduler.cpp entry_iterator.cpp
    entry_common.cpp entry_header.cpp entry_input.cpp entry_output.cpp
    encoder.cpp decoder.cpp
    misc/thread_pool.cpp misc/thread_safe_queue.cpp
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


set(SANITIZE "none" CACHE STRING "Enable sanitizer: none, address, thread")

string(TOLOWER ${SANITIZE} SANITIZE)

if (NOT SANITIZE MATCHES "(none|address|thread)")
    message(FATAL_ERROR "Invalid value for SANITIZE - " ${SANITIZE})
elseif (SANITIZE STREQUAL "none")
    message(STATUS "Not using any sanitizer")
else ()
    message(STATUS "Using ${SANITIZE} sanitizer")
    if (MSVC)
        message(SEND_ERROR "Sanitizer not supported for MSVC")
    else ()
        target_compile_options(${TARGET_NAME} PUBLIC -fsanitize=${SANITIZE})
        target_link_options(${TARGET_NAME} PUBLIC -fsanitize=${SANITIZE})
    endif ()
endif ()
