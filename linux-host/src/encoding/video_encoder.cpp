/**
 * @file video_encoder.cpp
 * @brief FFmpeg-based video encoder implementation
 */

#include "stream_linux/video_encoder.hpp"
#include <cstring>
#include <algorithm>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libavutil/hwcontext.h>
#ifdef HAVE_VAAPI
#include <libavutil/hwcontext_vaapi.h>
#endif
}

namespace stream_linux {

FFmpegVideoEncoder::FFmpegVideoEncoder() = default;

FFmpegVideoEncoder::~FFmpegVideoEncoder() {
    if (m_sws_ctx) {
        sws_freeContext(m_sws_ctx);
    }
    if (m_packet) {
        av_packet_free(&m_packet);
    }
    if (m_hw_frame) {
        av_frame_free(&m_hw_frame);
    }
    if (m_frame) {
        av_frame_free(&m_frame);
    }
    if (m_codec_ctx) {
        avcodec_free_context(&m_codec_ctx);
    }
    if (m_hw_frames_ctx) {
        av_buffer_unref(&m_hw_frames_ctx);
    }
    if (m_hw_device_ctx) {
        av_buffer_unref(&m_hw_device_ctx);
    }
}

Result<const AVCodec*> FFmpegVideoEncoder::find_encoder(VideoCodec codec, HardwareEncoder hw) {
    const char* codec_name = nullptr;
    
    // Try hardware encoder first
    if (hw != HardwareEncoder::None) {
        switch (codec) {
            case VideoCodec::H264:
                switch (hw) {
                    case HardwareEncoder::VAAPI:
                        codec_name = "h264_vaapi";
                        break;
                    case HardwareEncoder::NVENC:
                        codec_name = "h264_nvenc";
                        break;
                    case HardwareEncoder::AMF:
                        codec_name = "h264_amf";
                        break;
                    case HardwareEncoder::QSV:
                        codec_name = "h264_qsv";
                        break;
                    default:
                        break;
                }
                break;
            case VideoCodec::H265:
                switch (hw) {
                    case HardwareEncoder::VAAPI:
                        codec_name = "hevc_vaapi";
                        break;
                    case HardwareEncoder::NVENC:
                        codec_name = "hevc_nvenc";
                        break;
                    case HardwareEncoder::AMF:
                        codec_name = "hevc_amf";
                        break;
                    default:
                        break;
                }
                break;
            default:
                break;
        }
        
        if (codec_name) {
            const AVCodec* enc = avcodec_find_encoder_by_name(codec_name);
            if (enc) {
                return enc;
            }
        }
    }
    
    // Fall back to software encoder
    AVCodecID codec_id = AV_CODEC_ID_NONE;
    switch (codec) {
        case VideoCodec::H264:
            codec_id = AV_CODEC_ID_H264;
            break;
        case VideoCodec::H265:
            codec_id = AV_CODEC_ID_HEVC;
            break;
        case VideoCodec::VP9:
            codec_id = AV_CODEC_ID_VP9;
            break;
        case VideoCodec::AV1:
            codec_id = AV_CODEC_ID_AV1;
            break;
    }
    
    const AVCodec* enc = avcodec_find_encoder(codec_id);
    if (!enc) {
        return std::unexpected(Error{ErrorCode::EncoderNotFound,
            "No encoder found for codec"});
    }
    
    return enc;
}

Result<void> FFmpegVideoEncoder::init_hw_context(HardwareEncoder hw) {
    if (hw == HardwareEncoder::None) {
        return {};
    }
    
    AVHWDeviceType hw_type = AV_HWDEVICE_TYPE_NONE;
    switch (hw) {
        case HardwareEncoder::VAAPI:
            hw_type = AV_HWDEVICE_TYPE_VAAPI;
            break;
        case HardwareEncoder::NVENC:
            hw_type = AV_HWDEVICE_TYPE_CUDA;
            break;
        case HardwareEncoder::QSV:
            hw_type = AV_HWDEVICE_TYPE_QSV;
            break;
        default:
            return {};
    }
    
    int ret = av_hwdevice_ctx_create(&m_hw_device_ctx, hw_type, nullptr, nullptr, 0);
    if (ret < 0) {
        return std::unexpected(Error{ErrorCode::HardwareEncoderFailed,
            "Failed to create hardware device context"});
    }
    
    return {};
}

Result<void> FFmpegVideoEncoder::initialize(const VideoConfig& config) {
    m_config = config;
    
    // Determine hardware encoder
    HardwareEncoder hw = config.hw_encoder;
    if (hw == HardwareEncoder::None) {
        // Auto-detect: try VAAPI first, then NVENC
#ifdef HAVE_VAAPI
        hw = HardwareEncoder::VAAPI;
#endif
    }
    
    // Find encoder
    auto encoder_result = find_encoder(config.codec, hw);
    if (!encoder_result) {
        // Try software fallback
        encoder_result = find_encoder(config.codec, HardwareEncoder::None);
        if (!encoder_result) {
            return std::unexpected(encoder_result.error());
        }
        hw = HardwareEncoder::None;
    }
    
    const AVCodec* encoder = *encoder_result;
    m_active_hw = hw;
    
    // Initialize hardware context if needed
    if (hw != HardwareEncoder::None) {
        auto hw_result = init_hw_context(hw);
        if (!hw_result) {
            // Fall back to software
            encoder_result = find_encoder(config.codec, HardwareEncoder::None);
            if (!encoder_result) {
                return std::unexpected(hw_result.error());
            }
            encoder = *encoder_result;
            m_active_hw = HardwareEncoder::None;
        }
    }
    
    // Create codec context
    m_codec_ctx = avcodec_alloc_context3(encoder);
    if (!m_codec_ctx) {
        return std::unexpected(Error{ErrorCode::EncoderInitFailed,
            "Failed to allocate codec context"});
    }
    
    // Configure encoder
    m_codec_ctx->width = config.width;
    m_codec_ctx->height = config.height;
    m_codec_ctx->time_base = AVRational{1, static_cast<int>(config.fps)};
    m_codec_ctx->framerate = AVRational{static_cast<int>(config.fps), 1};
    m_codec_ctx->bit_rate = config.bitrate;
    m_codec_ctx->gop_size = config.gop_size;
    m_codec_ctx->max_b_frames = config.b_frames;
    
    if (m_active_hw != HardwareEncoder::None && m_hw_device_ctx) {
        m_codec_ctx->hw_device_ctx = av_buffer_ref(m_hw_device_ctx);
        
        // Set hardware pixel format
        switch (m_active_hw) {
            case HardwareEncoder::VAAPI:
                m_codec_ctx->pix_fmt = AV_PIX_FMT_VAAPI;
                break;
            case HardwareEncoder::NVENC:
                m_codec_ctx->pix_fmt = AV_PIX_FMT_CUDA;
                break;
            default:
                m_codec_ctx->pix_fmt = AV_PIX_FMT_NV12;
        }
        
        // Create hardware frames context
        m_hw_frames_ctx = av_hwframe_ctx_alloc(m_hw_device_ctx);
        if (m_hw_frames_ctx) {
            AVHWFramesContext* frames_ctx = reinterpret_cast<AVHWFramesContext*>(m_hw_frames_ctx->data);
            frames_ctx->format = m_codec_ctx->pix_fmt;
            frames_ctx->sw_format = AV_PIX_FMT_NV12;
            frames_ctx->width = config.width;
            frames_ctx->height = config.height;
            frames_ctx->initial_pool_size = 20;
            
            if (av_hwframe_ctx_init(m_hw_frames_ctx) >= 0) {
                m_codec_ctx->hw_frames_ctx = av_buffer_ref(m_hw_frames_ctx);
            }
        }
    } else {
        m_codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    }
    
    // Set low-latency options
    av_opt_set(m_codec_ctx->priv_data, "preset", "ultrafast", 0);
    av_opt_set(m_codec_ctx->priv_data, "tune", "zerolatency", 0);
    av_opt_set(m_codec_ctx->priv_data, "profile", "baseline", 0);
    
    // Open encoder
    int ret = avcodec_open2(m_codec_ctx, encoder, nullptr);
    if (ret < 0) {
        return std::unexpected(Error{ErrorCode::EncoderInitFailed,
            "Failed to open encoder"});
    }
    
    // Allocate frame and packet
    m_frame = av_frame_alloc();
    m_packet = av_packet_alloc();
    
    if (!m_frame || !m_packet) {
        return std::unexpected(Error{ErrorCode::OutOfMemory});
    }
    
    m_frame->format = (m_active_hw != HardwareEncoder::None) ? AV_PIX_FMT_NV12 : m_codec_ctx->pix_fmt;
    m_frame->width = config.width;
    m_frame->height = config.height;
    
    if (av_frame_get_buffer(m_frame, 32) < 0) {
        return std::unexpected(Error{ErrorCode::OutOfMemory,
            "Failed to allocate frame buffer"});
    }
    
    if (m_active_hw != HardwareEncoder::None && m_hw_frames_ctx) {
        m_hw_frame = av_frame_alloc();
        if (av_hwframe_get_buffer(m_codec_ctx->hw_frames_ctx, m_hw_frame, 0) < 0) {
            av_frame_free(&m_hw_frame);
            m_hw_frame = nullptr;
        }
    }
    
    m_initialized = true;
    return {};
}

Result<void> FFmpegVideoEncoder::convert_frame(const VideoFrame& src, AVFrame* dst) {
    // Determine source format
    AVPixelFormat src_fmt = AV_PIX_FMT_NONE;
    switch (src.format) {
        case PixelFormat::RGB24:
            src_fmt = AV_PIX_FMT_RGB24;
            break;
        case PixelFormat::RGBA32:
            src_fmt = AV_PIX_FMT_RGBA;
            break;
        case PixelFormat::BGR24:
            src_fmt = AV_PIX_FMT_BGR24;
            break;
        case PixelFormat::BGRA32:
            src_fmt = AV_PIX_FMT_BGRA;
            break;
        case PixelFormat::NV12:
            src_fmt = AV_PIX_FMT_NV12;
            break;
        case PixelFormat::YUV420P:
            src_fmt = AV_PIX_FMT_YUV420P;
            break;
        default:
            return std::unexpected(Error{ErrorCode::FrameConversionFailed,
                "Unsupported pixel format"});
    }
    
    // Create or update scaler
    AVPixelFormat dst_fmt = (m_active_hw != HardwareEncoder::None) ? AV_PIX_FMT_NV12 : m_codec_ctx->pix_fmt;
    
    m_sws_ctx = sws_getCachedContext(
        m_sws_ctx,
        src.width, src.height, src_fmt,
        dst->width, dst->height, dst_fmt,
        SWS_FAST_BILINEAR, nullptr, nullptr, nullptr
    );
    
    if (!m_sws_ctx) {
        return std::unexpected(Error{ErrorCode::FrameConversionFailed,
            "Failed to create scaler context"});
    }
    
    // Scale/convert
    const uint8_t* src_data[4] = {src.data.data(), nullptr, nullptr, nullptr};
    int src_linesize[4] = {static_cast<int>(src.stride), 0, 0, 0};
    
    sws_scale(m_sws_ctx, src_data, src_linesize, 0, src.height,
              dst->data, dst->linesize);
    
    return {};
}

Result<EncodedVideoFrame> FFmpegVideoEncoder::encode(const VideoFrame& frame) {
    if (!m_initialized) {
        return std::unexpected(Error{ErrorCode::NotInitialized});
    }
    
    auto start_time = Clock::now();
    
    // Convert frame
    if (av_frame_make_writable(m_frame) < 0) {
        return std::unexpected(Error{ErrorCode::EncodingFailed,
            "Failed to make frame writable"});
    }
    
    auto convert_result = convert_frame(frame, m_frame);
    if (!convert_result) {
        return std::unexpected(convert_result.error());
    }
    
    m_frame->pts = m_pts_counter++;
    
    // Handle keyframe request
    if (m_keyframe_requested) {
        m_frame->pict_type = AV_PICTURE_TYPE_I;
        m_keyframe_requested = false;
    } else {
        m_frame->pict_type = AV_PICTURE_TYPE_NONE;
    }
    
    // Upload to hardware if needed
    AVFrame* encode_frame = m_frame;
    if (m_hw_frame && m_active_hw != HardwareEncoder::None) {
        if (av_hwframe_transfer_data(m_hw_frame, m_frame, 0) >= 0) {
            m_hw_frame->pts = m_frame->pts;
            encode_frame = m_hw_frame;
        }
    }
    
    // Send frame to encoder
    int ret = avcodec_send_frame(m_codec_ctx, encode_frame);
    if (ret < 0) {
        return std::unexpected(Error{ErrorCode::EncodingFailed,
            "Failed to send frame to encoder"});
    }
    
    // Receive encoded packet
    ret = avcodec_receive_packet(m_codec_ctx, m_packet);
    if (ret == AVERROR(EAGAIN)) {
        // Need more input frames
        return std::unexpected(Error{ErrorCode::EncodingFailed,
            "Encoder needs more frames"});
    } else if (ret < 0) {
        return std::unexpected(Error{ErrorCode::EncodingFailed,
            "Failed to receive encoded packet"});
    }
    
    // Create output frame
    EncodedVideoFrame output;
    output.data.assign(m_packet->data, m_packet->data + m_packet->size);
    output.pts = frame.pts;
    output.dts = m_packet->dts;
    output.keyframe = (m_packet->flags & AV_PKT_FLAG_KEY) != 0;
    
    // Update stats
    ++m_stats.frames_encoded;
    m_stats.bytes_output += output.data.size();
    if (output.keyframe) ++m_stats.keyframes;
    
    auto encode_time = std::chrono::duration<double, std::milli>(
        Clock::now() - start_time).count();
    m_encode_times.push_back(encode_time);
    if (m_encode_times.size() > 100) {
        m_encode_times.erase(m_encode_times.begin());
    }
    
    double sum = 0;
    for (double t : m_encode_times) sum += t;
    m_stats.avg_encode_time_ms = sum / m_encode_times.size();
    
    av_packet_unref(m_packet);
    
    // Call callback if set
    if (m_callback) {
        m_callback(output);
    }
    
    return output;
}

Result<std::vector<EncodedVideoFrame>> FFmpegVideoEncoder::flush() {
    std::vector<EncodedVideoFrame> frames;
    
    avcodec_send_frame(m_codec_ctx, nullptr);
    
    while (true) {
        int ret = avcodec_receive_packet(m_codec_ctx, m_packet);
        if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) {
            break;
        }
        if (ret < 0) {
            break;
        }
        
        EncodedVideoFrame frame;
        frame.data.assign(m_packet->data, m_packet->data + m_packet->size);
        frame.pts = m_packet->pts;
        frame.dts = m_packet->dts;
        frame.keyframe = (m_packet->flags & AV_PKT_FLAG_KEY) != 0;
        
        frames.push_back(std::move(frame));
        av_packet_unref(m_packet);
    }
    
