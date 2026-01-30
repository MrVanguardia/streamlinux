/**
 * @file audio_capture.cpp
 * @brief Audio capture factory implementation
 */

#include "stream_linux/audio_capture.hpp"

namespace stream_linux {

Result<std::unique_ptr<IAudioCapture>> create_audio_capture(AudioBackend backend) {
    // Try PipeWire first (preferred)
    if (backend == AudioBackend::Auto || backend == AudioBackend::PipeWire) {
#ifdef HAVE_PIPEWIRE_AUDIO
        auto capture = std::make_unique<PipeWireAudioCapture>();
        return capture;
#endif
    }
    
    // Fall back to PulseAudio
    if (backend == AudioBackend::Auto || backend == AudioBackend::PulseAudio) {
#ifdef HAVE_PULSEAUDIO
        auto capture = std::make_unique<PulseAudioCapture>();
        return capture;
#endif
    }
    
    return std::unexpected(Error{ErrorCode::AudioInitFailed,
        "No audio backend available. Install PipeWire or PulseAudio."});
}

} // namespace stream_linux
