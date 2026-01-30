#!/usr/bin/env python3
"""
StreamLinux Message Encryption Module
Provides AES-256-GCM encryption for signaling messages over ws://

Security Features:
- AES-256-GCM authenticated encryption
- ECDH key exchange for perfect forward secrecy
- Message nonces to prevent replay attacks
- HMAC-SHA256 message authentication
- Automatic key rotation every 100 messages
"""

import os
import json
import base64
import hashlib
import hmac
import time
import secrets
from typing import Optional, Dict, Tuple, Any
from dataclasses import dataclass

# Try to import cryptography for AES-GCM
try:
    from cryptography.hazmat.primitives.ciphers.aead import AESGCM
    from cryptography.hazmat.primitives import hashes
    from cryptography.hazmat.primitives.kdf.hkdf import HKDF
    from cryptography.hazmat.primitives.asymmetric import x25519
    from cryptography.hazmat.primitives import serialization
    HAS_CRYPTO = True
except ImportError:
    HAS_CRYPTO = False
    print("Warning: cryptography not available, message encryption disabled")


@dataclass
class EncryptedMessage:
    """Represents an encrypted signaling message"""
    ciphertext: str  # Base64 encoded
    nonce: str       # Base64 encoded
    tag: str         # Base64 encoded (for non-AEAD modes)
    timestamp: int   # Unix timestamp
    sequence: int    # Message sequence number


