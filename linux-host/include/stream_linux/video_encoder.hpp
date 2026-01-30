#pragma once

/**
 * @file video_encoder.hpp
 * @brief Video encoding interface with hardware acceleration support
 * 
 * Supported encoders:
 * - VAAPI (Intel/AMD)
 * - NVENC (NVIDIA)
 * - AMF (AMD)
 * - Software (x264/FFmpeg fallback)
 * 
 * Codecs:
 * - H.264 (required)
 * - H.265/HEVC (optional)
 * - AV1 (optional, future)
 */

#include "common.hpp"
#include <memory>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext.h>
#include <libswscale/swscale.h>
}

namespace stream_linux {

/**
 * @brief Encoder capability information
 */
struct EncoderCapabilities {
    std::string name;
    VideoCodec codec;
    HardwareEncoder hw_type;
    bool supports_b_frames;
    uint32_t max_width;
    uint32_t max_height;
    std::vector<PixelFormat> supported_formats;
};

/**
 * @brief Encoding statistics
 */
struct EncoderStats {
    uint64_t frames_encoded = 0;
    uint64_t bytes_output = 0;
    double avg_encode_time_ms = 0;
    double current_bitrate = 0;
    uint32_t keyframes = 0;
};

/**
 * @brief Video encoder interface
 */
class IVideoEncoder {
public:
    virtual ~IVideoEncoder() = default;
    
    /**
     * @brief Get encoder information
     */
    [[nodiscard]] virtual EncoderCapabilities get_capabilities() const = 0;
    
    /**
     * @brief Initialize encoder with configuration
     */
    [[nodiscard]] virtual Result<void> initialize(const VideoConfig& config) = 0;
    
    /**
     * @brief Encode a video frame
     * @param frame Raw video frame
     * @return Encoded frame or error
     */
    [[nodiscard]] virtual Result<EncodedVideoFrame> encode(const VideoFrame& frame) = 0;
    
    /**
     * @brief Flush encoder (get remaining frames)
     * @return Vector of remaining encoded frames
     */
    [[nodiscard]] virtual Result<std::vector<EncodedVideoFrame>> flush() = 0;
    
    /**
     * @brief Request a keyframe on next encode
     */
    virtual void request_keyframe() = 0;
    
    /**
     * @brief Update bitrate dynamically
     * @param bitrate New bitrate in bits/sec
     */
    [[nodiscard]] virtual Result<void> set_bitrate(uint32_t bitrate) = 0;
    
    /**
     * @brief Get encoding statistics
     */
    [[nodiscard]] virtual EncoderStats get_stats() const = 0;
    
    /**
     * @brief Set callback for encoded frames
     */
    virtual void set_output_callback(EncodedVideoCallback callback) = 0;
};

/**
 * @brief FFmpeg-based video encoder
 */
class FFmpegVideoEncoder : public IVideoEncoder {
public:
    FFmpegVideoEncoder();
    ~FFmpegVideoEncoder() override;
    
    [[nodiscard]] EncoderCapabilities get_capabilities() const override;
    [[nodiscard]] Result<void> initialize(const VideoConfig& config) override;
    [[nodiscard]] Result<EncodedVideoFrame> encode(const VideoFrame& frame) override;
    [[nodiscard]] Result<std::vector<EncodedVideoFrame>> flush() override;
    void request_keyframe() override;
    [[nodiscard]] Result<void> set_bitrate(uint32_t bitrate) override;
    [[nodiscard]] EncoderStats get_stats() const override;
    void set_output_callback(EncodedVideoCallback callback) override;

private:
    /**
     * @brief Find best available encoder for codec
     */
    [[nodiscard]] Result<const AVCodec*> find_encoder(VideoCodec codec, HardwareEncoder hw);
    
    /**
     * @brief Initialize hardware context if needed
     */
    [[nodiscard]] Result<void> init_hw_context(HardwareEncoder hw);
    
    /**
     * @brief Convert frame to encoder format
     */
    [[nodiscard]] Result<void> convert_frame(const VideoFrame& src, AVFrame* dst);
    
    // FFmpeg resources
    AVCodecContext* m_codec_ctx = nullptr;
    AVFrame* m_frame = nullptr;
    AVFrame* m_hw_frame = nullptr;
    AVPacket* m_packet = nullptr;
    SwsContext* m_sws_ctx = nullptr;
    AVBufferRef* m_hw_device_ctx = nullptr;
    AVBufferRef* m_hw_frames_ctx = nullptr;
    
    // Configuration
    VideoConfig m_config;
    HardwareEncoder m_active_hw = HardwareEncoder::None;
    
    // State
    bool m_initialized = false;
    bool m_keyframe_requested = false;
    int64_t m_pts_counter = 0;
    
    // Statistics
    EncoderStats m_stats;
    std::vector<double> m_encode_times;
    
    // Callback
    EncodedVideoCallback m_callback;
};

/**
 * @brief Query available video encoders
 * @return List of available encoder capabilities
 */
[[nodiscard]] std::vector<EncoderCapabilities> get_available_encoders();

/**
 * @brief Create video encoder with automatic hardware selection
 * @param config Video configuration
 * @return Encoder instance or error
 */
[[nodiscard]] Result<std::unique_ptr<IVideoEncoder>> create_video_encoder(
    const VideoConfig& config
);

} // namespace stream_linux
