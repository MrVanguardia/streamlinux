package com.streamlinux.client.ui

import android.content.Context
import android.media.AudioManager
import android.os.Bundle
import android.view.WindowManager
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.animation.*
import androidx.compose.animation.core.*
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.viewinterop.AndroidView
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.lifecycle.lifecycleScope
import com.streamlinux.client.R
import com.streamlinux.client.network.ConnectionState
import com.streamlinux.client.network.HostInfo
import com.streamlinux.client.network.WebRTCClient
import com.streamlinux.client.settings.AppSettings
import com.streamlinux.client.ui.theme.StreamLinuxTheme
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import org.webrtc.RendererCommon
import org.webrtc.SurfaceViewRenderer

/**
 * Full-screen streaming activity with modern overlay controls
 */
class StreamActivity : ComponentActivity() {

    companion object {
        const val EXTRA_HOST_ADDRESS = "host_address"
        const val EXTRA_HOST_PORT = "host_port"
        const val EXTRA_HOST_NAME = "host_name"
        const val EXTRA_HOST_TOKEN = "host_token"
        const val EXTRA_SDP_OFFER = "sdp_offer"
    }

    private lateinit var webRTCClient: WebRTCClient
    private lateinit var settings: AppSettings
    private lateinit var audioManager: AudioManager
    private var surfaceViewRenderer: SurfaceViewRenderer? = null
    private var previousAudioMode: Int = AudioManager.MODE_NORMAL
    private var previousSpeakerphone: Boolean = false

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // Initialize settings
        settings = AppSettings.getInstance(this)
        
        // Initialize audio manager and auto-configure optimal audio output
        audioManager = getSystemService(Context.AUDIO_SERVICE) as AudioManager
        setupOptimalAudioOutput()

        // Enable edge-to-edge
        WindowCompat.setDecorFitsSystemWindows(window, false)

        // Apply settings: Keep screen on
        if (settings.keepScreenOn.value) {
            window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        }

        // Get connection info from intent
        val address = intent.getStringExtra(EXTRA_HOST_ADDRESS) ?: run {
            Toast.makeText(this, getString(R.string.error_invalid_host), Toast.LENGTH_SHORT).show()
            finish()
            return
        }
        val port = intent.getIntExtra(EXTRA_HOST_PORT, 0)
        val name = intent.getStringExtra(EXTRA_HOST_NAME) ?: "Unknown"
        val token = intent.getStringExtra(EXTRA_HOST_TOKEN)
        val sdpOffer = intent.getStringExtra(EXTRA_SDP_OFFER)

        val hostInfo = HostInfo(name, address, port, token = token)
        
        // Save last connection
        settings.lastHostIp = address
        settings.lastHostPort = port

        // Initialize WebRTC
        webRTCClient = WebRTCClient(this, lifecycleScope)