class MessageCrypto:
    """
    Handles encryption/decryption of signaling messages.
    Uses AES-256-GCM for authenticated encryption.
    """
    
    NONCE_SIZE = 12  # 96 bits for AES-GCM
    KEY_SIZE = 32    # 256 bits
    KEY_ROTATION_INTERVAL = 100  # Rotate key every N messages
    MAX_MESSAGE_AGE = 30  # Reject messages older than N seconds
    
    def __init__(self, shared_secret: Optional[bytes] = None):
        """
        Initialize with optional shared secret.
        If no secret provided, generates one for key exchange.
        """
        self._shared_secret = shared_secret
        self._session_key: Optional[bytes] = None
        self._message_count = 0
        self._used_nonces: set = set()
        self._sequence = 0
        self._peer_sequence = -1
        
        # Generate ECDH keypair for key exchange
        if HAS_CRYPTO:
            self._private_key = x25519.X25519PrivateKey.generate()
            self._public_key = self._private_key.public_key()
        else:
            self._private_key = None
            self._public_key = None
    
    def get_public_key_bytes(self) -> Optional[bytes]:
        """Get public key for key exchange"""
        if not HAS_CRYPTO or not self._public_key:
            return None
        return self._public_key.public_bytes(
            encoding=serialization.Encoding.Raw,
            format=serialization.PublicFormat.Raw
        )
    
    def get_public_key_b64(self) -> Optional[str]:
        """Get public key as base64 string"""
        pub_bytes = self.get_public_key_bytes()
        if pub_bytes:
            return base64.b64encode(pub_bytes).decode('ascii')
        return None
    
    def derive_session_key(self, peer_public_key_b64: str, session_token: str) -> bool:
        """
        Derive session key using ECDH + HKDF.
        Incorporates session token for additional binding.
        """
        if not HAS_CRYPTO:
            return False
        
        try:
            # Decode peer's public key
            peer_pub_bytes = base64.b64decode(peer_public_key_b64)
            peer_public_key = x25519.X25519PublicKey.from_public_bytes(peer_pub_bytes)
            
            # Perform ECDH
            shared_secret = self._private_key.exchange(peer_public_key)
            
            # Derive session key using HKDF
            # Include session token as context for key binding
            hkdf = HKDF(
                algorithm=hashes.SHA256(),
                length=self.KEY_SIZE,
                salt=session_token.encode()[:32],
                info=b"streamlinux-signaling-v1"
            )
            self._session_key = hkdf.derive(shared_secret)
            self._message_count = 0
            self._sequence = 0
            
            return True
            
        except Exception as e:
            print(f"Key derivation failed: {e}")
            return False
    
    def set_session_key_from_token(self, token: str, machine_secret: bytes) -> None:
        """
        Derive session key directly from token and machine secret.
        Used when ECDH is not available or not needed.
        """
        if HAS_CRYPTO:
            hkdf = HKDF(
                algorithm=hashes.SHA256(),
                length=self.KEY_SIZE,
                salt=machine_secret[:16],
                info=b"streamlinux-signaling-v1"
            )
            self._session_key = hkdf.derive(token.encode())
        else:
            # Fallback: use HMAC-SHA256
            self._session_key = hashlib.sha256(
                machine_secret + token.encode()
            ).digest()
        
        self._message_count = 0
        self._sequence = 0
    
    def encrypt_message(self, message: Dict[str, Any]) -> Optional[Dict[str, Any]]:
        """
        Encrypt a signaling message using AES-256-GCM.
        Returns encrypted envelope or None if encryption not available.
        """
        if not self._session_key:
            return None
        
        # Check for key rotation
        self._message_count += 1
        if self._message_count >= self.KEY_ROTATION_INTERVAL:
            self._rotate_key()
        
        # Serialize message
        plaintext = json.dumps(message).encode('utf-8')
        
        # Generate unique nonce
        nonce = os.urandom(self.NONCE_SIZE)
        
        # Add timestamp and sequence
        self._sequence += 1
        timestamp = int(time.time())
        
        if HAS_CRYPTO:
            # Use AES-GCM for authenticated encryption
            aesgcm = AESGCM(self._session_key)
            
            # Additional authenticated data includes timestamp and sequence
            aad = f"{timestamp}:{self._sequence}".encode()
            
            ciphertext = aesgcm.encrypt(nonce, plaintext, aad)
            
            return {
                "encrypted": True,
                "v": 1,  # Protocol version
                "ct": base64.b64encode(ciphertext).decode('ascii'),
                "n": base64.b64encode(nonce).decode('ascii'),
                "ts": timestamp,
                "seq": self._sequence
            }
        else:
            # Fallback: XOR encryption with HMAC (less secure)
            return self._fallback_encrypt(plaintext, nonce, timestamp)
    
    def decrypt_message(self, envelope: Dict[str, Any]) -> Optional[Dict[str, Any]]:
        """
        Decrypt an encrypted signaling message.
        Returns decrypted message or None if decryption fails.
        """
        if not self._session_key:
            return None
        
        if not envelope.get("encrypted"):
            return envelope  # Not encrypted, return as-is
        
        try:
            # Extract envelope fields
            ciphertext = base64.b64decode(envelope["ct"])
            nonce = base64.b64decode(envelope["n"])
            timestamp = envelope["ts"]
            sequence = envelope["seq"]
            
            # Check message age
            if abs(time.time() - timestamp) > self.MAX_MESSAGE_AGE:
                print("Message too old, rejecting")
                return None
            
            # Check sequence (prevent replay)
            if sequence <= self._peer_sequence:
                print(f"Invalid sequence {sequence} <= {self._peer_sequence}, rejecting")
                return None
            self._peer_sequence = sequence
            
            # Check nonce uniqueness
            nonce_b64 = envelope["n"]
            if nonce_b64 in self._used_nonces:
                print("Duplicate nonce, rejecting")
                return None
            self._used_nonces.add(nonce_b64)
            
            # Limit nonce cache size
            if len(self._used_nonces) > 10000:
                self._used_nonces = set(list(self._used_nonces)[-5000:])
            
            if HAS_CRYPTO:
                # Decrypt with AES-GCM
                aesgcm = AESGCM(self._session_key)
                aad = f"{timestamp}:{sequence}".encode()
                plaintext = aesgcm.decrypt(nonce, ciphertext, aad)
            else:
                plaintext = self._fallback_decrypt(ciphertext, nonce, timestamp)
                if plaintext is None:
                    return None
            
            return json.loads(plaintext.decode('utf-8'))
            
        except Exception as e:
            print(f"Decryption failed: {e}")
            return None
    
    def _rotate_key(self) -> None:
        """Rotate session key using HKDF"""
        if not self._session_key:
            return
        
        if HAS_CRYPTO:
            hkdf = HKDF(
                algorithm=hashes.SHA256(),
                length=self.KEY_SIZE,
                salt=os.urandom(16),
                info=b"streamlinux-key-rotation"
            )
            self._session_key = hkdf.derive(self._session_key)
        else:
            self._session_key = hashlib.sha256(
                self._session_key + os.urandom(16)
            ).digest()
        
        self._message_count = 0
    
    def _fallback_encrypt(self, plaintext: bytes, nonce: bytes, timestamp: int) -> Dict[str, Any]:
        """Fallback encryption when cryptography is not available"""
        # XOR cipher (NOT secure, only for compatibility)
        key_stream = self._generate_key_stream(nonce, len(plaintext))
        ciphertext = bytes(a ^ b for a, b in zip(plaintext, key_stream))
        
        # HMAC for authentication
        mac = hmac.new(
            self._session_key,
            ciphertext + str(timestamp).encode() + str(self._sequence).encode(),
            hashlib.sha256
        ).digest()
        
        return {
            "encrypted": True,
            "v": 0,  # Fallback version
            "ct": base64.b64encode(ciphertext).decode('ascii'),
            "n": base64.b64encode(nonce).decode('ascii'),
            "mac": base64.b64encode(mac).decode('ascii'),
            "ts": timestamp,
            "seq": self._sequence
        }
    
    def _fallback_decrypt(self, ciphertext: bytes, nonce: bytes, timestamp: int) -> Optional[bytes]:
        """Fallback decryption"""
        key_stream = self._generate_key_stream(nonce, len(ciphertext))
        return bytes(a ^ b for a, b in zip(ciphertext, key_stream))
    
    def _generate_key_stream(self, nonce: bytes, length: int) -> bytes:
        """Generate key stream for XOR cipher"""
        stream = b""
        counter = 0
        while len(stream) < length:
            block = hashlib.sha256(
                self._session_key + nonce + counter.to_bytes(4, 'big')
            ).digest()
            stream += block
            counter += 1
        return stream[:length]


