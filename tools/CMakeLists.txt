if (NOT UNIX)
    return()
endif()

set(TARGET_NAME sqz)
add_executable(${TARGET_NAME} sqz.cpp)
target_link_libraries(${TARGET_NAME} squeeze)

install(TARGETS sqz DESTINATION bin)