        setContent {
            StreamLinuxTheme {
                StreamScreen(
                    hostInfo = hostInfo,
                    sdpOffer = sdpOffer,
                    webRTCClient = webRTCClient,
                    settings = settings,
                    onSurfaceViewCreated = { renderer -> surfaceViewRenderer = renderer },
                    onDisconnect = { finish() },
                    onImmersiveMode = { immersive -> setImmersiveMode(immersive) }
                )
            }
        }
    }

    private fun setImmersiveMode(immersive: Boolean) {
        // Only apply if immersive mode is enabled in settings
        if (!settings.immersiveMode.value) return
        
        val windowInsetsController = WindowCompat.getInsetsController(window, window.decorView)
        windowInsetsController.systemBarsBehavior = 
            WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
        
        if (immersive) {
            windowInsetsController.hide(WindowInsetsCompat.Type.systemBars())
        } else {
            windowInsetsController.show(WindowInsetsCompat.Type.systemBars())
        }
    }
    
    /**
     * Automatically configure optimal audio output:
     * - If headphones/bluetooth connected -> use them
     * - If no headphones -> use speaker (loudspeaker)
     */
    private fun setupOptimalAudioOutput() {
        try {
            // Save previous state to restore later
            previousAudioMode = audioManager.mode
            previousSpeakerphone = audioManager.isSpeakerphoneOn
            
            // Check if headphones or bluetooth are connected
            val hasWiredHeadset = audioManager.isWiredHeadsetOn
            val hasBluetoothA2dp = audioManager.isBluetoothA2dpOn
            val hasBluetoothSco = audioManager.isBluetoothScoOn
            
            val hasExternalAudio = hasWiredHeadset || hasBluetoothA2dp || hasBluetoothSco
            
            if (hasExternalAudio) {
                // External audio device detected - use it (don't force speaker)
                audioManager.mode = AudioManager.MODE_NORMAL
                audioManager.isSpeakerphoneOn = false
                android.util.Log.d("StreamActivity", "External audio detected: wired=$hasWiredHeadset, bt_a2dp=$hasBluetoothA2dp, bt_sco=$hasBluetoothSco")
            } else {
                // No external audio - force speaker for loud playback
                audioManager.mode = AudioManager.MODE_IN_COMMUNICATION
                audioManager.isSpeakerphoneOn = true
                android.util.Log.d("StreamActivity", "No external audio - using speaker")
            }
            
            // Ensure media volume is adequate
            val maxVolume = audioManager.getStreamMaxVolume(AudioManager.STREAM_MUSIC)
            val currentVolume = audioManager.getStreamVolume(AudioManager.STREAM_MUSIC)
            
            // If volume is very low and using speaker, set to 70%
            if (!hasExternalAudio && currentVolume < maxVolume * 0.3) {
                audioManager.setStreamVolume(
                    AudioManager.STREAM_MUSIC,
                    (maxVolume * 0.7).toInt(),
                    0
                )
                android.util.Log.d("StreamActivity", "Volume boosted to 70%")
            }
            
            android.util.Log.d("StreamActivity", "Audio configured: mode=${audioManager.mode}, speaker=${audioManager.isSpeakerphoneOn}")
        } catch (e: Exception) {
            android.util.Log.e("StreamActivity", "Failed to setup audio output", e)
        }
    }
    
    /**
     * Restore previous audio configuration
     */
    private fun restoreAudioSettings() {
        try {
            audioManager.mode = previousAudioMode
            audioManager.isSpeakerphoneOn = previousSpeakerphone
            android.util.Log.d("StreamActivity", "Audio settings restored")
        } catch (e: Exception) {
            android.util.Log.e("StreamActivity", "Failed to restore audio settings", e)
        }
    }

    override fun onResume() {
        super.onResume()
        webRTCClient.resume()
        // Auto-detect and configure optimal audio output
        setupOptimalAudioOutput()
    }

    override fun onPause() {
        super.onPause()
        webRTCClient.pause()
    }

    override fun onDestroy() {
        super.onDestroy()
        // Restore audio settings before destroying
        restoreAudioSettings()
        surfaceViewRenderer?.release()
        webRTCClient.release()
    }
}