    return frames;
}

void FFmpegVideoEncoder::request_keyframe() {
    m_keyframe_requested = true;
}

Result<void> FFmpegVideoEncoder::set_bitrate(uint32_t bitrate) {
    m_config.bitrate = bitrate;
    // Note: Dynamic bitrate change requires encoder reinit for some codecs
    m_codec_ctx->bit_rate = bitrate;
    return {};
}

EncoderCapabilities FFmpegVideoEncoder::get_capabilities() const {
    EncoderCapabilities caps;
    caps.name = m_codec_ctx ? m_codec_ctx->codec->name : "unknown";
    caps.codec = m_config.codec;
    caps.hw_type = m_active_hw;
    caps.supports_b_frames = m_config.b_frames > 0;
    caps.max_width = 4096;
    caps.max_height = 4096;
    return caps;
}

EncoderStats FFmpegVideoEncoder::get_stats() const {
    return m_stats;
}

void FFmpegVideoEncoder::set_output_callback(EncodedVideoCallback callback) {
    m_callback = std::move(callback);
}

std::vector<EncoderCapabilities> get_available_encoders() {
    std::vector<EncoderCapabilities> encoders;
    
    // Check H.264 encoders
    const char* h264_names[] = {"h264_vaapi", "h264_nvenc", "h264_amf", "libx264"};
    HardwareEncoder h264_hw[] = {HardwareEncoder::VAAPI, HardwareEncoder::NVENC,
                                  HardwareEncoder::AMF, HardwareEncoder::None};
    
    for (size_t i = 0; i < 4; ++i) {
        const AVCodec* codec = avcodec_find_encoder_by_name(h264_names[i]);
        if (codec) {
            EncoderCapabilities caps;
            caps.name = h264_names[i];
            caps.codec = VideoCodec::H264;
            caps.hw_type = h264_hw[i];
            caps.max_width = 4096;
            caps.max_height = 4096;
            encoders.push_back(caps);
        }
    }
    
    return encoders;
}

Result<std::unique_ptr<IVideoEncoder>> create_video_encoder(const VideoConfig& config) {
    auto encoder = std::make_unique<FFmpegVideoEncoder>();
    
    auto result = encoder->initialize(config);
    if (!result) {
        return std::unexpected(result.error());
    }
    
    return encoder;
}

} // namespace stream_linux
