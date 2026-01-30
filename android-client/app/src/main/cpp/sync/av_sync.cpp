/**
 * @file av_sync.cpp
 * @brief Audio/Video synchronization for low-latency streaming
 * 
 * Implements master clock synchronization with drift correction
 * and adaptive buffering for smooth playback
 */

#include <android/log.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <mutex>
#include <deque>
#include <algorithm>

#define LOG_TAG "AVSync"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

namespace streamlinux {

/**
 * @brief Clock source type for synchronization
 */
enum class ClockSource {
    Audio,      // Use audio as master clock
    Video,      // Use video as master clock
    External    // Use external/system clock
};

/**
 * @brief Synchronization statistics
 */
struct SyncStats {
    std::atomic<int64_t> videoPts{0};
    std::atomic<int64_t> audioPts{0};
    std::atomic<int64_t> avDrift{0};          // Audio-video drift in microseconds
    std::atomic<int64_t> networkJitter{0};     // Estimated network jitter
    std::atomic<uint64_t> framesDropped{0};
    std::atomic<uint64_t> framesRepeated{0};
    std::atomic<double> playbackSpeed{1.0};
    
    void reset() {
        videoPts = 0;
        audioPts = 0;
        avDrift = 0;
        networkJitter = 0;
        framesDropped = 0;
        framesRepeated = 0;
        playbackSpeed = 1.0;
    }
};

/**
 * @brief Jitter buffer for smooth playback
 */
class JitterBuffer {
public:
    explicit JitterBuffer(size_t maxSize = 10) : m_maxSize(maxSize) {}

    void addSample(int64_t timestamp) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto now = std::chrono::steady_clock::now();
        int64_t arrivalTime = std::chrono::duration_cast<std::chrono::microseconds>(
            now.time_since_epoch()).count();

        m_samples.push_back({timestamp, arrivalTime});
        
        if (m_samples.size() > m_maxSize) {
            m_samples.pop_front();
        }
        
        updateJitterEstimate();
    }

    int64_t getJitterEstimate() const {
        return m_jitterEstimate.load();
    }

    int64_t getOptimalBufferDelay() const {
        // Buffer should be 2x jitter for smooth playback
        return m_jitterEstimate.load() * 2;
    }

    void reset() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_samples.clear();
        m_jitterEstimate = 0;
    }

private:
    struct Sample {
        int64_t pts;
        int64_t arrivalTime;
    };

    void updateJitterEstimate() {
        if (m_samples.size() < 2) return;

        // Calculate interarrival jitter (RFC 3550 style)
        double jitter = 0;
        for (size_t i = 1; i < m_samples.size(); i++) {
            int64_t expectedDelta = m_samples[i].pts - m_samples[i-1].pts;
            int64_t actualDelta = m_samples[i].arrivalTime - m_samples[i-1].arrivalTime;
            double deviation = std::abs(actualDelta - expectedDelta);
            
            // Exponential moving average
            jitter = jitter * 0.9375 + deviation * 0.0625;
        }
        
        m_jitterEstimate = static_cast<int64_t>(jitter);
    }

    std::deque<Sample> m_samples;
    size_t m_maxSize;
    std::atomic<int64_t> m_jitterEstimate{0};
    mutable std::mutex m_mutex;
};

/**
 * @brief Master clock for A/V synchronization
 */
class MasterClock {
public:
    MasterClock() {
        reset();
    }

    void setTime(int64_t pts) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_basePts = pts;
        m_baseTime = std::chrono::steady_clock::now();
        m_speed = 1.0;
    }

    int64_t getTime() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            now - m_baseTime).count();
        return m_basePts + static_cast<int64_t>(elapsed * m_speed);
    }

    void adjustSpeed(double factor) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // Update base values before changing speed
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            now - m_baseTime).count();
        m_basePts += static_cast<int64_t>(elapsed * m_speed);
        m_baseTime = now;
        
        // Apply speed adjustment with limits
        m_speed = std::clamp(factor, 0.9, 1.1);
    }

    double getSpeed() const {
        return m_speed.load();
    }

    void reset() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_basePts = 0;
        m_baseTime = std::chrono::steady_clock::now();
        m_speed = 1.0;
    }

private:
    int64_t m_basePts = 0;
    std::chrono::steady_clock::time_point m_baseTime;
    std::atomic<double> m_speed{1.0};
    mutable std::mutex m_mutex;
};

/**
 * @brief A/V Synchronizer with adaptive buffering
 */
class AVSynchronizer {
public:
    static constexpr int64_t SYNC_THRESHOLD_US = 40000;     // 40ms - acceptable drift
    static constexpr int64_t DROP_THRESHOLD_US = 100000;    // 100ms - drop video frame
    static constexpr int64_t REPEAT_THRESHOLD_US = -40000;  // -40ms - repeat video frame

    AVSynchronizer() {
        m_stats.reset();
    }

    /**
     * @brief Set synchronization source
     */
    void setClockSource(ClockSource source) {
        m_clockSource = source;
        LOGI("Clock source set to: %d", static_cast<int>(source));
    }

    /**
     * @brief Update video timestamp
     */
    void updateVideoTime(int64_t pts) {
        m_stats.videoPts = pts;
        m_videoJitter.addSample(pts);
        
        if (m_clockSource == ClockSource::Video) {
            m_masterClock.setTime(pts);
        }
        
        updateDrift();
    }

    /**
     * @brief Update audio timestamp
     */
    void updateAudioTime(int64_t pts) {
        m_stats.audioPts = pts;
        m_audioJitter.addSample(pts);
        
        if (m_clockSource == ClockSource::Audio) {
            m_masterClock.setTime(pts);
        }
        
        updateDrift();
    }

