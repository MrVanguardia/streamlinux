/**
 * StreamLinux Signaling Server
 *
 * WebSocket-based signaling server for WebRTC connection negotiation
 * between Linux hosts and Android clients.
 *
 * Features:
 * - WebSocket signaling for SDP/ICE exchange
 * - Room-based connection management
 * - QR code generation for easy pairing
 * - LAN discovery service
 * - TLS support for secure connections
 */
package main

import (
	"context"
	"flag"
	"fmt"
	"net"
	"net/http"
	"net/url"
	"os"
	"os/signal"
	"strings"
	"syscall"
	"time"

	"github.com/streamlinux/signaling-server/internal/discovery"
	"github.com/streamlinux/signaling-server/internal/qr"
	"github.com/streamlinux/signaling-server/internal/signaling"

	"go.uber.org/zap"
	"go.uber.org/zap/zapcore"
)

// Config holds server configuration
type Config struct {
	Host           string
	Port           int
	TLSCert        string
	TLSKey         string
	TokenTTL       time.Duration
	AllowInsecure  bool
	EnableQR       bool
	EnableMDNS     bool
	RoomTimeout    time.Duration
	Debug          bool
	AllowedOrigins []string
}

func main() {
	// Parse command line flags
	config := parseFlags()

	// Initialize logger
	logger := initLogger(config.Debug)
	defer logger.Sync()

	// Create signaling hub
	hub := signaling.NewHub(logger, config.RoomTimeout)
	signaling.SetAllowedOrigins(config.AllowedOrigins)

	// Create HTTP server and routes
	mux := http.NewServeMux()

	// WebSocket signaling endpoint
	wsHandler := func(w http.ResponseWriter, r *http.Request) {
		if !config.AllowInsecure && r.TLS == nil {
			http.Error(w, "TLS required", http.StatusUpgradeRequired)
			return
		}
		signaling.HandleWebSocket(hub, w, r, logger, signaling.WebSocketSecurity{
			RequireTLS:      !config.AllowInsecure,
			DefaultTokenTTL: config.TokenTTL,
		})
	}
	mux.HandleFunc("/ws", wsHandler)
	mux.HandleFunc("/ws/signaling", wsHandler) // Alternative path for Android client

	// Health check endpoint
	mux.HandleFunc("/health", func(w http.ResponseWriter, r *http.Request) {
		w.WriteHeader(http.StatusOK)
		w.Write([]byte(`{"status":"ok"}`))
	})

	// Room info endpoint
	mux.HandleFunc("/rooms", func(w http.ResponseWriter, r *http.Request) {
		if !requireToken(hub, w, r) {
			return
		}
		hub.HandleRoomInfo(w, r)
	})

	// Active hosts endpoint - allows clients to discover active streaming hosts
	mux.HandleFunc("/hosts", func(w http.ResponseWriter, r *http.Request) {
		if !requireToken(hub, w, r) {
			return
		}
		hub.HostsHandler(w, r)
	})
	mux.HandleFunc("/api/hosts", func(w http.ResponseWriter, r *http.Request) {
		if !requireToken(hub, w, r) {
			return
		}
		hub.HostsHandler(w, r)
	}) // Alternative path

	// QR code endpoint
	if config.EnableQR {
		qrHandler := qr.NewHandler(config.Host, config.Port, config.TLSCert != "")
		mux.HandleFunc("/qr", qrHandler.HandleQR)
		mux.HandleFunc("/qr/image", qrHandler.HandleQRImage)
	}

	// Create HTTP server
	addr := fmt.Sprintf("%s:%d", config.Host, config.Port)
	server := &http.Server{
		Addr:         addr,
		Handler:      corsMiddleware(mux, config.AllowedOrigins),
		ReadTimeout:  15 * time.Second,
		WriteTimeout: 15 * time.Second,
		IdleTimeout:  60 * time.Second,
		TLSConfig:    signaling.TLSConfig(),
	}

	// Start hub
	go hub.Run()

	// Start mDNS discovery if enabled
	var mdnsServer *discovery.MDNSServer
	if config.EnableMDNS {
		var err error
		mdnsServer, err = discovery.NewMDNSServer(config.Port, logger)
		if err != nil {
			logger.Warn("Failed to start mDNS server", zap.Error(err))
		} else {
			go mdnsServer.Start()
			logger.Info("mDNS discovery enabled", zap.String("service", "_streamlinux._tcp"))
		}
	}

	// Start server
	go func() {
		logger.Info("Starting signaling server",
			zap.String("address", addr),
			zap.Bool("tls", config.TLSCert != ""),
			zap.Bool("qr", config.EnableQR),
			zap.Bool("mdns", config.EnableMDNS))

		var err error
		if config.TLSCert != "" && config.TLSKey != "" {
			err = server.ListenAndServeTLS(config.TLSCert, config.TLSKey)
		} else if config.AllowInsecure {
			err = server.ListenAndServe()
		} else {
			err = fmt.Errorf("tls required: provide -tls-cert and -tls-key or set -allow-insecure true for local USB")
		}

		if err != nil && err != http.ErrServerClosed {
			logger.Fatal("Server failed", zap.Error(err))
		}
	}()

	// Print connection info
	printConnectionInfo(config, logger)

	// Wait for interrupt signal
	quit := make(chan os.Signal, 1)
	signal.Notify(quit, syscall.SIGINT, syscall.SIGTERM)
	<-quit

	logger.Info("Shutting down server...")

	// Graceful shutdown
	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()

	if mdnsServer != nil {
		mdnsServer.Stop()
	}

	hub.Shutdown()

	if err := server.Shutdown(ctx); err != nil {
		logger.Error("Server shutdown failed", zap.Error(err))
	}

	logger.Info("Server stopped")
}

