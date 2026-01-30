#pragma once

/**
 * @file common.hpp
 * @brief Common definitions, types, and utilities for stream-linux
 */

#include <cstdint>
#include <chrono>
#include <memory>
#include <string>
#include <string_view>
#include <optional>
#include <expected>
#include <functional>
#include <span>
#include <vector>
#include <array>
#include <format>

namespace stream_linux {

// ============================================================================
// Version Information
// ============================================================================
constexpr std::string_view VERSION = "1.0.0";
constexpr uint32_t VERSION_MAJOR = 1;
constexpr uint32_t VERSION_MINOR = 0;
constexpr uint32_t VERSION_PATCH = 0;

// ============================================================================
// Time Types
// ============================================================================
using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;
using Duration = Clock::duration;
using Microseconds = std::chrono::microseconds;
using Milliseconds = std::chrono::milliseconds;
using Nanoseconds = std::chrono::nanoseconds;

// Presentation timestamp in microseconds
using PTS = int64_t;

// ============================================================================
// Error Handling
// ============================================================================
enum class ErrorCode {
    Success = 0,
    
    // Generic errors
    Unknown = 1,
    InvalidArgument,
    NotSupported,
    NotInitialized,
    AlreadyInitialized,
    OutOfMemory,
    Timeout,
    
    // Backend detection errors
    BackendDetectionFailed = 100,
    X11NotAvailable,
    WaylandNotAvailable,
    NoDisplayServerFound,
    
    // Capture errors
    CaptureInitFailed = 200,
    CaptureStartFailed,
    CaptureReadFailed,
    FrameConversionFailed,
    PermissionDenied,
    PortalRequestFailed,
    
    // Audio errors
    AudioInitFailed = 300,
    AudioCaptureStartFailed,
    AudioReadFailed,
    NoAudioDeviceFound,
    
    // Encoding errors
    EncoderInitFailed = 400,
    EncoderNotFound,
    HardwareEncoderFailed,
    EncodingFailed,
    
    // Transport errors
    TransportInitFailed = 500,
    ConnectionFailed,
    SendFailed,
    ReceiveFailed,
    PeerDisconnected,
    
    // Configuration errors
    ConfigLoadFailed = 600,
    ConfigSaveFailed,
    InvalidConfig,
};

[[nodiscard]] constexpr std::string_view error_to_string(ErrorCode code) {
    switch (code) {
        case ErrorCode::Success: return "Success";
        case ErrorCode::Unknown: return "Unknown error";
        case ErrorCode::InvalidArgument: return "Invalid argument";
        case ErrorCode::NotSupported: return "Operation not supported";
        case ErrorCode::NotInitialized: return "Not initialized";
        case ErrorCode::AlreadyInitialized: return "Already initialized";
        case ErrorCode::OutOfMemory: return "Out of memory";
        case ErrorCode::Timeout: return "Operation timed out";
        case ErrorCode::BackendDetectionFailed: return "Backend detection failed";
        case ErrorCode::X11NotAvailable: return "X11 not available";
        case ErrorCode::WaylandNotAvailable: return "Wayland not available";
        case ErrorCode::NoDisplayServerFound: return "No display server found";
        case ErrorCode::CaptureInitFailed: return "Capture initialization failed";
        case ErrorCode::CaptureStartFailed: return "Capture start failed";
        case ErrorCode::CaptureReadFailed: return "Capture read failed";
        case ErrorCode::FrameConversionFailed: return "Frame conversion failed";
        case ErrorCode::PermissionDenied: return "Permission denied";
        case ErrorCode::PortalRequestFailed: return "Portal request failed";
        case ErrorCode::AudioInitFailed: return "Audio initialization failed";
        case ErrorCode::AudioCaptureStartFailed: return "Audio capture start failed";
        case ErrorCode::AudioReadFailed: return "Audio read failed";
        case ErrorCode::NoAudioDeviceFound: return "No audio device found";
        case ErrorCode::EncoderInitFailed: return "Encoder initialization failed";
        case ErrorCode::EncoderNotFound: return "Encoder not found";
        case ErrorCode::HardwareEncoderFailed: return "Hardware encoder failed";
        case ErrorCode::EncodingFailed: return "Encoding failed";
        case ErrorCode::TransportInitFailed: return "Transport initialization failed";
        case ErrorCode::ConnectionFailed: return "Connection failed";
        case ErrorCode::SendFailed: return "Send failed";
        case ErrorCode::ReceiveFailed: return "Receive failed";
        case ErrorCode::PeerDisconnected: return "Peer disconnected";
        case ErrorCode::ConfigLoadFailed: return "Configuration load failed";
        case ErrorCode::ConfigSaveFailed: return "Configuration save failed";
        case ErrorCode::InvalidConfig: return "Invalid configuration";
        default: return "Unknown error code";
    }
}

struct Error {
    ErrorCode code;
    std::string message;
    
