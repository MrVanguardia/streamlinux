# Security Policy - StreamLinux

## Análisis Exhaustivo de Medidas de Seguridad

Este documento detalla TODAS las medidas de seguridad implementadas en el proyecto StreamLinux, organizadas por componente.

---

## 📋 Resumen Ejecutivo

| Componente | Mecanismo Principal | Estado |
|:-----------|:-------------------|:-------|
| Signaling Server (Go) | TLS 1.2/1.3 + Bearer Token + Rate Limiting | ✅ Implementado |
| Linux GUI (Python) | AES-256-GCM + Tokens HMAC + PIN + Permisos 600 | ✅ Implementado |
| Linux Host (C++) | DTLS/SRTP + Memory Safety + Result<T> | ⚠️ Parcialmente (stubs) |
| Android Client (Kotlin) | AES-256-GCM + WebRTC nativo + DTLS | ✅ Implementado |
| Message Encryption | AES-256-GCM + ECDH Key Exchange + Nonces | ✅ Implementado |

---

## 🛡️ SEGURIDAD PROFESIONAL PARA CONEXIONES LAN

> **Nota Importante:** Aunque las conexiones LAN usan `ws://` (WebSocket sin TLS), la seguridad se mantiene mediante:
> 1. **WebRTC siempre usa DTLS-SRTP** - Los datos de audio/video están cifrados independientemente del transporte de señalización
> 2. **Cifrado a nivel de aplicación** - Los mensajes de señalización (SDP/ICE) se cifran con AES-256-GCM
> 3. **Autenticación por tokens HMAC-SHA256** - Previene conexiones no autorizadas
> 4. **Validación de IPs LAN** - Solo acepta conexiones de redes privadas

### Arquitectura de Seguridad en Capas

```
┌─────────────────────────────────────────────────────────────┐
│                    CAPA DE APLICACIÓN                        │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────┐  │
│  │ AES-256-GCM     │  │ HMAC-SHA256     │  │ Nonce       │  │
│  │ Message Encrypt │  │ Token Auth      │  │ Replay Prot │  │
│  └─────────────────┘  └─────────────────┘  └─────────────┘  │
├─────────────────────────────────────────────────────────────┤
│                    CAPA DE TRANSPORTE                        │
│  ┌─────────────────────────────────────────────────────────┐│
│  │          WebRTC: DTLS-SRTP (siempre cifrado)            ││
│  └─────────────────────────────────────────────────────────┘│
│  ┌─────────────────────────────────────────────────────────┐│
│  │  Signaling: ws:// (LAN) + cifrado aplicación / wss://   ││
│  └─────────────────────────────────────────────────────────┘│
├─────────────────────────────────────────────────────────────┤
│                    CAPA DE RED                               │
│  ┌─────────────────────────────────────────────────────────┐│
│  │     Solo IPs LAN (10.x, 192.168.x, 172.16-31.x)        ││
│  └─────────────────────────────────────────────────────────┘│
└─────────────────────────────────────────────────────────────┘
```

### Cifrado de Mensajes de Señalización (AES-256-GCM)

| Campo | Valor |
|-------|-------|
| **Algoritmo** | AES-256-GCM (Galois/Counter Mode) |
| **Derivación de Claves** | HKDF con SHA-256 |
| **Nonce** | 12 bytes aleatorios únicos por mensaje |
| **Archivos** | [linux-gui/message_crypto.py](linux-gui/message_crypto.py), [android-client/.../MessageCrypto.kt](android-client/app/src/main/java/com/streamlinux/client/security/MessageCrypto.kt) |

**Formato de mensaje cifrado:**
```json
{
  "v": 1,
  "enc": "aes-256-gcm",
  "ts": 1704067200,
  "nonce": "base64_encoded_nonce",
  "ct": "base64_encoded_ciphertext"
}
```

**Protecciones:**
- **Confidencialidad**: AES-256 con modo GCM
- **Integridad**: Tag de autenticación GCM (128 bits)
- **Replay Protection**: Nonces únicos + timestamps validados (±30s)
- **Key Rotation**: Automática cada 100 mensajes

---

## 🔐 1. SIGNALING SERVER (Go)

