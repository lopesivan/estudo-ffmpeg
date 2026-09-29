cmake_minimum_required(VERSION 3.14)
project(FFmpeg)

find_package(PkgConfig REQUIRED)
pkg_check_modules(AVUTIL      REQUIRED IMPORTED_TARGET libavutil)
pkg_check_modules(AVFORMAT    REQUIRED IMPORTED_TARGET libavformat)
pkg_check_modules(SWSCALE     REQUIRED IMPORTED_TARGET libswscale)
pkg_check_modules(AVCODEC     REQUIRED IMPORTED_TARGET libavcodec)
pkg_check_modules(AVFILTER    REQUIRED IMPORTED_TARGET libavfilter)

add_library(FFmpeg INTERFACE IMPORTED GLOBAL)

target_link_libraries(FFmpeg INTERFACE
    PkgConfig::AVUTIL
    PkgConfig::AVFILTER
    PkgConfig::AVFORMAT
    PkgConfig::SWSCALE
    PkgConfig::AVCODEC
)
