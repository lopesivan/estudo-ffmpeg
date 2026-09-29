cmake_minimum_required(VERSION 3.14)
project(video_frames C)
set(CMAKE_C_STANDARD 11)

# ----------------------------------------
# Opções de build para debug
# ----------------------------------------
set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(lib/FFmpeg)

list(APPEND SOURCES
    video_frames.c
)

add_executable(${PROJECT_NAME} MACOSX_BUNDLE WIN32 ${SOURCES})

list(APPEND EXTRA_LIBS
        "-lm")

target_link_libraries(${PROJECT_NAME} FFmpeg ${EXTRA_LIBS})
