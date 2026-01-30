/**
 * WebSocket Signaling Hub
 *
 * Manages WebRTC signaling between hosts and clients.
 * Supports both room-based and simple peer-to-peer signaling.
 */
package signaling

import (
	"encoding/json"
	"net/http"
	"net/url"
	"strings"
	"sync"
	"time"

	"github.com/gorilla/websocket"
	"go.uber.org/zap"
)

// MessageType defines the type of signaling message
type MessageType string

const (
	// Room management (legacy)
	MsgTypeJoin     MessageType = "join"
	MsgTypeLeave    MessageType = "leave"
	MsgTypeRoomInfo MessageType = "room_info"

	// Simple peer management
	MsgTypeRegister   MessageType = "register"
	MsgTypeRegistered MessageType = "registered"
	MsgTypePeerJoined MessageType = "peer-joined"
	MsgTypePeerLeft   MessageType = "peer-left"

	// WebRTC signaling
	MsgTypeOffer        MessageType = "offer"
	MsgTypeAnswer       MessageType = "answer"
	MsgTypeCandidate    MessageType = "candidate"
	MsgTypeIceCandidate MessageType = "ice-candidate"

	// Control
	MsgTypePing  MessageType = "ping"
	MsgTypePong  MessageType = "pong"
	MsgTypeError MessageType = "error"
)

// PeerRole defines the role of a peer in a room
type PeerRole string

const (
	RoleHost   PeerRole = "host"
	RoleClient PeerRole = "client"
)

// Message represents a signaling message
type Message struct {
	Type      MessageType     `json:"type"`
	Room      string          `json:"room,omitempty"`
	From      string          `json:"from,omitempty"`
	To        string          `json:"to,omitempty"`
	Role      PeerRole        `json:"role,omitempty"`
	Payload   json.RawMessage `json:"payload,omitempty"`
	Timestamp int64           `json:"timestamp,omitempty"`

	// Simple signaling fields
	PeerID        string `json:"peerId,omitempty"`
	Name          string `json:"name,omitempty"`
	SDP           string `json:"sdp,omitempty"`
	Candidate     string `json:"candidate,omitempty"`
	SDPMid        string `json:"sdpMid,omitempty"`
	SDPMLineIndex int    `json:"sdpMLineIndex,omitempty"`
}

// Peer represents a connected WebSocket peer
type Peer struct {
	ID       string
	Role     PeerRole
	Name     string
	Room     string
	Conn     *websocket.Conn
	Send     chan []byte
	Hub      *Hub
	Logger   *zap.Logger
	LastPing time.Time
	mu       sync.Mutex
}

// Room represents a signaling room
type Room struct {
	ID         string
	Host       *Peer
	Clients    map[string]*Peer
	CreatedAt  time.Time
	LastActive time.Time
	mu         sync.RWMutex
}

// SecurityConfig holds security-related settings
type SecurityConfig struct {
	RequireToken    bool          // Require token validation
	RequirePIN      bool          // Require PIN authorization
	TokenExpiry     time.Duration // Token validity duration
	MaxConnAttempts int           // Max connection attempts per window
	RateLimitWindow time.Duration // Time window for rate limiting
}

// DefaultSecurityConfig returns the default security configuration
func DefaultSecurityConfig() SecurityConfig {
	return SecurityConfig{
		RequireToken:    true,
		RequirePIN:      true,
		TokenExpiry:     5 * time.Minute,
		MaxConnAttempts: 10,
		RateLimitWindow: 1 * time.Minute,
	}
}

// RateLimiter tracks connection attempts
type RateLimiter struct {
	attempts map[string][]time.Time
	mu       sync.Mutex
}

func NewRateLimiter() *RateLimiter {
	return &RateLimiter{
		attempts: make(map[string][]time.Time),
	}
}

