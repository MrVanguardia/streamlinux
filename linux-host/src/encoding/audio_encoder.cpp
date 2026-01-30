/**
 * @file audio_encoder.cpp
 * @brief Opus audio encoder implementation
 */

#include "stream_linux/audio_encoder.hpp"
#include <cstring>

namespace stream_linux {

OpusEncoder::OpusEncoder() = default;

OpusEncoder::~OpusEncoder() {
    if (m_encoder) {
        opus_encoder_destroy(m_encoder);
    }
}

Result<void> OpusEncoder::initialize(const AudioConfig& config) {
    m_config = config;
    
    int error;
    m_encoder = opus_encoder_create(
        config.sample_rate,
        config.channels,
        OPUS_APPLICATION_RESTRICTED_LOWDELAY,  // Low latency mode
        &error
    );
    
    if (error != OPUS_OK || !m_encoder) {
        return std::unexpected(Error{ErrorCode::EncoderInitFailed,
            std::string("Opus encoder init failed: ") + opus_strerror(error)});
    }
    
    // Configure for low latency
    opus_encoder_ctl(m_encoder, OPUS_SET_BITRATE(config.bitrate));
    opus_encoder_ctl(m_encoder, OPUS_SET_COMPLEXITY(5));  // Balance quality/speed
    opus_encoder_ctl(m_encoder, OPUS_SET_SIGNAL(OPUS_SIGNAL_MUSIC));
    opus_encoder_ctl(m_encoder, OPUS_SET_INBAND_FEC(0));  // Disable FEC for low latency
    opus_encoder_ctl(m_encoder, OPUS_SET_DTX(0));  // Disable DTX
    
    m_initialized = true;
    return {};
}

Result<EncodedAudioFrame> OpusEncoder::encode(const AudioFrame& frame) {
    if (!m_initialized) {
        return std::unexpected(Error{ErrorCode::NotInitialized});
    }
    
    auto start_time = Clock::now();
    
    // Opus requires specific frame sizes (2.5, 5, 10, 20, 40, 60 ms)
    // At 48kHz: 120, 240, 480, 960, 1920, 2880 samples
    uint32_t opus_frame_size = m_config.sample_rate * m_config.frame_size_ms / 1000;
    
    // If input doesn't match expected frame size, buffer it
    const float* input_data = frame.data.data();
    uint32_t input_samples = frame.samples_per_channel;
    
    if (input_samples != opus_frame_size) {
        // Simple case: use what we have (may cause issues)
        // TODO: Implement proper buffering for mismatched frame sizes
        opus_frame_size = input_samples;
    }
    
    // Allocate output buffer (max Opus packet size is ~1275 bytes per frame)
    std::vector<uint8_t> output(4000);
    
    int encoded_bytes = opus_encode_float(
        m_encoder,
        input_data,
        opus_frame_size,
        output.data(),
        static_cast<int>(output.size())
    );
    
    if (encoded_bytes < 0) {
        return std::unexpected(Error{ErrorCode::EncodingFailed,
            std::string("Opus encode failed: ") + opus_strerror(encoded_bytes)});
    }
    
    EncodedAudioFrame encoded;
    encoded.data.assign(output.begin(), output.begin() + encoded_bytes);
    encoded.pts = frame.pts;
    
    // Update stats
    ++m_stats.frames_encoded;
    m_stats.bytes_output += encoded_bytes;
    
    auto encode_time = std::chrono::duration<double, std::milli>(
        Clock::now() - start_time).count();
    // Simple moving average
    m_stats.avg_encode_time_ms = m_stats.avg_encode_time_ms * 0.9 + encode_time * 0.1;
    
    if (m_callback) {
        m_callback(encoded);
    }
    
    return encoded;
}

Result<void> OpusEncoder::set_bitrate(uint32_t bitrate) {
    if (!m_initialized) {
        return std::unexpected(Error{ErrorCode::NotInitialized});
    }
    
    int result = opus_encoder_ctl(m_encoder, OPUS_SET_BITRATE(bitrate));
    if (result != OPUS_OK) {
        return std::unexpected(Error{ErrorCode::InvalidArgument,
            "Failed to set Opus bitrate"});
    }
    
    m_config.bitrate = bitrate;
    return {};
}

AudioEncoderStats OpusEncoder::get_stats() const {
    return m_stats;
}

void OpusEncoder::set_output_callback(EncodedAudioCallback callback) {
    m_callback = std::move(callback);
}

Result<std::unique_ptr<IAudioEncoder>> create_audio_encoder(const AudioConfig& config) {
    auto encoder = std::make_unique<OpusEncoder>();
    
    auto result = encoder->initialize(config);
    if (!result) {
        return std::unexpected(result.error());
    }
    
    return encoder;
}

} // namespace stream_linux
