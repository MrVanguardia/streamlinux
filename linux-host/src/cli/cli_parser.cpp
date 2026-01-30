/**
 * @file cli_parser.cpp
 * @brief Command line argument parser
 */

#include "stream_linux/cli.hpp"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace stream_linux {

Result<CLIOptions> CLIParser::parse(int argc, char* argv[]) {
    CLIOptions options;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        auto result = parse_arg(arg, options);
        if (!result) {
            return std::unexpected(result.error());
        }
    }
    
    return options;
}

Result<void> CLIParser::parse_arg(const std::string& arg, CLIOptions& options) {
    // Help flags
    if (arg == "-h" || arg == "--help") {
        options.show_help = true;
        return {};
    }
    
    if (arg == "-v" || arg == "--version") {
        options.show_version = true;
        return {};
    }
    
    if (arg == "--verbose") {
        options.verbose = true;
        return {};
    }
    
    if (arg == "--list-monitors") {
        options.list_monitors = true;
        return {};
    }
    
    if (arg == "--list-audio") {
        options.list_audio_devices = true;
        return {};
    }
    
    if (arg == "--no-cursor") {
        options.show_cursor = false;
        return {};
    }
    
    if (arg == "--no-audio") {
        options.audio_enabled = false;
        return {};
    }
    
    // Key=value arguments
    auto eq_pos = arg.find('=');
    if (eq_pos == std::string::npos || !arg.starts_with("--")) {
        return std::unexpected(Error{ErrorCode::InvalidArgument,
            std::format("Invalid argument: {}", arg)});
    }
    
    std::string key = arg.substr(2, eq_pos - 2);
    std::string value = arg.substr(eq_pos + 1);
    
    // Transform to lowercase
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    
    if (key == "backend") {
        auto result = parse_backend(value);
        if (!result) return std::unexpected(result.error());
        options.backend = *result;
    }
    else if (key == "audio") {
        auto result = parse_audio_source(value);
        if (!result) return std::unexpected(result.error());
        options.audio_source = *result;
    }
    else if (key == "codec") {
        auto result = parse_codec(value);
        if (!result) return std::unexpected(result.error());
        options.codec = *result;
    }
    else if (key == "quality") {
        auto result = parse_quality(value);
        if (!result) return std::unexpected(result.error());
        options.quality = *result;
    }
    else if (key == "bitrate") {
        if (value == "auto") {
            options.bitrate = 0;
        } else {
            try {
                options.bitrate = std::stoul(value) * 1000;  // kbps to bps
            } catch (...) {
                return std::unexpected(Error{ErrorCode::InvalidArgument,
                    std::format("Invalid bitrate: {}", value)});
            }
        }
    }
    else if (key == "fps") {
        try {
            options.fps = std::stoul(value);
        } catch (...) {
            return std::unexpected(Error{ErrorCode::InvalidArgument,
                std::format("Invalid fps: {}", value)});
        }
    }
    else if (key == "monitor") {
        try {
            options.monitor_id = std::stoi(value);
        } catch (...) {
            return std::unexpected(Error{ErrorCode::InvalidArgument,
                std::format("Invalid monitor id: {}", value)});
        }
    }
    else if (key == "port") {
        try {
            options.port = static_cast<uint16_t>(std::stoul(value));
        } catch (...) {
            return std::unexpected(Error{ErrorCode::InvalidArgument,
                std::format("Invalid port: {}", value)});
        }
    }
    else if (key == "bind") {
        options.bind_address = value;
    }
    else if (key == "stun") {
        options.stun_server = value;
    }
    else if (key == "config") {
        options.config_file = value;
    }
    else {
        return std::unexpected(Error{ErrorCode::InvalidArgument,
            std::format("Unknown option: --{}", key)});
    }
    
    return {};
}

Result<DisplayBackend> CLIParser::parse_backend(const std::string& value) {
    if (value == "auto") return DisplayBackend::Auto;
    if (value == "x11") return DisplayBackend::X11;
    if (value == "wayland") return DisplayBackend::Wayland;
    
    return std::unexpected(Error{ErrorCode::InvalidArgument,
        std::format("Invalid backend: {}. Use: auto, x11, wayland", value)});
}