func (r *RateLimiter) Allow(identifier string, maxAttempts int, window time.Duration) bool {
	r.mu.Lock()
	defer r.mu.Unlock()

	now := time.Now()
	cutoff := now.Add(-window)

	// Clean old attempts
	if attempts, ok := r.attempts[identifier]; ok {
		filtered := make([]time.Time, 0)
		for _, t := range attempts {
			if t.After(cutoff) {
				filtered = append(filtered, t)
			}
		}
		r.attempts[identifier] = filtered
	}

	// Check limit
	if len(r.attempts[identifier]) >= maxAttempts {
		return false
	}

	// Record this attempt
	r.attempts[identifier] = append(r.attempts[identifier], now)
	return true
}

// PendingAuth represents a connection awaiting PIN verification
type PendingAuth struct {
	ConnectionID string
	DeviceID     string
	DeviceName   string
	PIN          string
	CreatedAt    time.Time
	ExpiresAt    time.Time
	Attempts     int
	Peer         *Peer
}

// Hub manages all peers and rooms
type Hub struct {
	rooms       map[string]*Room
	peers       map[string]*Peer
	register    chan *Peer
	unregister  chan *Peer
	broadcast   chan *Message
	timeout     time.Duration
	logger      *zap.Logger
	mu          sync.RWMutex
	done        chan struct{}
	security    SecurityConfig
	rateLimiter *RateLimiter
	validTokens map[string]time.Time // token -> expiry time
	pendingAuth map[string]*PendingAuth
	tokenMu     sync.RWMutex
}

var allowedOrigins []string

// SetAllowedOrigins configures the origin allowlist for WebSocket upgrades.
func SetAllowedOrigins(origins []string) {
	allowedOrigins = origins
}

func originAllowed(origin string) bool {
	if origin == "" {
		return true // Native apps without Origin
	}
	parsed, err := url.Parse(origin)
	if err != nil || parsed.Host == "" {
		return false
	}
	host := strings.ToLower(parsed.Host)
	hostWithoutPort := strings.Split(host, ":")[0]

	for _, allowed := range allowedOrigins {
		allowedWithoutPort := strings.Split(strings.ToLower(allowed), ":")[0]
		if host == allowed || hostWithoutPort == allowedWithoutPort {
			return true
		}
	}
	// Allow localhost and local IPs by default
	if host == "localhost" || strings.HasPrefix(host, "localhost:") ||
		host == "127.0.0.1" || strings.HasPrefix(host, "127.0.0.1:") ||
		strings.HasPrefix(hostWithoutPort, "10.") ||
		strings.HasPrefix(hostWithoutPort, "192.168.") ||
		strings.HasPrefix(hostWithoutPort, "172.") {
		return true
	}
	return false
}

// WebSocket upgrader with security
var upgrader = websocket.Upgrader{
	ReadBufferSize:  1024,
	WriteBufferSize: 1024,
	CheckOrigin: func(r *http.Request) bool {
		return originAllowed(r.Header.Get("Origin"))
	},
}

// NewHub creates a new signaling hub with security
func NewHub(logger *zap.Logger, timeout time.Duration) *Hub {
	return &Hub{
		rooms:       make(map[string]*Room),
		peers:       make(map[string]*Peer),
		register:    make(chan *Peer),
		unregister:  make(chan *Peer),
		broadcast:   make(chan *Message, 256),
		timeout:     timeout,
		logger:      logger,
		done:        make(chan struct{}),
		security:    DefaultSecurityConfig(),
		rateLimiter: NewRateLimiter(),
		validTokens: make(map[string]time.Time),
		pendingAuth: make(map[string]*PendingAuth),
	}
}

// NewHubWithSecurity creates a new signaling hub with custom security config
func NewHubWithSecurity(logger *zap.Logger, timeout time.Duration, security SecurityConfig) *Hub {
	hub := NewHub(logger, timeout)
	hub.security = security
	return hub
}

// RegisterToken registers a valid session token from the host
func (h *Hub) RegisterToken(token string, expiry time.Duration) {
	h.tokenMu.Lock()
	defer h.tokenMu.Unlock()
	h.validTokens[token] = time.Now().Add(expiry)
	h.logger.Info("Token registered", zap.String("token", token[:8]+"..."))
}

