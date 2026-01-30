/**
 * @file main.cpp
 * @brief Main entry point for stream-linux server
 */

#include "stream_linux/common.hpp"
#include "stream_linux/cli.hpp"
#include "stream_linux/backend_detector.hpp"
#include "stream_linux/display_backend.hpp"
#include "stream_linux/audio_capture.hpp"
#include "stream_linux/video_encoder.hpp"
#include "stream_linux/audio_encoder.hpp"
#include "stream_linux/av_synchronizer.hpp"
#include "stream_linux/webrtc_transport.hpp"
#include "stream_linux/control_channel.hpp"

#include <iostream>
#include <csignal>
#include <atomic>
#include <thread>

using namespace stream_linux;

// Global flag for graceful shutdown
static std::atomic<bool> g_running{true};

void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "\nShutting down...\n";
        g_running = false;
    }
}

void print_session_info() {
    std::cout << BackendDetector::get_session_info() << std::endl;
}

void list_monitors(const CLIOptions& options) {
    auto backend_result = create_display_backend(options.backend);
    if (!backend_result) {
        std::cerr << "Error: " << backend_result.error().message << std::endl;
        return;
    }
    
    auto& backend = *backend_result;
    
    CaptureConfig config;
    auto init_result = backend->initialize(config);
    if (!init_result) {
        std::cerr << "Error: " << init_result.error().message << std::endl;
        return;
    }
    
    auto monitors_result = backend->get_monitors();
    if (!monitors_result) {
        std::cerr << "Error: " << monitors_result.error().message << std::endl;
        return;
    }
    
    std::cout << "Available Monitors:\n";
    for (const auto& monitor : *monitors_result) {
        std::cout << "  [" << monitor.id << "] " << monitor.name;
        std::cout << " - " << monitor.width << "x" << monitor.height;
        std::cout << " @ " << monitor.refresh_rate << "Hz";
        if (monitor.primary) std::cout << " (primary)";
        std::cout << "\n";
    }
}

void list_audio_devices() {
    auto audio_result = create_audio_capture(AudioBackend::Auto);
    if (!audio_result) {
        std::cerr << "Error: " << audio_result.error().message << std::endl;
        return;
    }
    
    auto& audio = *audio_result;
    
    AudioConfig config;
    auto init_result = audio->initialize(config);
    if (!init_result) {
        std::cerr << "Error: " << init_result.error().message << std::endl;
        return;
    }
    
    auto devices_result = audio->get_devices();
    if (!devices_result) {
        std::cerr << "Error: " << devices_result.error().message << std::endl;
        return;
    }
    
    std::cout << "Available Audio Devices:\n";
    for (const auto& device : *devices_result) {
        std::cout << "  [" << device.id << "] " << device.name;
        if (!device.description.empty()) {
            std::cout << " - " << device.description;
        }
        if (device.is_monitor) std::cout << " (monitor)";
        if (device.is_default) std::cout << " (default)";
        std::cout << "\n";
    }
}

