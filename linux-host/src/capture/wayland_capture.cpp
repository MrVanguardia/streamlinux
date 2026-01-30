/**
 * @file wayland_capture.cpp
 * @brief Wayland screen capture using xdg-desktop-portal and PipeWire
 */

#ifdef HAVE_WAYLAND

#include "stream_linux/wayland_capture.hpp"
#include <spa/param/video/format-utils.h>
#include <spa/debug/types.h>
#include <cstring>

namespace stream_linux {

// Portal D-Bus constants
static constexpr const char* PORTAL_BUS_NAME = "org.freedesktop.portal.Desktop";
static constexpr const char* PORTAL_OBJECT_PATH = "/org/freedesktop/portal/desktop";
static constexpr const char* PORTAL_SCREENCAST_IFACE = "org.freedesktop.portal.ScreenCast";
static constexpr const char* PORTAL_REQUEST_IFACE = "org.freedesktop.portal.Request";

// Security: Maximum allowed frame size (vuln-0002)
static constexpr size_t MAX_FRAME_SIZE = 256ULL * 1024ULL * 1024ULL;  // 256MB (4K RGBA with margin)
static constexpr uint32_t MAX_FRAME_DIMENSION = 16384;  // 16K max

// PipeWire stream events
static const struct pw_stream_events stream_events = {
    .version = PW_VERSION_STREAM_EVENTS,
    .state_changed = WaylandCapture::on_state_changed,
    .param_changed = WaylandCapture::on_param_changed,
    .process = WaylandCapture::on_process,
};

WaylandCapture::WaylandCapture() {
    pw_init(nullptr, nullptr);
}

WaylandCapture::~WaylandCapture() {
    stop();
    close_session();
    cleanup_pipewire();
    pw_deinit();
}

Result<void> WaylandCapture::initialize(const CaptureConfig& config) {
    if (m_initialized) {
        return std::unexpected(Error{ErrorCode::AlreadyInitialized});
    }
    
    m_config = config;
    
    // Connect to D-Bus
    GError* error = nullptr;
    m_dbus_connection = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &error);
    
    if (!m_dbus_connection) {
        std::string msg = error ? error->message : "Unknown error";
        if (error) g_error_free(error);
        return std::unexpected(Error{ErrorCode::CaptureInitFailed,
            "Failed to connect to D-Bus: " + msg});
    }
    
    // Create portal session
    auto result = create_session();
    if (!result) {
        return result;
    }
    
    // Wait for session creation
    {
        std::unique_lock<std::mutex> lock(m_portal_mutex);
        m_portal_cv.wait(lock, [this] {
            return m_portal_state != PortalState::RequestingSession;
        });
        
        if (m_portal_state == PortalState::Failed) {
            return std::unexpected(m_portal_error);
        }
    }
    
    // Select sources (triggers permission dialog)
    result = select_sources();
    if (!result) {
        return result;
    }
    
    // Wait for source selection
    {
        std::unique_lock<std::mutex> lock(m_portal_mutex);
        m_portal_cv.wait(lock, [this] {
            return m_portal_state != PortalState::SelectingSource;
        });
        
        if (m_portal_state == PortalState::Failed) {
            return std::unexpected(m_portal_error);
        }
    }
    
    m_initialized = true;
    return {};
}

