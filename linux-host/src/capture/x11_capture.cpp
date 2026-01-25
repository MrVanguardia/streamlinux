/**
 * @file x11_capture.cpp
 * @brief X11 screen capture implementation using XCB-SHM
 */

#ifdef HAVE_X11

#include "stream_linux/x11_capture.hpp"
#include <sys/shm.h>
#include <sys/ipc.h>
#include <cstring>
#include <algorithm>

namespace stream_linux {

X11Capture::X11Capture() = default;

X11Capture::~X11Capture() {
    stop();
    
    // Cleanup shared memory
    if (m_shm_data) {
        shmdt(m_shm_data);
        m_shm_data = nullptr;
    }
    if (m_shm_id >= 0) {
        shmctl(m_shm_id, IPC_RMID, nullptr);
        m_shm_id = -1;
    }
    
    // Cleanup XCB
    if (m_connection) {
        if (m_shm_seg) {
            xcb_shm_detach(m_connection, m_shm_seg);
        }
        xcb_disconnect(m_connection);
        m_connection = nullptr;
    }
}

Result<void> X11Capture::init_xcb() {
    int screen_num;
    m_connection = xcb_connect(nullptr, &screen_num);
    
    if (!m_connection || xcb_connection_has_error(m_connection)) {
        return std::unexpected(Error{ErrorCode::CaptureInitFailed,
            "Failed to connect to X server"});
    }
    
    // Get screen
    const xcb_setup_t* setup = xcb_get_setup(m_connection);
    xcb_screen_iterator_t iter = xcb_setup_roots_iterator(setup);
    for (int i = 0; i < screen_num; ++i) {
        xcb_screen_next(&iter);
    }
    m_screen = iter.data;
    
    if (!m_screen) {
        return std::unexpected(Error{ErrorCode::CaptureInitFailed,
            "Failed to get X screen"});
    }
    
    m_root_window = m_screen->root;
    
    // Check SHM extension
    auto shm_query = xcb_shm_query_version(m_connection);
    auto shm_reply = xcb_shm_query_version_reply(m_connection, shm_query, nullptr);
    
    if (!shm_reply) {
        return std::unexpected(Error{ErrorCode::CaptureInitFailed,
            "X server does not support SHM extension"});
    }
    free(shm_reply);
    
    return {};
}

Result<void> X11Capture::init_shm() {
    uint32_t width = m_config.region.width;
    uint32_t height = m_config.region.height;
    
    if (width == 0) width = m_screen->width_in_pixels;
    if (height == 0) height = m_screen->height_in_pixels;
    
    // 4 bytes per pixel (BGRA)
    m_shm_size = width * height * 4;
    
    // Create shared memory segment
    m_shm_id = shmget(IPC_PRIVATE, m_shm_size, IPC_CREAT | 0600);
    if (m_shm_id < 0) {
        return std::unexpected(Error{ErrorCode::CaptureInitFailed,
            "Failed to create shared memory segment"});
    }
    
    m_shm_data = static_cast<uint8_t*>(shmat(m_shm_id, nullptr, 0));
    if (m_shm_data == reinterpret_cast<void*>(-1)) {
        m_shm_data = nullptr;
        return std::unexpected(Error{ErrorCode::CaptureInitFailed,
            "Failed to attach shared memory"});
    }
    
    // Attach to X server
    m_shm_seg = xcb_generate_id(m_connection);
    xcb_shm_attach(m_connection, m_shm_seg, m_shm_id, 0);
    xcb_flush(m_connection);
    
    return {};
}

Result<void> X11Capture::query_monitors() {
    m_monitors.clear();
    
    // Check RANDR extension
    auto randr_query = xcb_randr_query_version(m_connection, 1, 5);
    auto randr_reply = xcb_randr_query_version_reply(m_connection, randr_query, nullptr);
    
    if (!randr_reply) {
        // No RANDR - single monitor fallback
        MonitorInfo monitor;
        monitor.name = "default";
        monitor.description = "Primary Display";
        monitor.width = m_screen->width_in_pixels;
        monitor.height = m_screen->height_in_pixels;
        monitor.primary = true;
        monitor.id = 0;
        m_monitors.push_back(monitor);
        return {};
    }
    free(randr_reply);
    
    // Get screen resources
    auto res_cookie = xcb_randr_get_screen_resources_current(m_connection, m_root_window);
    auto res_reply = xcb_randr_get_screen_resources_current_reply(m_connection, res_cookie, nullptr);
    
    if (!res_reply) {
        return std::unexpected(Error{ErrorCode::CaptureInitFailed,
            "Failed to get screen resources"});
    }
    
    // Get outputs
    int num_outputs = xcb_randr_get_screen_resources_current_outputs_length(res_reply);
    xcb_randr_output_t* outputs = xcb_randr_get_screen_resources_current_outputs(res_reply);
    
    // Get primary output
    auto primary_cookie = xcb_randr_get_output_primary(m_connection, m_root_window);
    auto primary_reply = xcb_randr_get_output_primary_reply(m_connection, primary_cookie, nullptr);
    xcb_randr_output_t primary_output = primary_reply ? primary_reply->output : XCB_NONE;
    if (primary_reply) free(primary_reply);
    
    for (int i = 0; i < num_outputs; ++i) {
        auto info_cookie = xcb_randr_get_output_info(m_connection, outputs[i], res_reply->timestamp);
        auto info_reply = xcb_randr_get_output_info_reply(m_connection, info_cookie, nullptr);
        
        if (!info_reply || info_reply->crtc == XCB_NONE) {
            if (info_reply) free(info_reply);
            continue;
        }
        
        // Get CRTC info for geometry
        auto crtc_cookie = xcb_randr_get_crtc_info(m_connection, info_reply->crtc, res_reply->timestamp);
        auto crtc_reply = xcb_randr_get_crtc_info_reply(m_connection, crtc_cookie, nullptr);
        
        if (crtc_reply && crtc_reply->width > 0 && crtc_reply->height > 0) {
            MonitorInfo monitor;
            
            // Get name
            int name_len = xcb_randr_get_output_info_name_length(info_reply);
            uint8_t* name = xcb_randr_get_output_info_name(info_reply);
            monitor.name = std::string(reinterpret_cast<char*>(name), name_len);
            
            monitor.x = crtc_reply->x;
            monitor.y = crtc_reply->y;
            monitor.width = crtc_reply->width;
            monitor.height = crtc_reply->height;
            monitor.primary = (outputs[i] == primary_output);
            monitor.id = static_cast<int32_t>(outputs[i]);
            monitor.description = monitor.name + " (" + 
                                  std::to_string(monitor.width) + "x" +
                                  std::to_string(monitor.height) + ")";
            
            m_monitors.push_back(monitor);
            free(crtc_reply);
        }
        
        free(info_reply);
    }
    
    free(res_reply);
    
    // Sort by position (left to right, top to bottom)
    std::sort(m_monitors.begin(), m_monitors.end(), 
              [](const MonitorInfo& a, const MonitorInfo& b) {
                  if (a.y != b.y) return a.y < b.y;
                  return a.x < b.x;
              });
    
    return {};
}

Result<void> X11Capture::initialize(const CaptureConfig& config) {
    if (m_initialized) {
        return std::unexpected(Error{ErrorCode::AlreadyInitialized});
    }
    
    m_config = config;
    
    // Initialize XCB connection
    if (auto result = init_xcb(); !result) {
        return result;
    }
    
    // Query monitors
    if (auto result = query_monitors(); !result) {
        return result;
    }
    
    // Resolve capture region
    if (m_config.region.monitor_id >= 0) {
        // Find specific monitor
        auto it = std::find_if(m_monitors.begin(), m_monitors.end(),
                              [this](const MonitorInfo& m) {
                                  return m.id == m_config.region.monitor_id;
                              });
        if (it != m_monitors.end()) {
            m_config.region.x = it->x;
            m_config.region.y = it->y;
            m_config.region.width = it->width;
            m_config.region.height = it->height;
        }
    }
    
    if (m_config.region.width == 0) {
        m_config.region.width = m_screen->width_in_pixels;
    }
    if (m_config.region.height == 0) {
        m_config.region.height = m_screen->height_in_pixels;
    }
    
    // Initialize shared memory
    if (auto result = init_shm(); !result) {
        return result;
    }
    
    m_initialized = true;
    return {};
}

Result<void> X11Capture::start() {
    if (!m_initialized) {
        return std::unexpected(Error{ErrorCode::NotInitialized});
    }
    if (m_running) {
        return std::unexpected(Error{ErrorCode::AlreadyInitialized,
            "Capture already running"});
    }
    
    m_running = true;
    m_frame_count = 0;
    m_start_time = Clock::now();
    m_last_frame_time = m_start_time;
    
    // Start capture thread if callback is set
    if (m_callback) {
        m_capture_thread = std::thread(&X11Capture::capture_loop, this);
    }
    
    return {};
}

void X11Capture::stop() {
    m_running = false;
    
    if (m_capture_thread.joinable()) {
        m_capture_thread.join();
    }
}

bool X11Capture::is_running() const {
    return m_running;
}

Result<VideoFrame> X11Capture::capture_shm() {
    VideoFrame frame;
    
    uint32_t x = m_config.region.x;
    uint32_t y = m_config.region.y;
    uint32_t width = m_config.region.width;
    uint32_t height = m_config.region.height;
    
    // Capture using SHM
    auto cookie = xcb_shm_get_image(
        m_connection,
        m_root_window,
        static_cast<int16_t>(x),
        static_cast<int16_t>(y),
        static_cast<uint16_t>(width),
        static_cast<uint16_t>(height),
        ~0,  // All planes
        XCB_IMAGE_FORMAT_Z_PIXMAP,
        m_shm_seg,
        0    // Offset
    );
    
    auto reply = xcb_shm_get_image_reply(m_connection, cookie, nullptr);
    if (!reply) {
        return std::unexpected(Error{ErrorCode::CaptureReadFailed,
            "Failed to capture screen"});
    }
    
    // Copy data to frame
    frame.width = width;
    frame.height = height;
    frame.stride = width * 4;
    frame.format = PixelFormat::BGRA32;
    frame.pts = get_monotonic_pts();
    frame.keyframe = false;
    
    size_t data_size = width * height * 4;
    frame.data.resize(data_size);
    std::memcpy(frame.data.data(), m_shm_data, data_size);
    
    free(reply);
    
    // Capture cursor if enabled
    if (m_config.show_cursor) {
        capture_cursor(frame);
    }
    
    return frame;
}

void X11Capture::capture_cursor(VideoFrame& frame) {
    // Query cursor position
    auto cursor_cookie = xcb_query_pointer(m_connection, m_root_window);
    auto cursor_reply = xcb_query_pointer_reply(m_connection, cursor_cookie, nullptr);
    
    if (!cursor_reply) return;
    
    [[maybe_unused]] int16_t cursor_x = cursor_reply->root_x;
    [[maybe_unused]] int16_t cursor_y = cursor_reply->root_y;
    free(cursor_reply);
    
    // TODO: Render cursor image onto frame
    // This requires XFIXES extension for cursor image
}

Result<VideoFrame> X11Capture::capture_frame() {
    if (!m_running && !m_initialized) {
        return std::unexpected(Error{ErrorCode::NotInitialized});
    }
    
    auto result = capture_shm();
    if (result) {
        ++m_frame_count;
        
        auto now = Clock::now();
        auto elapsed = std::chrono::duration<double>(now - m_start_time).count();
        if (elapsed > 0) {
            m_actual_fps = static_cast<double>(m_frame_count) / elapsed;
        }
        m_last_frame_time = now;
    }
    
    return result;
}

void X11Capture::capture_loop() {
    auto frame_duration = std::chrono::microseconds(1'000'000 / m_config.target_fps);
    
    while (m_running) {
        auto frame_start = Clock::now();
        
        auto result = capture_shm();
        if (result && m_callback) {
            m_callback(*result);
        }
        
        ++m_frame_count;
        
        // Calculate actual FPS
        auto elapsed = std::chrono::duration<double>(Clock::now() - m_start_time).count();
        if (elapsed > 0) {
            m_actual_fps = static_cast<double>(m_frame_count) / elapsed;
        }
        
        // Sleep until next frame
        auto frame_time = Clock::now() - frame_start;
        if (frame_time < frame_duration) {
            std::this_thread::sleep_for(frame_duration - frame_time);
        }
    }
}

void X11Capture::set_frame_callback(VideoFrameCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_callback = std::move(callback);
}

Result<std::vector<MonitorInfo>> X11Capture::get_monitors() {
    if (!m_initialized) {
        return std::unexpected(Error{ErrorCode::NotInitialized});
    }
    return m_monitors;
}

std::pair<uint32_t, uint32_t> X11Capture::get_resolution() const {
    return {m_config.region.width, m_config.region.height};
}

double X11Capture::get_actual_fps() const {
    return m_actual_fps;
}

Result<void> X11Capture::update_config(const CaptureConfig& config) {
    bool was_running = m_running;
    
    if (was_running) {
        stop();
    }
    
    // Update config
    m_config = config;
    
    // Reinitialize SHM if resolution changed
    if (m_shm_data) {
        shmdt(m_shm_data);
        m_shm_data = nullptr;
    }
    if (m_shm_id >= 0) {
        shmctl(m_shm_id, IPC_RMID, nullptr);
        m_shm_id = -1;
    }
    
    if (auto result = init_shm(); !result) {
        return result;
    }
    
    if (was_running) {
        return start();
    }
    
    return {};
}

} // namespace stream_linux

#endif // HAVE_X11
