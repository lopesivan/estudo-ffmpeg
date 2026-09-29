cmake_minimum_required(VERSION 3.14)
project(video-app C)
set(CMAKE_C_STANDARD 11)

add_subdirectory(lib/FFmpeg)

list(APPEND SOURCES
    main.c
)

add_executable(${PROJECT_NAME} MACOSX_BUNDLE WIN32 ${SOURCES})

list(APPEND EXTRA_LIBS
        "-lm")

target_link_libraries(${PROJECT_NAME} FFmpeg ${EXTRA_LIBS})
