/**
 * @file av_synchronizer.cpp
 * @brief Audio/Video synchronization implementation
 */

#include "stream_linux/av_synchronizer.hpp"
#include <algorithm>
#include <cmath>

namespace stream_linux {

AVSynchronizer::AVSynchronizer() = default;
AVSynchronizer::~AVSynchronizer() = default;

Result<void> AVSynchronizer::initialize(const SyncConfig& config) {
    m_config = config;
    reset();
    return {};
}

void AVSynchronizer::start() {
    m_running = true;
    m_base_time = get_monotonic_pts();
}

void AVSynchronizer::stop() {
    m_running = false;
    m_buffer_cv.notify_all();
}

void AVSynchronizer::reset() {
    std::lock_guard<std::mutex> lock(m_buffer_mutex);
    
    m_video_buffer.clear();
    m_audio_buffer.clear();
    m_video_drift_samples.clear();
    m_audio_drift_samples.clear();
    
    m_base_time = 0;
    m_last_video_pts = 0;
    m_last_audio_pts = 0;
    
    std::lock_guard<std::mutex> stats_lock(m_stats_mutex);
    m_stats = SyncStats{};
}

void AVSynchronizer::push_video(EncodedVideoFrame frame) {
    std::lock_guard<std::mutex> lock(m_buffer_mutex);
    
    // Track drift
    PTS local_time = get_monotonic_pts() - m_base_time;
    m_video_drift_samples.push_back({local_time, frame.pts});
    while (m_video_drift_samples.size() > DRIFT_SAMPLE_COUNT) {
        m_video_drift_samples.pop_front();
    }
    
    m_video_buffer.push_back(std::move(frame));
    m_last_video_pts = m_video_buffer.back().pts;
    
    // Limit buffer size
    while (m_video_buffer.size() > 30) {  // ~0.5 sec at 60fps
        m_video_buffer.pop_front();
        std::lock_guard<std::mutex> stats_lock(m_stats_mutex);
        ++m_stats.frames_dropped;
    }
    
    m_buffer_cv.notify_one();
}

void AVSynchronizer::push_audio(EncodedAudioFrame frame) {
    std::lock_guard<std::mutex> lock(m_buffer_mutex);
    
    // Track drift
    PTS local_time = get_monotonic_pts() - m_base_time;
    m_audio_drift_samples.push_back({local_time, frame.pts});
    while (m_audio_drift_samples.size() > DRIFT_SAMPLE_COUNT) {
        m_audio_drift_samples.pop_front();
    }
    
    m_audio_buffer.push_back(std::move(frame));
    m_last_audio_pts = m_audio_buffer.back().pts;
    
    // Limit buffer size
    while (m_audio_buffer.size() > 50) {  // ~1 sec at 20ms frames
        m_audio_buffer.pop_front();
    }
    
    m_buffer_cv.notify_one();
}

std::optional<SyncedFrames> AVSynchronizer::get_next(uint32_t timeout_ms) {
    std::unique_lock<std::mutex> lock(m_buffer_mutex);
    
    // Wait for frames
    bool have_frames = m_buffer_cv.wait_for(lock, std::chrono::milliseconds(timeout_ms), [this] {
        return (!m_video_buffer.empty() || !m_audio_buffer.empty()) || !m_running;
    });
    
    if (!have_frames || !m_running) {
        return std::nullopt;
    }
    
    SyncedFrames result;
    result.presentation_time = calculate_presentation_time();
    
    // Get video frame if available and in sync
    if (!m_video_buffer.empty()) {
        auto& video = m_video_buffer.front();
        
        // Check if video is in sync with target time
        int64_t video_offset = video.pts - result.presentation_time;
        
        if (std::abs(video_offset) < m_config.max_desync_us || video.keyframe) {
            result.video = std::move(video);
            result.video_valid = true;
            m_video_buffer.pop_front();
        } else if (video_offset < -m_config.max_desync_us) {
            // Video is late - drop it
            m_video_buffer.pop_front();
            std::lock_guard<std::mutex> stats_lock(m_stats_mutex);
            ++m_stats.frames_dropped;
        }
        // If video is early, keep it in buffer
    }
    
    // Get audio frame if available
    if (!m_audio_buffer.empty()) {
        auto& audio = m_audio_buffer.front();
        
        // Audio is more critical for sync - always include if close
        int64_t audio_offset = audio.pts - result.presentation_time;
        
        if (std::abs(audio_offset) < m_config.max_desync_us * 2) {
            result.audio = std::move(audio);
            result.audio_valid = true;
            m_audio_buffer.pop_front();
        } else if (audio_offset < -m_config.max_desync_us) {
            // Audio is late - drop it
            m_audio_buffer.pop_front();
        }
    }
    
    // Update A/V offset stats
    if (result.video_valid && result.audio_valid) {
        std::lock_guard<std::mutex> stats_lock(m_stats_mutex);
        m_stats.audio_video_offset_us = result.audio->pts - result.video->pts;
    }
    
    // Perform drift correction periodically
    if (m_config.enable_drift_correction) {
        correct_drift();
    }
    
    // Call callback if set
    if (m_callback && (result.video_valid || result.audio_valid)) {
        lock.unlock();
        m_callback(result);
    }
    
    return result;
}

PTS AVSynchronizer::calculate_presentation_time() {
    // Use the most recent timestamp as reference
    PTS ref_pts = std::max(m_last_video_pts, m_last_audio_pts);
    
    // Apply jitter buffer offset
    ref_pts -= m_config.jitter_buffer_ms * 1000;  // Convert ms to us
    
    // Apply manual offset
    ref_pts += m_config.target_offset_us;
    
    return ref_pts;
}

void AVSynchronizer::correct_drift() {
    if (m_video_drift_samples.size() < 10 || m_audio_drift_samples.size() < 10) {
        return;
    }
    
    // Calculate drift using linear regression
    auto calculate_drift = [](const std::deque<DriftSample>& samples) -> double {
        if (samples.size() < 2) return 0;
        
        double sum_x = 0, sum_y = 0, sum_xy = 0, sum_xx = 0;
        size_t n = samples.size();
        
        for (const auto& s : samples) {
            double x = static_cast<double>(s.local_time);
            double y = static_cast<double>(s.stream_time);
            sum_x += x;
            sum_y += y;
            sum_xy += x * y;
            sum_xx += x * x;
        }
        
        double slope = (n * sum_xy - sum_x * sum_y) / (n * sum_xx - sum_x * sum_x);
        
        // Drift in ppm (parts per million)
        return (slope - 1.0) * 1'000'000;
    };
    
    std::lock_guard<std::mutex> stats_lock(m_stats_mutex);
    m_stats.video_drift_ppm = calculate_drift(m_video_drift_samples);
    m_stats.audio_drift_ppm = calculate_drift(m_audio_drift_samples);
}

bool AVSynchronizer::check_sync(PTS video_pts, PTS audio_pts) {
    int64_t offset = video_pts - audio_pts;
    return std::abs(offset) < m_config.max_desync_us;
}

SyncStats AVSynchronizer::get_stats() const {
    std::lock_guard<std::mutex> lock(m_stats_mutex);
    return m_stats;
}

void AVSynchronizer::set_output_callback(std::function<void(const SyncedFrames&)> callback) {
    m_callback = std::move(callback);
}

void AVSynchronizer::adjust_offset(int64_t offset_us) {
    m_config.target_offset_us = offset_us;
}

} // namespace stream_linux
