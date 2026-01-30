package com.streamlinux.client.network

import android.content.Context
import android.util.Log
import com.streamlinux.client.settings.AppSettings
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.launch
import org.json.JSONObject
import org.webrtc.*
import org.webrtc.audio.JavaAudioDeviceModule

/**
 * Connection state
 */
enum class ConnectionState {
    DISCONNECTED,
    CONNECTING,
    SIGNALING,
    CONNECTED,
    RECONNECTING,
    FAILED
}

/**
 * Stream statistics
 */
data class StreamStats(
    val rttMs: Double = 0.0,
    val bitrate: Double = 0.0,
    val fps: Double = 0.0,
    val resolution: String = "",
    val packetLoss: Double = 0.0
)

/**
 * WebRTC client for receiving stream from Linux host
 * Now with proper signaling support and settings integration
 */
class WebRTCClient(
    private val context: Context,
    private val scope: CoroutineScope
) {
    companion object {
        private const val TAG = "WebRTCClient"
    }
    
    // App settings
    private val settings = AppSettings.getInstance(context)

    // State
    private val _connectionState = MutableStateFlow(ConnectionState.DISCONNECTED)
    val connectionState: StateFlow<ConnectionState> = _connectionState

    private val _stats = MutableStateFlow(StreamStats())
    val stats: StateFlow<StreamStats> = _stats

    // Signaling client
    private var signalingClient: SignalingClient? = null

    // WebRTC components
    private var peerConnectionFactory: PeerConnectionFactory? = null
    private var peerConnection: PeerConnection? = null
    private var eglBase: EglBase? = null
    private var videoSink: VideoSink? = null

    // Control channel
    private var dataChannel: DataChannel? = null

    // Connection info
    private var currentHost: HostInfo? = null
    private var pendingIceCandidates = mutableListOf<IceCandidate>()

    // Callbacks
    var onVideoTrack: ((VideoTrack) -> Unit)? = null
    var onAudioTrack: ((AudioTrack) -> Unit)? = null
    var onControlMessage: ((String) -> Unit)? = null
    var onStatsUpdate: ((StreamStats) -> Unit)? = null

    init {
        initializeWebRTC()
    }

    private fun initializeWebRTC() {
        try {
            eglBase = EglBase.create()

            val options = PeerConnectionFactory.InitializationOptions.builder(context)
                .setEnableInternalTracer(settings.verboseLogging.value)
                .createInitializationOptions()
            PeerConnectionFactory.initialize(options)

            // Use hardware decoders based on settings
            val encoderFactory = DefaultVideoEncoderFactory(
                eglBase?.eglBaseContext,
                true,
                settings.preferH265.value // Enable H.265 if preferred
            )
            val decoderFactory = DefaultVideoDecoderFactory(eglBase?.eglBaseContext)

            // Configure audio device module for stereo output
            val audioDeviceModule = JavaAudioDeviceModule.builder(context)
                .setUseStereoOutput(true)  // Enable stereo output
                .setUseStereoInput(false)   // We're not sending audio
                .setAudioRecordErrorCallback(null)
                .setAudioTrackErrorCallback(null)
                .createAudioDeviceModule()

            peerConnectionFactory = PeerConnectionFactory.builder()
                .setVideoEncoderFactory(encoderFactory)
                .setVideoDecoderFactory(decoderFactory)
                .setAudioDeviceModule(audioDeviceModule)
                .createPeerConnectionFactory()
            
            if (settings.verboseLogging.value) {
                Log.d(TAG, "WebRTC initialized with H.265=${settings.preferH265.value}, Stereo audio=true")
            }
        } catch (e: Exception) {
            Log.e(TAG, "Failed to initialize WebRTC", e)
        }
    }

    /**
     * Connect to host via signaling server
     */
    fun connect(host: HostInfo, offer: String? = null) {
        currentHost = host
        _connectionState.value = ConnectionState.CONNECTING
        
        if (settings.verboseLogging.value) {
            Log.d(TAG, "Connecting to host: ${host.address}:${host.port}")
            Log.d(TAG, "Settings: lowLatency=${settings.lowLatencyMode.value}, buffer=${settings.getBufferSizeMs()}ms")
        }

        // If we have a direct offer (from QR code), use it directly
        if (offer != null) {
            scope.launch(Dispatchers.IO) {
                try {
                    createPeerConnection(host)
                    setRemoteDescription(offer)
                } catch (e: Exception) {
                    Log.e(TAG, "Direct connection failed", e)
                    _connectionState.value = ConnectionState.FAILED
                }
            }
            return
        }

        // Otherwise, use signaling
        _connectionState.value = ConnectionState.SIGNALING
        
        signalingClient = SignalingClient(scope).apply {
            onConnected = {
                Log.d(TAG, "Signaling connected, waiting for host...")
                // Don't create peer connection yet - wait for offer from host
            }
            
            onRegistered = { peerId ->
                Log.d(TAG, "Registered as peer: $peerId, waiting for host...")
            }
            
            onHostJoined = { peerId, name ->
                Log.d(TAG, "Host joined: $name ($peerId)")
            }
            
            onHostLeft = {
                Log.d(TAG, "Host disconnected")
                _connectionState.value = ConnectionState.SIGNALING
            }
            
            onOffer = { sdp, fromPeerId ->
                Log.d(TAG, "Received offer from host: $fromPeerId")
                scope.launch(Dispatchers.IO) {
                    createPeerConnection(currentHost!!)
                    setRemoteDescription(sdp, fromPeerId)
                }
            }
            
            onAnswer = { sdp, fromPeerId ->
                Log.d(TAG, "Received answer from: $fromPeerId")
                scope.launch(Dispatchers.IO) {
                    val sessionDescription = SessionDescription(SessionDescription.Type.ANSWER, sdp)
                    peerConnection?.setRemoteDescription(createSdpObserver("Set remote answer"), sessionDescription)
                }
            }
            
            onIceCandidate = { candidate, sdpMid, sdpMLineIndex, fromPeerId ->
                Log.d(TAG, "Received ICE candidate from: $fromPeerId")
                val iceCandidate = IceCandidate(sdpMid, sdpMLineIndex, candidate)
                
                if (peerConnection?.remoteDescription != null) {
                    peerConnection?.addIceCandidate(iceCandidate)
                } else {
                    pendingIceCandidates.add(iceCandidate)
                }
            }
            
            onError = { error ->
                Log.e(TAG, "Signaling error: $error")
                _connectionState.value = ConnectionState.FAILED
            }
            
            onDisconnected = {
                Log.d(TAG, "Signaling disconnected")
                // Only mark as failed if we were actively streaming
                // If we were just waiting for host, this is expected
                if (_connectionState.value == ConnectionState.CONNECTED) {
                    _connectionState.value = ConnectionState.DISCONNECTED
                } else if (_connectionState.value == ConnectionState.SIGNALING) {
                    // Still waiting for host - keep state as SIGNALING
                    Log.d(TAG, "Connection closed while waiting for host")
                } else {
                    _connectionState.value = ConnectionState.FAILED
                }
            }
        }
        
        signalingClient?.connect(host)
    }

    private fun createPeerConnection(host: HostInfo) {
        val iceServers = listOf(
            PeerConnection.IceServer.builder("stun:stun.l.google.com:19302").createIceServer(),
            PeerConnection.IceServer.builder("stun:stun1.l.google.com:19302").createIceServer()
        )

        val config = PeerConnection.RTCConfiguration(iceServers).apply {
            sdpSemantics = PeerConnection.SdpSemantics.UNIFIED_PLAN
            continualGatheringPolicy = PeerConnection.ContinualGatheringPolicy.GATHER_CONTINUALLY
            bundlePolicy = PeerConnection.BundlePolicy.MAXBUNDLE
            rtcpMuxPolicy = PeerConnection.RtcpMuxPolicy.REQUIRE
            iceTransportsType = PeerConnection.IceTransportsType.ALL
            
            // Apply low latency mode settings
            if (settings.lowLatencyMode.value) {
                // Prioritize low latency over quality
                iceCandidatePoolSize = 1
            } else {
                iceCandidatePoolSize = 5
            }
        }

        peerConnection = peerConnectionFactory?.createPeerConnection(config, object : PeerConnection.Observer {
            override fun onSignalingChange(state: PeerConnection.SignalingState?) {
                Log.d(TAG, "Signaling state: $state")
            }

            override fun onIceConnectionChange(state: PeerConnection.IceConnectionState?) {
                Log.d(TAG, "ICE connection state: $state")
                when (state) {
                    PeerConnection.IceConnectionState.CONNECTED,
                    PeerConnection.IceConnectionState.COMPLETED -> {
                        _connectionState.value = ConnectionState.CONNECTED
                        startStatsCollection()
                    }
                    PeerConnection.IceConnectionState.DISCONNECTED -> {
                        _connectionState.value = ConnectionState.RECONNECTING
                    }
                    PeerConnection.IceConnectionState.FAILED -> {
                        _connectionState.value = ConnectionState.FAILED
                    }
                    PeerConnection.IceConnectionState.CLOSED -> {
                        _connectionState.value = ConnectionState.DISCONNECTED
                    }
                    else -> {}
                }
            }

            override fun onIceConnectionReceivingChange(receiving: Boolean) {
                Log.d(TAG, "ICE receiving: $receiving")
            }

            override fun onIceGatheringChange(state: PeerConnection.IceGatheringState?) {
                Log.d(TAG, "ICE gathering state: $state")
            }

            override fun onIceCandidate(candidate: IceCandidate?) {
                candidate?.let {
                    Log.d(TAG, "Local ICE candidate: ${it.sdp}")
                    signalingClient?.sendIceCandidate(it.sdp, it.sdpMid ?: "0", it.sdpMLineIndex)
                }
            }

            override fun onIceCandidatesRemoved(candidates: Array<out IceCandidate>?) {
                Log.d(TAG, "ICE candidates removed")
            }

            override fun onAddStream(stream: MediaStream?) {
                Log.d(TAG, "Stream added: ${stream?.id}")
                stream?.videoTracks?.firstOrNull()?.let { track ->
                    scope.launch(Dispatchers.Main) {
                        onVideoTrack?.invoke(track)
                    }
                }
                stream?.audioTracks?.firstOrNull()?.let { track ->
                    scope.launch(Dispatchers.Main) {
                        onAudioTrack?.invoke(track)
                    }
                }
            }

            override fun onRemoveStream(stream: MediaStream?) {
                Log.d(TAG, "Stream removed")
            }

            override fun onDataChannel(channel: DataChannel?) {
                Log.d(TAG, "Data channel received: ${channel?.label()}")
                dataChannel = channel
                setupDataChannel()
            }

            override fun onRenegotiationNeeded() {
                Log.d(TAG, "Renegotiation needed")
            }

            override fun onAddTrack(receiver: RtpReceiver?, streams: Array<out MediaStream>?) {
                val track = receiver?.track()
                Log.d(TAG, "Track added: ${track?.kind()}")
                
                when (track) {
                    is VideoTrack -> {
                        scope.launch(Dispatchers.Main) {
                            onVideoTrack?.invoke(track)
                        }
                    }
                    is AudioTrack -> {
                        scope.launch(Dispatchers.Main) {
                            onAudioTrack?.invoke(track)
                        }
                    }
                }
            }
        })
        
        // Add receive-only transceivers for video and audio
        peerConnection?.addTransceiver(
            MediaStreamTrack.MediaType.MEDIA_TYPE_VIDEO,
            RtpTransceiver.RtpTransceiverInit(RtpTransceiver.RtpTransceiverDirection.RECV_ONLY)
        )
        peerConnection?.addTransceiver(
            MediaStreamTrack.MediaType.MEDIA_TYPE_AUDIO,
            RtpTransceiver.RtpTransceiverInit(RtpTransceiver.RtpTransceiverDirection.RECV_ONLY)
        )
        
        Log.d(TAG, "PeerConnection created")
    }

    private fun setupDataChannel() {
        dataChannel?.registerObserver(object : DataChannel.Observer {
            override fun onBufferedAmountChange(amount: Long) {}

            override fun onStateChange() {
                Log.d(TAG, "Data channel state: ${dataChannel?.state()}")
            }

            override fun onMessage(buffer: DataChannel.Buffer?) {
                buffer?.let {
                    val data = ByteArray(it.data.remaining())
                    it.data.get(data)
                    val message = String(data)
                    Log.d(TAG, "Control message: $message")
                    scope.launch(Dispatchers.Main) {
                        onControlMessage?.invoke(message)
                    }
                }
            }
        })
    }

    private fun setRemoteDescription(sdp: String, fromPeerId: String? = null) {
        val sessionDescription = SessionDescription(SessionDescription.Type.OFFER, sdp)
        
        peerConnection?.setRemoteDescription(object : SdpObserver {
            override fun onCreateSuccess(p0: SessionDescription?) {}
            
            override fun onSetSuccess() {
                Log.d(TAG, "Remote description set successfully")
                
                // Add pending ICE candidates
                pendingIceCandidates.forEach { candidate ->
                    peerConnection?.addIceCandidate(candidate)
                }
                pendingIceCandidates.clear()
                
                createAnswer(fromPeerId)
            }
            
            override fun onCreateFailure(error: String?) {
                Log.e(TAG, "Create failure: $error")
                _connectionState.value = ConnectionState.FAILED
            }
            
            override fun onSetFailure(error: String?) {
                Log.e(TAG, "Set remote description failure: $error")
                _connectionState.value = ConnectionState.FAILED
            }
        }, sessionDescription)
    }

    private fun createAnswer(toPeerId: String? = null) {
        val constraints = MediaConstraints().apply {
            mandatory.add(MediaConstraints.KeyValuePair("OfferToReceiveVideo", "true"))
            mandatory.add(MediaConstraints.KeyValuePair("OfferToReceiveAudio", "true"))
        }
        
        peerConnection?.createAnswer(object : SdpObserver {
            override fun onCreateSuccess(sdp: SessionDescription?) {
                Log.d(TAG, "Answer created successfully")
                sdp?.let { setLocalDescription(it, toPeerId) }
            }
            
            override fun onSetSuccess() {}
            
            override fun onCreateFailure(error: String?) {
                Log.e(TAG, "Create answer failed: $error")
                _connectionState.value = ConnectionState.FAILED
            }
            
            override fun onSetFailure(error: String?) {}
        }, constraints)
    }

    private fun setLocalDescription(sdp: SessionDescription, toPeerId: String? = null) {
        peerConnection?.setLocalDescription(object : SdpObserver {
            override fun onCreateSuccess(p0: SessionDescription?) {}
            
            override fun onSetSuccess() {
                Log.d(TAG, "Local description set successfully")
                signalingClient?.sendAnswer(sdp.description, toPeerId)
            }
            
            override fun onCreateFailure(error: String?) {}
            
            override fun onSetFailure(error: String?) {
                Log.e(TAG, "Set local description failed: $error")
            }
        }, sdp)
    }
    
    private fun createSdpObserver(operation: String): SdpObserver {
        return object : SdpObserver {
            override fun onCreateSuccess(p0: SessionDescription?) {
                Log.d(TAG, "$operation: create success")
            }
            override fun onSetSuccess() {
                Log.d(TAG, "$operation: set success")
            }
            override fun onCreateFailure(error: String?) {
                Log.e(TAG, "$operation: create failure - $error")
            }
            override fun onSetFailure(error: String?) {
                Log.e(TAG, "$operation: set failure - $error")
            }
        }
    }

    private fun startStatsCollection() {
        scope.launch(Dispatchers.IO) {
            while (_connectionState.value == ConnectionState.CONNECTED) {
                peerConnection?.getStats { report ->
                    var bitrate = 0.0
                    var fps = 0.0
                    var rtt = 0.0
                    var resolution = ""
                    var packetLoss = 0.0
                    
                    report.statsMap.values.forEach { stats ->
                        when (stats.type) {
                            "inbound-rtp" -> {
                                if (stats.members["kind"] == "video") {
                                    fps = (stats.members["framesPerSecond"] as? Double) ?: 0.0
                                    val width = stats.members["frameWidth"] as? Long ?: 0
                                    val height = stats.members["frameHeight"] as? Long ?: 0
                                    if (width > 0 && height > 0) {
                                        resolution = "${width}x${height}"
                                    }
                                }
                                val packetsLost = (stats.members["packetsLost"] as? Long) ?: 0
                                val packetsReceived = (stats.members["packetsReceived"] as? Long) ?: 1
                                if (packetsReceived > 0) {
                                    packetLoss = (packetsLost.toDouble() / packetsReceived) * 100
                                }
                            }
                            "candidate-pair" -> {
                                if (stats.members["state"] == "succeeded") {
                                    rtt = (stats.members["currentRoundTripTime"] as? Double)?.times(1000) ?: 0.0
                                    bitrate = (stats.members["availableOutgoingBitrate"] as? Double)?.div(1000000) ?: 0.0
                                }
                            }
                        }
                    }
                    
                    val newStats = StreamStats(rtt, bitrate, fps, resolution, packetLoss)
                    _stats.value = newStats
                    scope.launch(Dispatchers.Main) {
                        onStatsUpdate?.invoke(newStats)
                    }
                }
                
                kotlinx.coroutines.delay(1000)
            }
        }
    }

    fun addIceCandidate(candidate: String, sdpMid: String, sdpMLineIndex: Int) {
        val iceCandidate = IceCandidate(sdpMid, sdpMLineIndex, candidate)
        if (peerConnection?.remoteDescription != null) {
            peerConnection?.addIceCandidate(iceCandidate)
        } else {
            pendingIceCandidates.add(iceCandidate)
        }
    }

    fun sendControlMessage(type: String, payload: JSONObject? = null) {
        val message = JSONObject().apply {
            put("type", type)
            put("timestamp", System.currentTimeMillis())
            payload?.let { put("payload", it) }
        }

        val buffer = DataChannel.Buffer(
            java.nio.ByteBuffer.wrap(message.toString().toByteArray()),
            false
        )
        dataChannel?.send(buffer)
    }

    fun pause() = sendControlMessage("pause")
    fun resume() = sendControlMessage("resume")
    fun requestKeyframe() = sendControlMessage("request_keyframe")

    fun setQuality(preset: String) {
        sendControlMessage("set_quality", JSONObject().put("preset", preset))
    }

    fun setBitrate(bitrate: Int) {
        sendControlMessage("set_bitrate", JSONObject().put("bitrate", bitrate))
    }
    
    fun setAudioEnabled(enabled: Boolean) {
        sendControlMessage(if (enabled) "audio_enable" else "audio_disable")
    }

    fun getEglBase(): EglBase? = eglBase

    fun disconnect() {
        signalingClient?.disconnect()
        signalingClient = null
        
        dataChannel?.close()
        dataChannel = null

        peerConnection?.close()
        peerConnection = null

        _connectionState.value = ConnectionState.DISCONNECTED
    }

    fun release() {
        disconnect()

        peerConnectionFactory?.dispose()
        peerConnectionFactory = null

        eglBase?.release()
        eglBase = null
    }
}
