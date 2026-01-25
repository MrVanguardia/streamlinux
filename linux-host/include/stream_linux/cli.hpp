#pragma once

/**
 * @file cli.hpp
 * @brief Command line interface definitions
 * 
 * Usage: stream-linux [OPTIONS]
 * 
 * Options:
 *   --backend=<auto|x11|wayland>  Display backend (default: auto)
 *   --audio=<system|mic|mixed|none>  Audio source (default: system)
 *   --codec=<h264|h265|av1>  Video codec (default: h264)
 *   --bitrate=<auto|kbps>  Video bitrate (default: auto)
 *   --fps=<30|60>  Target framerate (default: 60)
 *   --quality=<low|medium|high|ultra>  Quality preset
 *   --monitor=<id>  Monitor to capture (-1 = all)
 *   --port=<port>  Listen port (0 = auto)
 *   --config=<file>  Configuration file path
 *   --no-cursor  Hide cursor in capture
 *   --verbose  Enable verbose logging
 *   --help  Show help message
 *   --version  Show version
 */

#include "common.hpp"
#include "control_channel.hpp"
#include <string>
#include <optional>
#include <map>

namespace stream_linux {

/**
 * @brief Parsed command line arguments
 */
struct CLIOptions {
    // Display
    DisplayBackend backend = DisplayBackend::Auto;
    int32_t monitor_id = -1;
    bool show_cursor = true;
    
    // Video
    VideoCodec codec = VideoCodec::H264;
    uint32_t bitrate = 0;  // 0 = auto
    uint32_t fps = 60;
    QualityPreset quality = QualityPreset::Auto;
    HardwareEncoder hw_encoder = HardwareEncoder::None;  // None = auto-detect
    
    // Audio
    AudioSource audio_source = AudioSource::System;
    bool audio_enabled = true;
    
    // Network
    std::string bind_address = "0.0.0.0";
    uint16_t port = 0;  // 0 = auto
    std::string stun_server;
    
    // Config
    std::string config_file;
    
    // Logging
    bool verbose = false;
    
    // Actions
    bool show_help = false;
    bool show_version = false;
    bool list_monitors = false;
    bool list_audio_devices = false;
};

/**
 * @brief Command line parser
 */
class CLIParser {
public:
    /**
     * @brief Parse command line arguments
     * @param argc Argument count
     * @param argv Argument values
     * @return Parsed options or error
     */
    [[nodiscard]] static Result<CLIOptions> parse(int argc, char* argv[]);
    
    /**
     * @brief Get help text
     */
    [[nodiscard]] static std::string get_help();
    
    /**
     * @brief Get version string
     */
    [[nodiscard]] static std::string get_version();

private:
    /**
     * @brief Parse single argument
     */
    [[nodiscard]] static Result<void> parse_arg(const std::string& arg,
                                                CLIOptions& options);
    
    /**
     * @brief Parse backend string
     */
    [[nodiscard]] static Result<DisplayBackend> parse_backend(const std::string& value);
    
    /**
     * @brief Parse audio source string
     */
    [[nodiscard]] static Result<AudioSource> parse_audio_source(const std::string& value);
    
    /**
     * @brief Parse video codec string
     */
    [[nodiscard]] static Result<VideoCodec> parse_codec(const std::string& value);
    
    /**
     * @brief Parse quality preset string
     */
    [[nodiscard]] static Result<QualityPreset> parse_quality(const std::string& value);
};

/**
 * @brief Configuration file format (TOML)
 * 
 * Example stream-linux.toml:
 * 
 * [display]
 * backend = "auto"
 * monitor = -1
 * show_cursor = true
 * 
 * [video]
 * codec = "h264"
 * bitrate = "auto"
 * fps = 60
 * quality = "high"
 * hw_encoder = "auto"
 * 
 * [audio]
 * enabled = true
 * source = "system"
 * 
 * [network]
 * bind_address = "0.0.0.0"
 * port = 0
 * stun_server = ""
 * 
 * [logging]
 * verbose = false
 * log_file = ""
 */

/**
 * @brief Configuration file manager
 */
class ConfigManager {
public:
    /**
     * @brief Load configuration from file
     * @param path File path (empty = default location)
     * @return Loaded options or error
     */
    [[nodiscard]] static Result<CLIOptions> load(const std::string& path = "");
    
    /**
     * @brief Save configuration to file
     * @param options Options to save
     * @param path File path (empty = default location)
     * @return Success or error
     */
    [[nodiscard]] static Result<void> save(const CLIOptions& options,
                                           const std::string& path = "");
    
    /**
     * @brief Merge CLI options with config file (CLI takes precedence)
     */
    [[nodiscard]] static CLIOptions merge(const CLIOptions& cli,
                                          const CLIOptions& config);
    
    /**
     * @brief Get default config file path
     */
    [[nodiscard]] static std::string get_default_path();

private:
    /**
     * @brief Parse TOML content
     */
    [[nodiscard]] static Result<CLIOptions> parse_toml(const std::string& content);
    
    /**
     * @brief Generate TOML content
     */
    [[nodiscard]] static std::string to_toml(const CLIOptions& options);
};

} // namespace stream_linux
