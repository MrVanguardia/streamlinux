cmake_minimum_required(VERSION 3.20)
project(stream-linux VERSION 1.0.0 LANGUAGES CXX)

# ============================================================================
# C++20 Standard
# ============================================================================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ============================================================================
# Build Options
# ============================================================================
option(ENABLE_X11 "Enable X11 backend support" ON)
option(ENABLE_WAYLAND "Enable Wayland backend support" ON)
option(ENABLE_VAAPI "Enable VAAPI hardware encoding" ON)
option(ENABLE_NVENC "Enable NVIDIA NVENC encoding" ON)
option(ENABLE_PIPEWIRE "Enable PipeWire audio capture" ON)
option(ENABLE_PULSEAUDIO "Enable PulseAudio fallback" ON)
option(BUILD_TESTS "Build unit tests" OFF)

# ============================================================================
# Compiler Flags
# ============================================================================
add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
    -Werror=return-type
    -fPIC
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-g -O0 -DDEBUG)
else()
    add_compile_options(-O3 -DNDEBUG)
endif()

# ============================================================================
# Find Required Packages
# ============================================================================
find_package(PkgConfig REQUIRED)

# FFmpeg libraries
pkg_check_modules(AVCODEC REQUIRED libavcodec)
pkg_check_modules(AVFORMAT REQUIRED libavformat)
pkg_check_modules(AVUTIL REQUIRED libavutil)
pkg_check_modules(SWSCALE REQUIRED libswscale)
pkg_check_modules(SWRESAMPLE REQUIRED libswresample)

# X11 backend (optional)
if(ENABLE_X11)
    pkg_check_modules(XCB xcb xcb-shm xcb-randr)
    if(XCB_FOUND)
        add_compile_definitions(HAVE_X11)
    else()
        message(WARNING "XCB not found, X11 support disabled")
        set(ENABLE_X11 OFF)
    endif()
endif()

# Wayland backend (optional)
if(ENABLE_WAYLAND)
    pkg_check_modules(PIPEWIRE libpipewire-0.3)
    pkg_check_modules(GIO gio-2.0)
    if(PIPEWIRE_FOUND AND GIO_FOUND)
        add_compile_definitions(HAVE_WAYLAND)
    else()
        message(WARNING "PipeWire/GIO not found, Wayland support disabled")
        set(ENABLE_WAYLAND OFF)
    endif()
endif()

# PipeWire audio
if(ENABLE_PIPEWIRE)
    if(PIPEWIRE_FOUND)
        add_compile_definitions(HAVE_PIPEWIRE_AUDIO)
    else()
        pkg_check_modules(PIPEWIRE libpipewire-0.3)
        if(PIPEWIRE_FOUND)
            add_compile_definitions(HAVE_PIPEWIRE_AUDIO)
        else()
            message(WARNING "PipeWire not found, audio via PipeWire disabled")
            set(ENABLE_PIPEWIRE OFF)
        endif()
    endif()
endif()

# PulseAudio fallback
if(ENABLE_PULSEAUDIO)
    pkg_check_modules(PULSE libpulse)
    if(PULSE_FOUND)
        add_compile_definitions(HAVE_PULSEAUDIO)
    else()
        message(WARNING "PulseAudio not found, fallback disabled")
        set(ENABLE_PULSEAUDIO OFF)
    endif()
endif()

# VAAPI
if(ENABLE_VAAPI)
    pkg_check_modules(VAAPI libva libva-drm)
    if(VAAPI_FOUND)
        add_compile_definitions(HAVE_VAAPI)
    else()
        message(WARNING "VAAPI not found, hardware encoding via VAAPI disabled")
        set(ENABLE_VAAPI OFF)
    endif()
endif()

# Opus audio codec
pkg_check_modules(OPUS REQUIRED opus)

# Threading
find_package(Threads REQUIRED)

# ============================================================================
# Include Directories
# ============================================================================
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${AVCODEC_INCLUDE_DIRS}
    ${AVFORMAT_INCLUDE_DIRS}
    ${AVUTIL_INCLUDE_DIRS}
    ${SWSCALE_INCLUDE_DIRS}
    ${SWRESAMPLE_INCLUDE_DIRS}
    ${OPUS_INCLUDE_DIRS}
)

