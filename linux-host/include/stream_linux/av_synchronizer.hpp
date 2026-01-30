#pragma once

/**
 * @file av_synchronizer.hpp
 * @brief Audio/Video synchronization engine
 * 
 * Features:
 * - Monotonic timestamps
 * - Jitter compensation
 * - Drift correction
 * - Strict lip sync
 */

#include "common.hpp"
#include <mutex>
#include <atomic>
#include <deque>
#include <condition_variable>

namespace stream_linux {

/**
 * @brief Synchronization statistics
 */
struct SyncStats {
    int64_t audio_video_offset_us = 0;  // Positive = audio ahead
    double audio_drift_ppm = 0;          // Parts per million
    double video_drift_ppm = 0;
    uint64_t frames_dropped = 0;
    uint64_t frames_duplicated = 0;
};

/**
 * @brief Synchronized frame pair
 */
struct SyncedFrames {
    std::optional<EncodedVideoFrame> video;
    std::optional<EncodedAudioFrame> audio;
    PTS presentation_time;
    bool video_valid = false;
    bool audio_valid = false;
};

/**
 * @brief Configuration for A/V synchronizer
 */
struct SyncConfig {
    // Target A/V offset in microseconds (0 = perfect sync)
    int64_t target_offset_us = 0;
    
    // Maximum allowed A/V desync before correction
    int64_t max_desync_us = 100'000;  // 100ms
    
    // Buffer size for jitter compensation
    uint32_t jitter_buffer_ms = 50;
    
    // Enable drift correction
    bool enable_drift_correction = true;
    
    // Frame drop/duplicate policy
    bool allow_frame_drop = true;
    bool allow_frame_duplicate = false;
};

/**
 * @brief Audio/Video synchronizer
 */
class AVSynchronizer {
public:
    AVSynchronizer();
    ~AVSynchronizer();
    
    /**
     * @brief Initialize synchronizer
     */
    [[nodiscard]] Result<void> initialize(const SyncConfig& config);
    
    /**
     * @brief Start synchronization
     */
    void start();
    
    /**
     * @brief Stop synchronization
     */
    void stop();
    
    /**
     * @brief Reset timing state
     */
    void reset();
    
    /**
     * @brief Push encoded video frame
     * @param frame Encoded video frame with PTS (sink parameter - will be moved)
     */
    void push_video(EncodedVideoFrame frame) noexcept;
    
    /**
     * @brief Push encoded audio frame  
     * @param frame Encoded audio frame with PTS (sink parameter - will be moved)
     */
    void push_audio(EncodedAudioFrame frame) noexcept;
    
    /**
     * @brief Get next synchronized frame pair
     * @param timeout_ms Maximum wait time
     * @return Synchronized frames or nullopt if timeout
     */
    [[nodiscard]] std::optional<SyncedFrames> get_next(uint32_t timeout_ms = 100);
    
    /**
     * @brief Get current synchronization statistics
     */
    [[nodiscard]] SyncStats get_stats() const;
    
    /**
     * @brief Set callback for synchronized output
     */
    void set_output_callback(std::function<void(const SyncedFrames&)> callback);
    
    /**
     * @brief Manually adjust A/V offset
     * @param offset_us Offset in microseconds (positive = delay audio)
     */
    void adjust_offset(int64_t offset_us);

private:
    /**
     * @brief Calculate presentation time for next output
     */
    PTS calculate_presentation_time();
    
    /**
     * @brief Perform drift correction
     */
    void correct_drift();
    
    /**
     * @brief Check if frames are in sync
     */
    bool check_sync(PTS video_pts, PTS audio_pts);
    
    // Configuration
    SyncConfig m_config;
    
    // Frame buffers
    std::deque<EncodedVideoFrame> m_video_buffer;
    std::deque<EncodedAudioFrame> m_audio_buffer;
    std::mutex m_buffer_mutex;
    std::condition_variable m_buffer_cv;
    
    // Timing state
    std::atomic<bool> m_running{false};
    PTS m_base_time = 0;
    PTS m_last_video_pts = 0;
    PTS m_last_audio_pts = 0;
    
    // Drift tracking
    struct DriftSample {
        PTS local_time;
        PTS stream_time;
    };
    std::deque<DriftSample> m_video_drift_samples;
    std::deque<DriftSample> m_audio_drift_samples;
    static constexpr size_t DRIFT_SAMPLE_COUNT = 100;
    
    // Statistics
    mutable std::mutex m_stats_mutex;
    SyncStats m_stats;
    
    // Output
    std::function<void(const SyncedFrames&)> m_callback;
};

} // namespace stream_linux
