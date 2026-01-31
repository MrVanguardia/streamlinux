#pragma once

/**
 * @file control_channel.hpp
 * @brief Control channel for remote commands
 * 
 * Features:
 * - Pause/Resume streaming
 * - Resolution/bitrate changes
 * - Monitor selection
 * - JSON message protocol
 */

#include "common.hpp"
#include "webrtc_transport.hpp"
#include <functional>
#include <memory>

namespace stream_linux {

/**
 * @brief Quality preset
 */
enum class QualityPreset {
    Auto,
    Low,      // 720p, 2 Mbps
    Medium,   // 1080p, 5 Mbps
    High,     // 1080p, 10 Mbps
    Ultra     // 4K, 20 Mbps
};

/**
 * @brief Stream parameters that can be changed at runtime
 * @note Trivially copyable value type - pass by value is efficient
 */
struct StreamParameters {
    uint32_t width = 0;      // 0 = no change
    uint32_t height = 0;
    uint32_t bitrate = 0;
    uint32_t fps = 0;
    int32_t monitor_id = -1;
    QualityPreset quality = QualityPreset::Auto;
    
    constexpr StreamParameters() noexcept = default;
    constexpr bool has_resolution() const noexcept { return width > 0 && height > 0; }
    constexpr bool has_bitrate() const noexcept { return bitrate > 0; }
};

/**
 * @brief Control event handler interface
 */
class IControlHandler {
public:
    virtual ~IControlHandler() = default;
    
    virtual void on_pause() = 0;
    virtual void on_resume() = 0;
    virtual void on_parameters_changed(const StreamParameters& params) = 0;
    virtual void on_keyframe_requested() = 0;
    virtual void on_disconnect_requested() = 0;
};

/**
 * @brief Control channel manager
 */
class ControlChannel {
public:
    ControlChannel();
    ~ControlChannel();
    
    /**
     * @brief Initialize control channel
     * @param transport WebRTC transport for data channel
     */
    [[nodiscard]] Result<void> initialize(IWebRTCTransport* transport);
    
    /**
     * @brief Set control event handler
     * @param handler Raw pointer to handler (not owned, caller manages lifetime)
     */
    void set_handler(IControlHandler* handler) noexcept;
    
    /**
     * @brief Set authorized peer ID (security fix vuln-0002)
     * @param peer_id The peer ID authorized to send control messages
     */
    void set_authorized_peer(const std::string& peer_id);
    
    /**
     * @brief Check if a peer is authorized (security fix vuln-0002)
     * @param sender_id The sender's peer ID
     * @return true if authorized
     */
    [[nodiscard]] bool is_peer_authorized(const std::string& sender_id) const;
    
    /**
     * @brief Process incoming control message
     */
    void process_message(const ControlMessage& msg);
    
    /**
     * @brief Send current stream state to peer
     */
    [[nodiscard]] Result<void> send_state(bool paused,
                                          const StreamParameters& params);
    
    /**
     * @brief Send error notification to peer
     */
    [[nodiscard]] Result<void> send_error(const std::string& message);
    
    /**
     * @brief Send ping for latency measurement
     */
    [[nodiscard]] Result<void> send_ping();
    
    /**
     * @brief Get measured round-trip time
     */
    [[nodiscard]] double get_rtt_ms() const;

private:
    /**
     * @brief Parse JSON control message
     */
    [[nodiscard]] Result<void> parse_message(const std::string& json);
    
    /**
     * @brief Build JSON response
     */
    [[nodiscard]] std::string build_json(const std::string& type,
                                         const std::string& payload = "");
    
    IWebRTCTransport* m_transport = nullptr;
    IControlHandler* m_handler = nullptr;
    
    // Security: Authorized peer tracking (vuln-0002 fix)
    std::string m_authorized_peer_id;
    
    // Ping/pong tracking
    uint64_t m_last_ping_sequence = 0;
    TimePoint m_last_ping_time;
    std::atomic<double> m_rtt_ms{0};
};

/**
 * @brief JSON message format for control channel
 * 
 * All messages have this structure:
 * {
 *   "type": "string",
 *   "sequence": number,
 *   "timestamp": number,
 *   "payload": { ... }
 * }
 * 
 * Message types:
 * - "pause": No payload
 * - "resume": No payload
 * - "set_resolution": { "width": n, "height": n }
 * - "set_bitrate": { "bitrate": n }
 * - "set_quality": { "preset": "auto|low|medium|high|ultra" }
 * - "select_monitor": { "id": n }
 * - "request_keyframe": No payload
 * - "ping": No payload
 * - "pong": { "echo_sequence": n }
 * - "state": { "paused": bool, "width": n, "height": n, ... }
 * - "error": { "message": "string" }
 */

} // namespace stream_linux