int run_server(const CLIOptions& options) {
    std::cout << "Starting stream-linux server...\n";
    
    // Print configuration
    if (options.verbose) {
        print_session_info();
    }
    
    // Create display backend
    std::cout << "Initializing display capture...\n";
    auto backend_result = create_display_backend(options.backend);
    if (!backend_result) {
        std::cerr << "Error: " << backend_result.error().message << std::endl;
        return 1;
    }
    auto& display = *backend_result;
    
    // Configure capture
    CaptureConfig capture_config;
    capture_config.target_fps = options.fps;
    capture_config.show_cursor = options.show_cursor;
    capture_config.region.monitor_id = options.monitor_id;
    
    auto init_result = display->initialize(capture_config);
    if (!init_result) {
        std::cerr << "Error: " << init_result.error().message << std::endl;
        return 1;
    }
    
    auto [width, height] = display->get_resolution();
    std::cout << "Capture resolution: " << width << "x" << height << "\n";
    
    // Create audio capture
    std::unique_ptr<IAudioCapture> audio;
    if (options.audio_enabled) {
        std::cout << "Initializing audio capture...\n";
        auto audio_result = create_audio_capture(AudioBackend::Auto);
        if (audio_result) {
            audio = std::move(*audio_result);
            
            AudioConfig audio_config;
            audio_config.source = options.audio_source;
            
            auto audio_init = audio->initialize(audio_config);
            if (!audio_init) {
                std::cerr << "Warning: Audio init failed: " << audio_init.error().message << std::endl;
                audio.reset();
            }
        } else {
            std::cerr << "Warning: " << audio_result.error().message << std::endl;
        }
    }
    
    // Create video encoder
    std::cout << "Initializing video encoder...\n";
    VideoConfig video_config;
    video_config.width = width;
    video_config.height = height;
    video_config.fps = options.fps;
    video_config.codec = options.codec;
    
    if (options.bitrate > 0) {
        video_config.bitrate = options.bitrate * 1000;  // kbps to bps
    } else {
        // Auto bitrate based on resolution
        video_config.bitrate = width * height * options.fps / 10;  // Rough estimate
    }
    
    auto encoder_result = create_video_encoder(video_config);
    if (!encoder_result) {
        std::cerr << "Error: " << encoder_result.error().message << std::endl;
        return 1;
    }
    auto& video_encoder = *encoder_result;
    
    // Create audio encoder
    std::unique_ptr<IAudioEncoder> audio_encoder;
    if (audio) {
        AudioConfig audio_enc_config;
        auto aenc_result = create_audio_encoder(audio_enc_config);
        if (aenc_result) {
            audio_encoder = std::move(*aenc_result);
        }
    }
    
    // Create A/V synchronizer
    AVSynchronizer synchronizer;
    SyncConfig sync_config;
    synchronizer.initialize(sync_config);
    
    // Create WebRTC transport
    std::cout << "Initializing transport...\n";
    auto transport_result = create_webrtc_transport();
    if (!transport_result) {
        std::cerr << "Error: " << transport_result.error().message << std::endl;
        return 1;
    }
    auto& transport = *transport_result;
    
    TransportConfig transport_config;
    transport_config.local_address = options.bind_address;
    transport_config.port = options.port;
    transport_config.stun_server = options.stun_server;
    
    auto transport_init = transport->initialize(transport_config);
    if (!transport_init) {
        std::cerr << "Error: " << transport_init.error().message << std::endl;
        return 1;
    }
    
    // Set up control channel
    ControlChannel control;
    control.initialize(transport.get());
    
    // Start capture
    std::cout << "Starting capture...\n";
    auto start_result = display->start();
    if (!start_result) {
        std::cerr << "Error: " << start_result.error().message << std::endl;
        return 1;
    }
    
    if (audio) {
        audio->start();
    }
    
    synchronizer.start();
    
    std::cout << "Server running. Press Ctrl+C to stop.\n";
    
    // Main loop
    while (g_running) {
        // Capture and encode video
        auto frame_result = display->capture_frame();
        if (frame_result) {
            auto encode_result = video_encoder->encode(*frame_result);
            if (encode_result) {
                synchronizer.push_video(std::move(*encode_result));
            }
        }
        
        // Capture and encode audio
        if (audio && audio_encoder) {
            auto audio_frame = audio->read_frame();
            if (audio_frame) {
                auto audio_encode = audio_encoder->encode(*audio_frame);
                if (audio_encode) {
                    synchronizer.push_audio(std::move(*audio_encode));
                }
            }
        }
        
        // Get synchronized frames and send
        auto synced = synchronizer.get_next(10);
        if (synced && transport->get_connection_state() == ConnectionState::Connected) {
            transport->send_synced(*synced);
        }
        
        // Print stats periodically
        static auto last_stats = Clock::now();
        auto now = Clock::now();
        if (std::chrono::duration<double>(now - last_stats).count() >= 5.0) {
            auto enc_stats = video_encoder->get_stats();
            auto sync_stats = synchronizer.get_stats();
            
            if (options.verbose) {
                std::cout << "Stats: "
                          << "FPS=" << display->get_actual_fps()
                          << " Bitrate=" << (enc_stats.current_bitrate / 1'000'000) << "Mbps"
                          << " A/V offset=" << (sync_stats.audio_video_offset_us / 1000) << "ms"
                          << "\n";
            }
            last_stats = now;
        }
    }
    
    // Cleanup
    std::cout << "Stopping...\n";
    
    synchronizer.stop();
    if (audio) audio->stop();
    display->stop();
    transport->close();
    
    std::cout << "Server stopped.\n";
    return 0;
}

int main(int argc, char* argv[]) {
    // Setup signal handlers
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    // Parse command line arguments
    auto cli_result = CLIParser::parse(argc, argv);
    if (!cli_result) {
        std::cerr << "Error: " << cli_result.error().message << std::endl;
        std::cerr << "Use --help for usage information.\n";
        return 1;
    }
    
    CLIOptions options = *cli_result;
    
    // Handle simple commands
    if (options.show_help) {
        std::cout << CLIParser::get_help();
        return 0;
    }
    
    if (options.show_version) {
        std::cout << CLIParser::get_version();
        return 0;
    }
    
    // Load config file if specified
    if (!options.config_file.empty()) {
        auto config_result = ConfigManager::load(options.config_file);
        if (config_result) {
            options = ConfigManager::merge(options, *config_result);
        } else if (options.verbose) {
            std::cerr << "Warning: " << config_result.error().message << std::endl;
        }
    }
    
    // Handle info commands
    if (options.list_monitors) {
        list_monitors(options);
        return 0;
    }
    
    if (options.list_audio_devices) {
        list_audio_devices();
        return 0;
    }
    
    // Run server
    return run_server(options);
}