Result<void> WaylandCapture::create_session() {
    m_portal_state = PortalState::RequestingSession;
    
    // Generate unique token
    m_request_token = "streamlinux_" + std::to_string(g_random_int());
    
    // Subscribe to Response signal
    std::string sender_name = g_dbus_connection_get_unique_name(m_dbus_connection);
    std::replace(sender_name.begin(), sender_name.end(), '.', '_');
    std::replace(sender_name.begin(), sender_name.end(), ':', '_');
    
    std::string request_path = std::string(PORTAL_OBJECT_PATH) + 
                               "/request/" + sender_name + "/" + m_request_token;
    
    m_signal_id = g_dbus_connection_signal_subscribe(
        m_dbus_connection,
        PORTAL_BUS_NAME,
        PORTAL_REQUEST_IFACE,
        "Response",
        request_path.c_str(),
        nullptr,
        G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
        on_create_session_response,
        this,
        nullptr
    );
    
    // Build options
    GVariantBuilder options_builder;
    g_variant_builder_init(&options_builder, G_VARIANT_TYPE_VARDICT);
    g_variant_builder_add(&options_builder, "{sv}", "handle_token",
                         g_variant_new_string(m_request_token.c_str()));
    g_variant_builder_add(&options_builder, "{sv}", "session_handle_token",
                         g_variant_new_string(("session_" + m_request_token).c_str()));
    
    GError* error = nullptr;
    GVariant* result = g_dbus_connection_call_sync(
        m_dbus_connection,
        PORTAL_BUS_NAME,
        PORTAL_OBJECT_PATH,
        PORTAL_SCREENCAST_IFACE,
        "CreateSession",
        g_variant_new("(a{sv})", &options_builder),
        G_VARIANT_TYPE("(o)"),
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        nullptr,
        &error
    );
    
    if (!result) {
        std::string msg = error ? error->message : "Unknown error";
        if (error) g_error_free(error);
        return std::unexpected(Error{ErrorCode::PortalRequestFailed,
            "CreateSession failed: " + msg});
    }
    
    g_variant_unref(result);
    return {};
}

void WaylandCapture::on_create_session_response(
    [[maybe_unused]] GDBusConnection* connection,
    [[maybe_unused]] const gchar* sender_name,
    [[maybe_unused]] const gchar* object_path,
    [[maybe_unused]] const gchar* interface_name,
    [[maybe_unused]] const gchar* signal_name,
    GVariant* parameters,
    gpointer user_data
) {
    auto* self = static_cast<WaylandCapture*>(user_data);
    
    guint32 response;
    GVariant* results;
    g_variant_get(parameters, "(u@a{sv})", &response, &results);
    
    std::lock_guard<std::mutex> lock(self->m_portal_mutex);
    
    if (response != 0) {
        self->m_portal_state = PortalState::Failed;
        self->m_portal_error = Error{ErrorCode::PermissionDenied,
            "User denied screen sharing permission"};
    } else {
        // Get session handle
        const gchar* session_handle = nullptr;
        g_variant_lookup(results, "session_handle", "&s", &session_handle);
        if (session_handle) {
            self->m_session_handle = session_handle;
        }
        self->m_portal_state = PortalState::SelectingSource;
    }
    
    g_variant_unref(results);
    self->m_portal_cv.notify_all();
}

Result<void> WaylandCapture::select_sources() {
    m_portal_state = PortalState::SelectingSource;
    
    // Update signal subscription
    g_dbus_connection_signal_unsubscribe(m_dbus_connection, m_signal_id);
    
    m_request_token = "streamlinux_" + std::to_string(g_random_int());
    
    std::string sender_name = g_dbus_connection_get_unique_name(m_dbus_connection);
    std::replace(sender_name.begin(), sender_name.end(), '.', '_');
    std::replace(sender_name.begin(), sender_name.end(), ':', '_');
    
    std::string request_path = std::string(PORTAL_OBJECT_PATH) + 
                               "/request/" + sender_name + "/" + m_request_token;
    
    m_signal_id = g_dbus_connection_signal_subscribe(
        m_dbus_connection,
        PORTAL_BUS_NAME,
        PORTAL_REQUEST_IFACE,
        "Response",
        request_path.c_str(),
        nullptr,
        G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
        on_select_sources_response,
        this,
        nullptr
    );
    
    // Build options
    GVariantBuilder options_builder;
    g_variant_builder_init(&options_builder, G_VARIANT_TYPE_VARDICT);
    g_variant_builder_add(&options_builder, "{sv}", "handle_token",
                         g_variant_new_string(m_request_token.c_str()));
    g_variant_builder_add(&options_builder, "{sv}", "types",
                         g_variant_new_uint32(1));  // 1 = monitor, 2 = window
    g_variant_builder_add(&options_builder, "{sv}", "multiple",
                         g_variant_new_boolean(FALSE));
    g_variant_builder_add(&options_builder, "{sv}", "cursor_mode",
                         g_variant_new_uint32(m_config.show_cursor ? 2 : 1));
    
    GError* error = nullptr;
    GVariant* result = g_dbus_connection_call_sync(
        m_dbus_connection,
        PORTAL_BUS_NAME,
        PORTAL_OBJECT_PATH,
        PORTAL_SCREENCAST_IFACE,
        "SelectSources",
        g_variant_new("(oa{sv})", m_session_handle.c_str(), &options_builder),
        G_VARIANT_TYPE("(o)"),
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        nullptr,
        &error
    );
    
    if (!result) {
        std::string msg = error ? error->message : "Unknown error";
        if (error) g_error_free(error);
        return std::unexpected(Error{ErrorCode::PortalRequestFailed,
            "SelectSources failed: " + msg});
    }
    
    g_variant_unref(result);
    return {};
}