    Error(ErrorCode c) : code(c), message(std::string(error_to_string(c))) {}
    Error(ErrorCode c, std::string msg) : code(c), message(std::move(msg)) {}
    
    [[nodiscard]] bool is_success() const { return code == ErrorCode::Success; }
    [[nodiscard]] operator bool() const { return !is_success(); }
};

template<typename T>
using Result = std::expected<T, Error>;

// ============================================================================
// Display Backend
// ============================================================================
enum class DisplayBackend {
    Auto,
    X11,
    Wayland
};

[[nodiscard]] constexpr std::string_view backend_to_string(DisplayBackend backend) {
    switch (backend) {
        case DisplayBackend::Auto: return "auto";
        case DisplayBackend::X11: return "x11";
        case DisplayBackend::Wayland: return "wayland";
        default: return "unknown";
    }
}

// ============================================================================
// Video Types
// ============================================================================
enum class PixelFormat {
    Unknown,
    RGB24,
    RGBA32,
    BGR24,
    BGRA32,
    NV12,
    YUV420P,
    YUV444P
};

enum class VideoCodec {
    H264,
    H265,
    AV1,
    VP9
};

enum class HardwareEncoder {
    None,     // Software encoding
    VAAPI,    // Intel/AMD
    NVENC,    // NVIDIA
    AMF,      // AMD
    QSV       // Intel Quick Sync
};

struct VideoFrame {
    std::vector<uint8_t> data;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t stride = 0;
    PixelFormat format = PixelFormat::Unknown;
    PTS pts = 0;
    bool keyframe = false;
};

struct EncodedVideoFrame {
    std::vector<uint8_t> data;
    PTS pts = 0;
    PTS dts = 0;
    bool keyframe = false;
};

struct VideoConfig {
    uint32_t width = 1920;
    uint32_t height = 1080;
    uint32_t fps = 60;
    uint32_t bitrate = 5'000'000;  // 5 Mbps
    VideoCodec codec = VideoCodec::H264;
    HardwareEncoder hw_encoder = HardwareEncoder::None;
    uint32_t gop_size = 60;        // Keyframe interval
    uint32_t b_frames = 0;         // No B-frames for low latency
};

// ============================================================================
// Audio Types
// ============================================================================
enum class AudioSource {
    System,     // System audio output
    Microphone, // Microphone input
    Mixed       // Both mixed
};

enum class AudioCodec {
    Opus,
    AAC
};

struct AudioFrame {
    std::vector<float> data;
    uint32_t sample_rate = 48000;
    uint32_t channels = 2;
    uint32_t samples_per_channel = 0;
    PTS pts = 0;
};

struct EncodedAudioFrame {
    std::vector<uint8_t> data;
    PTS pts = 0;
};

struct AudioConfig {
    uint32_t sample_rate = 48000;
    uint32_t channels = 2;
    uint32_t bitrate = 128'000;  // 128 kbps
    AudioCodec codec = AudioCodec::Opus;
    AudioSource source = AudioSource::System;
    uint32_t frame_size_ms = 20;  // 20ms frames for Opus
};

// ============================================================================
// Transport Types
// ============================================================================
struct TransportConfig {
    std::string local_address = "0.0.0.0";
    uint16_t port = 0;  // 0 = auto-assign
    bool enable_dtls = true;
    std::string stun_server;  // Optional STUN server
    std::string turn_server;  // Optional TURN server
};

// ============================================================================
// Callbacks
// ============================================================================
using VideoFrameCallback = std::function<void(const VideoFrame&)>;
using AudioFrameCallback = std::function<void(const AudioFrame&)>;
using EncodedVideoCallback = std::function<void(const EncodedVideoFrame&)>;
using EncodedAudioCallback = std::function<void(const EncodedAudioFrame&)>;
using ErrorCallback = std::function<void(const Error&)>;

// ============================================================================
// Utility Functions
// ============================================================================
[[nodiscard]] inline PTS get_monotonic_pts() {
    return std::chrono::duration_cast<Microseconds>(
        Clock::now().time_since_epoch()
    ).count();
}

template<typename... Args>
[[nodiscard]] std::string format_error(std::format_string<Args...> fmt, Args&&... args) {
    return std::format(fmt, std::forward<Args>(args)...);
}

} // namespace stream_linux
