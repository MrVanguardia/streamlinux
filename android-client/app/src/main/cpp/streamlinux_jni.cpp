/**
 * @file streamlinux_jni.cpp
 * @brief JNI interface for StreamLinux Android client
 * 
 * Provides native methods for video/audio decoding and A/V synchronization
 */

#include <jni.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <android/log.h>
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaFormat.h>
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

#include <memory>
#include <atomic>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <cstring>

#define LOG_TAG "StreamLinuxJNI"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

namespace {

// ============================================================================
// Frame structures
// ============================================================================

struct VideoFrame {
    std::vector<uint8_t> data;
    int64_t pts;           // Presentation timestamp in microseconds
    bool isKeyFrame;
    int width;
    int height;
};

struct AudioFrame {
    std::vector<uint8_t> data;
    int64_t pts;           // Presentation timestamp in microseconds
    int sampleRate;
    int channels;
};

// ============================================================================
// Video Decoder using MediaCodec
// ============================================================================

class VideoDecoder {
public:
    VideoDecoder() = default;
    ~VideoDecoder() { release(); }

    bool initialize(ANativeWindow* window, int width, int height, 
                   const uint8_t* sps, size_t spsLen,
                   const uint8_t* pps, size_t ppsLen) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (m_codec) {
            release();
        }

        m_window = window;
        m_width = width;
        m_height = height;

        // Create H.264 decoder
        m_codec = AMediaCodec_createDecoderByType("video/avc");
        if (!m_codec) {
            LOGE("Failed to create H.264 decoder");
            return false;
        }

        // Configure format
        AMediaFormat* format = AMediaFormat_new();
        AMediaFormat_setString(format, AMEDIAFORMAT_KEY_MIME, "video/avc");
        AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_WIDTH, width);
        AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_HEIGHT, height);
        AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_COLOR_FORMAT, 21); // COLOR_FormatYUV420SemiPlanar
        
        // Set SPS/PPS if provided
        if (sps && spsLen > 0) {
            AMediaFormat_setBuffer(format, "csd-0", sps, spsLen);
        }
        if (pps && ppsLen > 0) {
            AMediaFormat_setBuffer(format, "csd-1", pps, ppsLen);
        }

        media_status_t status = AMediaCodec_configure(m_codec, format, window, nullptr, 0);
        AMediaFormat_delete(format);

        if (status != AMEDIA_OK) {
            LOGE("Failed to configure codec: %d", status);
            AMediaCodec_delete(m_codec);
            m_codec = nullptr;
            return false;
        }

        status = AMediaCodec_start(m_codec);
        if (status != AMEDIA_OK) {
            LOGE("Failed to start codec: %d", status);
            AMediaCodec_delete(m_codec);
            m_codec = nullptr;
            return false;
        }

        m_running = true;
        LOGI("Video decoder initialized: %dx%d", width, height);
        return true;
    }

    bool decode(const uint8_t* data, size_t size, int64_t pts, bool isKeyFrame) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (!m_codec || !m_running) {
            return false;
        }

        // Get input buffer
        ssize_t bufIdx = AMediaCodec_dequeueInputBuffer(m_codec, 10000); // 10ms timeout
        if (bufIdx < 0) {
            LOGW("No input buffer available");
            return false;
        }

        size_t bufSize = 0;
        uint8_t* buf = AMediaCodec_getInputBuffer(m_codec, bufIdx, &bufSize);
        if (!buf || bufSize < size) {
            LOGE("Input buffer too small: %zu < %zu", bufSize, size);
            return false;
        }

        memcpy(buf, data, size);
        
        uint32_t flags = isKeyFrame ? AMEDIACODEC_BUFFER_FLAG_KEY_FRAME : 0;
        AMediaCodec_queueInputBuffer(m_codec, bufIdx, 0, size, pts, flags);

        // Process output
        processOutput();
        
        return true;
    }

    void release() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_running = false;
        
        if (m_codec) {
            AMediaCodec_stop(m_codec);
            AMediaCodec_delete(m_codec);
            m_codec = nullptr;
        }
        
        m_window = nullptr;
        LOGI("Video decoder released");
    }

    int64_t getLastPts() const { return m_lastPts.load(); }

