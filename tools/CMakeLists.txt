if (NOT UNIX)
    return()
endif()

set(TARGET_NAME sqz)
add_executable(${TARGET_NAME} sqz.cpp)
target_link_libraries(${TARGET_NAME} squeeze)

set(TARGET_NAME unsqz)
add_executable(${TARGET_NAME} unsqz.cpp)
target_link_libraries(${TARGET_NAME} squeeze)

install(TARGETS sqz unsqz DESTINATION bin)
