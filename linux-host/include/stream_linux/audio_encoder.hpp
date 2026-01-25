#pragma once

/**
 * @file audio_encoder.hpp
 * @brief Audio encoding using Opus codec
 * 
 * Features:
 * - Opus encoding optimized for low latency
 * - Variable bitrate support
 * - Multiple frame sizes (2.5ms - 60ms)
 */

#include "common.hpp"
#include <memory>
#include <opus/opus.h>

namespace stream_linux {

/**
 * @brief Audio encoder statistics
 */
struct AudioEncoderStats {
    uint64_t frames_encoded = 0;
    uint64_t bytes_output = 0;
    double avg_encode_time_ms = 0;
    double current_bitrate = 0;
};

/**
 * @brief Audio encoder interface
 */
class IAudioEncoder {
public:
    virtual ~IAudioEncoder() = default;
    
    /**
     * @brief Initialize encoder
     */
    [[nodiscard]] virtual Result<void> initialize(const AudioConfig& config) = 0;
    
    /**
     * @brief Encode audio frame
     */
    [[nodiscard]] virtual Result<EncodedAudioFrame> encode(const AudioFrame& frame) = 0;
    
    /**
     * @brief Set bitrate dynamically
     */
    [[nodiscard]] virtual Result<void> set_bitrate(uint32_t bitrate) = 0;
    
    /**
     * @brief Get encoding statistics
     */
    [[nodiscard]] virtual AudioEncoderStats get_stats() const = 0;
    
    /**
     * @brief Set output callback
     */
    virtual void set_output_callback(EncodedAudioCallback callback) = 0;
};

/**
 * @brief Opus audio encoder
 */
class OpusEncoder : public IAudioEncoder {
public:
    OpusEncoder();
    ~OpusEncoder() override;
    
    [[nodiscard]] Result<void> initialize(const AudioConfig& config) override;
    [[nodiscard]] Result<EncodedAudioFrame> encode(const AudioFrame& frame) override;
    [[nodiscard]] Result<void> set_bitrate(uint32_t bitrate) override;
    [[nodiscard]] AudioEncoderStats get_stats() const override;
    void set_output_callback(EncodedAudioCallback callback) override;

private:
    ::OpusEncoder* m_encoder = nullptr;
    AudioConfig m_config;
    bool m_initialized = false;
    
    // Resampling buffer for non-standard frame sizes
    std::vector<float> m_resample_buffer;
    
    // Statistics
    AudioEncoderStats m_stats;
    
    // Callback
    EncodedAudioCallback m_callback;
};

/**
 * @brief Create audio encoder
 * @param config Audio configuration
 * @return Encoder instance or error
 */
[[nodiscard]] Result<std::unique_ptr<IAudioEncoder>> create_audio_encoder(
    const AudioConfig& config
);

} // namespace stream_linux