void WaylandCapture::on_select_sources_response(
    [[maybe_unused]] GDBusConnection* connection,
    [[maybe_unused]] const gchar* sender_name,
    [[maybe_unused]] const gchar* object_path,
    [[maybe_unused]] const gchar* interface_name,
    [[maybe_unused]] const gchar* signal_name,
    GVariant* parameters,
    gpointer user_data
) {
    auto* self = static_cast<WaylandCapture*>(user_data);
    
    guint32 response;
    GVariant* results;
    g_variant_get(parameters, "(u@a{sv})", &response, &results);
    
    std::lock_guard<std::mutex> lock(self->m_portal_mutex);
    
    if (response != 0) {
        self->m_portal_state = PortalState::Failed;
        self->m_portal_error = Error{ErrorCode::PermissionDenied,
            "User cancelled source selection"};
    } else {
        self->m_portal_state = PortalState::Starting;
    }
    
    g_variant_unref(results);
    self->m_portal_cv.notify_all();
}

Result<void> WaylandCapture::start() {
    if (!m_initialized) {
        return std::unexpected(Error{ErrorCode::NotInitialized});
    }
    if (m_running) {
        return std::unexpected(Error{ErrorCode::AlreadyInitialized,
            "Capture already running"});
    }
    
    // Start the stream
    auto result = start_stream();
    if (!result) {
        return result;
    }
    
    // Wait for stream to start
    {
        std::unique_lock<std::mutex> lock(m_portal_mutex);
        m_portal_cv.wait(lock, [this] {
            return m_portal_state == PortalState::Active ||
                   m_portal_state == PortalState::Failed;
        });
        
        if (m_portal_state == PortalState::Failed) {
            return std::unexpected(m_portal_error);
        }
    }
    
    m_running = true;
    m_frame_count = 0;
    m_start_time = Clock::now();
    
    return {};
}

Result<void> WaylandCapture::start_stream() {
    // Subscribe to Start response
    g_dbus_connection_signal_unsubscribe(m_dbus_connection, m_signal_id);
    
    m_request_token = "streamlinux_" + std::to_string(g_random_int());
    
    std::string sender_name = g_dbus_connection_get_unique_name(m_dbus_connection);
    std::replace(sender_name.begin(), sender_name.end(), '.', '_');
    std::replace(sender_name.begin(), sender_name.end(), ':', '_');
    
    std::string request_path = std::string(PORTAL_OBJECT_PATH) + 
                               "/request/" + sender_name + "/" + m_request_token;
    
    m_signal_id = g_dbus_connection_signal_subscribe(
        m_dbus_connection,
        PORTAL_BUS_NAME,
        PORTAL_REQUEST_IFACE,
        "Response",
        request_path.c_str(),
        nullptr,
        G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
        on_start_response,
        this,
        nullptr
    );
    
    // Build options
    GVariantBuilder options_builder;
    g_variant_builder_init(&options_builder, G_VARIANT_TYPE_VARDICT);
    g_variant_builder_add(&options_builder, "{sv}", "handle_token",
                         g_variant_new_string(m_request_token.c_str()));
    
    GError* error = nullptr;
    GVariant* result = g_dbus_connection_call_sync(
        m_dbus_connection,
        PORTAL_BUS_NAME,
        PORTAL_OBJECT_PATH,
        PORTAL_SCREENCAST_IFACE,
        "Start",
        g_variant_new("(osa{sv})", m_session_handle.c_str(), "", &options_builder),
        G_VARIANT_TYPE("(o)"),
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        nullptr,
        &error
    );
    
    if (!result) {
        std::string msg = error ? error->message : "Unknown error";
        if (error) g_error_free(error);
        return std::unexpected(Error{ErrorCode::CaptureStartFailed,
            "Start failed: " + msg});
    }
    
    g_variant_unref(result);
    return {};
}