if(ENABLE_X11)
    include_directories(${XCB_INCLUDE_DIRS})
endif()

if(ENABLE_WAYLAND OR ENABLE_PIPEWIRE)
    include_directories(${PIPEWIRE_INCLUDE_DIRS} ${GIO_INCLUDE_DIRS})
endif()

if(ENABLE_PULSEAUDIO)
    include_directories(${PULSE_INCLUDE_DIRS})
endif()

if(ENABLE_VAAPI)
    include_directories(${VAAPI_INCLUDE_DIRS})
endif()

# ============================================================================
# Source Files
# ============================================================================
set(CORE_SOURCES
    src/main.cpp
    src/capture/display_backend.cpp
    src/capture/backend_detector.cpp
    src/audio/audio_capture.cpp
    src/encoding/video_encoder.cpp
    src/encoding/audio_encoder.cpp
    src/sync/av_synchronizer.cpp
    src/transport/webrtc_transport.cpp
    src/control/control_channel.cpp
    src/cli/cli_parser.cpp
    src/cli/config_manager.cpp
)

if(ENABLE_X11)
    list(APPEND CORE_SOURCES src/capture/x11_capture.cpp)
endif()

if(ENABLE_WAYLAND)
    list(APPEND CORE_SOURCES 
        src/capture/wayland_capture.cpp
        src/capture/pipewire_stream.cpp
    )
endif()

if(ENABLE_PIPEWIRE)
    list(APPEND CORE_SOURCES src/audio/pipewire_audio.cpp)
endif()

if(ENABLE_PULSEAUDIO)
    list(APPEND CORE_SOURCES src/audio/pulseaudio_audio.cpp)
endif()

# ============================================================================
# Main Executable
# ============================================================================
add_executable(stream-linux ${CORE_SOURCES})

target_link_libraries(stream-linux PRIVATE
    ${AVCODEC_LIBRARIES}
    ${AVFORMAT_LIBRARIES}
    ${AVUTIL_LIBRARIES}
    ${SWSCALE_LIBRARIES}
    ${SWRESAMPLE_LIBRARIES}
    ${OPUS_LIBRARIES}
    Threads::Threads
)

if(ENABLE_X11)
    target_link_libraries(stream-linux PRIVATE ${XCB_LIBRARIES})
endif()

if(ENABLE_WAYLAND OR ENABLE_PIPEWIRE)
    target_link_libraries(stream-linux PRIVATE ${PIPEWIRE_LIBRARIES} ${GIO_LIBRARIES})
endif()

if(ENABLE_PULSEAUDIO)
    target_link_libraries(stream-linux PRIVATE ${PULSE_LIBRARIES})
endif()

if(ENABLE_VAAPI)
    target_link_libraries(stream-linux PRIVATE ${VAAPI_LIBRARIES})
endif()

# ============================================================================
# Installation
# ============================================================================
install(TARGETS stream-linux RUNTIME DESTINATION bin)
install(FILES config/stream-linux.toml DESTINATION etc/stream-linux OPTIONAL)

# ============================================================================
# Configuration Summary
# ============================================================================
message(STATUS "")
message(STATUS "=== Stream Linux Build Configuration ===")
message(STATUS "  C++ Standard:     C++${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Type:       ${CMAKE_BUILD_TYPE}")
message(STATUS "  X11 Support:      ${ENABLE_X11}")
message(STATUS "  Wayland Support:  ${ENABLE_WAYLAND}")
message(STATUS "  VAAPI Encoding:   ${ENABLE_VAAPI}")
message(STATUS "  NVENC Encoding:   ${ENABLE_NVENC}")
message(STATUS "  PipeWire Audio:   ${ENABLE_PIPEWIRE}")
message(STATUS "  PulseAudio:       ${ENABLE_PULSEAUDIO}")
message(STATUS "=========================================")
message(STATUS "")
