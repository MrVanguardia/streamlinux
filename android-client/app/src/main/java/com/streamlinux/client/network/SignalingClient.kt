package com.streamlinux.client.network

import android.util.Log
import kotlinx.coroutines.*
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import okhttp3.*
import org.json.JSONObject
import java.security.cert.X509Certificate
import java.util.concurrent.TimeUnit
import javax.net.ssl.SSLContext
import javax.net.ssl.TrustManager
import javax.net.ssl.X509TrustManager

/**
 * Signaling state
 */
enum class SignalingState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    WAITING_FOR_HOST,
    STREAMING,
    ERROR
}

/**
 * Signaling message types
 */
sealed class SignalingMessage {
    data class Offer(val sdp: String, val from: String) : SignalingMessage()
    data class Answer(val sdp: String, val from: String) : SignalingMessage()
    data class IceCandidate(val candidate: String, val sdpMid: String, val sdpMLineIndex: Int, val from: String) : SignalingMessage()
    data class PeerJoined(val peerId: String, val name: String, val role: String) : SignalingMessage()
    data class PeerLeft(val peerId: String) : SignalingMessage()
    data class Registered(val peerId: String) : SignalingMessage()
    data class Error(val message: String) : SignalingMessage()
}

/**
 * WebSocket-based signaling client for WebRTC connection establishment
 */