@Composable
fun StreamScreen(
    hostInfo: HostInfo,
    sdpOffer: String?,
    webRTCClient: WebRTCClient,
    settings: AppSettings,
    onSurfaceViewCreated: (SurfaceViewRenderer) -> Unit,
    onDisconnect: () -> Unit,
    onImmersiveMode: (Boolean) -> Unit
) {
    val context = LocalContext.current
    var controlsVisible by remember { mutableStateOf(true) }
    var showDisconnectDialog by remember { mutableStateOf(false) }
    var audioEnabled by remember { mutableStateOf(true) }
    
    // Get settings as state
    val showStats by settings.showStats.collectAsStateWithLifecycle()
    val autoReconnect by settings.autoReconnect.collectAsStateWithLifecycle()
    val immersiveMode by settings.immersiveMode.collectAsStateWithLifecycle()
    
    // Connection state
    var connectionState by remember { mutableStateOf(ConnectionState.CONNECTING) }
    
    // Stream stats - collect from WebRTC client
    val webrtcStats by webRTCClient.stats.collectAsStateWithLifecycle()
    val streamStats = remember(webrtcStats) {
        StreamStats(
            resolution = webrtcStats.resolution.ifEmpty { "Connecting..." },
            fps = if (webrtcStats.fps > 0) "${webrtcStats.fps.toInt()} fps" else "--",
            bitrate = if (webrtcStats.bitrate > 0) String.format("%.1f Mbps", webrtcStats.bitrate) else "--",
            latency = if (webrtcStats.rttMs > 0) "${webrtcStats.rttMs.toInt()} ms" else "--",
            packetsLost = if (webrtcStats.packetLoss > 0) String.format("%.1f%%", webrtcStats.packetLoss) else "0%"
        )
    }
    
    // Audio track reference for muting
    var currentAudioTrack by remember { mutableStateOf<org.webrtc.AudioTrack?>(null) }
    
    // Reference to the surface renderer
    var surfaceRenderer by remember { mutableStateOf<SurfaceViewRenderer?>(null) }
    
    // Auto-hide controls after delay
    LaunchedEffect(controlsVisible) {
        if (controlsVisible && immersiveMode) {
            delay(5000)
            controlsVisible = false
            onImmersiveMode(true)
        }
    }
    
    // Observe connection state
    LaunchedEffect(webRTCClient) {
        var wasConnected = false  // Track if we were ever successfully connected
        
        webRTCClient.connectionState.collect { state ->
            connectionState = state
            when (state) {
                ConnectionState.CONNECTED -> {
                    // Connected successfully - mark as having been connected
                    wasConnected = true
                    android.util.Log.d("StreamScreen", "Successfully connected!")
                }
                ConnectionState.FAILED -> {
                    // Only show toast and reconnect if we never successfully connected
                    if (!wasConnected) {
                        Toast.makeText(context, "No se pudo conectar al host. ¿El streaming está iniciado en Linux?", Toast.LENGTH_LONG).show()
                        // Auto-reconnect if enabled
                        if (autoReconnect) {
                            delay(3000)
                            webRTCClient.connect(hostInfo, sdpOffer)
                        }
                    } else {
                        // We were connected before - connection dropped
                        android.util.Log.d("StreamScreen", "Connection failed after being connected")
                    }
                }
                ConnectionState.DISCONNECTED -> {
                    // Only auto-reconnect if we were previously connected and auto-reconnect is enabled
                    if (wasConnected && autoReconnect) {
                        android.util.Log.d("StreamScreen", "Disconnected - waiting before reconnect...")
                        delay(5000)  // Wait longer before reconnecting
                        wasConnected = false  // Reset for new connection attempt
                        webRTCClient.connect(hostInfo, sdpOffer)
                    }
                }
                ConnectionState.SIGNALING -> {
                    // Waiting for host - this is normal
                    Toast.makeText(context, "Conectado al servidor. Esperando stream del host...", Toast.LENGTH_SHORT).show()
                }
                else -> {}
            }
        }
    }
    
    // Connect to host
    LaunchedEffect(Unit) {
        webRTCClient.onVideoTrack = { videoTrack ->
            // Video track received - attach to renderer
            android.util.Log.d("StreamScreen", "Video track received! Attaching to renderer...")
            surfaceRenderer?.let { renderer ->
                videoTrack.addSink(renderer)
                android.util.Log.d("StreamScreen", "Video track attached to renderer!")
            } ?: android.util.Log.e("StreamScreen", "Renderer is null, cannot attach video track!")
        }
        webRTCClient.onAudioTrack = { audioTrack ->
            android.util.Log.d("StreamScreen", "Audio track received! Enabling stereo playback...")
            // Store reference for muting control
            currentAudioTrack = audioTrack
            // Enable audio track and set volume to max for stereo
            audioTrack.setEnabled(audioEnabled)
            audioTrack.setVolume(1.0) // Full volume on both channels (stereo)
            android.util.Log.d("StreamScreen", "Audio track enabled: ${audioTrack.enabled()}, volume set to 1.0")
        }
        webRTCClient.onStatsUpdate = { stats ->
            // Stats are automatically collected via StateFlow
            android.util.Log.d("StreamScreen", "Stats update: ${stats.resolution} @ ${stats.fps}fps")
        }
        webRTCClient.connect(hostInfo, sdpOffer)
    }
    
    // Handle audio toggle
    LaunchedEffect(audioEnabled) {
        currentAudioTrack?.let { track ->
            track.setEnabled(audioEnabled)
            android.util.Log.d("StreamScreen", "Audio ${if (audioEnabled) "enabled" else "muted"}")
        }
    }

    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(Color.Black)
            .pointerInput(Unit) {
                detectTapGestures(
                    onTap = {
                        controlsVisible = !controlsVisible
                        onImmersiveMode(!controlsVisible)
                    }
                )
            }
    ) {
        // Video Surface
        AndroidView(
            factory = { ctx ->
                SurfaceViewRenderer(ctx).apply {
                    init(webRTCClient.getEglBase()?.eglBaseContext, null)
                    setScalingType(RendererCommon.ScalingType.SCALE_ASPECT_FIT)
                    setEnableHardwareScaler(true)
                    surfaceRenderer = this  // Save reference for video track
                    onSurfaceViewCreated(this)
                    android.util.Log.d("StreamScreen", "SurfaceViewRenderer created and initialized")
                }
            },
            modifier = Modifier.fillMaxSize()
        )
        
        // Connection loading overlay
        AnimatedVisibility(
            visible = connectionState == ConnectionState.CONNECTING,
            enter = fadeIn(),
            exit = fadeOut()
        ) {
            ConnectionLoadingOverlay(hostName = hostInfo.name)
        }
        
        // Controls Overlay
        AnimatedVisibility(
            visible = controlsVisible && connectionState == ConnectionState.CONNECTED,
            enter = fadeIn() + slideInVertically(),
            exit = fadeOut() + slideOutVertically()
        ) {
            StreamControlsOverlay(
                hostInfo = hostInfo,
                streamStats = streamStats,
                showStats = showStats,
                audioEnabled = audioEnabled,
                onToggleStats = { settings.setShowStats(!showStats) },
                onToggleAudio = { 
                    audioEnabled = !audioEnabled
                    // webRTCClient.setAudioEnabled(audioEnabled)
                },
                onDisconnect = { showDisconnectDialog = true }
            )
        }
        
        // Disconnect confirmation dialog
        if (showDisconnectDialog) {
            DisconnectDialog(
                onConfirm = {
                    showDisconnectDialog = false
                    onDisconnect()
                },
                onDismiss = { showDisconnectDialog = false }
            )
        }
    }
}

