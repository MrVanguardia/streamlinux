/**
 * mDNS/DNS-SD Discovery
 *
 * Advertises the signaling server on the local network using mDNS/DNS-SD
 */
package discovery

import (
	"fmt"
	"net"
	"os"
	"strings"
	"sync"

	"go.uber.org/zap"
)

const (
	mdnsPort    = 5353
	mdnsAddr    = "224.0.0.251"
	serviceName = "_streamlinux._tcp.local."
	serviceType = "_streamlinux._tcp"
)

// MDNSServer handles mDNS discovery
type MDNSServer struct {
	port     int
	hostname string
	conn     *net.UDPConn
	logger   *zap.Logger
	done     chan struct{}
	wg       sync.WaitGroup
}

// NewMDNSServer creates a new mDNS server
func NewMDNSServer(servicePort int, logger *zap.Logger) (*MDNSServer, error) {
	hostname, _ := os.Hostname()
	if hostname == "" {
		hostname = "streamlinux-host"
	}
	// Clean hostname for mDNS
	hostname = strings.ReplaceAll(hostname, " ", "-")

	return &MDNSServer{
		port:     servicePort,
		hostname: hostname,
		logger:   logger,
		done:     make(chan struct{}),
	}, nil
}

// Start starts the mDNS server
func (s *MDNSServer) Start() error {
	// Join multicast group
	addr, err := net.ResolveUDPAddr("udp4", fmt.Sprintf("%s:%d", mdnsAddr, mdnsPort))
	if err != nil {
		return err
	}

	conn, err := net.ListenMulticastUDP("udp4", nil, addr)
	if err != nil {
		return fmt.Errorf("failed to listen on mDNS multicast: %w", err)
	}

	s.conn = conn

	// Start listening for queries
	s.wg.Add(1)
	go s.listen()

	// Announce service
	s.announce()

	s.logger.Info("mDNS server started",
		zap.String("service", serviceName),
		zap.String("hostname", s.hostname),
		zap.Int("port", s.port))

	return nil
}

// Stop stops the mDNS server
func (s *MDNSServer) Stop() {
	close(s.done)
	if s.conn != nil {
		s.conn.Close()
	}
	s.wg.Wait()
	s.logger.Info("mDNS server stopped")
}

func (s *MDNSServer) listen() {
	defer s.wg.Done()
	buf := make([]byte, 1500)

	for {
		select {
		case <-s.done:
			return
		default:
		}

		n, remoteAddr, err := s.conn.ReadFromUDP(buf)
		if err != nil {
			if strings.Contains(err.Error(), "closed") {
				return
			}
			s.logger.Debug("mDNS read error", zap.Error(err))
			continue
		}

		// Check if this is a query for our service
		if s.isServiceQuery(buf[:n]) {
			s.respondTo(remoteAddr)
		}
	}
}

func (s *MDNSServer) isServiceQuery(data []byte) bool {
	// Simplified check - look for our service name in the query
	return strings.Contains(string(data), "_streamlinux") ||
		strings.Contains(string(data), serviceType)
}

func (s *MDNSServer) announce() {
	response := s.buildResponse()
	s.sendMulticast(response)
}

func (s *MDNSServer) respondTo(addr *net.UDPAddr) {
	response := s.buildResponse()
	s.sendTo(response, addr)
}

func (s *MDNSServer) buildResponse() []byte {
	txt := fmt.Sprintf("streamlinux=%s:%d", s.hostname, s.port)

	response := make([]byte, 0, 512)

	// Header (12 bytes)
	response = append(response,
		0, 0, // Transaction ID
		0x84, 0x00, // Flags: QR=1, AA=1
		0, 0, // Questions
		0, 1, // Answers
		0, 0, // Authority
		0, 0, // Additional
	)

	// Answer: PTR record pointing to our service
	response = append(response, s.encodeName(serviceName)...)
	response = append(response,
		0, 12, // Type: PTR
		0, 1, // Class: IN
		0, 0, 0x0e, 0x10, // TTL: 3600
	)
	instanceName := fmt.Sprintf("%s.%s", s.hostname, serviceName)
	nameData := s.encodeName(instanceName)
	response = append(response,
		byte(len(nameData)>>8), byte(len(nameData)),
	)
	response = append(response, nameData...)

	// TXT record with port info
	response = append(response, s.encodeName(instanceName)...)
	response = append(response,
		0, 16, // Type: TXT
		0, 1, // Class: IN
		0, 0, 0x0e, 0x10, // TTL: 3600
	)
	txtData := []byte{byte(len(txt))}
	txtData = append(txtData, []byte(txt)...)
	response = append(response,
		byte(len(txtData)>>8), byte(len(txtData)),
	)
	response = append(response, txtData...)

	return response
}

func (s *MDNSServer) encodeName(name string) []byte {
	var result []byte
	parts := strings.Split(name, ".")
	for _, part := range parts {
		if part == "" {
			continue
		}
		result = append(result, byte(len(part)))
		result = append(result, []byte(part)...)
	}
	result = append(result, 0) // Root label
	return result
}

func (s *MDNSServer) sendMulticast(data []byte) {
	addr, _ := net.ResolveUDPAddr("udp4", fmt.Sprintf("%s:%d", mdnsAddr, mdnsPort))
	s.sendTo(data, addr)
}

func (s *MDNSServer) sendTo(data []byte, addr *net.UDPAddr) {
	conn, err := net.DialUDP("udp4", nil, addr)
	if err != nil {
		s.logger.Debug("Failed to send mDNS response", zap.Error(err))
		return
	}
	defer conn.Close()
	conn.Write(data)
}

// LANScanner scans local network for StreamLinux hosts
type LANScanner struct {
	logger *zap.Logger
}

// NewLANScanner creates a new LAN scanner
func NewLANScanner(logger *zap.Logger) *LANScanner {
	return &LANScanner{logger: logger}
}

// DiscoveredHost represents a found StreamLinux host
type DiscoveredHost struct {
	IP       string `json:"ip"`
	Port     int    `json:"port"`
	Hostname string `json:"hostname"`
}

// Scan scans the local network for StreamLinux hosts
func (s *LANScanner) Scan(timeout int) []DiscoveredHost {
	var hosts []DiscoveredHost
	var mu sync.Mutex
	var wg sync.WaitGroup

	localIP := s.getLocalIP()
	if localIP == "" {
		return hosts
	}

	parts := strings.Split(localIP, ".")
	if len(parts) != 4 {
		return hosts
	}
	subnet := strings.Join(parts[:3], ".")

	for i := 1; i < 255; i++ {
		wg.Add(1)
		go func(ip string) {
			defer wg.Done()

			addr := fmt.Sprintf("%s:8080", ip)
			conn, err := net.DialTimeout("tcp", addr, 100*1e6)
			if err == nil {
				conn.Close()
				mu.Lock()
				hosts = append(hosts, DiscoveredHost{
					IP:   ip,
					Port: 8080,
				})
				mu.Unlock()
			}
		}(fmt.Sprintf("%s.%d", subnet, i))
	}

	wg.Wait()
	return hosts
}

func (s *LANScanner) getLocalIP() string {
	addrs, err := net.InterfaceAddrs()
	if err != nil {
		return ""
	}

	for _, addr := range addrs {
		if ipnet, ok := addr.(*net.IPNet); ok && !ipnet.IP.IsLoopback() {
			if ipnet.IP.To4() != nil {
				return ipnet.IP.String()
			}
		}
	}
	return ""
}
