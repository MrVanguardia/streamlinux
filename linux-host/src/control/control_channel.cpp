/**
 * @file control_channel.cpp
 * @brief Control channel implementation
 */

#include "stream_linux/control_channel.hpp"
#include <sstream>

namespace stream_linux {

ControlChannel::ControlChannel() = default;
ControlChannel::~ControlChannel() = default;

Result<void> ControlChannel::initialize(IWebRTCTransport* transport) {
    if (!transport) {
        return std::unexpected(Error{ErrorCode::InvalidArgument,
            "Transport cannot be null"});
    }
    m_transport = transport;
    return {};
}

void ControlChannel::set_handler(IControlHandler* handler) {
    m_handler = handler;
}

void ControlChannel::process_message(const ControlMessage& msg) {
    if (!m_handler) return;
    
    switch (msg.type) {
        case ControlMessageType::Pause:
            m_handler->on_pause();
            break;
            
        case ControlMessageType::Resume:
            m_handler->on_resume();
            break;
            
        case ControlMessageType::SetResolution:
        case ControlMessageType::SetBitrate:
        case ControlMessageType::SetQuality:
        case ControlMessageType::SelectMonitor: {
            auto params = parse_message(msg.payload);
            if (params) {
                // Parse parameters from JSON
                StreamParameters stream_params;
                // TODO: Parse JSON payload
                m_handler->on_parameters_changed(stream_params);
            }
            break;
        }
            
        case ControlMessageType::RequestKeyframe:
            m_handler->on_keyframe_requested();
            break;
            
        case ControlMessageType::Ping: {
            // Respond with pong
            ControlMessage pong;
            pong.type = ControlMessageType::Pong;
            pong.payload = build_json("pong", 
                std::format("{{\"echo_sequence\":{}}}", msg.sequence));
            pong.sequence = msg.sequence;
            m_transport->send_control(pong);
            break;
        }
            
        case ControlMessageType::Pong: {
            // Calculate RTT
            auto now = Clock::now();
            auto rtt = std::chrono::duration<double, std::milli>(
                now - m_last_ping_time).count();
            m_rtt_ms = rtt;
            break;
        }
            
        default:
            break;
    }
}

Result<void> ControlChannel::send_state(bool paused, const StreamParameters& params) {
    std::ostringstream payload;
    payload << "{"
            << "\"paused\":" << (paused ? "true" : "false");
    
    if (params.width > 0) {
        payload << ",\"width\":" << params.width;
    }
    if (params.height > 0) {
        payload << ",\"height\":" << params.height;
    }
    if (params.bitrate > 0) {
        payload << ",\"bitrate\":" << params.bitrate;
    }
    if (params.fps > 0) {
        payload << ",\"fps\":" << params.fps;
    }
    if (params.monitor_id >= 0) {
        payload << ",\"monitor_id\":" << params.monitor_id;
    }
    
    payload << "}";
    
    ControlMessage msg;
    msg.type = ControlMessageType::Ping;  // Using as state update
    msg.payload = build_json("state", payload.str());
    
    return m_transport->send_control(msg);
}

Result<void> ControlChannel::send_error(const std::string& message) {
    ControlMessage msg;
    msg.type = ControlMessageType::Ping;  // Using as error
    msg.payload = build_json("error", 
        std::format("{{\"message\":\"{}\"}}", message));
    
    return m_transport->send_control(msg);
}

Result<void> ControlChannel::send_ping() {
    ControlMessage msg;
    msg.type = ControlMessageType::Ping;
    msg.sequence = ++m_last_ping_sequence;
    msg.payload = build_json("ping");
    
    m_last_ping_time = Clock::now();
    
    return m_transport->send_control(msg);
}

double ControlChannel::get_rtt_ms() const {
    return m_rtt_ms;
}

Result<void> ControlChannel::parse_message(const std::string& json) {
    // Simple JSON parsing - in production use a proper JSON library
    (void)json;
    return {};
}

std::string ControlChannel::build_json(const std::string& type, const std::string& payload) {
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        Clock::now().time_since_epoch()).count();
    
    std::ostringstream ss;
    ss << "{"
       << "\"type\":\"" << type << "\","
       << "\"timestamp\":" << now;
    
    if (!payload.empty()) {
        ss << ",\"payload\":" << payload;
    }
    
    ss << "}";
    return ss.str();
}

} // namespace stream_linux