class SignalingClient(
    private val scope: CoroutineScope
) {
    companion object {
        private const val TAG = "SignalingClient"
        private const val SIGNALING_PATH = "/ws/signaling"
        private const val CONNECT_TIMEOUT = 10L
        private const val READ_TIMEOUT = 30L
        private const val PING_INTERVAL = 15L
    }
    
    /**
     * Security: Create OkHttpClient with proper TLS configuration
     * For LAN connections, allows self-signed certs from local hosts
     * For production, use certificate pinning
     */
    private fun createSecureClient(host: HostInfo): OkHttpClient {
        val builder = OkHttpClient.Builder()
            .connectTimeout(CONNECT_TIMEOUT, TimeUnit.SECONDS)
            .readTimeout(READ_TIMEOUT, TimeUnit.SECONDS)
            .pingInterval(PING_INTERVAL, TimeUnit.SECONDS)
        
        // For local/LAN connections, allow self-signed certificates
        if (isLocalAddress(host.address)) {
            Log.d(TAG, "Local connection - allowing self-signed certificates")
            val trustAllCerts = arrayOf<TrustManager>(object : X509TrustManager {
                override fun checkClientTrusted(chain: Array<X509Certificate>, authType: String) {}
                override fun checkServerTrusted(chain: Array<X509Certificate>, authType: String) {}
                override fun getAcceptedIssuers(): Array<X509Certificate> = arrayOf()
            })
            
            val sslContext = SSLContext.getInstance("TLS")
            sslContext.init(null, trustAllCerts, java.security.SecureRandom())
            builder.sslSocketFactory(sslContext.socketFactory, trustAllCerts[0] as X509TrustManager)
            builder.hostnameVerifier { _, _ -> true }
        }
        // For non-local connections, use system CA + optional pinning
        // Add certificate pinning for known production servers here:
        // .certificatePinner(CertificatePinner.Builder()
        //     .add("your-server.com", "sha256/AAAA...")
        //     .build())
        
        return builder.build()
    }
    
    /**
     * Check if address is a local/LAN address
     */
    private fun isLocalAddress(address: String): Boolean {
        return address == "localhost" ||
               address == "127.0.0.1" ||
               address.startsWith("192.168.") ||
               address.startsWith("10.") ||
               address.startsWith("172.16.") ||
               address.startsWith("172.17.") ||
               address.startsWith("172.18.") ||
               address.startsWith("172.19.") ||
               address.startsWith("172.2") ||
               address.startsWith("172.30.") ||
               address.startsWith("172.31.") ||
               address.endsWith(".local")
    }

    private val _state = MutableStateFlow(SignalingState.DISCONNECTED)
    val state: StateFlow<SignalingState> = _state

    private val messageChannel = Channel<SignalingMessage>(Channel.BUFFERED)
    
    private var webSocket: WebSocket? = null
    private var client: OkHttpClient? = null
    
    // Our peer ID assigned by server
    var myPeerId: String? = null
        private set
    
    // Connected host peer ID (for sending messages)
    var hostPeerId: String? = null
        private set

    // Callbacks
    var onOffer: ((String, String) -> Unit)? = null  // (sdp, fromPeerId)
    var onAnswer: ((String, String) -> Unit)? = null
    var onIceCandidate: ((String, String, Int, String) -> Unit)? = null  // (candidate, sdpMid, index, fromPeerId)
    var onHostJoined: ((String, String) -> Unit)? = null  // (peerId, name)
    var onHostLeft: (() -> Unit)? = null
    var onError: ((String) -> Unit)? = null
    var onConnected: (() -> Unit)? = null
    var onDisconnected: (() -> Unit)? = null
    var onRegistered: ((String) -> Unit)? = null  // (myPeerId)

    /**
     * Connect to signaling server
     */
    fun connect(host: HostInfo) {
        if (_state.value == SignalingState.CONNECTING || _state.value == SignalingState.CONNECTED) {
            Log.w(TAG, "Already connected or connecting")
            return
        }

        _state.value = SignalingState.CONNECTING
        hostPeerId = null
        myPeerId = null
        
        scope.launch(Dispatchers.IO) {
            try {
                // Security: Use secure client with proper TLS configuration
                client = createSecureClient(host)

                // Use wss:// for TLS connections, ws:// only for local LAN
                val protocol = if (isLocalAddress(host.address)) "ws" else "wss"
                val url = "$protocol://${host.address}:${host.port}$SIGNALING_PATH"
                Log.e(TAG, "CONNECTING to signaling server: $url")
                Log.e(TAG, "HOST INFO: name=${host.name}, address=${host.address}, port=${host.port}")
                Log.e(TAG, "FULL TOKEN FROM QR: ${host.token ?: "NULL"}")

                val requestBuilder = Request.Builder()
                    .url(url)
                    .addHeader("X-Client-Type", "android")
                    .addHeader("X-Client-Name", android.os.Build.MODEL)
                
                // Security: Add Authorization header if token is available
                if (host.token != null && host.token.isNotEmpty()) {
                    requestBuilder.addHeader("Authorization", "Bearer ${host.token}")
                    Log.e(TAG, "ADDED AUTH TOKEN: ${host.token.take(8)}...")
                } else {
                    Log.w(TAG, "WARNING: No token available for authentication!")
                }
                
                val request = requestBuilder.build()

                webSocket = client?.newWebSocket(request, createWebSocketListener())

            } catch (e: Exception) {
                Log.e(TAG, "Failed to connect", e)
                _state.value = SignalingState.ERROR
                onError?.invoke(e.message ?: "Connection failed")
            }
        }
    }

    /**
     * Disconnect from signaling server
     */
    fun disconnect() {
        webSocket?.close(1000, "Client disconnecting")
        webSocket = null
        client?.dispatcher?.executorService?.shutdown()
        client = null
        _state.value = SignalingState.DISCONNECTED
        hostPeerId = null
        myPeerId = null
    }

    /**
     * Register as a viewer
     */
    fun register(name: String = android.os.Build.MODEL) {
        val message = JSONObject().apply {
            put("type", "register")
            put("role", "viewer")
            put("name", name)
        }
        sendRaw(message)
    }

    /**
     * Send SDP answer to specific peer
     */
    fun sendAnswer(sdp: String, toPeerId: String? = null) {
        val target = toPeerId ?: hostPeerId
        if (target == null) {
            Log.e(TAG, "No target peer for answer")
            return
        }
        
        val message = JSONObject().apply {
            put("type", "answer")
            put("to", target)
            put("sdp", sdp)
        }
        sendRaw(message)
    }

    /**
     * Send ICE candidate to specific peer
     */
    fun sendIceCandidate(candidate: String, sdpMid: String, sdpMLineIndex: Int, toPeerId: String? = null) {
        val target = toPeerId ?: hostPeerId
        if (target == null) {
            Log.e(TAG, "No target peer for ICE candidate")
            return
        }
        
        val message = JSONObject().apply {
            put("type", "ice-candidate")
            put("to", target)
            put("candidate", candidate)
            put("sdpMid", sdpMid)
            put("sdpMLineIndex", sdpMLineIndex)
        }
        sendRaw(message)
    }

    private fun sendRaw(message: JSONObject) {
        if (_state.value != SignalingState.CONNECTED && _state.value != SignalingState.WAITING_FOR_HOST && _state.value != SignalingState.STREAMING) {
            Log.w(TAG, "Cannot send message: not connected (state=${_state.value})")
            return
        }

        val sent = webSocket?.send(message.toString()) ?: false
        if (sent) {
            Log.d(TAG, "Sent: ${message.optString("type", "unknown")}")
        } else {
            Log.e(TAG, "Failed to send: ${message.optString("type", "unknown")}")
        }
    }

    private fun createWebSocketListener(): WebSocketListener {
        return object : WebSocketListener() {
            override fun onOpen(webSocket: WebSocket, response: Response) {
                Log.d(TAG, "WebSocket connected")
                _state.value = SignalingState.CONNECTED
                scope.launch(Dispatchers.Main) {
                    onConnected?.invoke()
                    // Register as viewer
                    register()
                }
            }

            override fun onMessage(webSocket: WebSocket, text: String) {
                Log.d(TAG, "Received: $text")
                handleMessage(text)
            }

            override fun onClosing(webSocket: WebSocket, code: Int, reason: String) {
                Log.d(TAG, "WebSocket closing: $code - $reason")
            }

            override fun onClosed(webSocket: WebSocket, code: Int, reason: String) {
                Log.d(TAG, "WebSocket closed: $code - $reason")
                _state.value = SignalingState.DISCONNECTED
                scope.launch(Dispatchers.Main) {
                    onDisconnected?.invoke()
                }
            }

            override fun onFailure(webSocket: WebSocket, t: Throwable, response: Response?) {
                Log.e(TAG, "WebSocket failure", t)
                _state.value = SignalingState.ERROR
                scope.launch(Dispatchers.Main) {
                    onError?.invoke(t.message ?: "Connection failed")
                }
            }
        }
    }

    private fun handleMessage(text: String) {
        try {
            val json = JSONObject(text)
            val type = json.optString("type", "")

            scope.launch(Dispatchers.Main) {
                when (type) {
                    "registered" -> {
                        myPeerId = json.optString("peerId", "")
                        Log.d(TAG, "Registered as peer: $myPeerId")
                        _state.value = SignalingState.WAITING_FOR_HOST
                        onRegistered?.invoke(myPeerId!!)
                    }
                    
                    "peer-joined" -> {
                        val peerId = json.optString("peerId", "")
                        val name = json.optString("name", "Unknown")
                        val role = json.optString("role", "")
                        
                        Log.d(TAG, "Peer joined: $peerId ($name) - role: $role")
                        
                        if (role == "host") {
                            hostPeerId = peerId
                            _state.value = SignalingState.STREAMING
                            onHostJoined?.invoke(peerId, name)
                        }
                    }
                    
                    "peer-left" -> {
                        val peerId = json.optString("peerId", "")
                        Log.d(TAG, "Peer left: $peerId")
                        
                        if (peerId == hostPeerId) {
                            hostPeerId = null
                            _state.value = SignalingState.WAITING_FOR_HOST
                            onHostLeft?.invoke()
                        }
                    }
                    
                    "offer" -> {
                        val sdp = json.optString("sdp", "")
                        val from = json.optString("from", "")
                        
                        if (sdp.isNotEmpty()) {
                            Log.d(TAG, "Received offer from: $from")
                            hostPeerId = from
                            _state.value = SignalingState.STREAMING
                            onOffer?.invoke(sdp, from)
                        }
                    }
                    
                    "answer" -> {
                        val sdp = json.optString("sdp", "")
                        val from = json.optString("from", "")
                        
                        if (sdp.isNotEmpty()) {
                            onAnswer?.invoke(sdp, from)
                        }
                    }
                    
                    "ice-candidate" -> {
                        val candidate = json.optString("candidate", "")
                        val sdpMid = json.optString("sdpMid", "0")
                        val sdpMLineIndex = json.optInt("sdpMLineIndex", 0)
                        val from = json.optString("from", "")
                        
                        if (candidate.isNotEmpty()) {
                            onIceCandidate?.invoke(candidate, sdpMid, sdpMLineIndex, from)
                        }
                    }
                    
                    "error" -> {
                        val payload = json.optJSONObject("payload")
                        val message = payload?.optString("error", "Unknown error") ?: "Unknown error"
                        onError?.invoke(message)
                    }
                    
                    else -> {
                        Log.w(TAG, "Unknown message type: $type")
                    }
                }
            }
        } catch (e: Exception) {
            Log.e(TAG, "Failed to parse message", e)
        }
    }
}