@Composable
fun ConnectionLoadingOverlay(hostName: String) {
    val infiniteTransition = rememberInfiniteTransition(label = "loading")
    val alpha by infiniteTransition.animateFloat(
        initialValue = 0.3f,
        targetValue = 1f,
        animationSpec = infiniteRepeatable(
            animation = tween(1000),
            repeatMode = RepeatMode.Reverse
        ),
        label = "pulse"
    )
    
    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(Color.Black.copy(alpha = 0.85f)),
        contentAlignment = Alignment.Center
    ) {
        Column(
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.spacedBy(24.dp)
        ) {
            // Animated connection icon
            Box(
                modifier = Modifier
                    .size(100.dp)
                    .clip(CircleShape)
                    .background(
                        Brush.linearGradient(
                            colors = listOf(
                                Color(0xFF6C63FF).copy(alpha = alpha),
                                Color(0xFF00D9FF).copy(alpha = alpha)
                            )
                        )
                    ),
                contentAlignment = Alignment.Center
            ) {
                Icon(
                    Icons.Default.Cast,
                    contentDescription = null,
                    tint = Color.White,
                    modifier = Modifier.size(48.dp)
                )
            }
            
            Text(
                text = stringResource(R.string.connecting),
                style = MaterialTheme.typography.headlineSmall,
                fontWeight = FontWeight.Bold,
                color = Color.White
            )
            
            Text(
                text = hostName,
                style = MaterialTheme.typography.bodyLarge,
                color = Color.White.copy(alpha = 0.7f)
            )
            
            CircularProgressIndicator(
                modifier = Modifier.size(32.dp),
                color = Color(0xFF6C63FF),
                strokeWidth = 3.dp
            )
        }
    }
}