// ValidateToken checks if a token is valid
func (h *Hub) ValidateToken(token string) bool {
	h.tokenMu.RLock()
	defer h.tokenMu.RUnlock()

	expiry, ok := h.validTokens[token]
	if !ok {
		return false
	}
	if time.Now().After(expiry) {
		return false
	}
	return true
}

// InvalidateToken removes a token
func (h *Hub) InvalidateToken(token string) {
	h.tokenMu.Lock()
	defer h.tokenMu.Unlock()
	delete(h.validTokens, token)
}

// CleanupExpiredTokens removes expired tokens
func (h *Hub) CleanupExpiredTokens() {
	h.tokenMu.Lock()
	defer h.tokenMu.Unlock()

	now := time.Now()
	for token, expiry := range h.validTokens {
		if now.After(expiry) {
			delete(h.validTokens, token)
		}
	}
}

// Run starts the hub's main loop
func (h *Hub) Run() {
	ticker := time.NewTicker(30 * time.Second)
	defer ticker.Stop()

	for {
		select {
		case peer := <-h.register:
			h.registerPeer(peer)

		case peer := <-h.unregister:
			h.unregisterPeer(peer)

		case msg := <-h.broadcast:
			h.routeMessage(msg)

		case <-ticker.C:
			h.cleanupRooms()
			h.CleanupExpiredTokens()

		case <-h.done:
			h.closeAllPeers()
			return
		}
	}
}

// Shutdown gracefully shuts down the hub
func (h *Hub) Shutdown() {
	close(h.done)
}

func (h *Hub) registerPeer(peer *Peer) {
	h.mu.Lock()
	defer h.mu.Unlock()

	h.peers[peer.ID] = peer
	h.logger.Info("Peer registered", zap.String("id", peer.ID), zap.String("role", string(peer.Role)))
}

func (h *Hub) unregisterPeer(peer *Peer) {
	h.mu.Lock()
	defer h.mu.Unlock()

	if _, ok := h.peers[peer.ID]; ok {
		delete(h.peers, peer.ID)

		// Notify other peers that this peer left
		for _, otherPeer := range h.peers {
			h.sendToPeer(otherPeer, &Message{
				Type:   MsgTypePeerLeft,
				PeerID: peer.ID,
			})
		}

		// Remove from room
		if peer.Room != "" {
			if room, ok := h.rooms[peer.Room]; ok {
				room.mu.Lock()
				if peer.Role == RoleHost {
					room.Host = nil
					// Notify clients that host left
					for _, client := range room.Clients {
						h.sendToPeer(client, &Message{
							Type: MsgTypeLeave,
							From: peer.ID,
							Room: peer.Room,
						})
					}
				} else {
					delete(room.Clients, peer.ID)
					// Notify host that client left
					if room.Host != nil {
						h.sendToPeer(room.Host, &Message{
							Type: MsgTypeLeave,
							From: peer.ID,
							Room: peer.Room,
						})
					}
				}
				room.mu.Unlock()
			}
		}

		close(peer.Send)
		h.logger.Info("Peer unregistered", zap.String("id", peer.ID))
	}
}

func (h *Hub) routeMessage(msg *Message) {
	switch msg.Type {
	case MsgTypeRegister:
		h.handleRegister(msg)

	case MsgTypeJoin:
		h.mu.RLock()
		h.handleJoin(msg)
		h.mu.RUnlock()

	case MsgTypeOffer, MsgTypeAnswer, MsgTypeCandidate, MsgTypeIceCandidate:
		h.mu.RLock()
		defer h.mu.RUnlock()
		// Route to specific peer
		targetID := msg.To
		if targetID != "" {
			if peer, ok := h.peers[targetID]; ok {
				h.sendToPeer(peer, msg)
			} else {
				h.logger.Warn("Target peer not found", zap.String("to", targetID))
			}
		} else {
			// If no specific target, broadcast to all peers of opposite role
			fromPeer, ok := h.peers[msg.From]
			if !ok {
				return
			}
			for _, peer := range h.peers {
				if peer.ID != msg.From {
					// Host sends to viewers, viewers send to host
					if (fromPeer.Role == RoleHost && peer.Role == RoleClient) ||
						(fromPeer.Role == RoleClient && peer.Role == RoleHost) {
						h.sendToPeer(peer, msg)
					}
				}
			}
		}

	default:
		h.mu.RLock()
		defer h.mu.RUnlock()
		// Broadcast to room
		if msg.Room != "" {
			if room, ok := h.rooms[msg.Room]; ok {
				room.mu.RLock()
				for _, peer := range room.Clients {
					if peer.ID != msg.From {
						h.sendToPeer(peer, msg)
					}
				}
				if room.Host != nil && room.Host.ID != msg.From {
					h.sendToPeer(room.Host, msg)
				}
				room.mu.RUnlock()
			}
		}
	}
}

