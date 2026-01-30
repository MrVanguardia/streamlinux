/**
 * @file video_decoder.cpp
 * @brief Hardware-accelerated video decoder using Android MediaCodec
 * 
 * Provides H.264 decoding with surface rendering for low-latency playback
 */

#include <android/log.h>
#include <android/native_window.h>
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaFormat.h>
#include <media/NdkMediaExtractor.h>

#include <atomic>
#include <mutex>
#include <vector>
#include <queue>
#include <thread>
#include <chrono>
#include <cstring>

#define LOG_TAG "VideoDecoder"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

namespace streamlinux {

/**
 * @brief NAL unit parser for H.264 stream
 */
class NALParser {
public:
    struct NALUnit {
        const uint8_t* data;
        size_t size;
        uint8_t type;
        bool isKeyFrame;
    };

    static std::vector<NALUnit> parse(const uint8_t* data, size_t size) {
        std::vector<NALUnit> units;
        
        size_t pos = 0;
        while (pos < size) {
            // Find start code (0x00 0x00 0x01 or 0x00 0x00 0x00 0x01)
            size_t startPos = findStartCode(data, size, pos);
            if (startPos == size) break;
            
            size_t dataStart = startPos;
            if (startPos >= 3 && data[startPos - 3] == 0) {
                // 4-byte start code
                dataStart = startPos + 1;
            } else {
                // 3-byte start code
                dataStart = startPos + 1;
            }
            
            // Find next start code or end
            size_t nextStart = findStartCode(data, size, dataStart);
            size_t nalEnd = nextStart;
            
            // Remove trailing zeros
            while (nalEnd > dataStart && data[nalEnd - 1] == 0) {
                nalEnd--;
            }
            
            if (nalEnd > dataStart) {
                NALUnit unit;
                unit.data = data + dataStart;
                unit.size = nalEnd - dataStart;
                unit.type = data[dataStart] & 0x1F;
                unit.isKeyFrame = (unit.type == 5); // IDR slice
                units.push_back(unit);
            }
            
            pos = nextStart;
        }
        
        return units;
    }

    static bool isSPS(uint8_t nalType) { return nalType == 7; }
    static bool isPPS(uint8_t nalType) { return nalType == 8; }
    static bool isIDR(uint8_t nalType) { return nalType == 5; }
    static bool isSlice(uint8_t nalType) { return nalType == 1 || nalType == 5; }

private:
    static size_t findStartCode(const uint8_t* data, size_t size, size_t start) {
        for (size_t i = start; i + 2 < size; i++) {
            if (data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 1) {
                return i + 3;
            }
        }
        return size;
    }
};

/**
 * @brief Video decoder statistics
 */
struct DecoderStats {
    std::atomic<uint64_t> framesDecoded{0};
    std::atomic<uint64_t> framesDropped{0};
    std::atomic<uint64_t> bytesReceived{0};
    std::atomic<int64_t> lastPts{0};
    std::atomic<int64_t> decodeLatency{0};
    
    void reset() {
        framesDecoded = 0;
        framesDropped = 0;
        bytesReceived = 0;
        lastPts = 0;
        decodeLatency = 0;
    }
};

/**
 * @brief Hardware-accelerated H.264 video decoder
 */
class VideoDecoder {
public:
    VideoDecoder() = default;
    
    ~VideoDecoder() {
        release();
    }

    /**
     * @brief Initialize decoder with output surface
     */
    bool initialize(ANativeWindow* surface, int width, int height) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (m_initialized) {
            LOGW("Decoder already initialized");
            return true;
        }

        m_surface = surface;
        m_width = width;
        m_height = height;

        // Create decoder
        m_codec = AMediaCodec_createDecoderByType("video/avc");
        if (!m_codec) {
            LOGE("Failed to create H.264 decoder");
            return false;
        }

        LOGI("Video decoder created: %dx%d", width, height);
        return true;
    }

    /**
     * @brief Configure decoder with SPS/PPS
     */
    bool configure(const uint8_t* sps, size_t spsLen, 
                   const uint8_t* pps, size_t ppsLen) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (!m_codec) {
            LOGE("Decoder not created");
            return false;
        }