void WaylandCapture::on_start_response(
    [[maybe_unused]] GDBusConnection* connection,
    [[maybe_unused]] const gchar* sender_name,
    [[maybe_unused]] const gchar* object_path,
    [[maybe_unused]] const gchar* interface_name,
    [[maybe_unused]] const gchar* signal_name,
    GVariant* parameters,
    gpointer user_data
) {
    auto* self = static_cast<WaylandCapture*>(user_data);
    
    guint32 response;
    GVariant* results;
    g_variant_get(parameters, "(u@a{sv})", &response, &results);
    
    if (response != 0) {
        std::lock_guard<std::mutex> lock(self->m_portal_mutex);
        self->m_portal_state = PortalState::Failed;
        self->m_portal_error = Error{ErrorCode::CaptureStartFailed,
            "Failed to start screen cast"};
        g_variant_unref(results);
        self->m_portal_cv.notify_all();
        return;
    }
    
    // Get streams
    GVariant* streams = g_variant_lookup_value(results, "streams", G_VARIANT_TYPE("a(ua{sv})"));
    if (!streams || g_variant_n_children(streams) == 0) {
        std::lock_guard<std::mutex> lock(self->m_portal_mutex);
        self->m_portal_state = PortalState::Failed;
        self->m_portal_error = Error{ErrorCode::CaptureStartFailed,
            "No streams returned"};
        if (streams) g_variant_unref(streams);
        g_variant_unref(results);
        self->m_portal_cv.notify_all();
        return;
    }
    
    // Get first stream's PipeWire node ID
    GVariant* stream = g_variant_get_child_value(streams, 0);
    guint32 pipewire_node;
    GVariant* stream_properties;
    g_variant_get(stream, "(u@a{sv})", &pipewire_node, &stream_properties);
    
    // Initialize PipeWire with this node
    auto result = self->init_pipewire(pipewire_node);
    
    {
        std::lock_guard<std::mutex> lock(self->m_portal_mutex);
        if (!result) {
            self->m_portal_state = PortalState::Failed;
            self->m_portal_error = result.error();
        } else {
            self->m_portal_state = PortalState::Active;
        }
    }
    
    g_variant_unref(stream_properties);
    g_variant_unref(stream);
    g_variant_unref(streams);
    g_variant_unref(results);
    
    self->m_portal_cv.notify_all();
}

