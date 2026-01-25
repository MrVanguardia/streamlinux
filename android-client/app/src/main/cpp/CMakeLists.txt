cmake_minimum_required(VERSION 3.22.1)

project("streamlinux_client")

# C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Native library
add_library(streamlinux_client SHARED
    streamlinux_jni.cpp
    decoder/video_decoder.cpp
    decoder/audio_decoder.cpp
    sync/av_sync.cpp
)

# Find libraries
find_library(log-lib log)
find_library(android-lib android)

# Link libraries
target_link_libraries(streamlinux_client
    ${log-lib}
    ${android-lib}
    mediandk
    OpenSLES
)

# Include directories
target_include_directories(streamlinux_client PRIVATE
    ${CMAKE_SOURCE_DIR}
)