func (h *Hub) handleRegister(msg *Message) {
	h.mu.Lock()
	defer h.mu.Unlock()

	peer, ok := h.peers[msg.From]
	if !ok {
		return
	}

	// Set peer info
	if msg.Role != "" {
		peer.Role = msg.Role
	} else {
		// Default to viewer if not specified
		peer.Role = RoleClient
	}
	peer.Name = msg.Name
	peer.LastPing = time.Now() // Update last ping time

	h.logger.Info("Peer registered",
		zap.String("id", peer.ID),
		zap.String("role", string(peer.Role)),
		zap.String("name", peer.Name))

	// Send confirmation
	h.sendToPeer(peer, &Message{
		Type:   MsgTypeRegistered,
		PeerID: peer.ID,
	})

	// Notify other peers
	for _, otherPeer := range h.peers {
		if otherPeer.ID != peer.ID {
			// Notify existing peers about new peer
			h.sendToPeer(otherPeer, &Message{
				Type:   MsgTypePeerJoined,
				PeerID: peer.ID,
				Name:   peer.Name,
				Role:   peer.Role,
			})

			// Notify new peer about existing peers
			h.sendToPeer(peer, &Message{
				Type:   MsgTypePeerJoined,
				PeerID: otherPeer.ID,
				Name:   otherPeer.Name,
				Role:   otherPeer.Role,
			})
		}
	}
}

func (h *Hub) handleJoin(msg *Message) {
	peer, ok := h.peers[msg.From]
	if !ok {
		return
	}

	roomID := msg.Room
	if roomID == "" {
		h.sendError(peer, "Room ID required")
		return
	}

	// Create or get room
	room, ok := h.rooms[roomID]
	if !ok {
		room = &Room{
			ID:         roomID,
			Clients:    make(map[string]*Peer),
			CreatedAt:  time.Now(),
			LastActive: time.Now(),
		}
		h.rooms[roomID] = room
		h.logger.Info("Room created", zap.String("room", roomID))
	}

	room.mu.Lock()
	defer room.mu.Unlock()

	peer.Room = roomID
	room.LastActive = time.Now()

	if msg.Role == RoleHost {
		if room.Host != nil && room.Host.ID != peer.ID {
			h.sendError(peer, "Room already has a host")
			return
		}
		room.Host = peer
		peer.Role = RoleHost
		h.logger.Info("Host joined room", zap.String("room", roomID), zap.String("peer", peer.ID))
	} else {
		room.Clients[peer.ID] = peer
		peer.Role = RoleClient
		h.logger.Info("Client joined room", zap.String("room", roomID), zap.String("peer", peer.ID))

		// Notify host of new client
		if room.Host != nil {
			h.sendToPeer(room.Host, &Message{
				Type: MsgTypeJoin,
				From: peer.ID,
				Room: roomID,
				Role: RoleClient,
			})
		}
	}

	// Send room info to peer
	h.sendRoomInfo(peer, room)
}