Result<void> WaylandCapture::init_pipewire(uint32_t pipewire_node) {
    m_pw_loop = pw_thread_loop_new("stream-linux", nullptr);
    if (!m_pw_loop) {
        return std::unexpected(Error{ErrorCode::CaptureInitFailed,
            "Failed to create PipeWire thread loop"});
    }
    
    m_pw_context = pw_context_new(pw_thread_loop_get_loop(m_pw_loop), nullptr, 0);
    if (!m_pw_context) {
        return std::unexpected(Error{ErrorCode::CaptureInitFailed,
            "Failed to create PipeWire context"});
    }
    
    if (pw_thread_loop_start(m_pw_loop) < 0) {
        return std::unexpected(Error{ErrorCode::CaptureInitFailed,
            "Failed to start PipeWire thread loop"});
    }
    
    pw_thread_loop_lock(m_pw_loop);
    
    m_pw_core = pw_context_connect(m_pw_context, nullptr, 0);
    if (!m_pw_core) {
        pw_thread_loop_unlock(m_pw_loop);
        return std::unexpected(Error{ErrorCode::CaptureInitFailed,
            "Failed to connect PipeWire context"});
    }
    
    // Create stream
    auto props = pw_properties_new(
        PW_KEY_MEDIA_TYPE, "Video",
        PW_KEY_MEDIA_CATEGORY, "Capture",
        PW_KEY_MEDIA_ROLE, "Screen",
        nullptr
    );
    
    m_pw_stream = pw_stream_new(m_pw_core, "screen-capture", props);
    if (!m_pw_stream) {
        pw_thread_loop_unlock(m_pw_loop);
        return std::unexpected(Error{ErrorCode::CaptureInitFailed,
            "Failed to create PipeWire stream"});
    }
    
    // Add listener
    pw_stream_add_listener(m_pw_stream, &m_stream_listener, &stream_events, this);
    
    // Build format parameters
    uint8_t buffer[1024];
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
    
    const struct spa_pod* params[1];
    params[0] = static_cast<const spa_pod*>(spa_pod_builder_add_object(&b,
        SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat,
        SPA_FORMAT_mediaType, SPA_POD_Id(SPA_MEDIA_TYPE_video),
        SPA_FORMAT_mediaSubtype, SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw),
        SPA_FORMAT_VIDEO_format, SPA_POD_CHOICE_ENUM_Id(4,
            SPA_VIDEO_FORMAT_BGRA,
            SPA_VIDEO_FORMAT_RGBA,
            SPA_VIDEO_FORMAT_BGRx,
            SPA_VIDEO_FORMAT_RGBx),
        SPA_FORMAT_VIDEO_size, SPA_POD_CHOICE_RANGE_Rectangle(
            &SPA_RECTANGLE(1920, 1080),
            &SPA_RECTANGLE(1, 1),
            &SPA_RECTANGLE(4096, 4096)),
        SPA_FORMAT_VIDEO_framerate, SPA_POD_CHOICE_RANGE_Fraction(
            &SPA_FRACTION(60, 1),
            &SPA_FRACTION(0, 1),
            &SPA_FRACTION(144, 1))));
    
    // Connect stream
    int ret = pw_stream_connect(
        m_pw_stream,
        PW_DIRECTION_INPUT,
        pipewire_node,
        static_cast<pw_stream_flags>(
            PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS),
        params, 1
    );
    
    pw_thread_loop_unlock(m_pw_loop);
    
    if (ret < 0) {
        return std::unexpected(Error{ErrorCode::CaptureInitFailed,
            "Failed to connect PipeWire stream"});
    }
    
    return {};
}

void WaylandCapture::on_state_changed(
    void* userdata,
    [[maybe_unused]] enum pw_stream_state old,
    enum pw_stream_state state,
    const char* error
) {
    auto* self = static_cast<WaylandCapture*>(userdata);
    
    if (state == PW_STREAM_STATE_ERROR && error) {
        // Handle error
        (void)self;
    }
}

void WaylandCapture::on_param_changed(
    void* userdata,
    uint32_t id,
    const struct spa_pod* param
) {
    auto* self = static_cast<WaylandCapture*>(userdata);
    
    if (!param || id != SPA_PARAM_Format) return;
    
    if (spa_format_parse(param, &self->m_video_format.media_type,
                        &self->m_video_format.media_subtype) < 0) {
        return;
    }
    
    if (self->m_video_format.media_type != SPA_MEDIA_TYPE_video ||
        self->m_video_format.media_subtype != SPA_MEDIA_SUBTYPE_raw) {
        return;
    }
    
    spa_format_video_raw_parse(param, &self->m_video_format.info.raw);
    self->m_format_negotiated = true;
}

void WaylandCapture::on_process(void* userdata) {
    auto* self = static_cast<WaylandCapture*>(userdata);
    self->process_pipewire_frame();
}

void WaylandCapture::process_pipewire_frame() {
    struct pw_buffer* b = pw_stream_dequeue_buffer(m_pw_stream);
    if (!b) return;
    
    struct spa_buffer* buf = b->buffer;
    if (!buf->datas[0].data) {
        pw_stream_queue_buffer(m_pw_stream, b);
        return;
    }
    
    // Create video frame
    VideoFrame frame;
    frame.pts = get_monotonic_pts();
    frame.width = m_video_format.info.raw.size.width;
    frame.height = m_video_format.info.raw.size.height;
    frame.stride = buf->datas[0].chunk->stride;
    
    // Determine pixel format
    switch (m_video_format.info.raw.format) {
        case SPA_VIDEO_FORMAT_BGRA:
        case SPA_VIDEO_FORMAT_BGRx:
            frame.format = PixelFormat::BGRA32;
            break;
        case SPA_VIDEO_FORMAT_RGBA:
        case SPA_VIDEO_FORMAT_RGBx:
            frame.format = PixelFormat::RGBA32;
            break;
        default:
            frame.format = PixelFormat::Unknown;
    }
    
    // Copy data
    size_t data_size = buf->datas[0].chunk->size;
    frame.data.resize(data_size);
    std::memcpy(frame.data.data(), buf->datas[0].data, data_size);
    
    pw_stream_queue_buffer(m_pw_stream, b);
    
    // Update stats
    ++m_frame_count;
    auto elapsed = std::chrono::duration<double>(Clock::now() - m_start_time).count();
    if (elapsed > 0) {
        m_actual_fps = static_cast<double>(m_frame_count) / elapsed;
    }
    
    // Deliver frame
    if (m_callback) {
        m_callback(frame);
    } else {
        std::lock_guard<std::mutex> lock(m_queue_mutex);
        if (m_frame_queue.size() >= MAX_QUEUE_SIZE) {
            m_frame_queue.pop();
        }
        m_frame_queue.push(std::move(frame));
        m_frame_available.notify_one();
    }
}

