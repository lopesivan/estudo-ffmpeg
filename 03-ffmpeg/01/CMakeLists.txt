cmake_minimum_required(VERSION 3.8)
project(app)

set(CMAKE_CXX_STANDARD 20)
# ----------------------------------------
# Opções de build para debug
# ----------------------------------------
set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SOURCE_FILES ffmpeg_job.hpp main.cpp unix_command.hpp)
add_executable(app ${SOURCE_FILES})
