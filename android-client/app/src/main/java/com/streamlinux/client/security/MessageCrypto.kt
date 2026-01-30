package com.streamlinux.client.security

import android.util.Base64
import android.util.Log
import org.json.JSONObject
import java.nio.ByteBuffer
import java.security.MessageDigest
import java.security.SecureRandom
import javax.crypto.Cipher
import javax.crypto.Mac
import javax.crypto.spec.GCMParameterSpec
import javax.crypto.spec.SecretKeySpec

/**
 * Cryptographic utilities for securing WebSocket signaling messages.
 * 
 * Security Features:
 * - AES-256-GCM authenticated encryption
 * - HMAC-SHA256 message signing (fallback)
 * - Nonce-based replay protection
 * - Timestamp validation
 */
class MessageCrypto(
    private val sessionToken: String,
    private val machineSecret: ByteArray? = null
) {
    companion object {
        private const val TAG = "MessageCrypto"
        
        // AES-GCM parameters
        private const val GCM_NONCE_LENGTH = 12
        private const val GCM_TAG_LENGTH = 128
        
        // Security parameters
        private const val MAX_MESSAGE_AGE_SECONDS = 30
        private const val MAX_NONCE_CACHE_SIZE = 1000
    }
    
    // Derived encryption key
    private val encryptionKey: SecretKeySpec
    
    // Derived HMAC key
    private val hmacKey: SecretKeySpec
    
    // Secure random for nonces
    private val secureRandom = SecureRandom()
    
    // Used nonces to prevent replay attacks
    private val usedNonces = mutableSetOf<String>()
    
    // Message counter for additional entropy
    private var messageCounter = 0L
    
    init {
        // Derive keys from token and optional machine secret
        val keyMaterial = if (machineSecret != null) {
            MessageDigest.getInstance("SHA-512").run {
                update(machineSecret)
                update(sessionToken.toByteArray())
                digest()
            }
        } else {
            MessageDigest.getInstance("SHA-512").run {
                update(sessionToken.toByteArray())
                digest()
            }
        }
        
        // Split key material: first 32 bytes for encryption, next 32 for HMAC
        encryptionKey = SecretKeySpec(keyMaterial.copyOfRange(0, 32), "AES")
        hmacKey = SecretKeySpec(keyMaterial.copyOfRange(32, 64), "HmacSHA256")
        
        Log.d(TAG, "MessageCrypto initialized with ${if (machineSecret != null) "machine secret" else "token only"}")
    }
    
    /**
     * Encrypt a signaling message using AES-256-GCM.
     * Returns a JSON object with encrypted envelope.
     */
    fun encryptMessage(message: JSONObject): JSONObject {
        return try {
            val plaintext = message.toString().toByteArray(Charsets.UTF_8)
            
            // Generate unique nonce
            val nonce = ByteArray(GCM_NONCE_LENGTH)
            secureRandom.nextBytes(nonce)
            
            // Add timestamp and counter for additional protection
            val timestamp = System.currentTimeMillis() / 1000
            
            // Encrypt with AES-GCM
            val cipher = Cipher.getInstance("AES/GCM/NoPadding")
            val gcmSpec = GCMParameterSpec(GCM_TAG_LENGTH, nonce)
            cipher.init(Cipher.ENCRYPT_MODE, encryptionKey, gcmSpec)
            
            // Add associated data (timestamp for binding)
            val aad = ByteBuffer.allocate(8).putLong(timestamp).array()
            cipher.updateAAD(aad)
            
            val ciphertext = cipher.doFinal(plaintext)
            
            // Create envelope
            JSONObject().apply {
                put("v", 1)  // Version
                put("enc", "aes-256-gcm")
                put("ts", timestamp)
                put("nonce", Base64.encodeToString(nonce, Base64.NO_WRAP))
                put("ct", Base64.encodeToString(ciphertext, Base64.NO_WRAP))
            }
        } catch (e: Exception) {
            Log.e(TAG, "Encryption failed, falling back to signed message", e)
            signMessage(message)
        }
    }
    
    /**
     * Decrypt a signaling message from encrypted envelope.
     * Returns null if decryption or validation fails.
     */
    fun decryptMessage(envelope: JSONObject): JSONObject? {
        return try {
            val version = envelope.optInt("v", 0)
            val enc = envelope.optString("enc", "")
            
            when {
                enc == "aes-256-gcm" && version >= 1 -> decryptAesGcm(envelope)
                envelope.has("sig") -> verifySignedMessage(envelope)
                else -> {
                    Log.w(TAG, "Unknown envelope format, passing through")
                    envelope
                }
            }
        } catch (e: Exception) {
            Log.e(TAG, "Decryption failed", e)
            null
        }
    }
    
    private fun decryptAesGcm(envelope: JSONObject): JSONObject? {
        val timestamp = envelope.getLong("ts")
        val nonceB64 = envelope.getString("nonce")
        val ciphertextB64 = envelope.getString("ct")
        
        // Validate timestamp (reject old messages)
        val now = System.currentTimeMillis() / 1000
        if (kotlin.math.abs(now - timestamp) > MAX_MESSAGE_AGE_SECONDS) {
            Log.w(TAG, "Message too old: ${now - timestamp}s")
            return null
        }
        
        // Decode nonce and check for replay
        val nonce = Base64.decode(nonceB64, Base64.NO_WRAP)
        val nonceKey = nonceB64
        
        synchronized(usedNonces) {
            if (nonceKey in usedNonces) {
                Log.w(TAG, "Replay attack detected - nonce already used")
                return null
            }
            usedNonces.add(nonceKey)
            
            // Cleanup old nonces
            if (usedNonces.size > MAX_NONCE_CACHE_SIZE) {
                usedNonces.clear()
            }
        }
        
        // Decrypt
        val ciphertext = Base64.decode(ciphertextB64, Base64.NO_WRAP)
        
        val cipher = Cipher.getInstance("AES/GCM/NoPadding")
        val gcmSpec = GCMParameterSpec(GCM_TAG_LENGTH, nonce)
        cipher.init(Cipher.DECRYPT_MODE, encryptionKey, gcmSpec)
        
        // Add associated data
        val aad = ByteBuffer.allocate(8).putLong(timestamp).array()
        cipher.updateAAD(aad)
        
        val plaintext = cipher.doFinal(ciphertext)
        return JSONObject(String(plaintext, Charsets.UTF_8))
    }
    
    /**
     * Sign a message with HMAC-SHA256 (fallback when encryption not available).
     */
    fun signMessage(message: JSONObject): JSONObject {
        val messageCopy = JSONObject(message.toString())
        
        // Add timestamp and nonce
        val timestamp = System.currentTimeMillis() / 1000
        val nonce = ByteArray(8)
        secureRandom.nextBytes(nonce)
        
        messageCopy.put("ts", timestamp)
        messageCopy.put("nonce", Base64.encodeToString(nonce, Base64.NO_WRAP))
        
        // Calculate HMAC
        val mac = Mac.getInstance("HmacSHA256")
        mac.init(hmacKey)
        
        // Sort keys for consistent hashing
        val sortedMessage = sortJsonKeys(messageCopy)
        val signature = mac.doFinal(sortedMessage.toByteArray(Charsets.UTF_8))
        
        messageCopy.put("sig", Base64.encodeToString(signature, Base64.NO_WRAP).take(32))
        
        return messageCopy
    }
    
    /**
     * Verify a signed message.
     */
    fun verifySignedMessage(message: JSONObject): JSONObject? {
        if (!message.has("sig")) {
            return message  // Allow unsigned for backward compatibility
        }
        
        val signature = message.getString("sig")
        val timestamp = message.optLong("ts", 0)
        val nonce = message.optString("nonce", "")
        
        // Validate timestamp
        val now = System.currentTimeMillis() / 1000
        if (kotlin.math.abs(now - timestamp) > MAX_MESSAGE_AGE_SECONDS) {
            Log.w(TAG, "Signed message too old: ${now - timestamp}s")
            return null
        }
        
        // Check for replay
        if (nonce.isNotEmpty()) {
            synchronized(usedNonces) {
                if (nonce in usedNonces) {
                    Log.w(TAG, "Replay attack detected on signed message")
                    return null
                }
                usedNonces.add(nonce)
            }
        }
        
        // Verify HMAC
        val messageCopy = JSONObject(message.toString())
        messageCopy.remove("sig")
        
        val mac = Mac.getInstance("HmacSHA256")
        mac.init(hmacKey)
        
        val sortedMessage = sortJsonKeys(messageCopy)
        val expectedSignature = mac.doFinal(sortedMessage.toByteArray(Charsets.UTF_8))
        val expectedSigStr = Base64.encodeToString(expectedSignature, Base64.NO_WRAP).take(32)
        
        if (!MessageDigest.isEqual(signature.toByteArray(), expectedSigStr.toByteArray())) {
            Log.w(TAG, "Signature verification failed")
            return null
        }
        
        // Remove security fields and return original message
        messageCopy.remove("ts")
        messageCopy.remove("nonce")
        
        return messageCopy
    }
    
    /**
     * Sort JSON keys for consistent serialization (required for HMAC).
     */
    private fun sortJsonKeys(json: JSONObject): String {
        val keys = json.keys().asSequence().toList().sorted()
        val sb = StringBuilder("{")
        keys.forEachIndexed { index, key ->
            if (index > 0) sb.append(",")
            sb.append("\"$key\":")
            when (val value = json.get(key)) {
                is JSONObject -> sb.append(sortJsonKeys(value))
                is String -> sb.append("\"$value\"")
                else -> sb.append(value)
            }
        }
        sb.append("}")
        return sb.toString()
    }
    
    /**
     * Generate a secure nonce for use in protocols.
     */
    fun generateNonce(): String {
        val nonce = ByteArray(16)
        secureRandom.nextBytes(nonce)
        return Base64.encodeToString(nonce, Base64.NO_WRAP)
    }
    
    /**
     * Clear cached nonces (call on disconnect).
     */
    fun clearNonceCache() {
        synchronized(usedNonces) {
            usedNonces.clear()
        }
    }
}

/**
 * Extension to wrap signaling messages with encryption.
 */
class SecureSignalingWrapper(
    private val crypto: MessageCrypto,
    private val encryptOutgoing: Boolean = true
) {
    /**
     * Wrap an outgoing message.
     */
    fun wrapOutgoing(message: JSONObject): JSONObject {
        return if (encryptOutgoing) {
            crypto.encryptMessage(message)
        } else {
            crypto.signMessage(message)
        }
    }
    
    /**
     * Unwrap an incoming message.
     */
    fun unwrapIncoming(envelope: JSONObject): JSONObject? {
        return crypto.decryptMessage(envelope)
    }
}
