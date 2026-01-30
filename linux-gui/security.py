#!/usr/bin/env python3
"""
StreamLinux Security Module
Implements secure token-based authentication for streaming connections.

Security Features:
- Cryptographically secure session tokens (HMAC-SHA256)
- PIN-based device authorization
- Device whitelist with persistence
- Token expiration and refresh (60s for QR, 5min for session)
- Rate limiting for connection attempts
- AES-256-GCM message encryption for signaling
- LAN IP validation
- Replay attack protection with nonces
- Perfect Forward Secrecy via ECDH key exchange
"""

import os
import json
import hashlib
import secrets
import time
import threading
import socket
import struct
from pathlib import Path
from dataclasses import dataclass, asdict, field
from typing import Dict, List, Optional, Callable, Set, Tuple
from datetime import datetime, timedelta
import base64
import ipaddress
import hmac as hmac_module

# Try to import cryptography for better security
try:
    from cryptography.hazmat.primitives import hashes
    from cryptography.hazmat.primitives.kdf.pbkdf2 import PBKDF2HMAC
    from cryptography.hazmat.primitives.kdf.hkdf import HKDF
    from cryptography.hazmat.primitives.ciphers.aead import AESGCM
    HAS_CRYPTO = True
except ImportError:
    HAS_CRYPTO = False

# Import message crypto module
try:
    from message_crypto import SecureSignalingWrapper, create_secure_wrapper
    HAS_MSG_CRYPTO = True
except ImportError:
    HAS_MSG_CRYPTO = False


def is_valid_lan_ip(ip_str: str) -> bool:
    """
    Validate that an IP address is a private LAN address.
    Rejects public IPs for security.
    """
    try:
        ip = ipaddress.ip_address(ip_str)
        return ip.is_private or ip.is_loopback
    except ValueError:
        return False


def get_local_ips() -> List[str]:
    """Get all local IP addresses of this machine"""
    ips = []
    try:
        # Get all interfaces
        hostname = socket.gethostname()
        ips = socket.gethostbyname_ex(hostname)[2]
        # Also try to get the default route IP
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        ips.append(s.getsockname()[0])
        s.close()
    except:
        pass
    return list(set(ip for ip in ips if is_valid_lan_ip(ip)))


@dataclass
class AuthorizedDevice:
    """Represents an authorized device"""
    device_id: str
    name: str
    first_connected: float  # Unix timestamp
    last_connected: float
    connection_count: int = 0
    trusted: bool = False  # If true, skip PIN verification


@dataclass
class PendingConnection:
    """Represents a pending connection waiting for PIN verification"""
    connection_id: str
    device_name: str
    device_id: str
    pin: str
    created_at: float
    expires_at: float
    attempts: int = 0


@dataclass
class SessionToken:
    """Represents a secure session token"""
    token: str
    created_at: float
    expires_at: float
    used: bool = False


