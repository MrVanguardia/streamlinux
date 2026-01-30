/**
 * @file display_backend.cpp
 * @brief Display backend factory implementation
 */

#include "stream_linux/display_backend.hpp"
#include "stream_linux/backend_detector.hpp"

#ifdef HAVE_X11
#include "stream_linux/x11_capture.hpp"
#endif

#ifdef HAVE_WAYLAND
#include "stream_linux/wayland_capture.hpp"
#endif

namespace stream_linux {

Result<std::unique_ptr<IDisplayBackend>> create_display_backend(DisplayBackend backend) {
    // Resolve auto backend
    auto resolved = BackendDetector::resolve(backend);
    if (!resolved) {
        return std::unexpected(resolved.error());
    }
    
    DisplayBackend actual_backend = *resolved;
    
    switch (actual_backend) {
#ifdef HAVE_X11
        case DisplayBackend::X11:
            return std::make_unique<X11Capture>();
#endif

#ifdef HAVE_WAYLAND
        case DisplayBackend::Wayland:
            return std::make_unique<WaylandCapture>();
#endif

        default:
            return std::unexpected(Error{ErrorCode::NotSupported,
                std::format("Display backend '{}' is not compiled in",
                           backend_to_string(actual_backend))});
    }
}

} // namespace stream_linux