private:
    void processOutput() {
        AMediaCodecBufferInfo info;
        ssize_t outIdx = AMediaCodec_dequeueOutputBuffer(m_codec, &info, 0);
        
        while (outIdx >= 0) {
            // Render to surface
            AMediaCodec_releaseOutputBuffer(m_codec, outIdx, true);
            m_lastPts = info.presentationTimeUs;
            m_framesDecoded++;
            
            outIdx = AMediaCodec_dequeueOutputBuffer(m_codec, &info, 0);
        }
        
        if (outIdx == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
            AMediaFormat* format = AMediaCodec_getOutputFormat(m_codec);
            if (format) {
                int32_t width, height;
                AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_WIDTH, &width);
                AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_HEIGHT, &height);
                LOGI("Output format changed: %dx%d", width, height);
                AMediaFormat_delete(format);
            }
        }
    }

    AMediaCodec* m_codec = nullptr;
    ANativeWindow* m_window = nullptr;
    int m_width = 0;
    int m_height = 0;
    std::atomic<bool> m_running{false};
    std::atomic<int64_t> m_lastPts{0};
    uint64_t m_framesDecoded = 0;
    std::mutex m_mutex;
};

// ============================================================================
// Audio Decoder and Player using OpenSL ES
// ============================================================================

class AudioPlayer {
public:
    AudioPlayer() = default;
    ~AudioPlayer() { release(); }

    bool initialize(int sampleRate, int channels) {
        m_sampleRate = sampleRate;
        m_channels = channels;

        // Create engine
        SLresult result = slCreateEngine(&m_engineObj, 0, nullptr, 0, nullptr, nullptr);
        if (result != SL_RESULT_SUCCESS) {
            LOGE("Failed to create OpenSL engine");
            return false;
        }

        result = (*m_engineObj)->Realize(m_engineObj, SL_BOOLEAN_FALSE);
        if (result != SL_RESULT_SUCCESS) {
            LOGE("Failed to realize engine");
            return false;
        }

        result = (*m_engineObj)->GetInterface(m_engineObj, SL_IID_ENGINE, &m_engine);
        if (result != SL_RESULT_SUCCESS) {
            LOGE("Failed to get engine interface");
            return false;
        }

        // Create output mix
        result = (*m_engine)->CreateOutputMix(m_engine, &m_outputMixObj, 0, nullptr, nullptr);
        if (result != SL_RESULT_SUCCESS) {
            LOGE("Failed to create output mix");
            return false;
        }

        result = (*m_outputMixObj)->Realize(m_outputMixObj, SL_BOOLEAN_FALSE);
        if (result != SL_RESULT_SUCCESS) {
            LOGE("Failed to realize output mix");
            return false;
        }

        // Configure audio source (buffer queue)
        SLDataLocator_AndroidSimpleBufferQueue bufferQueue = {
            SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 
            BUFFER_COUNT
        };
        
        SLDataFormat_PCM formatPcm = {
            SL_DATAFORMAT_PCM,
            static_cast<SLuint32>(channels),
            static_cast<SLuint32>(sampleRate * 1000), // milliHz
            SL_PCMSAMPLEFORMAT_FIXED_16,
            SL_PCMSAMPLEFORMAT_FIXED_16,
            channels == 2 ? (SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT) : SL_SPEAKER_FRONT_CENTER,
            SL_BYTEORDER_LITTLEENDIAN
        };
        
        SLDataSource audioSrc = {&bufferQueue, &formatPcm};

        // Configure audio sink
        SLDataLocator_OutputMix outputMix = {SL_DATALOCATOR_OUTPUTMIX, m_outputMixObj};
        SLDataSink audioSnk = {&outputMix, nullptr};

        // Create player
        const SLInterfaceID ids[] = {SL_IID_BUFFERQUEUE, SL_IID_VOLUME};
        const SLboolean req[] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};
        