func parseFlags() Config {
	config := Config{}

	flag.StringVar(&config.Host, "host", "0.0.0.0", "Host to bind to")
	flag.IntVar(&config.Port, "port", 8080, "Port to listen on")
	flag.StringVar(&config.TLSCert, "tls-cert", "", "Path to TLS certificate")
	flag.StringVar(&config.TLSKey, "tls-key", "", "Path to TLS private key")
	flag.DurationVar(&config.TokenTTL, "token-ttl", 24*time.Hour, "Default token TTL for host registration")
	flag.BoolVar(&config.AllowInsecure, "allow-insecure", false, "Allow ws (insecure) for USB/local-only")
	flag.BoolVar(&config.EnableQR, "qr", true, "Enable QR code generation")
	flag.BoolVar(&config.EnableMDNS, "mdns", true, "Enable mDNS discovery")
	flag.DurationVar(&config.RoomTimeout, "room-timeout", 5*time.Minute, "Room inactivity timeout")
	flag.BoolVar(&config.Debug, "debug", false, "Enable debug logging")
	allowedOrigins := flag.String("allowed-origins", "localhost,127.0.0.1", "Comma-separated list of allowed Origin hosts (host or host:port)")

	flag.Parse()

	config.AllowedOrigins = parseAllowedOrigins(*allowedOrigins)
	return config
}

func initLogger(debug bool) *zap.Logger {
	var config zap.Config

	if debug {
		config = zap.NewDevelopmentConfig()
		config.EncoderConfig.EncodeLevel = zapcore.CapitalColorLevelEncoder
	} else {
		config = zap.NewProductionConfig()
	}

	logger, err := config.Build()
	if err != nil {
		panic(err)
	}

	return logger
}

func corsMiddleware(next http.Handler, allowed []string) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		origin := r.Header.Get("Origin")
		if origin != "" {
			if hostAllowed(origin, allowed) {
				w.Header().Set("Access-Control-Allow-Origin", origin)
				w.Header().Set("Vary", "Origin")
			} else {
				http.Error(w, "Origin not allowed", http.StatusForbidden)
				return
			}
		}

		w.Header().Set("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
		w.Header().Set("Access-Control-Allow-Headers", "Content-Type, Authorization")

		if r.Method == http.MethodOptions {
			w.WriteHeader(http.StatusOK)
			return
		}

		next.ServeHTTP(w, r)
	})
}

func printConnectionInfo(config Config, logger *zap.Logger) {
	// Get local IP addresses
	addrs, err := net.InterfaceAddrs()
	if err != nil {
		return
	}

	protocol := "ws"
	if config.TLSCert != "" {
		protocol = "wss"
	}

	logger.Info("Server listening on:")
	for _, addr := range addrs {
		if ipnet, ok := addr.(*net.IPNet); ok && !ipnet.IP.IsLoopback() {
			if ipnet.IP.To4() != nil {
				url := fmt.Sprintf("%s://%s:%d/ws", protocol, ipnet.IP.String(), config.Port)
				logger.Info("  " + url)
			}
		}
	}

	if config.EnableQR {
		logger.Info(fmt.Sprintf("QR Code available at: http://localhost:%d/qr", config.Port))
	}
}

func parseAllowedOrigins(raw string) []string {
	parts := strings.Split(raw, ",")
	hosts := make([]string, 0, len(parts))
	for _, p := range parts {
		p = strings.TrimSpace(p)
		if p == "" {
			continue
		}
		// Remove schemes if present
		if strings.HasPrefix(p, "http://") || strings.HasPrefix(p, "https://") {
			if u, err := url.Parse(p); err == nil && u.Host != "" {
				p = u.Host
			}
		}
		hosts = append(hosts, strings.ToLower(p))
	}
	if len(hosts) == 0 {
		hosts = []string{"localhost", "127.0.0.1"}
	}
	return hosts
}

func hostAllowed(origin string, allowed []string) bool {
	if origin == "" {
		return true
	}
	u, err := url.Parse(origin)
	if err != nil || u.Host == "" {
		return false
	}
	host := strings.ToLower(u.Host)
	// Remove port for comparison
	hostWithoutPort := strings.Split(host, ":")[0]
	for _, a := range allowed {
		aWithoutPort := strings.Split(a, ":")[0]
		if host == a || hostWithoutPort == aWithoutPort {
			return true
		}
	}
	// Also allow localhost and local IPs
	if host == "localhost" || strings.HasPrefix(host, "localhost:") ||
		host == "127.0.0.1" || strings.HasPrefix(host, "127.0.0.1:") ||
		strings.HasPrefix(hostWithoutPort, "10.") ||
		strings.HasPrefix(hostWithoutPort, "192.168.") {
		return true
	}
	return false
}

func tokenFromRequest(r *http.Request) string {
	authz := r.Header.Get("Authorization")
	if strings.HasPrefix(strings.ToLower(authz), "bearer ") {
		return strings.TrimSpace(authz[7:])
	}
	return r.URL.Query().Get("token")
}

func requireToken(hub *signaling.Hub, w http.ResponseWriter, r *http.Request) bool {
	token := tokenFromRequest(r)
	if token == "" {
		http.Error(w, "Token required", http.StatusUnauthorized)
		return false
	}
	if !hub.ValidateToken(token) {
		http.Error(w, "Invalid or expired token", http.StatusUnauthorized)
		return false
	}
	return true
}
