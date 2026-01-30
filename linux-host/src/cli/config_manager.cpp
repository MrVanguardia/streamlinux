/**
 * @file config_manager.cpp
 * @brief Configuration file management (TOML format)
 */

#include "stream_linux/cli.hpp"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cstdlib>
#include <algorithm>

namespace stream_linux {

// Security: Validate config path to prevent path traversal (vuln-0004)
static Result<std::string> validate_config_path(const std::string& path) {
    if (path.empty()) {
        return path;  // Will use default
    }
    
    // Reject path traversal sequences
    if (path.find("..") != std::string::npos) {
        return std::unexpected(Error{ErrorCode::InvalidArgument,
            "Path traversal sequences not allowed in config path"});
    }
    
    // Reject paths starting with / unless in allowed directories
    std::filesystem::path fs_path(path);
    std::filesystem::path canonical;
    
    try {
        // If file exists, get canonical path
        if (std::filesystem::exists(path)) {
            canonical = std::filesystem::canonical(path);
        } else {
            // For non-existent paths, resolve parent and check
            auto parent = fs_path.parent_path();
            if (!parent.empty() && std::filesystem::exists(parent)) {
                canonical = std::filesystem::canonical(parent) / fs_path.filename();
            } else {
                canonical = fs_path;
            }
        }
    } catch (const std::exception&) {
        canonical = fs_path;
    }
    
    std::string canonical_str = canonical.string();
    
    // Allowed directories
    const char* home = std::getenv("HOME");
    std::vector<std::string> allowed_prefixes;
    
    if (home) {
        allowed_prefixes.push_back(std::string(home) + "/.config/");
        allowed_prefixes.push_back(std::string(home) + "/.local/");
    }
    allowed_prefixes.push_back("/etc/stream-linux/");
    allowed_prefixes.push_back("/tmp/stream-linux/");
    
    bool allowed = false;
    for (const auto& prefix : allowed_prefixes) {
        if (canonical_str.rfind(prefix, 0) == 0) {
            allowed = true;
            break;
        }
    }
    
    if (!allowed) {
        return std::unexpected(Error{ErrorCode::InvalidArgument,
            "Config path must be in user config directory or /etc/stream-linux/"});
    }
    
    return canonical_str;
}

std::string ConfigManager::get_default_path() {
    // Check XDG_CONFIG_HOME first
    const char* xdg_config = std::getenv("XDG_CONFIG_HOME");
    std::filesystem::path config_dir;
    
    if (xdg_config && xdg_config[0] != '\0') {
        config_dir = xdg_config;
    } else {
        // Fall back to ~/.config
        const char* home = std::getenv("HOME");
        if (home) {
            config_dir = std::filesystem::path(home) / ".config";
        } else {
            config_dir = "/etc";
        }
    }
    
    return (config_dir / "stream-linux" / "config.toml").string();
}

Result<CLIOptions> ConfigManager::load(const std::string& path) {
    // Security: Validate path before use (vuln-0004)
    auto validated = validate_config_path(path);
    if (!validated && !path.empty()) {
        return std::unexpected(validated.error());
    }
    
    std::string config_path = path.empty() ? get_default_path() : *validated;
    
    if (!std::filesystem::exists(config_path)) {
        return CLIOptions{};  // Return defaults if no config file
    }
    
    std::ifstream file(config_path);
    if (!file) {
        return std::unexpected(Error{ErrorCode::ConfigLoadFailed,
            std::format("Cannot open config file: {}", config_path)});
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    
    return parse_toml(buffer.str());
}

Result<void> ConfigManager::save(const CLIOptions& options, const std::string& path) {
    std::string config_path = path.empty() ? get_default_path() : path;
    
    // Create directory if needed
    std::filesystem::path dir = std::filesystem::path(config_path).parent_path();
    if (!dir.empty()) {
        std::filesystem::create_directories(dir);
    }
    
    std::ofstream file(config_path);
    if (!file) {
        return std::unexpected(Error{ErrorCode::ConfigSaveFailed,
            std::format("Cannot write config file: {}", config_path)});
    }
    
    file << to_toml(options);
    
    return {};
}

CLIOptions ConfigManager::merge(const CLIOptions& cli, const CLIOptions& config) {
    CLIOptions result = config;
    
    // CLI options override config
    if (cli.backend != DisplayBackend::Auto) result.backend = cli.backend;
    if (cli.monitor_id != -1) result.monitor_id = cli.monitor_id;
    if (!cli.show_cursor) result.show_cursor = false;
    
    if (cli.codec != VideoCodec::H264) result.codec = cli.codec;
    if (cli.bitrate != 0) result.bitrate = cli.bitrate;
    if (cli.fps != 60) result.fps = cli.fps;
    if (cli.quality != QualityPreset::Auto) result.quality = cli.quality;
    
    if (cli.audio_source != AudioSource::System) result.audio_source = cli.audio_source;
    if (!cli.audio_enabled) result.audio_enabled = false;
    
    if (cli.port != 0) result.port = cli.port;
    if (cli.bind_address != "0.0.0.0") result.bind_address = cli.bind_address;
    if (!cli.stun_server.empty()) result.stun_server = cli.stun_server;
    
    if (cli.verbose) result.verbose = true;
    
    // Flags are always taken from CLI
    result.show_help = cli.show_help;
    result.show_version = cli.show_version;
    result.list_monitors = cli.list_monitors;
    result.list_audio_devices = cli.list_audio_devices;
    
    return result;
}

Result<CLIOptions> ConfigManager::parse_toml(const std::string& content) {
    CLIOptions options;
    
    // Simple TOML parser (for production, use a proper TOML library)
    std::istringstream stream(content);
    std::string line;
    std::string current_section;
    
    while (std::getline(stream, line)) {
        // Remove comments
        auto comment_pos = line.find('#');
        if (comment_pos != std::string::npos) {
            line = line.substr(0, comment_pos);
        }
        
        // Trim whitespace
        auto start = line.find_first_not_of(" \t");
        if (start == std::string::npos) continue;
        auto end = line.find_last_not_of(" \t\r\n");
        line = line.substr(start, end - start + 1);
        
        if (line.empty()) continue;
        
        // Section header
        if (line.front() == '[' && line.back() == ']') {
            current_section = line.substr(1, line.size() - 2);
            continue;
        }
        
        // Key = value
        auto eq_pos = line.find('=');
        if (eq_pos == std::string::npos) continue;
        
        std::string key = line.substr(0, eq_pos);
        std::string value = line.substr(eq_pos + 1);
        
        // Trim
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        
        // Remove quotes from string values
        if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
            value = value.substr(1, value.size() - 2);
        }
        
        // Parse based on section and key
        if (current_section == "display") {
            if (key == "backend") {
                if (value == "x11") options.backend = DisplayBackend::X11;
                else if (value == "wayland") options.backend = DisplayBackend::Wayland;
            }
            else if (key == "monitor") {
                // Security: Safe integer parsing with validation (vuln-0007)
                try {
                    int val = std::stoi(value);
                    if (val < -1 || val > 255) {
                        return std::unexpected(Error{ErrorCode::InvalidConfig,
                            "monitor_id out of range (-1 to 255)"});
                    }
                    options.monitor_id = val;
                } catch (const std::exception& e) {
                    return std::unexpected(Error{ErrorCode::InvalidConfig,
                        std::format("Invalid monitor value '{}': {}", value, e.what())});
                }
            }
            else if (key == "show_cursor") {
                options.show_cursor = (value == "true");
            }
        }
        else if (current_section == "video") {
            if (key == "codec") {
                if (value == "h265") options.codec = VideoCodec::H265;
                else if (value == "vp9") options.codec = VideoCodec::VP9;
                else if (value == "av1") options.codec = VideoCodec::AV1;
            }
            else if (key == "bitrate" && value != "auto") {
                try {
                    unsigned long val = std::stoul(value);
                    if (val < 100000 || val > 100000000) {  // 100Kbps to 100Mbps
                        return std::unexpected(Error{ErrorCode::InvalidConfig,
                            "bitrate out of range (100000 to 100000000)"});
                    }
                    options.bitrate = static_cast<uint32_t>(val);
                } catch (const std::exception& e) {
                    return std::unexpected(Error{ErrorCode::InvalidConfig,
                        std::format("Invalid bitrate '{}': {}", value, e.what())});
                }
            }
            else if (key == "fps") {
                try {
                    unsigned long val = std::stoul(value);
                    if (val < 1 || val > 240) {
                        return std::unexpected(Error{ErrorCode::InvalidConfig,
                            "fps out of range (1 to 240)"});
                    }
                    options.fps = static_cast<uint32_t>(val);
                } catch (const std::exception& e) {
                    return std::unexpected(Error{ErrorCode::InvalidConfig,
                        std::format("Invalid fps '{}': {}", value, e.what())});
                }
            }
            else if (key == "quality") {
                if (value == "low") options.quality = QualityPreset::Low;
                else if (value == "medium") options.quality = QualityPreset::Medium;
                else if (value == "high") options.quality = QualityPreset::High;
                else if (value == "ultra") options.quality = QualityPreset::Ultra;
            }
        }
        else if (current_section == "audio") {
            if (key == "enabled") {
                options.audio_enabled = (value == "true");
            }
            else if (key == "source") {
                if (value == "microphone") options.audio_source = AudioSource::Microphone;
                else if (value == "mixed") options.audio_source = AudioSource::Mixed;
            }
        }
        else if (current_section == "network") {
            if (key == "bind_address") {
                options.bind_address = value;
            }
            else if (key == "port") {
                try {
                    unsigned long val = std::stoul(value);
                    if (val < 1024 || val > 65535) {
                        return std::unexpected(Error{ErrorCode::InvalidConfig,
                            "port out of range (1024 to 65535)"});
                    }
                    options.port = static_cast<uint16_t>(val);
                } catch (const std::exception& e) {
                    return std::unexpected(Error{ErrorCode::InvalidConfig,
                        std::format("Invalid port '{}': {}", value, e.what())});
                }
            }
            else if (key == "stun_server") {
                options.stun_server = value;
            }
        }
        else if (current_section == "logging") {
            if (key == "verbose") {
                options.verbose = (value == "true");
            }
        }
    }
    
    return options;
}

std::string ConfigManager::to_toml(const CLIOptions& options) {
    std::ostringstream ss;
    
    ss << "# stream-linux configuration\n\n";
    
    ss << "[display]\n";
    ss << "backend = \"" << backend_to_string(options.backend) << "\"\n";
    ss << "monitor = " << options.monitor_id << "\n";
    ss << "show_cursor = " << (options.show_cursor ? "true" : "false") << "\n\n";
    
    ss << "[video]\n";
    switch (options.codec) {
        case VideoCodec::H264: ss << "codec = \"h264\"\n"; break;
        case VideoCodec::H265: ss << "codec = \"h265\"\n"; break;
        case VideoCodec::VP9: ss << "codec = \"vp9\"\n"; break;
        case VideoCodec::AV1: ss << "codec = \"av1\"\n"; break;
    }
    if (options.bitrate == 0) {
        ss << "bitrate = \"auto\"\n";
    } else {
        ss << "bitrate = " << options.bitrate << "\n";
    }
    ss << "fps = " << options.fps << "\n";
    switch (options.quality) {
        case QualityPreset::Auto: ss << "quality = \"auto\"\n"; break;
        case QualityPreset::Low: ss << "quality = \"low\"\n"; break;
        case QualityPreset::Medium: ss << "quality = \"medium\"\n"; break;
        case QualityPreset::High: ss << "quality = \"high\"\n"; break;
        case QualityPreset::Ultra: ss << "quality = \"ultra\"\n"; break;
    }
    ss << "\n";
    
    ss << "[audio]\n";
    ss << "enabled = " << (options.audio_enabled ? "true" : "false") << "\n";
    switch (options.audio_source) {
        case AudioSource::System: ss << "source = \"system\"\n"; break;
        case AudioSource::Microphone: ss << "source = \"microphone\"\n"; break;
        case AudioSource::Mixed: ss << "source = \"mixed\"\n"; break;
    }
    ss << "\n";
    
    ss << "[network]\n";
    ss << "bind_address = \"" << options.bind_address << "\"\n";
    ss << "port = " << options.port << "\n";
    if (!options.stun_server.empty()) {
        ss << "stun_server = \"" << options.stun_server << "\"\n";
    }
    ss << "\n";
    
    ss << "[logging]\n";
    ss << "verbose = " << (options.verbose ? "true" : "false") << "\n";
    
    return ss.str();
}

} // namespace stream_linux