void WaylandCapture::stop() {
    m_running = false;
    m_frame_available.notify_all();
}

bool WaylandCapture::is_running() const {
    return m_running;
}

Result<VideoFrame> WaylandCapture::capture_frame() {
    std::unique_lock<std::mutex> lock(m_queue_mutex);
    
    if (!m_frame_available.wait_for(lock, std::chrono::milliseconds(100),
                                    [this] { return !m_frame_queue.empty() || !m_running; })) {
        return std::unexpected(Error{ErrorCode::Timeout, "No frame available"});
    }
    
    if (!m_running && m_frame_queue.empty()) {
        return std::unexpected(Error{ErrorCode::CaptureReadFailed, "Capture stopped"});
    }
    
    VideoFrame frame = std::move(m_frame_queue.front());
    m_frame_queue.pop();
    return frame;
}

void WaylandCapture::set_frame_callback(VideoFrameCallback callback) {
    m_callback = std::move(callback);
}

Result<std::vector<MonitorInfo>> WaylandCapture::get_monitors() {
    // Wayland doesn't provide monitor list before selection
    // Return empty or query via portal
    return m_monitors;
}

std::pair<uint32_t, uint32_t> WaylandCapture::get_resolution() const {
    if (m_format_negotiated) {
        return {m_video_format.info.raw.size.width, m_video_format.info.raw.size.height};
    }
    return {0, 0};
}

double WaylandCapture::get_actual_fps() const {
    return m_actual_fps;
}

Result<void> WaylandCapture::update_config(const CaptureConfig& config) {
    m_config = config;
    // TODO: Restart stream with new parameters if needed
    return {};
}

void WaylandCapture::close_session() {
    if (m_signal_id) {
        g_dbus_connection_signal_unsubscribe(m_dbus_connection, m_signal_id);
        m_signal_id = 0;
    }
    
    if (!m_session_handle.empty() && m_dbus_connection) {
        // Close portal session
        g_dbus_connection_call_sync(
            m_dbus_connection,
            PORTAL_BUS_NAME,
            m_session_handle.c_str(),
            "org.freedesktop.portal.Session",
            "Close",
            nullptr,
            nullptr,
            G_DBUS_CALL_FLAGS_NONE,
            -1,
            nullptr,
            nullptr
        );
        m_session_handle.clear();
    }
    
    if (m_dbus_connection) {
        g_object_unref(m_dbus_connection);
        m_dbus_connection = nullptr;
    }
}

void WaylandCapture::cleanup_pipewire() {
    if (m_pw_loop) {
        pw_thread_loop_lock(m_pw_loop);
    }
    
    if (m_pw_stream) {
        pw_stream_destroy(m_pw_stream);
        m_pw_stream = nullptr;
    }
    
    if (m_pw_core) {
        pw_core_disconnect(m_pw_core);
        m_pw_core = nullptr;
    }
    
    if (m_pw_context) {
        pw_context_destroy(m_pw_context);
        m_pw_context = nullptr;
    }
    
    if (m_pw_loop) {
        pw_thread_loop_unlock(m_pw_loop);
        pw_thread_loop_stop(m_pw_loop);
        pw_thread_loop_destroy(m_pw_loop);
        m_pw_loop = nullptr;
    }
}

} // namespace stream_linux

#endif // HAVE_WAYLAND
