#pragma once

/**
 * @file display_backend.hpp
 * @brief Abstract interface for screen capture backends
 */

#include "common.hpp"
#include <vector>
#include <memory>

namespace stream_linux {

/**
 * @brief Monitor/display information
 */
struct MonitorInfo {
    std::string name;
    std::string description;
    uint32_t x = 0;
    uint32_t y = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t refresh_rate = 60;  // Hz
    bool primary = false;
    int32_t id = -1;             // Backend-specific ID
};

/**
 * @brief Capture region specification
 */
struct CaptureRegion {
    uint32_t x = 0;
    uint32_t y = 0;
    uint32_t width = 0;   // 0 = full width
    uint32_t height = 0;  // 0 = full height
    int32_t monitor_id = -1;  // -1 = all monitors / full screen
};

/**
 * @brief Configuration for display capture
 */
struct CaptureConfig {
    CaptureRegion region;
    uint32_t target_fps = 60;
    bool show_cursor = true;
    bool follow_cursor = false;
    PixelFormat preferred_format = PixelFormat::NV12;
};

/**
 * @brief Abstract base class for display capture backends
 */
class IDisplayBackend {
public:
    virtual ~IDisplayBackend() = default;
    
    /**
     * @brief Get the backend type
     */
    [[nodiscard]] virtual DisplayBackend get_type() const = 0;
    
    /**
     * @brief Initialize the capture backend
     * @param config Capture configuration
     * @return Success or error
     */
    [[nodiscard]] virtual Result<void> initialize(const CaptureConfig& config) = 0;
    
    /**
     * @brief Start capturing frames
     * @return Success or error
     */
    [[nodiscard]] virtual Result<void> start() = 0;
    
    /**
     * @brief Stop capturing frames
     */
    virtual void stop() = 0;
    
    /**
     * @brief Check if capture is running
     */
    [[nodiscard]] virtual bool is_running() const = 0;
    
    /**
     * @brief Capture a single frame (blocking)
     * @return Captured frame or error
     */
    [[nodiscard]] virtual Result<VideoFrame> capture_frame() = 0;
    
    /**
     * @brief Set callback for received frames (async mode)
     * @param callback Function to call with each new frame
     */
    virtual void set_frame_callback(VideoFrameCallback callback) = 0;
    
    /**
     * @brief Get list of available monitors
     * @return Vector of monitor information
     */
    [[nodiscard]] virtual Result<std::vector<MonitorInfo>> get_monitors() = 0;
    
    /**
     * @brief Get current capture resolution
     */
    [[nodiscard]] virtual std::pair<uint32_t, uint32_t> get_resolution() const = 0;
    
    /**
     * @brief Get actual capture FPS
     */
    [[nodiscard]] virtual double get_actual_fps() const = 0;
    
    /**
     * @brief Update capture configuration
     * @param config New configuration
     * @return Success or error
     */
    [[nodiscard]] virtual Result<void> update_config(const CaptureConfig& config) = 0;
};

/**
 * @brief Factory function to create appropriate backend
 * @param backend Backend type (Auto will detect automatically)
 * @return Created backend instance or error
 */
[[nodiscard]] Result<std::unique_ptr<IDisplayBackend>> create_display_backend(
    DisplayBackend backend = DisplayBackend::Auto
);

} // namespace stream_linux