func (h *Hub) sendRoomInfo(peer *Peer, room *Room) {
	type RoomInfoPayload struct {
		RoomID    string   `json:"room_id"`
		HasHost   bool     `json:"has_host"`
		HostID    string   `json:"host_id,omitempty"`
		ClientIDs []string `json:"client_ids"`
	}

	payload := RoomInfoPayload{
		RoomID:    room.ID,
		HasHost:   room.Host != nil,
		ClientIDs: make([]string, 0, len(room.Clients)),
	}

	if room.Host != nil {
		payload.HostID = room.Host.ID
	}

	for id := range room.Clients {
		payload.ClientIDs = append(payload.ClientIDs, id)
	}

	payloadBytes, _ := json.Marshal(payload)
	h.sendToPeer(peer, &Message{
		Type:    MsgTypeRoomInfo,
		Room:    room.ID,
		Payload: payloadBytes,
	})
}

func (h *Hub) sendToPeer(peer *Peer, msg *Message) {
	msg.Timestamp = time.Now().UnixMilli()
	data, err := json.Marshal(msg)
	if err != nil {
		h.logger.Error("Failed to marshal message", zap.Error(err))
		return
	}

	select {
	case peer.Send <- data:
	default:
		h.logger.Warn("Peer send buffer full", zap.String("peer", peer.ID))
	}
}

func (h *Hub) sendError(peer *Peer, errMsg string) {
	payload, _ := json.Marshal(map[string]string{"error": errMsg})
	h.sendToPeer(peer, &Message{
		Type:    MsgTypeError,
		Payload: payload,
	})
}

func (h *Hub) cleanupRooms() {
	h.mu.Lock()
	defer h.mu.Unlock()

	now := time.Now()
	for id, room := range h.rooms {
		room.mu.RLock()
		isEmpty := room.Host == nil && len(room.Clients) == 0
		isStale := now.Sub(room.LastActive) > h.timeout
		room.mu.RUnlock()

		if isEmpty || isStale {
			delete(h.rooms, id)
			h.logger.Info("Room cleaned up", zap.String("room", id))
		}
	}
}

func (h *Hub) closeAllPeers() {
	h.mu.Lock()
	defer h.mu.Unlock()

	for _, peer := range h.peers {
		close(peer.Send)
	}
}

// HandleRoomInfo handles HTTP requests for room information
func (h *Hub) HandleRoomInfo(w http.ResponseWriter, r *http.Request) {
	h.mu.RLock()
	defer h.mu.RUnlock()

	type RoomSummary struct {
		ID         string    `json:"id"`
		HasHost    bool      `json:"has_host"`
		NumClients int       `json:"num_clients"`
		CreatedAt  time.Time `json:"created_at"`
		LastActive time.Time `json:"last_active"`
	}

	rooms := make([]RoomSummary, 0, len(h.rooms))
	for _, room := range h.rooms {
		room.mu.RLock()
		rooms = append(rooms, RoomSummary{
			ID:         room.ID,
			HasHost:    room.Host != nil,
			NumClients: len(room.Clients),
			CreatedAt:  room.CreatedAt,
			LastActive: room.LastActive,
		})
		room.mu.RUnlock()
	}

	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(rooms)
}

// HandleWebSocket handles WebSocket upgrade and connection with security
type WebSocketSecurity struct {
	RequireTLS      bool
	DefaultTokenTTL time.Duration
}