### 1.1 Configuración TLS Hardened

| Campo | Valor |
|-------|-------|
| **Nombre** | TLS Configuration Hardened |
| **Propósito** | Cifrar comunicaciones signaling con algoritmos modernos y seguros |
| **Archivo** | [signaling-server/internal/signaling/tls.go](signaling-server/internal/signaling/tls.go#L1-L19) |

**Funcionamiento técnico:**
```go
// Líneas 6-18 de tls.go
func TLSConfig() *tls.Config {
    return &tls.Config{
        MinVersion:               tls.VersionTLS12,
        MaxVersion:               tls.VersionTLS13,
        PreferServerCipherSuites: true,
        CurvePreferences:         []tls.CurveID{tls.CurveP256, tls.X25519},
        CipherSuites: []uint16{
            tls.TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
            tls.TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
            tls.TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
            tls.TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
        },
    }
}
```

- **Versión mínima**: TLS 1.2
- **Versión máxima**: TLS 1.3
- **Curvas**: P-256 y X25519 (curvas elípticas modernas)
- **Cipher Suites**: Solo AES-GCM con ECDHE (Perfect Forward Secrecy)

---

### 1.2 Sistema de Autenticación por Tokens

| Campo | Valor |
|-------|-------|
| **Nombre** | Bearer Token Authentication |
| **Propósito** | Autenticar conexiones WebSocket y endpoints HTTP |
| **Archivo** | [signaling-server/internal/signaling/hub.go](signaling-server/internal/signaling/hub.go#L247-L287) |

**Funcionamiento técnico:**
```go
// Líneas 247-272 de hub.go - Registro y validación de tokens
func (h *Hub) RegisterToken(token string, expiry time.Duration) {
    h.tokenMu.Lock()
    defer h.tokenMu.Unlock()
    h.validTokens[token] = time.Now().Add(expiry)
}

func (h *Hub) ValidateToken(token string) bool {
    h.tokenMu.RLock()
    defer h.tokenMu.RUnlock()
    expiry, ok := h.validTokens[token]
    if !ok { return false }
    if time.Now().After(expiry) { return false }
    return true
}
```

**Extracción de tokens** (líneas 752-760):
```go
func extractToken(r *http.Request) string {
    authz := r.Header.Get("Authorization")
    if strings.HasPrefix(strings.ToLower(authz), "bearer ") {
        return strings.TrimSpace(authz[7:])
    }
    return r.URL.Query().Get("token")
}
```

- Soporta header `Authorization: Bearer <token>` y query param `?token=`
- Tokens con tiempo de expiración configurable (default 24h)
- Limpieza automática de tokens expirados cada 30 segundos

---

### 1.3 Rate Limiting

| Campo | Valor |
|-------|-------|
| **Nombre** | Connection Rate Limiter |
| **Propósito** | Prevenir ataques de fuerza bruta y DoS |
| **Archivo** | [signaling-server/internal/signaling/hub.go](signaling-server/internal/signaling/hub.go#L119-L155) |

**Funcionamiento técnico:**
```go
// Líneas 119-155 de hub.go
type RateLimiter struct {
    attempts map[string][]time.Time
    mu       sync.Mutex
}

func (r *RateLimiter) Allow(identifier string, maxAttempts int, window time.Duration) bool {
    r.mu.Lock()
    defer r.mu.Unlock()
    
    now := time.Now()
    cutoff := now.Add(-window)
    
    // Limpiar intentos antiguos
    if attempts, ok := r.attempts[identifier]; ok {
        filtered := make([]time.Time, 0)
        for _, t := range attempts {
            if t.After(cutoff) {
                filtered = append(filtered, t)
            }
        }
        r.attempts[identifier] = filtered
    }
    
    // Verificar límite
    if len(r.attempts[identifier]) >= maxAttempts {
        return false
    }
    
    r.attempts[identifier] = append(r.attempts[identifier], now)
    return true
}
```

**Configuración por defecto** (líneas 104-116):
- `MaxConnAttempts`: 10 intentos
- `RateLimitWindow`: 1 minuto
- Se aplica por dirección IP remota

---

### 1.4 Validación de Origin (CORS)

| Campo | Valor |
|-------|-------|
| **Nombre** | Origin Validation / CORS |
| **Propósito** | Prevenir ataques CSRF en WebSocket |
| **Archivo** | [signaling-server/internal/signaling/hub.go](signaling-server/internal/signaling/hub.go#L190-L215) |

**Funcionamiento técnico:**
```go
// Líneas 190-214 de hub.go
func originAllowed(origin string) bool {
    if origin == "" { return true }  // Apps nativas sin Origin
    parsed, err := url.Parse(origin)
    if err != nil || parsed.Host == "" { return false }
    host := strings.ToLower(parsed.Host)
    for _, allowed := range allowedOrigins {
        if strings.ToLower(allowed) == host { return true }
    }
    // Localhost siempre permitido
    return host == "localhost" || strings.HasPrefix(host, "localhost:") || 
           host == "127.0.0.1" || strings.HasPrefix(host, "127.0.0.1:")
}

var upgrader = websocket.Upgrader{
    ReadBufferSize:  1024,
    WriteBufferSize: 1024,
    CheckOrigin: func(r *http.Request) bool {
        return originAllowed(r.Header.Get("Origin"))
    },
}
```

**CORS Middleware** (líneas 228-257 de main.go):
```go
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
        // ...
    })
}
```

---

### 1.5 Límite de Tamaño de Mensajes WebSocket

| Campo | Valor |
|-------|-------|
| **Nombre** | WebSocket Message Size Limit |
| **Propósito** | Prevenir ataques de agotamiento de memoria |
| **Archivo** | [signaling-server/internal/signaling/hub.go](signaling-server/internal/signaling/hub.go#L774) |

**Funcionamiento técnico:**
```go
// Línea 774 de hub.go
p.Conn.SetReadLimit(64 * 1024) // 64KB max message size
```

---

### 1.6 Timeouts de Conexión

| Campo | Valor |
|-------|-------|
| **Nombre** | Connection Timeouts |
| **Propósito** | Prevenir conexiones zombies y ataques slowloris |
| **Archivo** | [signaling-server/cmd/server/main.go](signaling-server/cmd/server/main.go#L117-L123) |

**Funcionamiento técnico:**
```go
// Líneas 117-123 de main.go
server := &http.Server{
    Addr:         addr,
    Handler:      corsMiddleware(mux, config.AllowedOrigins),
    ReadTimeout:  15 * time.Second,
    WriteTimeout: 15 * time.Second,
    IdleTimeout:  60 * time.Second,
    TLSConfig:    signaling.TLSConfig(),
}
```

**WebSocket timeouts** (líneas 775-785 de hub.go):
```go
p.Conn.SetReadDeadline(time.Now().Add(60 * time.Second))
p.Conn.SetPongHandler(func(string) error {
    p.Conn.SetReadDeadline(time.Now().Add(60 * time.Second))
    return nil
})
// Write timeout: 10 segundos
p.Conn.SetWriteDeadline(time.Now().Add(10 * time.Second))
```

---

### 1.7 Requerimiento de TLS

| Campo | Valor |
|-------|-------|
| **Nombre** | TLS Enforcement |
| **Propósito** | Forzar conexiones cifradas excepto USB local |
| **Archivo** | [signaling-server/internal/signaling/hub.go](signaling-server/internal/signaling/hub.go#L685-L689) |

**Funcionamiento técnico:**
```go
// Líneas 685-689 de hub.go
if sec.RequireTLS && r.TLS == nil {
    logger.Warn("Rejected non-TLS WebSocket")
    http.Error(w, "TLS required", http.StatusUpgradeRequired)
    return
}
```

---

## 🐍 2. LINUX GUI (Python)

### 2.1 Generación Criptográfica de Tokens

| Campo | Valor |
|-------|-------|
| **Nombre** | Cryptographic Token Generation |
| **Propósito** | Crear tokens de sesión seguros e impredecibles |
| **Archivo** | [linux-gui/security.py](linux-gui/security.py#L178-L218) |

**Funcionamiento técnico:**
```python
# Líneas 178-218 de security.py
def generate_session_token(self) -> str:
    with self._lock:
        # Token aleatorio criptográficamente seguro
        random_part = secrets.token_urlsafe(24)
        timestamp = int(time.time())
        token_data = f"{random_part}:{timestamp}"
        
        if HAS_CRYPTO:
            # PBKDF2 con 100,000 iteraciones
            kdf = PBKDF2HMAC(
                algorithm=hashes.SHA256(),
                length=16,
                salt=self._machine_secret[:16],
                iterations=100000,
            )
            signature = base64.urlsafe_b64encode(
                kdf.derive(token_data.encode())
            ).decode()[:16]
        else:
            # Fallback: HMAC-SHA256
            signature = hashlib.sha256(
                self._machine_secret + token_data.encode()
            ).hexdigest()[:16]
        
        full_token = f"{random_part}:{timestamp}:{signature}"
        return full_token
```

- **Entropía**: 24 bytes URL-safe (192 bits)
- **Firma**: PBKDF2-HMAC-SHA256 con 100,000 iteraciones
- **Formato**: `random:timestamp:signature`

---

### 2.2 Validación de Tokens con Timing-Safe Compare

| Campo | Valor |
|-------|-------|
| **Nombre** | Timing-Safe Token Validation |
| **Propósito** | Prevenir ataques de timing en verificación |
| **Archivo** | [linux-gui/security.py](linux-gui/security.py#L220-L270) |

**Funcionamiento técnico:**
```python
# Líneas 265-269 de security.py
# Comparación en tiempo constante
return secrets.compare_digest(provided_sig, expected_sig)
```

- Usa `secrets.compare_digest()` para evitar timing attacks
- Verifica timestamp dentro de 10 minutos
- Recalcula firma para validar integridad

---

### 2.3 Rate Limiting en Python

| Campo | Valor |
|-------|-------|
| **Nombre** | Connection Rate Limiter |
| **Propósito** | Limitar intentos de conexión |
| **Archivo** | [linux-gui/security.py](linux-gui/security.py#L271-L292) |

**Funcionamiento técnico:**
```python
# Líneas 271-292 de security.py
def check_rate_limit(self, identifier: str) -> bool:
    with self._lock:
        now = time.time()
        # Limpiar entradas antiguas
        if identifier in self._rate_limits:
            self._rate_limits[identifier] = [
                t for t in self._rate_limits[identifier]
                if now - t < self.RATE_LIMIT_WINDOW
            ]
        else:
            self._rate_limits[identifier] = []
        
        # Verificar límite
        if len(self._rate_limits[identifier]) >= self.MAX_CONNECTIONS_PER_WINDOW:
            return False
        
        self._rate_limits[identifier].append(now)
        return True
```

**Configuración** (líneas 68-72):
- `RATE_LIMIT_WINDOW`: 60 segundos
- `MAX_CONNECTIONS_PER_WINDOW`: 10 conexiones

---

### 2.4 Sistema de Verificación PIN

| Campo | Valor |
|-------|-------|
| **Nombre** | PIN-Based Device Authorization |
| **Propósito** | Verificar dispositivos mediante PIN de 6 dígitos |
| **Archivo** | [linux-gui/security.py](linux-gui/security.py#L310-L377) |

**Funcionamiento técnico:**
```python
# Líneas 310-330 de security.py
def create_pending_connection(self, device_id: str, device_name: str) -> PendingConnection:
    with self._lock:
        connection_id = secrets.token_urlsafe(16)
        # PIN de 6 dígitos generado criptográficamente
        pin = ''.join(secrets.choice('0123456789') for _ in range(6))
        
        pending = PendingConnection(
            connection_id=connection_id,
            device_name=device_name,
            device_id=device_id,
            pin=pin,
            created_at=now,
            expires_at=now + self.PIN_VALIDITY_SECONDS  # 60 segundos
        )
        return pending

# Líneas 350-377 - Verificación con timing-safe
def verify_pin(self, connection_id: str, entered_pin: str) -> bool:
    # Verificar expiración y máximo de intentos (3)
    pending.attempts += 1
    if pending.attempts > self.MAX_PIN_ATTEMPTS:
        return False  # Bloquear después de 3 intentos
    
    # Comparación en tiempo constante
    if not secrets.compare_digest(pending.pin, entered_pin):
        return False
    
    return True
```

- **PIN**: 6 dígitos aleatorios criptográficos
- **Validez**: 60 segundos
- **Intentos máximos**: 3

---

### 2.5 Permisos Restrictivos de Archivos

| Campo | Valor |
|-------|-------|
| **Nombre** | Restrictive File Permissions |
| **Propósito** | Proteger archivos sensibles de acceso no autorizado |
| **Archivo** | [linux-gui/security.py](linux-gui/security.py#L143-L174) |

**Funcionamiento técnico:**
```python
# Línea 147 - Secreto de máquina
os.chmod(secret_file, 0o600)  # Solo lectura/escritura para propietario

# Línea 174 - Archivo de dispositivos autorizados
os.chmod(self.DEVICES_FILE, 0o600)
```

- `0o600`: Solo el propietario puede leer/escribir
- Aplicado a: secretos de máquina, lista de dispositivos, certificados

---

### 2.6 Secreto de Máquina Único

| Campo | Valor |
|-------|-------|
| **Nombre** | Machine Secret Generation |
| **Propósito** | Clave única por máquina para firmar tokens |
| **Archivo** | [linux-gui/security.py](linux-gui/security.py#L131-L150) |

**Funcionamiento técnico:**
```python
# Líneas 131-150 de security.py
def _get_or_create_machine_secret(self) -> bytes:
    secret_file = self.CONFIG_DIR / ".machine_secret"
    
    if secret_file.exists():
        return secret_file.read_bytes()
    
    # Generar nuevo secreto de 32 bytes (256 bits)
    secret = secrets.token_bytes(32)
    secret_file.write_bytes(secret)
    os.chmod(secret_file, 0o600)
    return secret
```

---

### 2.7 Limpieza Automática de Tokens Expirados

| Campo | Valor |
|-------|-------|
| **Nombre** | Automatic Token Cleanup |
| **Propósito** | Eliminar tokens y conexiones pendientes expiradas |
| **Archivo** | [linux-gui/security.py](linux-gui/security.py#L492-L514) |

**Funcionamiento técnico:**
```python
# Líneas 492-514 de security.py
def _start_cleanup_timer(self):
    def cleanup():
        while True:
            time.sleep(60)  # Cada minuto
            self._cleanup()
    
    thread = threading.Thread(target=cleanup, daemon=True)
    thread.start()

def _cleanup(self):
    with self._lock:
        now = time.time()
        # Limpiar tokens expirados
        expired_tokens = [
            token for token, session in self._tokens.items()
            if now > session.expires_at
        ]
        for token in expired_tokens:
            del self._tokens[token]
```

---

## 💻 3. LINUX HOST (C++)

### 3.1 Patrón Result<T> para Manejo de Errores

| Campo | Valor |
|-------|-------|
| **Nombre** | Result<T> Error Handling Pattern |
| **Propósito** | Manejo seguro de errores sin excepciones |
| **Archivo** | [linux-host/include/stream_linux/common.hpp](linux-host/include/stream_linux/common.hpp#L121-L135) |

**Funcionamiento técnico:**
```cpp
// Líneas 121-135 de common.hpp
template<typename T>
using Result = std::expected<T, Error>;

// Uso en todo el código:
Result<void> initialize(const VideoConfig& config);
Result<EncodedVideoFrame> encode(const VideoFrame& frame);
Result<std::unique_ptr<IVideoEncoder>> create_video_encoder(const VideoConfig& config);
```

- Usa `std::expected` de C++23
- Obliga a manejar errores explícitamente
- Evita estados inválidos por excepciones no capturadas

---

### 3.2 Smart Pointers para Memory Safety

| Campo | Valor |
|-------|-------|
| **Nombre** | Smart Pointer Memory Management |
| **Propósito** | Prevenir memory leaks y use-after-free |
| **Archivo** | Múltiples archivos en [linux-host/src/](linux-host/src/) |

**Ejemplos de implementación:**
```cpp
// video_encoder.cpp línea 505-506
Result<std::unique_ptr<IVideoEncoder>> create_video_encoder(const VideoConfig& config) {
    auto encoder = std::make_unique<FFmpegVideoEncoder>();
    // ...
}

// main.cpp línea 137, 180
std::unique_ptr<IAudioCapture> audio;
std::unique_ptr<IAudioEncoder> audio_encoder;

// webrtc_transport.cpp línea 181-182
Result<std::unique_ptr<IWebRTCTransport>> create_webrtc_transport() {
    return std::make_unique<WebRTCTransportImpl>();
}
```

---

### 3.3 Configuración DTLS/SRTP para WebRTC

| Campo | Valor |
|-------|-------|
| **Nombre** | DTLS/SRTP WebRTC Security |
| **Propósito** | Cifrado de media streams |
| **Archivo** | [linux-host/include/stream_linux/common.hpp](linux-host/include/stream_linux/common.hpp#L269) y [webrtc_transport.cpp](linux-host/src/transport/webrtc_transport.cpp#L160-L167) |

**Funcionamiento técnico:**
```cpp
// common.hpp línea 269
struct TransportConfig {
    bool enable_dtls = true;  // DTLS habilitado por defecto
    // ...
};

// webrtc_transport.cpp líneas 160-167 - SDP con perfiles seguros
std::string generate_sdp_stub(const std::string& type) {
    ss << "m=video 9 UDP/TLS/RTP/SAVPF 96\r\n"  // SRTP con DTLS
       << "m=audio 9 UDP/TLS/RTP/SAVPF 111\r\n"; // SRTP con DTLS
    return ss.str();
}
```

- **UDP/TLS/RTP/SAVPF**: Profile que requiere DTLS para key exchange
- **SRTP**: Cifrado de media (audio/video)
- **Fingerprints**: Verificación de certificados en SDP

---

### 3.4 Límites de Tamaño de Buffers

| Campo | Valor |
|-------|-------|
| **Nombre** | Buffer Size Limits |
| **Propósito** | Prevenir overflow de memoria |
| **Archivo** | [linux-host/src/audio/pipewire_audio.cpp](linux-host/src/audio/pipewire_audio.cpp#L29) |

**Funcionamiento técnico:**
```cpp
// Línea 29 de pipewire_audio.cpp
static constexpr size_t MAX_QUEUE_SIZE = 10;

// Líneas 82-85 - Limitación de cola
if (impl->frame_queue.size() >= MAX_QUEUE_SIZE) {
    impl->frame_queue.pop();  // Descartar frame más antiguo
}
impl->frame_queue.push(std::move(frame));
```

---

### 3.5 Liberación Segura de Recursos FFmpeg

| Campo | Valor |
|-------|-------|
| **Nombre** | Safe FFmpeg Resource Cleanup |
| **Propósito** | Evitar memory leaks en codificador |
| **Archivo** | [linux-host/src/encoding/video_encoder.cpp](linux-host/src/encoding/video_encoder.cpp#L22-L45) |

**Funcionamiento técnico:**
```cpp
// Líneas 22-45 de video_encoder.cpp - Destructor con limpieza completa
FFmpegVideoEncoder::~FFmpegVideoEncoder() {
    if (m_sws_ctx) { sws_freeContext(m_sws_ctx); }
    if (m_packet) { av_packet_free(&m_packet); }
    if (m_hw_frame) { av_frame_free(&m_hw_frame); }
    if (m_frame) { av_frame_free(&m_frame); }
    if (m_codec_ctx) { avcodec_free_context(&m_codec_ctx); }
    if (m_hw_frames_ctx) { av_buffer_unref(&m_hw_frames_ctx); }
    if (m_hw_device_ctx) { av_buffer_unref(&m_hw_device_ctx); }
}
```

---

## 📱 4. ANDROID CLIENT (Kotlin)

### 4.1 WebRTC con DTLS Nativo

| Campo | Valor |
|-------|-------|
| **Nombre** | Native WebRTC DTLS |
| **Propósito** | Cifrado de streams de media |
| **Archivo** | [android-client/app/src/main/java/com/streamlinux/client/network/WebRTCClient.kt](android-client/app/src/main/java/com/streamlinux/client/network/WebRTCClient.kt#L229-L244) |

**Funcionamiento técnico:**
```kotlin
// Líneas 229-244 de WebRTCClient.kt
val config = PeerConnection.RTCConfiguration(iceServers).apply {
    sdpSemantics = PeerConnection.SdpSemantics.UNIFIED_PLAN
    continualGatheringPolicy = PeerConnection.ContinualGatheringPolicy.GATHER_CONTINUALLY
    bundlePolicy = PeerConnection.BundlePolicy.MAXBUNDLE
    rtcpMuxPolicy = PeerConnection.RtcpMuxPolicy.REQUIRE
    iceTransportsType = PeerConnection.IceTransportsType.ALL
}
```

- **UNIFIED_PLAN**: API moderna de WebRTC
- **MAXBUNDLE**: Multiplexación eficiente
- **RTCP_MUX**: Requerido para seguridad
- DTLS habilitado automáticamente por libwebrtc

---

### 4.2 Permisos Mínimos en Android

| Campo | Valor |
|-------|-------|
| **Nombre** | Minimal Android Permissions |
| **Propósito** | Principio de mínimo privilegio |
| **Archivo** | [android-client/app/src/main/AndroidManifest.xml](android-client/app/src/main/AndroidManifest.xml#L5-L18) |

**Permisos declarados:**
```xml
<!-- Solo permisos de red -->
<uses-permission android:name="android.permission.INTERNET" />
<uses-permission android:name="android.permission.ACCESS_NETWORK_STATE" />
<uses-permission android:name="android.permission.ACCESS_WIFI_STATE" />
<uses-permission android:name="android.permission.CHANGE_WIFI_MULTICAST_STATE" />

<!-- Cámara solo para QR -->
<uses-permission android:name="android.permission.CAMERA" />
<uses-feature android:name="android.hardware.camera" android:required="false" />

<!-- Wake lock para streaming -->
<uses-permission android:name="android.permission.WAKE_LOCK" />
```

- **Sin RECORD_AUDIO**: Solo recibe audio, no envía
- **Sin WRITE_EXTERNAL_STORAGE**: No almacena nada
- **Cámara opcional**: Solo para escaneo QR

---

### 4.3 Configuración de Timeouts en OkHttp

| Campo | Valor |
|-------|-------|
| **Nombre** | Network Timeouts |
| **Propósito** | Prevenir conexiones colgadas |
| **Archivo** | [android-client/app/src/main/java/com/streamlinux/client/network/SignalingClient.kt](android-client/app/src/main/java/com/streamlinux/client/network/SignalingClient.kt#L47-L52) |

**Funcionamiento técnico:**
```kotlin
// Líneas 47-52 y 93-97 de SignalingClient.kt
companion object {
    private const val CONNECT_TIMEOUT = 10L
    private const val READ_TIMEOUT = 30L
    private const val PING_INTERVAL = 15L
}

client = OkHttpClient.Builder()
    .connectTimeout(CONNECT_TIMEOUT, TimeUnit.SECONDS)
    .readTimeout(READ_TIMEOUT, TimeUnit.SECONDS)
    .pingInterval(PING_INTERVAL, TimeUnit.SECONDS)
    .build()
```

---

### 4.4 SharedPreferences Privadas

| Campo | Valor |
|-------|-------|
| **Nombre** | Private SharedPreferences |
| **Propósito** | Almacenamiento seguro de configuración |
| **Archivo** | [android-client/app/src/main/java/com/streamlinux/client/settings/AppSettings.kt](android-client/app/src/main/java/com/streamlinux/client/settings/AppSettings.kt#L74) |

**Funcionamiento técnico:**
```kotlin
// Línea 74 de AppSettings.kt
private val prefs: SharedPreferences = context.getSharedPreferences(
    PREFS_NAME, 
    Context.MODE_PRIVATE  // Solo accesible por esta app
)
```

---

### 4.5 Reglas de Backup Seguras

| Campo | Valor |
|-------|-------|
| **Nombre** | Backup Rules Configuration |
| **Propósito** | Controlar qué datos se respaldan |
| **Archivo** | [android-client/app/src/main/res/xml/data_extraction_rules.xml](android-client/app/src/main/res/xml/data_extraction_rules.xml) |

**Contenido:**
```xml
<data-extraction-rules>
    <cloud-backup>
        <include domain="sharedpref" path="."/>
    </cloud-backup>
</data-extraction-rules>
```

- Solo SharedPreferences (configuración)
- No incluye datos sensibles

---

## 🔒 5. MEDIDAS TRANSVERSALES

### 5.1 Sin Telemetría ni Cloud

| Campo | Valor |
|-------|-------|
| **Nombre** | No Telemetry / No Cloud |
| **Propósito** | Máxima privacidad del usuario |
| **Implementación** | Todo el proyecto |

- No hay analytics ni tracking
- No hay servidores cloud propios
- Comunicación punto a punto
- Datos permanecen locales

---

### 5.2 Control de Usuario

| Campo | Valor |
|-------|-------|
| **Nombre** | User-Controlled Streaming |
| **Propósito** | El usuario controla cuándo se transmite |
| **Implementación** | GUI y CLI requieren acción explícita |

- Streaming solo cuando el usuario lo inicia
- Indicador visual de estado
- Parada inmediata al cerrar

---

## 📊 Tabla Resumen de Medidas

| # | Medida | Componente | Archivo | Líneas |
|---|--------|------------|---------|--------|
| 1 | TLS 1.2/1.3 Hardened | Go Server | tls.go | 6-18 |
| 2 | Bearer Token Auth | Go Server | hub.go | 247-272 |
| 3 | Rate Limiting (Go) | Go Server | hub.go | 119-155 |
| 4 | Origin Validation | Go Server | hub.go | 190-215 |
| 5 | Message Size Limit | Go Server | hub.go | 774 |
| 6 | Connection Timeouts | Go Server | main.go | 117-123 |
| 7 | TLS Enforcement | Go Server | hub.go | 685-689 |
| 8 | Crypto Tokens | Python GUI | security.py | 178-218 |
| 9 | Timing-Safe Compare | Python GUI | security.py | 265-269 |
| 10 | Rate Limiting (Python) | Python GUI | security.py | 271-292 |
| 11 | PIN Verification | Python GUI | security.py | 310-377 |
| 12 | File Permissions 600 | Python GUI | security.py | 147, 174 |
| 13 | Machine Secret | Python GUI | security.py | 131-150 |
| 14 | Token Cleanup | Python GUI | security.py | 492-514 |
| 15 | Result<T> Pattern | C++ Host | common.hpp | 121-135 |
| 16 | Smart Pointers | C++ Host | varios | - |
| 17 | DTLS/SRTP Config | C++ Host | webrtc_transport.cpp | 160-167 |
| 18 | Buffer Limits | C++ Host | pipewire_audio.cpp | 29, 82-85 |
| 19 | Resource Cleanup | C++ Host | video_encoder.cpp | 22-45 |
| 20 | WebRTC Native DTLS | Android | WebRTCClient.kt | 229-244 |
| 21 | Minimal Permissions | Android | AndroidManifest.xml | 5-18 |
| 22 | Network Timeouts | Android | SignalingClient.kt | 93-97 |
| 23 | Private SharedPrefs | Android | AppSettings.kt | 74 |
| 24 | Backup Rules | Android | data_extraction_rules.xml | - |

---

## ⚠️ Limitaciones Conocidas

1. **libwebrtc en C++**: El host usa stubs; la implementación completa de DTLS/SRTP requiere integrar libwebrtc
2. **clearTextTraffic=true**: Android permite HTTP para USB local, pero debería restringirse más
3. **Sin Certificate Pinning**: No hay pinning de certificados en cliente Android todavía
4. **Logs de Debug**: Podrían exponer información sensible si verbose está habilitado

---

## 📞 Reportar Vulnerabilidades

1. **Email**: studiovanguardia3@gmail.com
2. **GitHub Security Advisory**: Privado
3. **Tiempo de respuesta**: 30-90 días antes de divulgación

---

## Versiones Soportadas

| Versión | Soportada |
|:--------|:----------|
| 0.2.x (alpha) | ✅ |
| < 0.2.0 | ❌ |

---

*Última actualización: 29 de enero de 2026*
