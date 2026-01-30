/**
 * @file backend_detector.cpp
 * @brief Implementation of display backend detection
 */

#include "stream_linux/backend_detector.hpp"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

#ifdef HAVE_X11
#include <xcb/xcb.h>
#endif

namespace stream_linux {

std::optional<std::string> BackendDetector::get_env(const char* name) {
    const char* value = std::getenv(name);
    if (value && value[0] != '\0') {
        return std::string(value);
    }
    return std::nullopt;
}

Result<DisplayBackend> BackendDetector::detect() {
    // Step 1: Check XDG_SESSION_TYPE (most reliable)
    if (auto session_type = get_env("XDG_SESSION_TYPE")) {
        if (*session_type == "wayland") {
            return DisplayBackend::Wayland;
        }
        if (*session_type == "x11") {
            return DisplayBackend::X11;
        }
        // Might be "tty" or other - continue checking
    }
    
    // Step 2: Check WAYLAND_DISPLAY
    if (auto wayland_display = get_env("WAYLAND_DISPLAY")) {
        // Verify Wayland socket exists
        std::string runtime_dir;
        if (auto xdg_runtime = get_env("XDG_RUNTIME_DIR")) {
            runtime_dir = *xdg_runtime;
        } else {
            runtime_dir = "/run/user/" + std::to_string(getuid());
        }
        
        std::filesystem::path socket_path = runtime_dir;
        socket_path /= *wayland_display;
        
        if (std::filesystem::exists(socket_path)) {
            return DisplayBackend::Wayland;
        }
    }
    
    // Step 3: Check DISPLAY for X11
    if (auto display = get_env("DISPLAY")) {
        // Could be X11 or XWayland
        if (is_xwayland()) {
            // Running X11 app on Wayland - prefer Wayland capture
#ifdef HAVE_WAYLAND
            return DisplayBackend::Wayland;
#else
            return DisplayBackend::X11;
#endif
        }
        return DisplayBackend::X11;
    }
    
    return std::unexpected(Error{ErrorCode::NoDisplayServerFound,
        "No display server detected. Set DISPLAY or WAYLAND_DISPLAY environment variable."});
}

bool BackendDetector::is_x11_available() {
#ifdef HAVE_X11
    auto display = get_env("DISPLAY");
    if (!display) {
        return false;
    }
    
    // Try to connect to X server
    int screen_num;
    xcb_connection_t* conn = xcb_connect(nullptr, &screen_num);
    if (!conn || xcb_connection_has_error(conn)) {
        if (conn) xcb_disconnect(conn);
        return false;
    }
    xcb_disconnect(conn);
    return true;
#else
    return false;
#endif
}

bool BackendDetector::is_wayland_available() {
#ifdef HAVE_WAYLAND
    auto wayland_display = get_env("WAYLAND_DISPLAY");
    if (!wayland_display) {
        return false;
    }
    
    // Check if socket exists
    std::string runtime_dir;
    if (auto xdg_runtime = get_env("XDG_RUNTIME_DIR")) {
        runtime_dir = *xdg_runtime;
    } else {
        return false;
    }
    
    std::filesystem::path socket_path = runtime_dir;
    socket_path /= *wayland_display;
    
    return std::filesystem::exists(socket_path);
#else
    return false;
#endif
}

bool BackendDetector::is_xwayland() {
    // Check if running under XWayland
    // XWayland sets WAYLAND_DISPLAY even when providing X11 compatibility
    auto wayland_display = get_env("WAYLAND_DISPLAY");
    auto display = get_env("DISPLAY");
    
    if (wayland_display && display) {
        // Both set - likely XWayland
        // Additional check: look for XWayland process or check X11 root window property
        
        // Simple heuristic: if XDG_SESSION_TYPE is wayland but DISPLAY is set,
        // we're likely in XWayland
        if (auto session_type = get_env("XDG_SESSION_TYPE")) {
            if (*session_type == "wayland") {
                return true;
            }
        }
    }
    
    return false;
}

Result<DisplayBackend> BackendDetector::resolve(DisplayBackend requested) {
    if (requested != DisplayBackend::Auto) {
        // Verify requested backend is available
        switch (requested) {
            case DisplayBackend::X11:
                if (!is_x11_available()) {
                    return std::unexpected(Error{ErrorCode::X11NotAvailable,
                        "X11 backend requested but not available"});
                }
                return DisplayBackend::X11;
                
            case DisplayBackend::Wayland:
                if (!is_wayland_available()) {
                    return std::unexpected(Error{ErrorCode::WaylandNotAvailable,
                        "Wayland backend requested but not available"});
                }
                return DisplayBackend::Wayland;
                
            default:
                break;
        }
    }
    
    // Auto-detect
    return detect();
}

std::string BackendDetector::get_session_info() {
    std::ostringstream info;
    
    info << "Session Information:\n";
    
    if (auto val = get_env("XDG_SESSION_TYPE")) {
        info << "  XDG_SESSION_TYPE: " << *val << "\n";
    }
    if (auto val = get_env("WAYLAND_DISPLAY")) {
        info << "  WAYLAND_DISPLAY: " << *val << "\n";
    }
    if (auto val = get_env("DISPLAY")) {
        info << "  DISPLAY: " << *val << "\n";
    }
    if (auto val = get_env("XDG_CURRENT_DESKTOP")) {
        info << "  XDG_CURRENT_DESKTOP: " << *val << "\n";
    }
    if (auto val = get_env("DESKTOP_SESSION")) {
        info << "  DESKTOP_SESSION: " << *val << "\n";
    }
    
    info << "\nAvailability:\n";
    info << "  X11: " << (is_x11_available() ? "Yes" : "No") << "\n";
    info << "  Wayland: " << (is_wayland_available() ? "Yes" : "No") << "\n";
    info << "  XWayland: " << (is_xwayland() ? "Yes" : "No") << "\n";
    
    auto detected = detect();
    if (detected) {
        info << "\nDetected Backend: " << backend_to_string(*detected) << "\n";
    } else {
        info << "\nDetected Backend: None (error: " << detected.error().message << ")\n";
    }
    
    return info.str();
}

} // namespace stream_linux