        result = (*m_engine)->CreateAudioPlayer(m_engine, &m_playerObj, &audioSrc, &audioSnk,
                                                 2, ids, req);
        if (result != SL_RESULT_SUCCESS) {
            LOGE("Failed to create audio player");
            return false;
        }

        result = (*m_playerObj)->Realize(m_playerObj, SL_BOOLEAN_FALSE);
        if (result != SL_RESULT_SUCCESS) {
            LOGE("Failed to realize player");
            return false;
        }

        result = (*m_playerObj)->GetInterface(m_playerObj, SL_IID_PLAY, &m_player);
        if (result != SL_RESULT_SUCCESS) {
            LOGE("Failed to get play interface");
            return false;
        }

        result = (*m_playerObj)->GetInterface(m_playerObj, SL_IID_BUFFERQUEUE, &m_bufferQueue);
        if (result != SL_RESULT_SUCCESS) {
            LOGE("Failed to get buffer queue interface");
            return false;
        }

        // Register callback
        result = (*m_bufferQueue)->RegisterCallback(m_bufferQueue, bufferQueueCallback, this);
        if (result != SL_RESULT_SUCCESS) {
            LOGE("Failed to register callback");
            return false;
        }

        // Initialize buffers
        m_bufferSize = (sampleRate * channels * sizeof(int16_t)) / 50; // 20ms buffers
        for (auto& buffer : m_buffers) {
            buffer.resize(m_bufferSize);
        }

        // Start playback
        result = (*m_player)->SetPlayState(m_player, SL_PLAYSTATE_PLAYING);
        if (result != SL_RESULT_SUCCESS) {
            LOGE("Failed to start playback");
            return false;
        }

        m_running = true;
        LOGI("Audio player initialized: %d Hz, %d channels", sampleRate, channels);
        return true;
    }

    void enqueue(const int16_t* data, size_t samples, int64_t pts) {
        if (!m_running) return;

        std::lock_guard<std::mutex> lock(m_queueMutex);
        
        AudioChunk chunk;
        chunk.data.assign(data, data + samples * m_channels);
        chunk.pts = pts;
        m_audioQueue.push(std::move(chunk));
    }

    void release() {
        m_running = false;

        if (m_playerObj) {
            (*m_player)->SetPlayState(m_player, SL_PLAYSTATE_STOPPED);
            (*m_playerObj)->Destroy(m_playerObj);
            m_playerObj = nullptr;
            m_player = nullptr;
            m_bufferQueue = nullptr;
        }

        if (m_outputMixObj) {
            (*m_outputMixObj)->Destroy(m_outputMixObj);
            m_outputMixObj = nullptr;
        }

        if (m_engineObj) {
            (*m_engineObj)->Destroy(m_engineObj);
            m_engineObj = nullptr;
            m_engine = nullptr;
        }

        LOGI("Audio player released");
    }

    int64_t getLastPts() const { return m_lastPts.load(); }

private:
    static void bufferQueueCallback(SLAndroidSimpleBufferQueueItf bq, void* context) {
        auto* player = static_cast<AudioPlayer*>(context);
        player->fillBuffer();
    }

    void fillBuffer() {
        if (!m_running) return;

        auto& buffer = m_buffers[m_currentBuffer];
        
        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            
            if (!m_audioQueue.empty()) {
                auto& chunk = m_audioQueue.front();
                size_t copySize = std::min(chunk.data.size() * sizeof(int16_t), buffer.size());
                memcpy(buffer.data(), chunk.data.data(), copySize);
                m_lastPts = chunk.pts;
                m_audioQueue.pop();
            } else {
                // Silence if no data
                memset(buffer.data(), 0, buffer.size());
            }
        }

        (*m_bufferQueue)->Enqueue(m_bufferQueue, buffer.data(), buffer.size());
        m_currentBuffer = (m_currentBuffer + 1) % BUFFER_COUNT;
    }

    static constexpr int BUFFER_COUNT = 4;

    struct AudioChunk {
        std::vector<int16_t> data;
        int64_t pts;
    };

    SLObjectItf m_engineObj = nullptr;
    SLEngineItf m_engine = nullptr;
    SLObjectItf m_outputMixObj = nullptr;
    SLObjectItf m_playerObj = nullptr;
    SLPlayItf m_player = nullptr;
    SLAndroidSimpleBufferQueueItf m_bufferQueue = nullptr;

    std::array<std::vector<uint8_t>, BUFFER_COUNT> m_buffers;
    size_t m_bufferSize = 0;
    int m_currentBuffer = 0;

    std::queue<AudioChunk> m_audioQueue;
    std::mutex m_queueMutex;

    int m_sampleRate = 48000;
    int m_channels = 2;
    std::atomic<bool> m_running{false};
    std::atomic<int64_t> m_lastPts{0};
};

