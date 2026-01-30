#pragma once

/**
 * @file wayland_capture.hpp
 * @brief Wayland screen capture using xdg-desktop-portal and PipeWire
 * 
 * Features:
 * - Permission-based screen sharing via desktop portal
 * - PipeWire stream for frame reception
 * - Compatible with GNOME, KDE, wlroots compositors
 * - Hardware buffer support (DMA-BUF)
 */

#include "display_backend.hpp"

#ifdef HAVE_WAYLAND

#include <gio/gio.h>
#include <pipewire/pipewire.h>
#include <spa/param/video/format-utils.h>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>

namespace stream_linux {

/**
 * @brief Portal-based screen sharing state
 */
enum class PortalState {
    Idle,
    RequestingSession,
    SelectingSource,
    Starting,
    Active,
    Failed
};

/**
 * @brief Wayland screen capture using xdg-desktop-portal
 */
class WaylandCapture : public IDisplayBackend {
public:
    WaylandCapture();
    ~WaylandCapture() override;
    
    // Non-copyable, non-movable
    WaylandCapture(const WaylandCapture&) = delete;
    WaylandCapture& operator=(const WaylandCapture&) = delete;
    WaylandCapture(WaylandCapture&&) = delete;
    WaylandCapture& operator=(WaylandCapture&&) = delete;
    
    // IDisplayBackend interface
    [[nodiscard]] DisplayBackend get_type() const override { return DisplayBackend::Wayland; }
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
    // Portal session management
    [[nodiscard]] Result<void> create_session();
    [[nodiscard]] Result<void> select_sources();
    [[nodiscard]] Result<void> start_stream();
    void close_session();
    
    // Portal D-Bus callbacks
    static void on_create_session_response(GDBusConnection* connection,
                                           const gchar* sender_name,
                                           const gchar* object_path,
                                           const gchar* interface_name,
                                           const gchar* signal_name,
                                           GVariant* parameters,
                                           gpointer user_data);
    
    static void on_select_sources_response(GDBusConnection* connection,
                                           const gchar* sender_name,
                                           const gchar* object_path,
                                           const gchar* interface_name,
                                           const gchar* signal_name,
                                           GVariant* parameters,
                                           gpointer user_data);
    
    static void on_start_response(GDBusConnection* connection,
                                  const gchar* sender_name,
                                  const gchar* object_path,
                                  const gchar* interface_name,
                                  const gchar* signal_name,
                                  GVariant* parameters,
                                  gpointer user_data);
    
    // PipeWire stream management
    [[nodiscard]] Result<void> init_pipewire(uint32_t pipewire_node);
    void cleanup_pipewire();
    
    // PipeWire callbacks
    static void on_process(void* userdata);
    static void on_param_changed(void* userdata, uint32_t id, const struct spa_pod* param);
    static void on_state_changed(void* userdata, enum pw_stream_state old,
                                 enum pw_stream_state state, const char* error);
    
    // Frame processing
    void process_pipewire_frame();
    void convert_frame_format(const uint8_t* src, VideoFrame& dst,
                             uint32_t src_format, uint32_t width, uint32_t height);
    
    // Portal state
    GDBusConnection* m_dbus_connection = nullptr;
    std::string m_session_handle;
    std::string m_request_token;
    guint m_signal_id = 0;
    PortalState m_portal_state = PortalState::Idle;
    
    // PipeWire state
    struct pw_thread_loop* m_pw_loop = nullptr;
    struct pw_context* m_pw_context = nullptr;
    struct pw_core* m_pw_core = nullptr;
    struct pw_stream* m_pw_stream = nullptr;
    struct spa_hook m_stream_listener;
    
    // Stream parameters
    struct spa_video_info m_video_format;
    bool m_format_negotiated = false;
    
    // Configuration
    CaptureConfig m_config;
    std::vector<MonitorInfo> m_monitors;
    
    // Frame buffer
    std::queue<VideoFrame> m_frame_queue;
    std::mutex m_queue_mutex;
    std::condition_variable m_frame_available;
    static constexpr size_t MAX_QUEUE_SIZE = 3;
    
    // Capture state
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_initialized{false};
    VideoFrameCallback m_callback;
    
    // Synchronization for portal responses
    std::mutex m_portal_mutex;
    std::condition_variable m_portal_cv;
    Error m_portal_error{ErrorCode::Success};
    
    // Statistics
    std::atomic<uint64_t> m_frame_count{0};
    TimePoint m_start_time;
    std::atomic<double> m_actual_fps{0.0};
};

} // namespace stream_linux

#endif // HAVE_WAYLAND
