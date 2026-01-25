#pragma once

/**
 * @file x11_capture.hpp
 * @brief X11 screen capture implementation using XCB
 * 
 * Features:
 * - Direct framebuffer access via XCB-SHM
 * - Multi-monitor support via XCB-RANDR
 * - Low latency capture
 * - Cursor capture support
 */

#include "display_backend.hpp"

#ifdef HAVE_X11

#include <xcb/xcb.h>
#include <xcb/shm.h>
#include <xcb/randr.h>
#include <atomic>
#include <thread>
#include <mutex>

namespace stream_linux {

/**
 * @brief X11 screen capture using XCB
 */
class X11Capture : public IDisplayBackend {
public:
    X11Capture();
    ~X11Capture() override;
    
    // Non-copyable, non-movable
    X11Capture(const X11Capture&) = delete;
    X11Capture& operator=(const X11Capture&) = delete;
    X11Capture(X11Capture&&) = delete;
    X11Capture& operator=(X11Capture&&) = delete;
    
    // IDisplayBackend interface
    [[nodiscard]] DisplayBackend get_type() const override { return DisplayBackend::X11; }
    [[nodiscard]] Result<void> initialize(const CaptureConfig& config) override;
    [[nodiscard]] Result<void> start() override;
    void stop() override;
    [[nodiscard]] bool is_running() const override;
    [[nodiscard]] Result<VideoFrame> capture_frame() override;
    void set_frame_callback(VideoFrameCallback callback) override;
    [[nodiscard]] Result<std::vector<MonitorInfo>> get_monitors() override;
    [[nodiscard]] std::pair<uint32_t, uint32_t> get_resolution() const override;
    [[nodiscard]] double get_actual_fps() const override;
    [[nodiscard]] Result<void> update_config(const CaptureConfig& config) override;

private:
    /**
     * @brief Initialize XCB connection
     */
    [[nodiscard]] Result<void> init_xcb();
    
    /**
     * @brief Initialize shared memory segment
     */
    [[nodiscard]] Result<void> init_shm();
    
    /**
     * @brief Query RANDR extension for monitors
     */
    [[nodiscard]] Result<void> query_monitors();
    
    /**
     * @brief Capture thread main loop
     */
    void capture_loop();
    
    /**
     * @brief Capture single frame using SHM
     */
    [[nodiscard]] Result<VideoFrame> capture_shm();
    
    /**
     * @brief Get cursor image and position
     */
    void capture_cursor(VideoFrame& frame);
    
    // XCB resources
    xcb_connection_t* m_connection = nullptr;
    xcb_screen_t* m_screen = nullptr;
    xcb_window_t m_root_window = 0;
    xcb_shm_seg_t m_shm_seg = 0;
    int m_shm_id = -1;
    uint8_t* m_shm_data = nullptr;
    size_t m_shm_size = 0;
    
    // Configuration
    CaptureConfig m_config;
    std::vector<MonitorInfo> m_monitors;
    
    // Capture state
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_initialized{false};
    std::thread m_capture_thread;
    std::mutex m_mutex;
    VideoFrameCallback m_callback;
    
    // Statistics
    std::atomic<uint64_t> m_frame_count{0};
    TimePoint m_start_time;
    TimePoint m_last_frame_time;
    std::atomic<double> m_actual_fps{0.0};
};

} // namespace stream_linux

#endif // HAVE_X11
