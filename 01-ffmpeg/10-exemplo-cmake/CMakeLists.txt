cmake_minimum_required(VERSION 3.14)
project(video-app C )
set(CMAKE_CXX_STANDARD 11)

add_subdirectory(lib/FFmpeg)

list(APPEND SOURCES
    main.c
)

add_executable(video-app MACOSX_BUNDLE WIN32 ${SOURCES})

list(APPEND EXTRA_LIBS
        "-lm")

target_link_libraries(video-app FFmpeg ${EXTRA_LIBS})