func HandleWebSocket(hub *Hub, w http.ResponseWriter, r *http.Request, logger *zap.Logger, sec WebSocketSecurity) {
	remoteAddr := r.RemoteAddr
	clientType := r.Header.Get("X-Client-Type")
	token := extractToken(r)
	deviceID := r.URL.Query().Get("device_id")

	logger.Info("WebSocket connection attempt",
		zap.String("remote", remoteAddr),
		zap.String("path", r.URL.Path),
		zap.String("origin", r.Header.Get("Origin")),
		zap.String("client-type", clientType),
		zap.Bool("has-token", token != ""))

	// Rate limiting check
	if !hub.rateLimiter.Allow(remoteAddr, hub.security.MaxConnAttempts, hub.security.RateLimitWindow) {
		logger.Warn("Rate limited connection attempt", zap.String("remote", remoteAddr))
		http.Error(w, "Too many connection attempts", http.StatusTooManyRequests)
		return
	}

	if sec.RequireTLS && r.TLS == nil {
		logger.Warn("Rejected non-TLS WebSocket")
		http.Error(w, "TLS required", http.StatusUpgradeRequired)
		return
	}

	// Token validation for non-host connections (clients/viewers)
	// Hosts register tokens, clients must provide valid tokens
	// Exception: localhost connections (USB via ADB reverse) don't require tokens
	// Check both header and query param for host identification
	isHost := clientType == "host" || r.URL.Query().Get("is_host") == "true"
	isLocalhost := strings.HasPrefix(remoteAddr, "127.0.0.1:") || strings.HasPrefix(remoteAddr, "[::1]:")

	logger.Info("Connection type detection",
		zap.Bool("is-host", isHost),
		zap.Bool("is-localhost", isLocalhost),
		zap.String("client-type-header", clientType),
		zap.String("is_host-param", r.URL.Query().Get("is_host")))

	if isHost {
		if token == "" {
			logger.Warn("Host connection without token rejected")
			http.Error(w, "Token required for host", http.StatusUnauthorized)
			return
		}
		hub.RegisterToken(token, sec.DefaultTokenTTL)
		logger.Info("Host token registered", zap.String("remote", remoteAddr))
	} else if hub.security.RequireToken && !isLocalhost {
		// Non-localhost clients require valid token
		if token == "" {
			logger.Warn("Client without token rejected", zap.String("remote", remoteAddr))
			http.Error(w, "Token required", http.StatusUnauthorized)
			return
		}
		if !hub.ValidateToken(token) {
			logger.Warn("Invalid token rejected",
				zap.String("remote", remoteAddr),
				zap.String("token", token[:min(8, len(token))]+"..."))
			http.Error(w, "Invalid or expired token", http.StatusUnauthorized)
			return
		}
		logger.Info("Token validated successfully", zap.String("remote", remoteAddr))
	} else if isLocalhost {
		// Localhost (USB) connections are trusted
		logger.Info("Localhost connection allowed (USB)", zap.String("remote", remoteAddr))
	}

	conn, err := upgrader.Upgrade(w, r, nil)
	if err != nil {
		logger.Error("WebSocket upgrade failed",
			zap.Error(err),
			zap.String("remote", remoteAddr))
		return
	}

	logger.Info("WebSocket connected",
		zap.String("remote", remoteAddr),
		zap.Bool("is-host", isHost),
		zap.String("device-id", deviceID))

	// Generate peer ID
	peerID := generatePeerID()

	peer := &Peer{
		ID:       peerID,
		Conn:     conn,
		Send:     make(chan []byte, 256),
		Hub:      hub,
		Logger:   logger,
		LastPing: time.Now(),
	}

	hub.register <- peer

	// Start read/write pumps
	go peer.writePump()
	go peer.readPump()
}

func extractToken(r *http.Request) string {
	authz := r.Header.Get("Authorization")
	if strings.HasPrefix(strings.ToLower(authz), "bearer ") {
		return strings.TrimSpace(authz[7:])
	}
	return r.URL.Query().Get("token")
}

// min returns the minimum of two integers
func min(a, b int) int {
	if a < b {
		return a
	}
	return b
}

func (p *Peer) readPump() {
	defer func() {
		p.Hub.unregister <- p
		p.Conn.Close()
	}()

	p.Conn.SetReadLimit(64 * 1024) // 64KB max message size
	p.Conn.SetReadDeadline(time.Now().Add(60 * time.Second))
	p.Conn.SetPongHandler(func(string) error {
		p.Conn.SetReadDeadline(time.Now().Add(60 * time.Second))
		p.mu.Lock()
		p.LastPing = time.Now()
		p.mu.Unlock()
		return nil
	})

	for {
		_, data, err := p.Conn.ReadMessage()
		if err != nil {
			if websocket.IsUnexpectedCloseError(err, websocket.CloseGoingAway, websocket.CloseAbnormalClosure) {
				p.Logger.Error("WebSocket read error", zap.Error(err))
			}
			break
		}

		var msg Message
		if err := json.Unmarshal(data, &msg); err != nil {
			p.Logger.Error("Failed to parse message", zap.Error(err))
			continue
		}

		// Handle ping/pong
		if msg.Type == MsgTypePing {
			p.Hub.sendToPeer(p, &Message{Type: MsgTypePong})
			continue
		}

		msg.From = p.ID
		p.Hub.broadcast <- &msg
	}
}