@Composable
fun StreamControlsOverlay(
    hostInfo: HostInfo,
    streamStats: StreamStats,
    showStats: Boolean,
    audioEnabled: Boolean,
    onToggleStats: () -> Unit,
    onToggleAudio: () -> Unit,
    onDisconnect: () -> Unit
) {
    Box(modifier = Modifier.fillMaxSize()) {
        // Top bar with gradient
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .align(Alignment.TopCenter)
                .background(
                    Brush.verticalGradient(
                        colors = listOf(
                            Color.Black.copy(alpha = 0.7f),
                            Color.Transparent
                        )
                    )
                )
                .statusBarsPadding()
                .padding(16.dp)
        ) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                // Host info
                Row(
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.spacedBy(12.dp)
                ) {
                    Box(
                        modifier = Modifier
                            .size(40.dp)
                            .clip(CircleShape)
                            .background(
                                Brush.linearGradient(
                                    colors = listOf(Color(0xFF6C63FF), Color(0xFF00D9FF))
                                )
                            ),
                        contentAlignment = Alignment.Center
                    ) {
                        Icon(
                            Icons.Default.Computer,
                            contentDescription = null,
                            tint = Color.White,
                            modifier = Modifier.size(20.dp)
                        )
                    }
                    
                    Column {
                        Text(
                            text = hostInfo.name,
                            style = MaterialTheme.typography.titleMedium,
                            fontWeight = FontWeight.Bold,
                            color = Color.White
                        )
                        Row(
                            verticalAlignment = Alignment.CenterVertically,
                            horizontalArrangement = Arrangement.spacedBy(4.dp)
                        ) {
                            Box(
                                modifier = Modifier
                                    .size(8.dp)
                                    .clip(CircleShape)
                                    .background(Color(0xFF4ADE80))
                            )
                            Text(
                                text = stringResource(R.string.connected),
                                style = MaterialTheme.typography.bodySmall,
                                color = Color(0xFF4ADE80)
                            )
                        }
                    }
                }
                
                // Settings button
                ControlButton(
                    icon = Icons.Default.Settings,
                    onClick = onToggleStats
                )
            }
        }
        
        // Stats panel (when visible)
        AnimatedVisibility(
            visible = showStats,
            enter = fadeIn() + expandVertically(),
            exit = fadeOut() + shrinkVertically(),
            modifier = Modifier
                .align(Alignment.TopEnd)
                .padding(top = 100.dp, end = 16.dp)
        ) {
            StatsPanel(stats = streamStats)
        }
        
        // Bottom controls with gradient
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .align(Alignment.BottomCenter)
                .background(
                    Brush.verticalGradient(
                        colors = listOf(
                            Color.Transparent,
                            Color.Black.copy(alpha = 0.7f)
                        )
                    )
                )
                .navigationBarsPadding()
                .padding(24.dp)
        ) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceEvenly,
                verticalAlignment = Alignment.CenterVertically
            ) {
                // Audio toggle
                ControlButton(
                    icon = if (audioEnabled) Icons.Default.VolumeUp else Icons.Default.VolumeOff,
                    label = if (audioEnabled) 
                        stringResource(R.string.audio_on) 
                    else 
                        stringResource(R.string.audio_off),
                    isActive = audioEnabled,
                    onClick = onToggleAudio
                )
                
                // Disconnect button
                Box(
                    modifier = Modifier
                        .size(64.dp)
                        .clip(CircleShape)
                        .background(Color(0xFFEF4444))
                        .clickable(
                            indication = null,
                            interactionSource = remember { MutableInteractionSource() }
                        ) { onDisconnect() },
                    contentAlignment = Alignment.Center
                ) {
                    Icon(
                        Icons.Default.CallEnd,
                        contentDescription = stringResource(R.string.disconnect),
                        tint = Color.White,
                        modifier = Modifier.size(28.dp)
                    )
                }
                
                // Fullscreen toggle (placeholder)
                ControlButton(
                    icon = Icons.Default.Fullscreen,
                    label = stringResource(R.string.fullscreen),
                    onClick = { /* Toggle aspect ratio */ }
                )
            }
        }
    }
}

