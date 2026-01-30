/**
 * QR Code Generation Handler
 *
 * Generates QR codes for easy device pairing
 */
package qr

import (
	"encoding/base64"
	"encoding/json"
	"fmt"
	"net"
	"net/http"

	"github.com/skip2/go-qrcode"
)

// ConnectionInfo contains information for client connection
type ConnectionInfo struct {
	Protocol string `json:"protocol"`
	Host     string `json:"host"`
	Port     int    `json:"port"`
	Room     string `json:"room,omitempty"`
	URL      string `json:"url"`
}

// Handler handles QR code generation requests
type Handler struct {
	host     string
	port     int
	useTLS   bool
	localIPs []string
}

// NewHandler creates a new QR handler
func NewHandler(host string, port int, useTLS bool) *Handler {
	h := &Handler{
		host:   host,
		port:   port,
		useTLS: useTLS,
	}
	h.localIPs = h.getLocalIPs()
	return h
}

// HandleQR returns connection info as JSON
func (h *Handler) HandleQR(w http.ResponseWriter, r *http.Request) {
	room := r.URL.Query().Get("room")

	infos := h.getConnectionInfos(room)

	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(infos)
}

// HandleQRImage returns QR code as PNG image
func (h *Handler) HandleQRImage(w http.ResponseWriter, r *http.Request) {
	room := r.URL.Query().Get("room")
	size := 256 // Default size

	infos := h.getConnectionInfos(room)
	if len(infos) == 0 {
		http.Error(w, "No network interfaces found", http.StatusInternalServerError)
		return
	}

	// Use first non-loopback IP
	var info ConnectionInfo
	for _, i := range infos {
		if i.Host != "127.0.0.1" && i.Host != "localhost" {
			info = i
			break
		}
	}
	if info.Host == "" {
		info = infos[0]
	}

	// Generate QR code
	data, err := json.Marshal(info)
	if err != nil {
		http.Error(w, "Failed to generate QR data", http.StatusInternalServerError)
		return
	}

	png, err := qrcode.Encode(string(data), qrcode.Medium, size)
	if err != nil {
		http.Error(w, "Failed to generate QR code", http.StatusInternalServerError)
		return
	}

	w.Header().Set("Content-Type", "image/png")
	w.Header().Set("Cache-Control", "no-cache")
	w.Write(png)
}

// HandleQRBase64 returns QR code as base64 encoded string
func (h *Handler) HandleQRBase64(w http.ResponseWriter, r *http.Request) {
	room := r.URL.Query().Get("room")

	infos := h.getConnectionInfos(room)
	if len(infos) == 0 {
		http.Error(w, "No network interfaces found", http.StatusInternalServerError)
		return
	}

	// Use first non-loopback IP
	var info ConnectionInfo
	for _, i := range infos {
		if i.Host != "127.0.0.1" {
			info = i
			break
		}
	}
	if info.Host == "" {
		info = infos[0]
	}

	data, _ := json.Marshal(info)
	png, err := qrcode.Encode(string(data), qrcode.Medium, 256)
	if err != nil {
		http.Error(w, "Failed to generate QR code", http.StatusInternalServerError)
		return
	}

	response := map[string]interface{}{
		"info":   info,
		"qr_png": base64.StdEncoding.EncodeToString(png),
	}

	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(response)
}

func (h *Handler) getConnectionInfos(room string) []ConnectionInfo {
	protocol := "ws"
	if h.useTLS {
		protocol = "wss"
	}

	infos := make([]ConnectionInfo, 0, len(h.localIPs))
	for _, ip := range h.localIPs {
		url := fmt.Sprintf("%s://%s:%d/ws", protocol, ip, h.port)
		if room != "" {
			url += "?room=" + room
		}

		infos = append(infos, ConnectionInfo{
			Protocol: protocol,
			Host:     ip,
			Port:     h.port,
			Room:     room,
			URL:      url,
		})
	}

	return infos
}

func (h *Handler) getLocalIPs() []string {
	var ips []string

	addrs, err := net.InterfaceAddrs()
	if err != nil {
		return ips
	}

	for _, addr := range addrs {
		if ipnet, ok := addr.(*net.IPNet); ok {
			if ipnet.IP.To4() != nil {
				ips = append(ips, ipnet.IP.String())
			}
		}
	}

	return ips
}
