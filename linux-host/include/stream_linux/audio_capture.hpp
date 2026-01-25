#pragma once

/**
 * @file audio_capture.hpp
 * @brief Audio capture interface and implementations
 * 
 * Supports:
 * - PipeWire (primary)
 * - PulseAudio (fallback)
 * 
 * Capture modes:
 * - System audio (monitor source)
 * - Microphone
 * - Mixed
 */

#include "common.hpp"
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>

namespace stream_linux {

/**
 * @brief Audio device information
 */
struct AudioDeviceInfo {
    std::string id;
    std::string name;
    std::string description;
    bool is_monitor = false;  // true for system audio output
    bool is_default = false;
    uint32_t sample_rate = 0;
    uint32_t channels = 0;
};

/**
 * @brief Audio capture backend type
 */
enum class AudioBackend {
    Auto,
    PipeWire,
    PulseAudio
};

/**
 * @brief Abstract interface for audio capture
 */
class IAudioCapture {
public:
    virtual ~IAudioCapture() = default;
    
    /**
     * @brief Get backend type
     */
    [[nodiscard]] virtual AudioBackend get_backend() const = 0;
    
    /**
     * @brief Initialize audio capture
     * @param config Audio configuration
     * @return Success or error
     */
    [[nodiscard]] virtual Result<void> initialize(const AudioConfig& config) = 0;
    
    /**
     * @brief Start capturing audio
     */
    [[nodiscard]] virtual Result<void> start() = 0;
    
    /**
     * @brief Stop capturing audio
     */
    virtual void stop() = 0;
    
    /**
     * @brief Check if capture is running
     */
    [[nodiscard]] virtual bool is_running() const = 0;
    
    /**
     * @brief Read audio frame (blocking)
     * @return Audio frame or error
     */
    [[nodiscard]] virtual Result<AudioFrame> read_frame() = 0;
    
    /**
     * @brief Set callback for received audio frames
     */
    virtual void set_frame_callback(AudioFrameCallback callback) = 0;
    
    /**
     * @brief Get available audio devices
     */
    [[nodiscard]] virtual Result<std::vector<AudioDeviceInfo>> get_devices() = 0;
    
    /**
     * @brief Select specific device for capture
     * @param device_id Device ID from get_devices()
     */
    [[nodiscard]] virtual Result<void> select_device(const std::string& device_id) = 0;
    
    /**
     * @brief Get current capture latency in milliseconds
     */
    [[nodiscard]] virtual double get_latency_ms() const = 0;
};

/**
 * @brief Factory to create audio capture backend
 * @param backend Preferred backend (Auto = try PipeWire first)
 * @return Audio capture instance or error
 */
[[nodiscard]] Result<std::unique_ptr<IAudioCapture>> create_audio_capture(
    AudioBackend backend = AudioBackend::Auto
);

#ifdef HAVE_PIPEWIRE_AUDIO

/**
 * @brief PipeWire audio capture implementation
 */
class PipeWireAudioCapture : public IAudioCapture {
public:
    PipeWireAudioCapture();
    ~PipeWireAudioCapture() override;
    
    [[nodiscard]] AudioBackend get_backend() const override { return AudioBackend::PipeWire; }
    [[nodiscard]] Result<void> initialize(const AudioConfig& config) override;
    [[nodiscard]] Result<void> start() override;
    void stop() override;
    [[nodiscard]] bool is_running() const override;
    [[nodiscard]] Result<AudioFrame> read_frame() override;
    void set_frame_callback(AudioFrameCallback callback) override;
    [[nodiscard]] Result<std::vector<AudioDeviceInfo>> get_devices() override;
    [[nodiscard]] Result<void> select_device(const std::string& device_id) override;
    [[nodiscard]] double get_latency_ms() const override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

#endif // HAVE_PIPEWIRE_AUDIO

#ifdef HAVE_PULSEAUDIO

/**
 * @brief PulseAudio capture implementation (fallback)
 */
class PulseAudioCapture : public IAudioCapture {
public:
    PulseAudioCapture();
    ~PulseAudioCapture() override;
    
    [[nodiscard]] AudioBackend get_backend() const override { return AudioBackend::PulseAudio; }
    [[nodiscard]] Result<void> initialize(const AudioConfig& config) override;
    [[nodiscard]] Result<void> start() override;
    void stop() override;
    [[nodiscard]] bool is_running() const override;
    [[nodiscard]] Result<AudioFrame> read_frame() override;
    void set_frame_callback(AudioFrameCallback callback) override;
    [[nodiscard]] Result<std::vector<AudioDeviceInfo>> get_devices() override;
    [[nodiscard]] Result<void> select_device(const std::string& device_id) override;
    [[nodiscard]] double get_latency_ms() const override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

#endif // HAVE_PULSEAUDIO

} // namespace stream_linux