    /**
     * @brief Check if video frame should be displayed
     * @return Action to take: 0 = display, -1 = drop, 1 = delay
     */
    int checkVideoSync(int64_t framePts) {
        int64_t masterTime = m_masterClock.getTime();
        int64_t diff = framePts - masterTime;
        
        if (diff > REPEAT_THRESHOLD_US && diff < SYNC_THRESHOLD_US) {
            // Frame is on time
            return 0;
        } else if (diff <= REPEAT_THRESHOLD_US) {
            // Frame is early, wait
            return 1;
        } else if (diff > DROP_THRESHOLD_US) {
            // Frame is too late, drop
            m_stats.framesDropped++;
            return -1;
        }
        
        // Within acceptable range
        return 0;
    }

    /**
     * @brief Calculate delay before displaying video frame
     */
    int64_t calculateVideoDelay(int64_t framePts) {
        int64_t masterTime = m_masterClock.getTime();
        int64_t diff = framePts - masterTime;
        
        if (diff > 0) {
            // Add jitter buffer delay
            return diff + m_videoJitter.getOptimalBufferDelay();
        }
        
        return 0;
    }

    /**
     * @brief Get current A/V drift
     */
    int64_t getDrift() const {
        return m_stats.avDrift.load();
    }

    /**
     * @brief Perform drift correction
     */
    void correctDrift() {
        int64_t drift = m_stats.avDrift.load();
        
        if (std::abs(drift) > SYNC_THRESHOLD_US) {
            // Adjust playback speed to compensate
            double correction = 1.0;
            if (drift > 0) {
                // Video ahead of audio, slow down video
                correction = 0.98;
            } else {
                // Audio ahead of video, speed up video
                correction = 1.02;
            }
            
            m_masterClock.adjustSpeed(correction);
            m_stats.playbackSpeed = correction;
            
            LOGD("Drift correction: drift=%lld us, speed=%.3f", 
                 (long long)drift, correction);
        } else {
            m_masterClock.adjustSpeed(1.0);
            m_stats.playbackSpeed = 1.0;
        }
    }

    /**
     * @brief Reset synchronization state
     */
    void reset() {
        m_masterClock.reset();
        m_videoJitter.reset();
        m_audioJitter.reset();
        m_stats.reset();
        LOGI("Synchronizer reset");
    }

    /**
     * @brief Get synchronization statistics
     */
    const SyncStats& getStats() const {
        return m_stats;
    }

    /**
     * @brief Get master clock time
     */
    int64_t getMasterTime() const {
        return m_masterClock.getTime();
    }

private:
    void updateDrift() {
        int64_t videoPts = m_stats.videoPts.load();
        int64_t audioPts = m_stats.audioPts.load();
        
        if (videoPts > 0 && audioPts > 0) {
            m_stats.avDrift = videoPts - audioPts;
        }
        
        // Update jitter stats
        m_stats.networkJitter = std::max(
            m_videoJitter.getJitterEstimate(),
            m_audioJitter.getJitterEstimate()
        );
    }

    ClockSource m_clockSource = ClockSource::Audio;
    MasterClock m_masterClock;
    JitterBuffer m_videoJitter{20};
    JitterBuffer m_audioJitter{50};
    SyncStats m_stats;
};

/**
 * @brief Adaptive buffer manager for latency optimization
 */
class AdaptiveBuffer {
public:
    static constexpr size_t MIN_BUFFER_MS = 20;
    static constexpr size_t MAX_BUFFER_MS = 200;
    static constexpr size_t TARGET_BUFFER_MS = 50;

    AdaptiveBuffer() : m_bufferSize(TARGET_BUFFER_MS) {}

    /**
     * @brief Update buffer size based on network conditions
     */
    void update(int64_t jitter, int64_t packetLoss) {
        // Increase buffer if high jitter or packet loss
        if (jitter > 20000 || packetLoss > 1) {
            m_bufferSize = std::min(m_bufferSize + 10, MAX_BUFFER_MS);
        } 
        // Decrease buffer if conditions are good
        else if (jitter < 5000 && packetLoss == 0) {
            m_bufferSize = std::max(m_bufferSize - 5, MIN_BUFFER_MS);
        }
    }

    /**
     * @brief Get current buffer size in milliseconds
     */
    size_t getBufferSize() const {
        return m_bufferSize;
    }

    /**
     * @brief Get buffer size in microseconds
     */
    int64_t getBufferSizeUs() const {
        return static_cast<int64_t>(m_bufferSize) * 1000;
    }

    void reset() {
        m_bufferSize = TARGET_BUFFER_MS;
    }

private:
    size_t m_bufferSize;
};

/**
 * @brief Lip sync monitor for quality measurement
 */
class LipSyncMonitor {
public:
    void addMeasurement(int64_t avDrift) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        m_measurements.push_back(std::abs(avDrift));
        if (m_measurements.size() > MAX_MEASUREMENTS) {
            m_measurements.pop_front();
        }
    }

    /**
     * @brief Check if lip sync is acceptable
     * ITU-R BT.1359-1: ±40ms for imperceptible, ±80ms for acceptable
     */
    bool isAcceptable() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (m_measurements.empty()) return true;
        
        int64_t avg = 0;
        for (auto m : m_measurements) {
            avg += m;
        }
        avg /= m_measurements.size();
        
        return avg < 80000; // 80ms threshold
    }

    /**
     * @brief Get average drift
     */
    int64_t getAverageDrift() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (m_measurements.empty()) return 0;
        
        int64_t sum = 0;
        for (auto m : m_measurements) {
            sum += m;
        }
        return sum / m_measurements.size();
    }

private:
    static constexpr size_t MAX_MEASUREMENTS = 100;
    
    std::deque<int64_t> m_measurements;
    mutable std::mutex m_mutex;
};

} // namespace streamlinux
