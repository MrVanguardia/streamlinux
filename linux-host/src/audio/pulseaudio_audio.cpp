/**
 * @file pulseaudio_audio.cpp
 * @brief PulseAudio audio capture fallback implementation
 */

#ifdef HAVE_PULSEAUDIO

#include "stream_linux/audio_capture.hpp"
#include <pulse/pulseaudio.h>
#include <cstring>
#include <queue>

namespace stream_linux {

struct PulseAudioCapture::Impl {
    pa_threaded_mainloop* mainloop = nullptr;
    pa_context* context = nullptr;
    pa_stream* stream = nullptr;
    
    AudioConfig config;
    std::string selected_device;
    
    std::queue<AudioFrame> frame_queue;
    std::mutex queue_mutex;
    std::condition_variable frame_cv;
    static constexpr size_t MAX_QUEUE_SIZE = 10;
    
    std::atomic<bool> running{false};
    std::atomic<bool> initialized{false};
    std::atomic<bool> context_ready{false};
    AudioFrameCallback callback;
    
    std::atomic<double> latency_ms{0};
    
    static void context_state_callback(pa_context* c, void* userdata);
    static void stream_read_callback(pa_stream* s, size_t length, void* userdata);
    static void stream_state_callback(pa_stream* s, void* userdata);
};

void PulseAudioCapture::Impl::context_state_callback(pa_context* c, void* userdata) {
    auto* impl = static_cast<Impl*>(userdata);
    
    switch (pa_context_get_state(c)) {
        case PA_CONTEXT_READY:
            impl->context_ready = true;
            pa_threaded_mainloop_signal(impl->mainloop, 0);
            break;
        case PA_CONTEXT_FAILED:
        case PA_CONTEXT_TERMINATED:
            impl->context_ready = false;
            pa_threaded_mainloop_signal(impl->mainloop, 0);
            break;
        default:
            break;
    }
}

void PulseAudioCapture::Impl::stream_state_callback(
    [[maybe_unused]] pa_stream* s,
    void* userdata
) {
    auto* impl = static_cast<Impl*>(userdata);
    pa_threaded_mainloop_signal(impl->mainloop, 0);
}

void PulseAudioCapture::Impl::stream_read_callback(pa_stream* s, size_t length, void* userdata) {
    auto* impl = static_cast<Impl*>(userdata);
    
    const void* data;
    if (pa_stream_peek(s, &data, &length) < 0) {
        return;
    }
    
    if (!data || length == 0) {
        pa_stream_drop(s);
        return;
    }
    
    // Convert to float (PulseAudio gives us S16LE by default)
    const int16_t* samples = static_cast<const int16_t*>(data);
    size_t n_samples = length / sizeof(int16_t);
    
    AudioFrame frame;
    frame.sample_rate = impl->config.sample_rate;
    frame.channels = impl->config.channels;
    frame.samples_per_channel = static_cast<uint32_t>(n_samples / impl->config.channels);
    frame.pts = get_monotonic_pts();
    
    frame.data.resize(n_samples);
    for (size_t i = 0; i < n_samples; ++i) {
        frame.data[i] = static_cast<float>(samples[i]) / 32768.0f;
    }
    
    pa_stream_drop(s);
    
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

PulseAudioCapture::PulseAudioCapture() : m_impl(std::make_unique<Impl>()) {}

PulseAudioCapture::~PulseAudioCapture() {
    stop();
    
    if (m_impl->stream) {
        pa_stream_disconnect(m_impl->stream);
        pa_stream_unref(m_impl->stream);
    }
    
    if (m_impl->context) {
        pa_context_disconnect(m_impl->context);
        pa_context_unref(m_impl->context);
    }
    
    if (m_impl->mainloop) {
        pa_threaded_mainloop_stop(m_impl->mainloop);
        pa_threaded_mainloop_free(m_impl->mainloop);
    }
}

Result<void> PulseAudioCapture::initialize(const AudioConfig& config) {
    if (m_impl->initialized) {
        return std::unexpected(Error{ErrorCode::AlreadyInitialized});
    }
    
    m_impl->config = config;
    
    m_impl->mainloop = pa_threaded_mainloop_new();
    if (!m_impl->mainloop) {
        return std::unexpected(Error{ErrorCode::AudioInitFailed,
            "Failed to create PulseAudio mainloop"});
    }
    
    pa_mainloop_api* api = pa_threaded_mainloop_get_api(m_impl->mainloop);
    
    m_impl->context = pa_context_new(api, "stream-linux");
    if (!m_impl->context) {
        return std::unexpected(Error{ErrorCode::AudioInitFailed,
            "Failed to create PulseAudio context"});
    }
    
    pa_context_set_state_callback(m_impl->context, Impl::context_state_callback, m_impl.get());
    
    if (pa_context_connect(m_impl->context, nullptr, PA_CONTEXT_NOFLAGS, nullptr) < 0) {
        return std::unexpected(Error{ErrorCode::AudioInitFailed,
            "Failed to connect to PulseAudio server"});
    }
    
    pa_threaded_mainloop_start(m_impl->mainloop);
    
    // Wait for context to be ready
    pa_threaded_mainloop_lock(m_impl->mainloop);
    while (!m_impl->context_ready) {
        pa_context_state_t state = pa_context_get_state(m_impl->context);
        if (state == PA_CONTEXT_FAILED || state == PA_CONTEXT_TERMINATED) {
            pa_threaded_mainloop_unlock(m_impl->mainloop);
            return std::unexpected(Error{ErrorCode::AudioInitFailed,
                "PulseAudio connection failed"});
        }
        pa_threaded_mainloop_wait(m_impl->mainloop);
    }
    pa_threaded_mainloop_unlock(m_impl->mainloop);
    
    m_impl->initialized = true;
    return {};
}

Result<void> PulseAudioCapture::start() {
    if (!m_impl->initialized) {
        return std::unexpected(Error{ErrorCode::NotInitialized});
    }
    if (m_impl->running) {
        return std::unexpected(Error{ErrorCode::AlreadyInitialized});
    }
    
    pa_sample_spec spec = {
        .format = PA_SAMPLE_S16LE,
        .rate = m_impl->config.sample_rate,
        .channels = static_cast<uint8_t>(m_impl->config.channels)
    };
    
    pa_threaded_mainloop_lock(m_impl->mainloop);
    
    m_impl->stream = pa_stream_new(m_impl->context, "capture", &spec, nullptr);
    if (!m_impl->stream) {
        pa_threaded_mainloop_unlock(m_impl->mainloop);
        return std::unexpected(Error{ErrorCode::AudioCaptureStartFailed,
            "Failed to create PulseAudio stream"});
    }
    
    pa_stream_set_state_callback(m_impl->stream, Impl::stream_state_callback, m_impl.get());
    pa_stream_set_read_callback(m_impl->stream, Impl::stream_read_callback, m_impl.get());
    
    // Determine source name
    const char* source = nullptr;
    if (m_impl->config.source == AudioSource::System) {
        // Use monitor of default sink
        source = nullptr;  // Will be resolved to default monitor
    } else if (!m_impl->selected_device.empty()) {
        source = m_impl->selected_device.c_str();
    }
    
    pa_buffer_attr attr = {
        .maxlength = static_cast<uint32_t>(-1),
        .tlength = static_cast<uint32_t>(-1),
        .prebuf = static_cast<uint32_t>(-1),
        .minreq = static_cast<uint32_t>(-1),
        .fragsize = m_impl->config.sample_rate * m_impl->config.channels * 
                    sizeof(int16_t) * m_impl->config.frame_size_ms / 1000
    };
    
    if (pa_stream_connect_record(m_impl->stream, source, &attr,
                                  static_cast<pa_stream_flags_t>(
                                      PA_STREAM_ADJUST_LATENCY |
                                      PA_STREAM_AUTO_TIMING_UPDATE)) < 0) {
        pa_stream_unref(m_impl->stream);
        m_impl->stream = nullptr;
        pa_threaded_mainloop_unlock(m_impl->mainloop);
        return std::unexpected(Error{ErrorCode::AudioCaptureStartFailed,
            "Failed to connect PulseAudio stream"});
    }
    
    // Wait for stream to be ready
    while (pa_stream_get_state(m_impl->stream) != PA_STREAM_READY) {
        pa_stream_state_t state = pa_stream_get_state(m_impl->stream);
        if (state == PA_STREAM_FAILED || state == PA_STREAM_TERMINATED) {
            pa_threaded_mainloop_unlock(m_impl->mainloop);
            return std::unexpected(Error{ErrorCode::AudioCaptureStartFailed,
                "PulseAudio stream failed"});
        }
        pa_threaded_mainloop_wait(m_impl->mainloop);
    }
    
    pa_threaded_mainloop_unlock(m_impl->mainloop);
    
    m_impl->running = true;
    return {};
}

void PulseAudioCapture::stop() {
    m_impl->running = false;
    m_impl->frame_cv.notify_all();
    
    if (m_impl->stream) {
        pa_threaded_mainloop_lock(m_impl->mainloop);
        pa_stream_disconnect(m_impl->stream);
        pa_stream_unref(m_impl->stream);
        m_impl->stream = nullptr;
        pa_threaded_mainloop_unlock(m_impl->mainloop);
    }
}

bool PulseAudioCapture::is_running() const {
    return m_impl->running;
}

Result<AudioFrame> PulseAudioCapture::read_frame() {
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

void PulseAudioCapture::set_frame_callback(AudioFrameCallback callback) {
    m_impl->callback = std::move(callback);
}

Result<std::vector<AudioDeviceInfo>> PulseAudioCapture::get_devices() {
    std::vector<AudioDeviceInfo> devices;
    
    // Default monitor
    AudioDeviceInfo monitor;
    monitor.id = "@DEFAULT_MONITOR@";
    monitor.name = "System Audio";
    monitor.description = "Monitor of default audio output";
    monitor.is_monitor = true;
    monitor.is_default = true;
    monitor.sample_rate = 48000;
    monitor.channels = 2;
    devices.push_back(monitor);
    
    // TODO: Enumerate actual devices via pa_context_get_source_info_list
    
    return devices;
}

Result<void> PulseAudioCapture::select_device(const std::string& device_id) {
    m_impl->selected_device = device_id;
    return {};
}

double PulseAudioCapture::get_latency_ms() const {
    return m_impl->latency_ms;
}

} // namespace stream_linux

#endif // HAVE_PULSEAUDIO