// ============================================================================
// A/V Synchronizer
// ============================================================================

class AVSynchronizer {
public:
    void setMasterClock(int64_t pts) {
        m_masterClock = pts;
        m_clockSetTime = std::chrono::steady_clock::now();
    }

    int64_t getCurrentClock() const {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            now - m_clockSetTime).count();
        return m_masterClock + elapsed;
    }

    int64_t calculateDelay(int64_t framePts) const {
        int64_t currentClock = getCurrentClock();
        return framePts - currentClock;
    }

    bool shouldDropFrame(int64_t framePts) const {
        int64_t delay = calculateDelay(framePts);
        return delay < -LATE_THRESHOLD_US; // Drop if more than 30ms late
    }

    void reset() {
        m_masterClock = 0;
        m_clockSetTime = std::chrono::steady_clock::now();
    }

private:
    static constexpr int64_t LATE_THRESHOLD_US = 30000; // 30ms

    std::atomic<int64_t> m_masterClock{0};
    std::chrono::steady_clock::time_point m_clockSetTime;
};

// ============================================================================
// Global state
// ============================================================================

struct StreamState {
    std::unique_ptr<VideoDecoder> videoDecoder;
    std::unique_ptr<AudioPlayer> audioPlayer;
    AVSynchronizer synchronizer;
    ANativeWindow* window = nullptr;
    std::atomic<bool> connected{false};
};

std::unique_ptr<StreamState> g_state;
std::mutex g_stateMutex;

} // anonymous namespace

// ============================================================================
// JNI Methods
// ============================================================================

extern "C" {

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    LOGI("StreamLinux native library loaded");
    return JNI_VERSION_1_6;
}

JNIEXPORT void JNICALL JNI_OnUnload(JavaVM* vm, void* reserved) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    g_state.reset();
    LOGI("StreamLinux native library unloaded");
}

// Initialize streaming session
JNIEXPORT jboolean JNICALL
Java_com_streamlinux_client_NativeDecoder_initialize(
    JNIEnv* env, jobject thiz,
    jobject surface,
    jint videoWidth, jint videoHeight,
    jbyteArray sps, jbyteArray pps,
    jint audioSampleRate, jint audioChannels) {
    
    std::lock_guard<std::mutex> lock(g_stateMutex);

    // Create new state
    g_state = std::make_unique<StreamState>();

    // Get native window from surface
    if (surface) {
        g_state->window = ANativeWindow_fromSurface(env, surface);
        if (!g_state->window) {
            LOGE("Failed to get native window");
            return JNI_FALSE;
        }
    }

    // Initialize video decoder
    g_state->videoDecoder = std::make_unique<VideoDecoder>();
    
    jbyte* spsData = nullptr;
    jsize spsLen = 0;
    if (sps) {
        spsData = env->GetByteArrayElements(sps, nullptr);
        spsLen = env->GetArrayLength(sps);
    }
    
    jbyte* ppsData = nullptr;
    jsize ppsLen = 0;
    if (pps) {
        ppsData = env->GetByteArrayElements(pps, nullptr);
        ppsLen = env->GetArrayLength(pps);
    }

    bool videoOk = g_state->videoDecoder->initialize(
        g_state->window, videoWidth, videoHeight,
        reinterpret_cast<const uint8_t*>(spsData), spsLen,
        reinterpret_cast<const uint8_t*>(ppsData), ppsLen
    );

    if (spsData) env->ReleaseByteArrayElements(sps, spsData, JNI_ABORT);
    if (ppsData) env->ReleaseByteArrayElements(pps, ppsData, JNI_ABORT);

    if (!videoOk) {
        LOGE("Failed to initialize video decoder");
        return JNI_FALSE;
    }

    // Initialize audio player
    g_state->audioPlayer = std::make_unique<AudioPlayer>();
    if (!g_state->audioPlayer->initialize(audioSampleRate, audioChannels)) {
        LOGE("Failed to initialize audio player");
        return JNI_FALSE;
    }

    g_state->connected = true;
    LOGI("StreamLinux session initialized");
    return JNI_TRUE;
}

