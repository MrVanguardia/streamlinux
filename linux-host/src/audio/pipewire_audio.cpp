/**
 * @file pipewire_audio.cpp
 * @brief PipeWire audio capture implementation
 */

#ifdef HAVE_PIPEWIRE_AUDIO

#include "stream_linux/audio_capture.hpp"
#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <cstring>
#include <queue>

namespace stream_linux {

struct PipeWireAudioCapture::Impl {
    struct pw_thread_loop* loop = nullptr;
    struct pw_context* context = nullptr;
    struct pw_core* core = nullptr;
    struct pw_stream* stream = nullptr;
    struct spa_hook stream_listener;
    
    AudioConfig config;
    std::string selected_device;
    
    std::queue<AudioFrame> frame_queue;
    std::mutex queue_mutex;
    std::condition_variable frame_cv;
    static constexpr size_t MAX_QUEUE_SIZE = 10;
    
    std::atomic<bool> running{false};
    std::atomic<bool> initialized{false};
    AudioFrameCallback callback;
    
    std::atomic<double> latency_ms{0};
    
    static void on_process(void* userdata);
    static void on_param_changed(void* userdata, uint32_t id, const struct spa_pod* param);
    static void on_state_changed(void* userdata, enum pw_stream_state old,
                                 enum pw_stream_state state, const char* error);
};

static const struct pw_stream_events audio_stream_events = {
    .version = PW_VERSION_STREAM_EVENTS,
    .state_changed = PipeWireAudioCapture::Impl::on_state_changed,
    .param_changed = PipeWireAudioCapture::Impl::on_param_changed,
    .process = PipeWireAudioCapture::Impl::on_process,
};

void PipeWireAudioCapture::Impl::on_process(void* userdata) {
    auto* impl = static_cast<Impl*>(userdata);
    
    struct pw_buffer* b = pw_stream_dequeue_buffer(impl->stream);
    if (!b) return;
    
    struct spa_buffer* buf = b->buffer;
    float* data = static_cast<float*>(buf->datas[0].data);
    
    if (!data) {
        pw_stream_queue_buffer(impl->stream, b);
        return;
    }
    
    uint32_t n_samples = buf->datas[0].chunk->size / sizeof(float) / impl->config.channels;
    
    AudioFrame frame;
    frame.sample_rate = impl->config.sample_rate;
    frame.channels = impl->config.channels;
    frame.samples_per_channel = n_samples;
    frame.pts = get_monotonic_pts();
    
    size_t total_samples = n_samples * impl->config.channels;
    frame.data.resize(total_samples);
    std::memcpy(frame.data.data(), data, total_samples * sizeof(float));
    
    pw_stream_queue_buffer(impl->stream, b);
    
    if (impl->callback) {
        impl->callback(frame);
    } else {
        std::lock_guard<std::mutex> lock(impl->queue_mutex);
        if (impl->frame_queue.size() >= MAX_QUEUE_SIZE) {
            impl->frame_queue.pop();
        }
        impl->frame_queue.push(std::move(frame));
        impl->frame_cv.notify_one();
    }
}

void PipeWireAudioCapture::Impl::on_param_changed(
    [[maybe_unused]] void* userdata,
    [[maybe_unused]] uint32_t id,
    [[maybe_unused]] const struct spa_pod* param
) {
    // Handle format negotiation if needed
}

void PipeWireAudioCapture::Impl::on_state_changed(
    [[maybe_unused]] void* userdata,
    [[maybe_unused]] enum pw_stream_state old,
    [[maybe_unused]] enum pw_stream_state state,
    [[maybe_unused]] const char* error
) {
    // Handle state changes
}

PipeWireAudioCapture::PipeWireAudioCapture() : m_impl(std::make_unique<Impl>()) {
    pw_init(nullptr, nullptr);
}

PipeWireAudioCapture::~PipeWireAudioCapture() {
    stop();
    
    if (m_impl->loop) {
        pw_thread_loop_lock(m_impl->loop);
    }
    
    if (m_impl->stream) {
        pw_stream_destroy(m_impl->stream);
    }
    if (m_impl->core) {
        pw_core_disconnect(m_impl->core);
    }
    if (m_impl->context) {
        pw_context_destroy(m_impl->context);
    }
    
    if (m_impl->loop) {
        pw_thread_loop_unlock(m_impl->loop);
        pw_thread_loop_stop(m_impl->loop);
        pw_thread_loop_destroy(m_impl->loop);
    }
    
    pw_deinit();
}

Result<void> PipeWireAudioCapture::initialize(const AudioConfig& config) {
    if (m_impl->initialized) {
        return std::unexpected(Error{ErrorCode::AlreadyInitialized});
    }
    
    m_impl->config = config;
    
    m_impl->loop = pw_thread_loop_new("audio-capture", nullptr);
    if (!m_impl->loop) {
        return std::unexpected(Error{ErrorCode::AudioInitFailed,
            "Failed to create PipeWire loop"});
    }
    
    m_impl->context = pw_context_new(pw_thread_loop_get_loop(m_impl->loop), nullptr, 0);
    if (!m_impl->context) {
        return std::unexpected(Error{ErrorCode::AudioInitFailed,
            "Failed to create PipeWire context"});
    }
    
    if (pw_thread_loop_start(m_impl->loop) < 0) {
        return std::unexpected(Error{ErrorCode::AudioInitFailed,
            "Failed to start PipeWire loop"});
    }
    
    pw_thread_loop_lock(m_impl->loop);
    
    m_impl->core = pw_context_connect(m_impl->context, nullptr, 0);
    if (!m_impl->core) {
        pw_thread_loop_unlock(m_impl->loop);
        return std::unexpected(Error{ErrorCode::AudioInitFailed,
            "Failed to connect PipeWire"});
    }
    
    pw_thread_loop_unlock(m_impl->loop);
    
    m_impl->initialized = true;
    return {};
}

Result<void> PipeWireAudioCapture::start() {
    if (!m_impl->initialized) {
        return std::unexpected(Error{ErrorCode::NotInitialized});
    }
    if (m_impl->running) {
        return std::unexpected(Error{ErrorCode::AlreadyInitialized});
    }
    
    pw_thread_loop_lock(m_impl->loop);
    
    auto props = pw_properties_new(
        PW_KEY_MEDIA_TYPE, "Audio",
        PW_KEY_MEDIA_CATEGORY, "Capture",
        PW_KEY_MEDIA_ROLE, "Music",
        nullptr
    );
    
    // Capture system audio (monitor)
    if (m_impl->config.source == AudioSource::System) {
        pw_properties_set(props, PW_KEY_STREAM_CAPTURE_SINK, "true");
    }
    
    m_impl->stream = pw_stream_new(m_impl->core, "audio-capture", props);
    if (!m_impl->stream) {
        pw_thread_loop_unlock(m_impl->loop);
        return std::unexpected(Error{ErrorCode::AudioCaptureStartFailed,
            "Failed to create audio stream"});
    }
    
    pw_stream_add_listener(m_impl->stream, &m_impl->stream_listener,
                          &audio_stream_events, m_impl.get());
    
    // Build format
    uint8_t buffer[1024];
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
    
    const struct spa_pod* params[1];
    struct spa_audio_info_raw info = {
        .format = SPA_AUDIO_FORMAT_F32,
        .rate = m_impl->config.sample_rate,
        .channels = m_impl->config.channels,
    };
    
    params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &info);
    
