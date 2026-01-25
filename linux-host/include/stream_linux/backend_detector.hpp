#pragma once

/**
 * @file backend_detector.hpp
 * @brief Automatic detection of display server (X11 / Wayland)
 * 
 * Detection algorithm priority:
 * 1. XDG_SESSION_TYPE environment variable
 * 2. WAYLAND_DISPLAY environment variable
 * 3. DISPLAY environment variable
 */

#include "common.hpp"
#include <string>
#include <optional>

namespace stream_linux {

/**
 * @brief Detects the current display backend
 * 
 * Implements the detection algorithm specified in the technical documentation:
 * 1. Check XDG_SESSION_TYPE for "wayland" or "x11"
 * 2. Check WAYLAND_DISPLAY presence (indicates Wayland)
 * 3. Check DISPLAY presence (indicates X11)
 * 
 * @return Result containing detected DisplayBackend or Error
 */
class BackendDetector {
public:
    /**
     * @brief Detect the current display server
     * @return Detected backend (X11 or Wayland) or error if none found
     */
    [[nodiscard]] static Result<DisplayBackend> detect();
    
    /**
     * @brief Check if X11 is available on the system
     * @return true if X11 libraries and display are available
     */
    [[nodiscard]] static bool is_x11_available();
    
    /**
     * @brief Check if Wayland is available on the system
     * @return true if Wayland compositor is running
     */
    [[nodiscard]] static bool is_wayland_available();
    
    /**
     * @brief Resolve backend selection (handles Auto mode)
     * @param requested The requested backend (may be Auto)
     * @return Resolved concrete backend or error
     */
    [[nodiscard]] static Result<DisplayBackend> resolve(DisplayBackend requested);
    
    /**
     * @brief Get detailed information about the current session
     * @return Human-readable session information
     */
    [[nodiscard]] static std::string get_session_info();
    
private:
    /**
     * @brief Get environment variable value
     * @param name Variable name
     * @return Optional string value
     */
    [[nodiscard]] static std::optional<std::string> get_env(const char* name);
    
    /**
     * @brief Check if running under XWayland
     * @return true if X11 is running on Wayland
     */
    [[nodiscard]] static bool is_xwayland();
};

} // namespace stream_linux
