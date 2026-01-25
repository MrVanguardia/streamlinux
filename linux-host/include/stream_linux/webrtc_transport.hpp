#pragma once

/**
 * @file webrtc_transport.hpp
 * @brief WebRTC-based transport layer
 * 
 * Features:
 * - UDP transport with DTLS encryption
 * - Native A/V sync
 * - LAN mode without server
 * - Internet mode with signaling
 * - Data channel for control messages
 */

#include "common.hpp"
#include "av_synchronizer.hpp"
#include <memory>
#include <functional>

namespace stream_linux {

/**
 * @brief WebRTC connection state
 */
enum class ConnectionState {
    New,
    Connecting,
    Connected,
    Disconnected,
    Failed,
    Closed
};

/**
 * @brief ICE connection state
 */
enum class IceState {
    New,
    Checking,
    Connected,
    Completed,
    Failed,
    Disconnected,
    Closed
};

/**
 * @brief Peer connection information
 */
struct PeerInfo {
    std::string id;
    std::string address;
    uint16_t port = 0;
    ConnectionState state = ConnectionState::New;
    double rtt_ms = 0;
    double packet_loss = 0;
};

/**
 * @brief SDP offer/answer
 */
struct SessionDescription {
    enum class Type { Offer, Answer, Pranswer };
    Type type;
    std::string sdp;
};

/**
 * @brief ICE candidate
 */
struct IceCandidate {
    std::string candidate;
    std::string sdp_mid;
    int sdp_mline_index;
};

/**
 * @brief Control message types
 */
enum class ControlMessageType {
    Pause,
    Resume,
    SetResolution,
    SetBitrate,
    SetQuality,
    SelectMonitor,
    RequestKeyframe,
    Ping,
    Pong
};

/**
 * @brief Control message
 */
struct ControlMessage {
    ControlMessageType type;
    std::string payload;  // JSON string
    uint64_t sequence = 0;
};

/**
 * @brief Transport callbacks
 */
struct TransportCallbacks {
    std::function<void(ConnectionState)> on_connection_state;
    std::function<void(IceState)> on_ice_state;
    std::function<void(const SessionDescription&)> on_local_description;
    std::function<void(const IceCandidate&)> on_ice_candidate;
    std::function<void(const ControlMessage&)> on_control_message;
    std::function<void(const Error&)> on_error;
};

/**
 * @brief WebRTC transport interface
 */
class IWebRTCTransport {
public:
    virtual ~IWebRTCTransport() = default;
    
    /**
     * @brief Initialize transport
     */
    [[nodiscard]] virtual Result<void> initialize(const TransportConfig& config) = 0;
    
    /**
     * @brief Set event callbacks
     */
    virtual void set_callbacks(const TransportCallbacks& callbacks) = 0;
    
    /**
     * @brief Create offer (initiator)
     */
    [[nodiscard]] virtual Result<SessionDescription> create_offer() = 0;
    
    /**
     * @brief Create answer (responder)
     */
    [[nodiscard]] virtual Result<SessionDescription> create_answer(
        const SessionDescription& offer) = 0;
    
    /**
     * @brief Set remote description
     */
    [[nodiscard]] virtual Result<void> set_remote_description(
        const SessionDescription& desc) = 0;
    
    /**
     * @brief Add ICE candidate
     */
    [[nodiscard]] virtual Result<void> add_ice_candidate(const IceCandidate& candidate) = 0;
    
    /**
     * @brief Send encoded video frame
     */
    [[nodiscard]] virtual Result<void> send_video(const EncodedVideoFrame& frame) = 0;
    
    /**
     * @brief Send encoded audio frame
     */
    [[nodiscard]] virtual Result<void> send_audio(const EncodedAudioFrame& frame) = 0;
    
    /**
     * @brief Send synchronized frames
     */
    [[nodiscard]] virtual Result<void> send_synced(const SyncedFrames& frames) = 0;
    
    /**
     * @brief Send control message via data channel
     */
    [[nodiscard]] virtual Result<void> send_control(const ControlMessage& msg) = 0;
    
    /**
     * @brief Get current connection state
     */
    [[nodiscard]] virtual ConnectionState get_connection_state() const = 0;
    
    /**
     * @brief Get connected peer information
     */
    [[nodiscard]] virtual std::optional<PeerInfo> get_peer_info() const = 0;
    
    /**
     * @brief Close connection
     */
    virtual void close() = 0;
    
    /**
     * @brief Get transport statistics
     */
    struct Stats {
        uint64_t bytes_sent = 0;
        uint64_t bytes_received = 0;
        uint64_t packets_sent = 0;
        uint64_t packets_lost = 0;
        double current_bitrate = 0;
        double rtt_ms = 0;
        double jitter_ms = 0;
    };
    [[nodiscard]] virtual Stats get_stats() const = 0;
};

/**
 * @brief Simple LAN discovery for direct connections
 */
class LANDiscovery {
public:
    struct HostInfo {
        std::string name;
        std::string address;
        uint16_t port;
        std::string fingerprint;  // For verification
    };
    
    /**
     * @brief Broadcast presence on LAN
     */
    [[nodiscard]] static Result<void> announce(const HostInfo& info);
    
    /**
     * @brief Discover available hosts
     */
    [[nodiscard]] static Result<std::vector<HostInfo>> discover(
        uint32_t timeout_ms = 3000);
    
    /**
     * @brief Generate QR code data for manual connection
     */
    [[nodiscard]] static std::string generate_qr_data(const HostInfo& info);
};

/**
 * @brief Create WebRTC transport
 */
[[nodiscard]] Result<std::unique_ptr<IWebRTCTransport>> create_webrtc_transport();

} // namespace stream_linux