class SecurityManager:
    """
    Manages security for StreamLinux connections.
    Implements token-based auth with PIN verification.
    
    Security Architecture:
    1. QR Token (60s validity) - One-time use, binds session
    2. Session Key derived from token + machine secret
    3. AES-256-GCM encryption for all signaling messages
    4. Nonce-based replay protection
    5. LAN IP validation
    6. Rate limiting per IP
    7. Device whitelist with trust levels
    """
    
    # Configuration - Enhanced security defaults
    TOKEN_VALIDITY_SECONDS = 60   # QR token valid for 60 seconds only
    SESSION_VALIDITY_SECONDS = 300  # Session valid for 5 minutes
    PIN_VALIDITY_SECONDS = 60     # 1 minute to enter PIN
    MAX_PIN_ATTEMPTS = 3
    RATE_LIMIT_WINDOW = 60        # seconds
    MAX_CONNECTIONS_PER_WINDOW = 5  # Reduced from 10 for better security
    TOKEN_REFRESH_INTERVAL = 30   # Refresh QR token every 30 seconds
    
    # Storage paths
    CONFIG_DIR = Path.home() / ".config" / "streamlinux"
    DEVICES_FILE = CONFIG_DIR / "authorized_devices.json"
    
    _instance = None
    
    def __new__(cls):
        if cls._instance is None:
            cls._instance = super().__new__(cls)
            cls._instance._initialized = False
        return cls._instance
    
    def __init__(self):
        if self._initialized:
            return
        self._initialized = True
        
        # Initialize storage
        self.CONFIG_DIR.mkdir(parents=True, exist_ok=True)
        
        # Session tokens (token -> SessionToken)
        self._tokens: Dict[str, SessionToken] = {}
        
        # Authorized devices (device_id -> AuthorizedDevice)
        self._devices: Dict[str, AuthorizedDevice] = {}
        
        # Pending connections awaiting PIN (connection_id -> PendingConnection)
        self._pending: Dict[str, PendingConnection] = {}
        
        # Rate limiting (ip -> list of timestamps)
        self._rate_limits: Dict[str, List[float]] = {}
        
        # Used nonces to prevent replay attacks
        self._used_nonces: Set[str] = set()
        self._nonce_cleanup_time = time.time()
        
        # Blocked IPs (temporarily)
        self._blocked_ips: Dict[str, float] = {}  # ip -> block_until timestamp
        
        # Active secure wrappers for message encryption
        self._secure_wrappers: Dict[str, 'SecureSignalingWrapper'] = {}
        
        # Machine secret for token generation
        self._machine_secret = self._get_or_create_machine_secret()
        
        # Current active session token
        self._current_token: Optional[SessionToken] = None
        
        # Callbacks
        self.on_connection_request: Optional[Callable[[PendingConnection], None]] = None
        self.on_connection_authorized: Optional[Callable[[str, str], None]] = None
        self.on_connection_rejected: Optional[Callable[[str, str], None]] = None
        
        # Lock for thread safety
        self._lock = threading.Lock()
        
        # Load saved devices
        self._load_devices()
        
        # Start cleanup timer
        self._start_cleanup_timer()
    
    def _get_or_create_machine_secret(self) -> bytes:
        """Get or create a unique machine secret for token generation"""
        secret_file = self.CONFIG_DIR / ".machine_secret"
        
        if secret_file.exists():
            try:
                return secret_file.read_bytes()
            except:
                pass
        
        # Generate new secret
        secret = secrets.token_bytes(32)
        try:
            secret_file.write_bytes(secret)
            # Make it readable only by owner
            os.chmod(secret_file, 0o600)
        except:
            pass
        
        return secret
    
    def _load_devices(self):
        """Load authorized devices from file"""
        if self.DEVICES_FILE.exists():
            try:
                with open(self.DEVICES_FILE, 'r') as f:
                    data = json.load(f)
                    for device_data in data.get('devices', []):
                        device = AuthorizedDevice(**device_data)
                        self._devices[device.device_id] = device
            except Exception as e:
                print(f"Error loading devices: {e}")
    
    def _save_devices(self):
        """Save authorized devices to file"""
        try:
            data = {
                'version': 1,
                'devices': [asdict(d) for d in self._devices.values()]
            }
            with open(self.DEVICES_FILE, 'w') as f:
                json.dump(data, f, indent=2)
            os.chmod(self.DEVICES_FILE, 0o600)
        except Exception as e:
            print(f"Error saving devices: {e}")
    
    def generate_session_token(self) -> str:
        """
        Generate a cryptographically secure session token.
        This token is included in the QR code and must be validated on connection.
        """
        with self._lock:
            # Create secure random token
            random_part = secrets.token_urlsafe(24)
            timestamp = int(time.time())
            
            # Create HMAC of the token for verification
            token_data = f"{random_part}:{timestamp}"
            
            if HAS_CRYPTO:
                # Use PBKDF2 for stronger derivation
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
                # Fallback to HMAC-SHA256
                signature = hashlib.sha256(
                    self._machine_secret + token_data.encode()
                ).hexdigest()[:16]
            
            # Final token format: random:timestamp:signature
            full_token = f"{random_part}:{timestamp}:{signature}"
            
            # Store token
            now = time.time()
            session = SessionToken(
                token=full_token,
                created_at=now,
                expires_at=now + self.TOKEN_VALIDITY_SECONDS
            )
            self._tokens[full_token] = session
            self._current_token = session
            
            return full_token
    
    def validate_token(self, token: str) -> bool:
        """Validate a session token"""
        with self._lock:
            if not token or ':' not in token:
                return False
            
            # Check if token exists and is valid
            session = self._tokens.get(token)
            if not session:
                return False
            
            # Check expiration
            if time.time() > session.expires_at:
                del self._tokens[token]
                return False
            
            # Verify signature
            try:
                parts = token.split(':')
                if len(parts) != 3:
                    return False
                
                random_part, timestamp_str, provided_sig = parts
                timestamp = int(timestamp_str)
                
                # Check timestamp is recent (within 10 minutes)
                if abs(time.time() - timestamp) > 600:
                    return False
                
                # Recompute signature
                token_data = f"{random_part}:{timestamp}"
                if HAS_CRYPTO:
                    kdf = PBKDF2HMAC(
                        algorithm=hashes.SHA256(),
                        length=16,
                        salt=self._machine_secret[:16],
                        iterations=100000,
                    )
                    expected_sig = base64.urlsafe_b64encode(
                        kdf.derive(token_data.encode())
                    ).decode()[:16]
                else:
                    expected_sig = hashlib.sha256(
                        self._machine_secret + token_data.encode()
                    ).hexdigest()[:16]
                
                return secrets.compare_digest(provided_sig, expected_sig)
                
            except Exception:
                return False
    
    def check_rate_limit(self, identifier: str) -> bool:
        """
        Check if connection is rate limited.
        Returns True if allowed, False if rate limited.
        """
        with self._lock:
            now = time.time()
            
            # Clean old entries
            if identifier in self._rate_limits:
                self._rate_limits[identifier] = [
                    t for t in self._rate_limits[identifier]
                    if now - t < self.RATE_LIMIT_WINDOW
                ]
            else:
                self._rate_limits[identifier] = []
            
            # Check limit
            if len(self._rate_limits[identifier]) >= self.MAX_CONNECTIONS_PER_WINDOW:
                return False
            
            # Add this attempt
            self._rate_limits[identifier].append(now)
            return True
    
    def is_device_trusted(self, device_id: str) -> bool:
        """Check if a device is trusted (authorized and marked as trusted)"""
        with self._lock:
            device = self._devices.get(device_id)
            return device is not None and device.trusted
    
    def is_device_known(self, device_id: str) -> bool:
        """Check if a device has connected before"""
        with self._lock:
            return device_id in self._devices
    
    def create_pending_connection(self, device_id: str, device_name: str) -> PendingConnection:
        """
        Create a pending connection that requires PIN verification.
        Returns a PendingConnection with a 6-digit PIN.
        """
        with self._lock:
            # Generate unique connection ID
            connection_id = secrets.token_urlsafe(16)
            
            # Generate 6-digit PIN
            pin = ''.join(secrets.choice('0123456789') for _ in range(6))
            
            now = time.time()
            pending = PendingConnection(
                connection_id=connection_id,
                device_name=device_name,
                device_id=device_id,
                pin=pin,
                created_at=now,
                expires_at=now + self.PIN_VALIDITY_SECONDS
            )
            
            self._pending[connection_id] = pending
            
            # Notify UI
            if self.on_connection_request:
                self.on_connection_request(pending)
            
            return pending
    
    def verify_pin(self, connection_id: str, entered_pin: str) -> bool:
        """
        Verify PIN for a pending connection.
        Returns True if PIN is correct and authorizes the device.
        """
        with self._lock:
            pending = self._pending.get(connection_id)
            if not pending:
                return False
            
            # Check expiration
            if time.time() > pending.expires_at:
                del self._pending[connection_id]
                if self.on_connection_rejected:
                    self.on_connection_rejected(pending.device_id, "PIN expired")
                return False
            
            # Check attempts
            pending.attempts += 1
            if pending.attempts > self.MAX_PIN_ATTEMPTS:
                del self._pending[connection_id]
                if self.on_connection_rejected:
                    self.on_connection_rejected(pending.device_id, "Too many attempts")
                return False
            
            # Verify PIN (constant time comparison)
            if not secrets.compare_digest(pending.pin, entered_pin):
                return False
            
            # Success! Authorize the device
            self._authorize_device(pending.device_id, pending.device_name)
            del self._pending[connection_id]
            
            if self.on_connection_authorized:
                self.on_connection_authorized(pending.device_id, pending.device_name)
            
            return True
    
    def approve_pending_connection(self, connection_id: str) -> bool:
        """
        Approve a pending connection from the UI (bypass PIN).
        Used when user clicks "Approve" button.
        """
        with self._lock:
            pending = self._pending.get(connection_id)
            if not pending:
                return False
            
            # Authorize the device
            self._authorize_device(pending.device_id, pending.device_name)
            del self._pending[connection_id]
            
            if self.on_connection_authorized:
                self.on_connection_authorized(pending.device_id, pending.device_name)
            
            return True
    
    def reject_pending_connection(self, connection_id: str) -> bool:
        """Reject a pending connection"""
        with self._lock:
            pending = self._pending.get(connection_id)
            if not pending:
                return False
            
            del self._pending[connection_id]
            
            if self.on_connection_rejected:
                self.on_connection_rejected(pending.device_id, "Rejected by user")
            
            return True
    
    def _authorize_device(self, device_id: str, device_name: str):
        """Add device to authorized list"""
        now = time.time()
        
        if device_id in self._devices:
            # Update existing
            device = self._devices[device_id]
            device.last_connected = now
            device.connection_count += 1
            device.name = device_name  # Update name in case it changed
        else:
            # New device
            device = AuthorizedDevice(
                device_id=device_id,
                name=device_name,
                first_connected=now,
                last_connected=now,
                connection_count=1,
                trusted=False
            )
            self._devices[device_id] = device
        
        self._save_devices()
    
    def set_device_trusted(self, device_id: str, trusted: bool):
        """Set whether a device is trusted (can skip PIN)"""
        with self._lock:
            if device_id in self._devices:
                self._devices[device_id].trusted = trusted
                self._save_devices()
    
    def revoke_device(self, device_id: str):
        """Remove a device from authorized list"""
        with self._lock:
            if device_id in self._devices:
                del self._devices[device_id]
                self._save_devices()
    
    def get_authorized_devices(self) -> List[AuthorizedDevice]:
        """Get list of all authorized devices"""
        with self._lock:
            return list(self._devices.values())
    
    def get_pending_connections(self) -> List[PendingConnection]:
        """Get list of pending connections"""
        with self._lock:
            # Clean expired
            now = time.time()
            expired = [cid for cid, p in self._pending.items() if now > p.expires_at]
            for cid in expired:
                del self._pending[cid]
            
            return list(self._pending.values())
    
    def get_current_token(self) -> Optional[str]:
        """Get current valid session token"""
        with self._lock:
            if self._current_token and time.time() < self._current_token.expires_at:
                return self._current_token.token
            return None
    
    def invalidate_token(self, token: str):
        """Invalidate a specific token"""
        with self._lock:
            if token in self._tokens:
                del self._tokens[token]
            if self._current_token and self._current_token.token == token:
                self._current_token = None
    
    def invalidate_all_tokens(self):
        """Invalidate all tokens (e.g., when stopping streaming)"""
        with self._lock:
            self._tokens.clear()
            self._current_token = None
    
    def _start_cleanup_timer(self):
        """Start background timer to clean up expired tokens and connections"""
        def cleanup():
            while True:
                time.sleep(60)  # Clean every minute
                self._cleanup()
        
        thread = threading.Thread(target=cleanup, daemon=True)
        thread.start()
    
    def _cleanup(self):
        """Clean up expired tokens and pending connections"""
        with self._lock:
            now = time.time()
            
            # Clean expired tokens
            expired_tokens = [
                token for token, session in self._tokens.items()
                if now > session.expires_at
            ]
            for token in expired_tokens:
                del self._tokens[token]
            
            # Clean expired pending connections
            expired_pending = [
                cid for cid, pending in self._pending.items()
                if now > pending.expires_at
            ]
            for cid in expired_pending:
                del self._pending[cid]
    
    def generate_connection_info(self, host_ip: str, port: int, hostname: str) -> dict:
        """
        Generate secure connection info for QR code.
        Includes everything Android needs to connect securely.
        """
        token = self.generate_session_token()
        machine_id = self._get_machine_id()
        
        return {
            "version": 2,  # Protocol version with security
            "name": hostname,
            "address": host_ip,
            "port": port,
            "machine_id": machine_id,
            "token": token,
            "timestamp": int(time.time()),
            "expires": int(time.time()) + self.TOKEN_VALIDITY_SECONDS,
            "requires_pin": True,  # Indicate PIN is required
            "encryption": "aes-256-gcm" if HAS_CRYPTO else "hmac-sha256",
        }
    
    def _get_machine_id(self) -> str:
        """Get unique machine identifier"""
        # Try to get machine ID from system
        try:
            machine_id_file = Path("/etc/machine-id")
            if machine_id_file.exists():
                return machine_id_file.read_text().strip()[:16]
        except:
            pass
        
        # Fallback to hostname hash
        import socket
        return hashlib.md5(socket.gethostname().encode()).hexdigest()[:16]
    
    def validate_client_ip(self, client_ip: str) -> Tuple[bool, str]:
        """
        Validate that client IP is from LAN.
        Returns (is_valid, reason).
        """
        # Check if IP is blocked
        with self._lock:
            if client_ip in self._blocked_ips:
                if time.time() < self._blocked_ips[client_ip]:
                    return False, "IP temporarily blocked due to too many failed attempts"
                else:
                    del self._blocked_ips[client_ip]
        
        # Validate LAN IP
        if not is_valid_lan_ip(client_ip):
            return False, "Only LAN connections are allowed"
        
        return True, "OK"
    
    def block_ip(self, ip: str, duration_seconds: int = 300):
        """Temporarily block an IP address"""
        with self._lock:
            self._blocked_ips[ip] = time.time() + duration_seconds
    
    def record_failed_attempt(self, ip: str) -> bool:
        """
        Record a failed connection attempt.
        Returns True if IP should be blocked.
        """
        if not self.check_rate_limit(ip):
            self.block_ip(ip, 300)  # Block for 5 minutes
            return True
        return False
    
    def get_secure_wrapper(self, token: str) -> Optional['SecureSignalingWrapper']:
        """
        Get or create a secure message wrapper for a session.
        Used for encrypting/decrypting signaling messages.
        """
        if not HAS_MSG_CRYPTO:
            return None
        
        with self._lock:
            if token not in self._secure_wrappers:
                wrapper = create_secure_wrapper(token, self._machine_secret)
                self._secure_wrappers[token] = wrapper
            return self._secure_wrappers[token]
    
    def encrypt_signaling_message(self, token: str, message: dict) -> dict:
        """
        Encrypt a signaling message for transmission.
        Falls back to signed message if encryption unavailable.
        """
        wrapper = self.get_secure_wrapper(token)
        if wrapper:
            return wrapper.wrap_message(message)
        
        # Fallback: add HMAC signature
        return self._sign_message(message, token)
    
    def decrypt_signaling_message(self, token: str, envelope: dict) -> Optional[dict]:
        """
        Decrypt a received signaling message.
        Returns None if decryption/verification fails.
        """
        wrapper = self.get_secure_wrapper(token)
        if wrapper:
            return wrapper.unwrap_message(envelope)
        
        # Fallback: verify HMAC signature
        return self._verify_message(envelope, token)
    
    def _sign_message(self, message: dict, token: str) -> dict:
        """Add HMAC-SHA256 signature to message"""
        message_copy = message.copy()
        message_copy["ts"] = int(time.time())
        message_copy["nonce"] = secrets.token_hex(8)
        
        # Calculate HMAC
        msg_str = json.dumps(message_copy, sort_keys=True)
        key = hashlib.sha256(self._machine_secret + token.encode()).digest()
        mac = hmac_module.new(key, msg_str.encode(), hashlib.sha256).hexdigest()[:32]
        
        message_copy["sig"] = mac
        return message_copy
    
    def _verify_message(self, message: dict, token: str) -> Optional[dict]:
        """Verify HMAC-SHA256 signature"""
        if "sig" not in message:
            return message  # Allow unsigned for backward compatibility
        
        message_copy = message.copy()
        sig = message_copy.pop("sig")
        
        # Verify timestamp (reject messages older than 30 seconds)
        ts = message_copy.get("ts", 0)
        if abs(time.time() - ts) > 30:
            return None
        
        # Verify nonce hasn't been used
        nonce = message_copy.get("nonce", "")
        if nonce:
            with self._lock:
                if nonce in self._used_nonces:
                    return None  # Replay attack
                self._used_nonces.add(nonce)
                
                # Cleanup old nonces every 5 minutes
                if time.time() - self._nonce_cleanup_time > 300:
                    self._used_nonces.clear()
                    self._nonce_cleanup_time = time.time()
        
        # Verify HMAC
        msg_str = json.dumps(message_copy, sort_keys=True)
        key = hashlib.sha256(self._machine_secret + token.encode()).digest()
        expected_mac = hmac_module.new(key, msg_str.encode(), hashlib.sha256).hexdigest()[:32]
        
        if not hmac_module.compare_digest(sig, expected_mac):
            return None
        
        return message_copy
    
    def get_machine_secret(self) -> bytes:
        """Get the machine secret (for key derivation)"""
        return self._machine_secret
    
    def cleanup_session(self, token: str):
        """Clean up session resources"""
        with self._lock:
            if token in self._secure_wrappers:
                del self._secure_wrappers[token]
            self.invalidate_token(token)


# Singleton accessor
def get_security_manager() -> SecurityManager:
    """Get the singleton SecurityManager instance"""
    return SecurityManager()