// Decode video frame
JNIEXPORT jboolean JNICALL
Java_com_streamlinux_client_NativeDecoder_decodeVideoFrame(
    JNIEnv* env, jobject thiz,
    jbyteArray data, jlong pts, jboolean isKeyFrame) {
    
    std::lock_guard<std::mutex> lock(g_stateMutex);
    
    if (!g_state || !g_state->videoDecoder) {
        return JNI_FALSE;
    }

    // Check if we should drop this frame
    if (!isKeyFrame && g_state->synchronizer.shouldDropFrame(pts)) {
        LOGD("Dropping late frame: pts=%lld", (long long)pts);
        return JNI_TRUE; // Frame handled (dropped)
    }

    jsize len = env->GetArrayLength(data);
    jbyte* bytes = env->GetByteArrayElements(data, nullptr);
    
    bool result = g_state->videoDecoder->decode(
        reinterpret_cast<const uint8_t*>(bytes), 
        len, pts, isKeyFrame
    );
    
    env->ReleaseByteArrayElements(data, bytes, JNI_ABORT);

    // Update sync clock on keyframes
    if (isKeyFrame) {
        g_state->synchronizer.setMasterClock(pts);
    }
    
    return result ? JNI_TRUE : JNI_FALSE;
}

// Decode audio frame
JNIEXPORT void JNICALL
Java_com_streamlinux_client_NativeDecoder_decodeAudioFrame(
    JNIEnv* env, jobject thiz,
    jshortArray data, jlong pts) {
    
    std::lock_guard<std::mutex> lock(g_stateMutex);
    
    if (!g_state || !g_state->audioPlayer) {
        return;
    }

    jsize samples = env->GetArrayLength(data);
    jshort* audioData = env->GetShortArrayElements(data, nullptr);
    
    g_state->audioPlayer->enqueue(audioData, samples, pts);
    
    env->ReleaseShortArrayElements(data, audioData, JNI_ABORT);
}

// Release resources
JNIEXPORT void JNICALL
Java_com_streamlinux_client_NativeDecoder_release(JNIEnv* env, jobject thiz) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    
    if (g_state) {
        g_state->connected = false;
        g_state->videoDecoder.reset();
        g_state->audioPlayer.reset();
        
        if (g_state->window) {
            ANativeWindow_release(g_state->window);
            g_state->window = nullptr;
        }
        
        g_state.reset();
    }
    
    LOGI("StreamLinux session released");
}

// Get sync info
JNIEXPORT jlong JNICALL
Java_com_streamlinux_client_NativeDecoder_getVideoLatency(JNIEnv* env, jobject thiz) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    
    if (g_state && g_state->videoDecoder) {
        return g_state->synchronizer.calculateDelay(g_state->videoDecoder->getLastPts());
    }
    return 0;
}

JNIEXPORT jlong JNICALL
Java_com_streamlinux_client_NativeDecoder_getAudioLatency(JNIEnv* env, jobject thiz) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    
    if (g_state && g_state->audioPlayer) {
        return g_state->synchronizer.calculateDelay(g_state->audioPlayer->getLastPts());
    }
    return 0;
}

JNIEXPORT jboolean JNICALL
Java_com_streamlinux_client_NativeDecoder_isConnected(JNIEnv* env, jobject thiz) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    return (g_state && g_state->connected) ? JNI_TRUE : JNI_FALSE;
}

} // extern "C"