func (p *Peer) writePump() {
	ticker := time.NewTicker(30 * time.Second)
	defer func() {
		ticker.Stop()
		p.Conn.Close()
	}()

	for {
		select {
		case message, ok := <-p.Send:
			p.Conn.SetWriteDeadline(time.Now().Add(10 * time.Second))
			if !ok {
				p.Conn.WriteMessage(websocket.CloseMessage, []byte{})
				return
			}

			if err := p.Conn.WriteMessage(websocket.TextMessage, message); err != nil {
				p.Logger.Error("WebSocket write error", zap.Error(err))
				return
			}

		case <-ticker.C:
			p.Conn.SetWriteDeadline(time.Now().Add(10 * time.Second))
			if err := p.Conn.WriteMessage(websocket.PingMessage, nil); err != nil {
				return
			}
		}
	}
}

func generatePeerID() string {
	n := time.Now().UnixNano()
	const hex = "0123456789abcdef"
	if n == 0 {
		return "0"
	}
	b := make([]byte, 0, 16)
	for n > 0 {
		b = append(b, hex[n&0xf])
		n >>= 4
	}
	// Reverse
	for i, j := 0, len(b)-1; i < j; i, j = i+1, j-1 {
		b[i], b[j] = b[j], b[i]
	}
	return string(b)
}

// HostStatus represents information about an active host
type HostStatus struct {
	PeerID     string `json:"peer_id"`
	Name       string `json:"name"`
	Role       string `json:"role"`
	Room       string `json:"room,omitempty"`
	ActiveTime int64  `json:"active_time_seconds"`
	HasClients bool   `json:"has_clients"`
}

// GetActiveHosts returns a list of currently active hosts
func (h *Hub) GetActiveHosts() []HostStatus {
	h.mu.RLock()
	defer h.mu.RUnlock()

	hosts := make([]HostStatus, 0)
	now := time.Now()

	for _, peer := range h.peers {
		if peer.Role == RoleHost {
			hasClients := false

			// Check if host has any clients
			if peer.Room != "" {
				if room, ok := h.rooms[peer.Room]; ok {
					room.mu.RLock()
					hasClients = len(room.Clients) > 0
					room.mu.RUnlock()
				}
			}

			hosts = append(hosts, HostStatus{
				PeerID:     peer.ID,
				Name:       peer.Name,
				Role:       string(peer.Role),
				Room:       peer.Room,
				ActiveTime: int64(now.Sub(peer.LastPing).Seconds()),
				HasClients: hasClients,
			})
		}
	}

	return hosts
}

// HostsHandler handles HTTP requests for active hosts list
func (h *Hub) HostsHandler(w http.ResponseWriter, r *http.Request) {
	// Allow CORS for mobile apps
	w.Header().Set("Access-Control-Allow-Origin", "*")
	w.Header().Set("Access-Control-Allow-Methods", "GET, OPTIONS")
	w.Header().Set("Access-Control-Allow-Headers", "Content-Type")
	w.Header().Set("Content-Type", "application/json")

	if r.Method == "OPTIONS" {
		w.WriteHeader(http.StatusOK)
		return
	}

	hosts := h.GetActiveHosts()

	response := struct {
		Hosts     []HostStatus `json:"hosts"`
		Count     int          `json:"count"`
		Timestamp int64        `json:"timestamp"`
	}{
		Hosts:     hosts,
		Count:     len(hosts),
		Timestamp: time.Now().Unix(),
	}

	json.NewEncoder(w).Encode(response)
}
