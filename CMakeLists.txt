cmake_minimum_required(VERSION 3.16)
project(cskip VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)

# Use PkgConfig to find FFmpeg libraries (avcodec, avformat, avutil, swscale)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
    libavcodec libavformat libavutil libswscale
)

add_executable(cskip main.cpp FrameDecoder.cpp)

# Link using the IMPORTED_TARGETs
target_link_libraries(${PROJECT_NAME} PRIVATE PkgConfig::FFMPEG)