Result<AudioSource> CLIParser::parse_audio_source(const std::string& value) {
    if (value == "system") return AudioSource::System;
    if (value == "mic" || value == "microphone") return AudioSource::Microphone;
    if (value == "mixed" || value == "both") return AudioSource::Mixed;
    if (value == "none") return AudioSource::System;  // Will be disabled via flag
    
    return std::unexpected(Error{ErrorCode::InvalidArgument,
        std::format("Invalid audio source: {}. Use: system, mic, mixed, none", value)});
}

Result<VideoCodec> CLIParser::parse_codec(const std::string& value) {
    if (value == "h264" || value == "avc") return VideoCodec::H264;
    if (value == "h265" || value == "hevc") return VideoCodec::H265;
    if (value == "vp9") return VideoCodec::VP9;
    if (value == "av1") return VideoCodec::AV1;
    
    return std::unexpected(Error{ErrorCode::InvalidArgument,
        std::format("Invalid codec: {}. Use: h264, h265, vp9, av1", value)});
}

Result<QualityPreset> CLIParser::parse_quality(const std::string& value) {
    if (value == "auto") return QualityPreset::Auto;
    if (value == "low") return QualityPreset::Low;
    if (value == "medium") return QualityPreset::Medium;
    if (value == "high") return QualityPreset::High;
    if (value == "ultra") return QualityPreset::Ultra;
    
    return std::unexpected(Error{ErrorCode::InvalidArgument,
        std::format("Invalid quality: {}. Use: auto, low, medium, high, ultra", value)});
}

std::string CLIParser::get_help() {
    std::ostringstream ss;
    
    ss << "stream-linux - Screen and audio streaming from Linux to Android\n"
       << "\n"
       << "Usage: stream-linux [OPTIONS]\n"
       << "\n"
       << "Display Options:\n"
       << "  --backend=<auto|x11|wayland>  Display backend (default: auto)\n"
       << "  --monitor=<id>                Monitor to capture (-1 = all)\n"
       << "  --no-cursor                   Hide cursor in capture\n"
       << "\n"
       << "Video Options:\n"
       << "  --codec=<h264|h265|av1>       Video codec (default: h264)\n"
       << "  --bitrate=<auto|kbps>         Video bitrate (default: auto)\n"
       << "  --fps=<30|60>                 Target framerate (default: 60)\n"
       << "  --quality=<preset>            Quality preset: low, medium, high, ultra\n"
       << "\n"
       << "Audio Options:\n"
       << "  --audio=<system|mic|mixed>    Audio source (default: system)\n"
       << "  --no-audio                    Disable audio capture\n"
       << "\n"
       << "Network Options:\n"
       << "  --port=<port>                 Listen port (0 = auto)\n"
       << "  --bind=<address>              Bind address (default: 0.0.0.0)\n"
       << "  --stun=<server>               STUN server for NAT traversal\n"
       << "\n"
       << "Other Options:\n"
       << "  --config=<file>               Configuration file path\n"
       << "  --list-monitors               List available monitors\n"
       << "  --list-audio                  List available audio devices\n"
       << "  --verbose                     Enable verbose logging\n"
       << "  -h, --help                    Show this help message\n"
       << "  -v, --version                 Show version\n"
       << "\n"
       << "Examples:\n"
       << "  stream-linux --backend=auto --audio=system\n"
       << "  stream-linux --backend=wayland --codec=h264 --bitrate=5000\n"
       << "  stream-linux --monitor=0 --fps=60 --quality=high\n";
    
    return ss.str();
}

std::string CLIParser::get_version() {
    std::ostringstream ss;
    ss << "stream-linux " << VERSION << "\n"
       << "Screen and audio streaming for Linux\n"
       << "\n"
       << "Compiled with:\n"
#ifdef HAVE_X11
       << "  - X11 support\n"
#endif
#ifdef HAVE_WAYLAND
       << "  - Wayland support\n"
#endif
#ifdef HAVE_VAAPI
       << "  - VAAPI hardware encoding\n"
#endif
#ifdef HAVE_PIPEWIRE_AUDIO
       << "  - PipeWire audio\n"
#endif
#ifdef HAVE_PULSEAUDIO
       << "  - PulseAudio audio\n"
#endif
       ;
    
    return ss.str();
}

} // namespace stream_linux