    int ret = pw_stream_connect(
        m_impl->stream,
        PW_DIRECTION_INPUT,
        PW_ID_ANY,
        static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS),
        params, 1
    );
    
    pw_thread_loop_unlock(m_impl->loop);
    
    if (ret < 0) {
        return std::unexpected(Error{ErrorCode::AudioCaptureStartFailed,
            "Failed to connect audio stream"});
    }
    
    m_impl->running = true;
    return {};
}

void PipeWireAudioCapture::stop() {
    m_impl->running = false;
    m_impl->frame_cv.notify_all();
}

bool PipeWireAudioCapture::is_running() const {
    return m_impl->running;
}

Result<AudioFrame> PipeWireAudioCapture::read_frame() {
    std::unique_lock<std::mutex> lock(m_impl->queue_mutex);
    
    if (!m_impl->frame_cv.wait_for(lock, std::chrono::milliseconds(100),
                                   [this] { return !m_impl->frame_queue.empty() || !m_impl->running; })) {
        return std::unexpected(Error{ErrorCode::Timeout});
    }
    
    if (!m_impl->running && m_impl->frame_queue.empty()) {
        return std::unexpected(Error{ErrorCode::AudioReadFailed});
    }
    
    AudioFrame frame = std::move(m_impl->frame_queue.front());
    m_impl->frame_queue.pop();
    return frame;
}

void PipeWireAudioCapture::set_frame_callback(AudioFrameCallback callback) {
    m_impl->callback = std::move(callback);
}

Result<std::vector<AudioDeviceInfo>> PipeWireAudioCapture::get_devices() {
    // TODO: Enumerate PipeWire devices
    std::vector<AudioDeviceInfo> devices;
    
    AudioDeviceInfo monitor;
    monitor.id = "default";
    monitor.name = "System Audio";
    monitor.description = "Monitor of default audio output";
    monitor.is_monitor = true;
    monitor.is_default = true;
    monitor.sample_rate = 48000;
    monitor.channels = 2;
    devices.push_back(monitor);
    
    return devices;
}

Result<void> PipeWireAudioCapture::select_device(const std::string& device_id) {
    m_impl->selected_device = device_id;
    return {};
}

double PipeWireAudioCapture::get_latency_ms() const {
    return m_impl->latency_ms;
}

} // namespace stream_linux

#endif // HAVE_PIPEWIRE_AUDIO