@Composable
fun ControlButton(
    icon: ImageVector,
    label: String? = null,
    isActive: Boolean = true,
    onClick: () -> Unit
) {
    Column(
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.spacedBy(4.dp)
    ) {
        Box(
            modifier = Modifier
                .size(48.dp)
                .clip(CircleShape)
                .background(
                    if (isActive) Color.White.copy(alpha = 0.2f) 
                    else Color.White.copy(alpha = 0.1f)
                )
                .clickable(
                    indication = null,
                    interactionSource = remember { MutableInteractionSource() }
                ) { onClick() },
            contentAlignment = Alignment.Center
        ) {
            Icon(
                icon,
                contentDescription = label,
                tint = if (isActive) Color.White else Color.White.copy(alpha = 0.5f),
                modifier = Modifier.size(24.dp)
            )
        }
        label?.let {
            Text(
                text = it,
                style = MaterialTheme.typography.labelSmall,
                color = Color.White.copy(alpha = 0.7f)
            )
        }
    }
}

@Composable
fun StatsPanel(stats: StreamStats) {
    Card(
        modifier = Modifier.width(200.dp),
        shape = RoundedCornerShape(16.dp),
        colors = CardDefaults.cardColors(
            containerColor = Color.Black.copy(alpha = 0.8f)
        )
    ) {
        Column(
            modifier = Modifier.padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(12.dp)
        ) {
            Text(
                text = stringResource(R.string.stream_stats),
                style = MaterialTheme.typography.titleSmall,
                fontWeight = FontWeight.Bold,
                color = Color.White
            )
            
            StatItem(
                label = stringResource(R.string.resolution),
                value = stats.resolution
            )
            StatItem(
                label = stringResource(R.string.fps),
                value = stats.fps
            )
            StatItem(
                label = stringResource(R.string.bitrate),
                value = stats.bitrate
            )
            StatItem(
                label = stringResource(R.string.latency),
                value = stats.latency
            )
            StatItem(
                label = stringResource(R.string.packets_lost),
                value = stats.packetsLost
            )
        }
    }
}

@Composable
fun StatItem(label: String, value: String) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.SpaceBetween
    ) {
        Text(
            text = label,
            style = MaterialTheme.typography.bodySmall,
            color = Color.White.copy(alpha = 0.6f)
        )
        Text(
            text = value,
            style = MaterialTheme.typography.bodySmall,
            fontWeight = FontWeight.Medium,
            color = Color.White
        )
    }
}

@Composable
fun DisconnectDialog(
    onConfirm: () -> Unit,
    onDismiss: () -> Unit
) {
    AlertDialog(
        onDismissRequest = onDismiss,
        icon = {
            Icon(
                Icons.Default.Warning,
                contentDescription = null,
                tint = Color(0xFFEF4444)
            )
        },
        title = {
            Text(
                text = stringResource(R.string.disconnect_title),
                fontWeight = FontWeight.Bold
            )
        },
        text = {
            Text(text = stringResource(R.string.disconnect_message))
        },
        confirmButton = {
            Button(
                onClick = onConfirm,
                colors = ButtonDefaults.buttonColors(
                    containerColor = Color(0xFFEF4444)
                )
            ) {
                Text(stringResource(R.string.disconnect))
            }
        },
        dismissButton = {
            TextButton(onClick = onDismiss) {
                Text(stringResource(R.string.cancel))
            }
        },
        containerColor = MaterialTheme.colorScheme.surface,
        shape = RoundedCornerShape(20.dp)
    )
}

// Data class for stream statistics display
data class StreamStats(
    val resolution: String = "Connecting...",
    val fps: String = "--",
    val bitrate: String = "--",
    val latency: String = "--",
    val packetsLost: String = "0%"
)