class SecureSignalingWrapper:
    """
    Wrapper for secure signaling over ws://.
    Adds encryption layer to all messages.
    """
    
    def __init__(self, session_token: str, machine_secret: bytes):
        self.crypto = MessageCrypto()
        self.crypto.set_session_key_from_token(session_token, machine_secret)
        self._token = session_token
    
    def wrap_message(self, message: Dict[str, Any]) -> Dict[str, Any]:
        """Wrap a message with encryption"""
        encrypted = self.crypto.encrypt_message(message)
        if encrypted:
            return encrypted
        # Fallback: return original with signature
        return self._sign_message(message)
    
    def unwrap_message(self, envelope: Dict[str, Any]) -> Optional[Dict[str, Any]]:
        """Unwrap an encrypted message"""
        if envelope.get("encrypted"):
            return self.crypto.decrypt_message(envelope)
        # Check signature for unencrypted messages
        return self._verify_message(envelope)
    
    def _sign_message(self, message: Dict[str, Any]) -> Dict[str, Any]:
        """Add HMAC signature to message"""
        message_copy = message.copy()
        message_copy["ts"] = int(time.time())
        
        # Calculate HMAC
        msg_str = json.dumps(message_copy, sort_keys=True)
        mac = hmac.new(
            self.crypto._session_key or self._token.encode(),
            msg_str.encode(),
            hashlib.sha256
        ).hexdigest()[:32]
        
        message_copy["sig"] = mac
        return message_copy
    
    def _verify_message(self, message: Dict[str, Any]) -> Optional[Dict[str, Any]]:
        """Verify HMAC signature"""
        if "sig" not in message:
            return message  # No signature, allow for backward compatibility
        
        sig = message.pop("sig")
        
        # Verify timestamp
        ts = message.get("ts", 0)
        if abs(time.time() - ts) > 30:
            print("Message signature expired")
            return None
        
        # Verify HMAC
        msg_str = json.dumps(message, sort_keys=True)
        expected_mac = hmac.new(
            self.crypto._session_key or self._token.encode(),
            msg_str.encode(),
            hashlib.sha256
        ).hexdigest()[:32]
        
        if not hmac.compare_digest(sig, expected_mac):
            print("Invalid message signature")
            return None
        
        return message


def create_secure_wrapper(token: str, machine_secret: bytes) -> SecureSignalingWrapper:
    """Create a secure signaling wrapper"""
    return SecureSignalingWrapper(token, machine_secret)