        // Create format
        AMediaFormat* format = AMediaFormat_new();
        AMediaFormat_setString(format, AMEDIAFORMAT_KEY_MIME, "video/avc");
        AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_WIDTH, m_width);
        AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_HEIGHT, m_height);
        AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_MAX_INPUT_SIZE, m_width * m_height);
        
        // Low latency mode
        AMediaFormat_setInt32(format, "low-latency", 1);
        AMediaFormat_setInt32(format, "priority", 0); // Real-time priority

        // Set codec-specific data (SPS/PPS)
        if (sps && spsLen > 0) {
            // Add start code prefix for csd-0
            std::vector<uint8_t> spsWithPrefix(4 + spsLen);
            spsWithPrefix[0] = 0x00;
            spsWithPrefix[1] = 0x00;
            spsWithPrefix[2] = 0x00;
            spsWithPrefix[3] = 0x01;
            memcpy(spsWithPrefix.data() + 4, sps, spsLen);
            AMediaFormat_setBuffer(format, "csd-0", spsWithPrefix.data(), spsWithPrefix.size());
            
            m_sps.assign(sps, sps + spsLen);
        }
        
        if (pps && ppsLen > 0) {
            // Add start code prefix for csd-1
            std::vector<uint8_t> ppsWithPrefix(4 + ppsLen);
            ppsWithPrefix[0] = 0x00;
            ppsWithPrefix[1] = 0x00;
            ppsWithPrefix[2] = 0x00;
            ppsWithPrefix[3] = 0x01;
            memcpy(ppsWithPrefix.data() + 4, pps, ppsLen);
            AMediaFormat_setBuffer(format, "csd-1", ppsWithPrefix.data(), ppsWithPrefix.size());
            
            m_pps.assign(pps, pps + ppsLen);
        }

        // Configure codec
        media_status_t status = AMediaCodec_configure(m_codec, format, m_surface, nullptr, 0);
        AMediaFormat_delete(format);

        if (status != AMEDIA_OK) {
            LOGE("Failed to configure codec: %d", status);
            return false;
        }

        // Start codec
        status = AMediaCodec_start(m_codec);
        if (status != AMEDIA_OK) {
            LOGE("Failed to start codec: %d", status);
            return false;
        }

        m_initialized = true;
        m_running = true;
        LOGI("Video decoder configured and started");
        return true;
    }

    /**
     * @brief Decode a video frame
     */
    bool decode(const uint8_t* data, size_t size, int64_t pts, bool isKeyFrame) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (!m_initialized || !m_running) {
            return false;
        }

        auto startTime = std::chrono::steady_clock::now();
        m_stats.bytesReceived += size;

        // Get input buffer
        ssize_t bufIdx = AMediaCodec_dequeueInputBuffer(m_codec, INPUT_TIMEOUT_US);
        if (bufIdx < 0) {
            if (bufIdx == AMEDIACODEC_INFO_TRY_AGAIN_LATER) {
                LOGW("No input buffer available, dropping frame");
                m_stats.framesDropped++;
                return false;
            }
            LOGE("Error getting input buffer: %zd", bufIdx);
            return false;
        }

        // Copy data to input buffer
        size_t bufSize = 0;
        uint8_t* buf = AMediaCodec_getInputBuffer(m_codec, bufIdx, &bufSize);
        if (!buf || bufSize < size) {
            LOGE("Input buffer too small: %zu < %zu", bufSize, size);
            AMediaCodec_queueInputBuffer(m_codec, bufIdx, 0, 0, 0, 0);
            return false;
        }

        memcpy(buf, data, size);

        // Queue input buffer
        uint32_t flags = 0;
        if (isKeyFrame) {
            flags |= AMEDIACODEC_BUFFER_FLAG_KEY_FRAME;
        }

        media_status_t status = AMediaCodec_queueInputBuffer(
            m_codec, bufIdx, 0, size, pts, flags
        );

        if (status != AMEDIA_OK) {
            LOGE("Failed to queue input buffer: %d", status);
            return false;
        }

        // Process output buffers
        processOutputBuffers();

        // Update stats
        auto endTime = std::chrono::steady_clock::now();
        m_stats.decodeLatency = std::chrono::duration_cast<std::chrono::microseconds>(
            endTime - startTime).count();
        m_stats.lastPts = pts;

        return true;
    }

    /**
     * @brief Flush decoder (call on seek or reset)
     */
    void flush() {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (m_codec) {
            AMediaCodec_flush(m_codec);
            LOGI("Decoder flushed");
        }
    }

    /**
     * @brief Release decoder resources
     */
    void release() {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        m_running = false;
        
        if (m_codec) {
            AMediaCodec_stop(m_codec);
            AMediaCodec_delete(m_codec);
            m_codec = nullptr;
        }

        m_surface = nullptr;
        m_initialized = false;
        m_sps.clear();
        m_pps.clear();
        m_stats.reset();

        LOGI("Video decoder released");
    }

    // Getters
    bool isInitialized() const { return m_initialized; }
    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }
    const DecoderStats& getStats() const { return m_stats; }

private:
    static constexpr int64_t INPUT_TIMEOUT_US = 10000;  // 10ms
    static constexpr int64_t OUTPUT_TIMEOUT_US = 0;      // Non-blocking

    void processOutputBuffers() {
        AMediaCodecBufferInfo info;
        
        while (true) {
            ssize_t outIdx = AMediaCodec_dequeueOutputBuffer(m_codec, &info, OUTPUT_TIMEOUT_US);
            
            if (outIdx >= 0) {
                // Render frame to surface
                bool render = (info.size > 0);
                AMediaCodec_releaseOutputBuffer(m_codec, outIdx, render);
                
                if (render) {
                    m_stats.framesDecoded++;
                }
            } else if (outIdx == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
                AMediaFormat* format = AMediaCodec_getOutputFormat(m_codec);
                if (format) {
                    int32_t width = 0, height = 0;
                    AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_WIDTH, &width);
                    AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_HEIGHT, &height);
                    LOGI("Output format changed: %dx%d", width, height);
                    
                    if (width > 0 && height > 0) {
                        m_width = width;
                        m_height = height;
                    }
                    AMediaFormat_delete(format);
                }
            } else if (outIdx == AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED) {
                // Output buffers changed (API < 21, rarely happens)
                LOGD("Output buffers changed");
            } else {
                // No more output available
                break;
            }
        }
    }

    AMediaCodec* m_codec = nullptr;
    ANativeWindow* m_surface = nullptr;
    
    int m_width = 0;
    int m_height = 0;
    
    std::vector<uint8_t> m_sps;
    std::vector<uint8_t> m_pps;
    
    std::atomic<bool> m_initialized{false};
    std::atomic<bool> m_running{false};
    
    DecoderStats m_stats;
    mutable std::mutex m_mutex;
};

} // namespace streamlinux
