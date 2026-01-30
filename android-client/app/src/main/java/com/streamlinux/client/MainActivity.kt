package com.streamlinux.client

import android.content.Intent
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.animation.*
import androidx.compose.animation.core.*
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
import androidx.compose.material.icons.outlined.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.scale
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.streamlinux.client.network.LANDiscovery
import com.streamlinux.client.network.HostInfo
import com.streamlinux.client.network.ConnectionType
import com.streamlinux.client.settings.AppSettings
import com.streamlinux.client.ui.ConnectActivity
import com.streamlinux.client.ui.QRScanActivity
import com.streamlinux.client.ui.SettingsActivity
import com.streamlinux.client.ui.StreamActivity
import com.streamlinux.client.ui.theme.StreamLinuxTheme
import kotlinx.coroutines.launch

/**
 * Main activity - Modern host discovery and connection UI
 */
class MainActivity : ComponentActivity() {
    
    private lateinit var settings: AppSettings

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        
        settings = AppSettings.getInstance(this)

        setContent {
            // Get theme from settings
            val theme by settings.theme.collectAsStateWithLifecycle()
            
            StreamLinuxTheme(forcedTheme = theme) {
                Surface(
                    modifier = Modifier.fillMaxSize(),
                    color = MaterialTheme.colorScheme.background
                ) {
                    MainScreen(
                        onHostSelected = { host -> connectToHost(host) },
                        onScanQR = { startQRScan() },
                        onManualConnect = { startManualConnect() },
                        onSettings = { openSettings() }
                    )
                }
            }
        }
    }

    private fun connectToHost(host: HostInfo) {
        val intent = Intent(this, StreamActivity::class.java).apply {
            putExtra(StreamActivity.EXTRA_HOST_ADDRESS, host.address)
            putExtra(StreamActivity.EXTRA_HOST_PORT, host.port)
            putExtra(StreamActivity.EXTRA_HOST_NAME, host.name)
        }
        startActivity(intent)
    }

    private fun startQRScan() {
        startActivity(Intent(this, QRScanActivity::class.java))
    }

    private fun startManualConnect() {
        startActivity(Intent(this, ConnectActivity::class.java))
    }

    private fun openSettings() {
        startActivity(Intent(this, SettingsActivity::class.java))
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun MainScreen(
    onHostSelected: (HostInfo) -> Unit,
    onScanQR: () -> Unit,
    onManualConnect: () -> Unit,
    onSettings: () -> Unit
) {
    var hosts by remember { mutableStateOf<List<HostInfo>>(emptyList()) }
    var isSearching by remember { mutableStateOf(false) }
    val scope = rememberCoroutineScope()

    LaunchedEffect(Unit) {
        isSearching = true
        hosts = LANDiscovery.discover()
        isSearching = false
    }

    Box(modifier = Modifier.fillMaxSize()) {
        // Background gradient
        Box(
            modifier = Modifier
                .fillMaxSize()
                .background(
                    Brush.verticalGradient(
                        colors = listOf(
                            MaterialTheme.colorScheme.background,
                            MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.3f)
                        )
                    )
                )
        )

        Column(
            modifier = Modifier
                .fillMaxSize()
                .statusBarsPadding()
        ) {
            // Top App Bar
            ModernTopBar(
                onSettings = onSettings,
                onRefresh = {
                    scope.launch {
                        isSearching = true
                        hosts = LANDiscovery.discover()
                        isSearching = false
                    }
                },
                isSearching = isSearching
            )

            Column(
                modifier = Modifier
                    .fillMaxSize()
                    .padding(horizontal = 20.dp)
            ) {
                Spacer(modifier = Modifier.height(16.dp))

                // Header
                HeaderSection()

                Spacer(modifier = Modifier.height(24.dp))

                // Quick connect options
                ConnectionOptionsRow(
                    onScanQR = onScanQR,
                    onManualConnect = onManualConnect
                )

                Spacer(modifier = Modifier.height(24.dp))

                // Hosts list
                HostsSection(
                    hosts = hosts,
                    isSearching = isSearching,
                    onHostSelected = onHostSelected,
                    onRefresh = {
                        scope.launch {
                            isSearching = true
                            hosts = LANDiscovery.discover()
                            isSearching = false
                        }
                    }
                )
            }
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun ModernTopBar(
    onSettings: () -> Unit,
    onRefresh: () -> Unit,
    isSearching: Boolean
) {
    TopAppBar(
        title = {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Box(
                    modifier = Modifier
                        .size(40.dp)
                        .clip(RoundedCornerShape(12.dp))
                        .background(
                            Brush.linearGradient(
                                colors = listOf(
                                    MaterialTheme.colorScheme.primary,
                                    MaterialTheme.colorScheme.secondary
                                )
                            )
                        ),
                    contentAlignment = Alignment.Center
                ) {
                    Icon(
                        Icons.Filled.ScreenShare,
                        contentDescription = null,
                        tint = Color.White,
                        modifier = Modifier.size(24.dp)
                    )
                }
                Spacer(modifier = Modifier.width(12.dp))
                Column {
                    Text(
                        "StreamLinux",
                        style = MaterialTheme.typography.titleLarge,
                        fontWeight = FontWeight.Bold
                    )
                    Text(
                        "Screen Streaming",
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                }
            }
        },
        actions = {
            IconButton(onClick = onRefresh, enabled = !isSearching) {
                if (isSearching) {
                    CircularProgressIndicator(
                        modifier = Modifier.size(24.dp),
                        strokeWidth = 2.dp
                    )
                } else {
                    Icon(Icons.Outlined.Refresh, contentDescription = "Refresh")
                }
            }
            IconButton(onClick = onSettings) {
                Icon(Icons.Outlined.Settings, contentDescription = "Settings")
            }
        },
        colors = TopAppBarDefaults.topAppBarColors(containerColor = Color.Transparent)
    )
}

@Composable
fun HeaderSection() {
    Column {
        Text(
            text = "Connect to your\nLinux computer",
            style = MaterialTheme.typography.headlineMedium,
            fontWeight = FontWeight.Bold
        )
        Spacer(modifier = Modifier.height(8.dp))
        Text(
            text = "Stream your screen with ultra-low latency",
            style = MaterialTheme.typography.bodyLarge,
            color = MaterialTheme.colorScheme.onSurfaceVariant
        )
    }
}

@Composable
fun ConnectionOptionsRow(onScanQR: () -> Unit, onManualConnect: () -> Unit) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.spacedBy(12.dp)
    ) {
        ConnectionOptionCard(
            modifier = Modifier.weight(1f),
            icon = Icons.Outlined.QrCodeScanner,
            title = "Scan QR",
            subtitle = "Quick connect",
            gradient = listOf(
                MaterialTheme.colorScheme.primary,
                MaterialTheme.colorScheme.primary.copy(alpha = 0.7f)
            ),
            onClick = onScanQR
        )
        ConnectionOptionCard(
            modifier = Modifier.weight(1f),
            icon = Icons.Outlined.Edit,
            title = "Manual",
            subtitle = "Enter IP address",
            gradient = listOf(
                MaterialTheme.colorScheme.secondary,
                MaterialTheme.colorScheme.secondary.copy(alpha = 0.7f)
            ),
            onClick = onManualConnect
        )
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun ConnectionOptionCard(
    modifier: Modifier = Modifier,
    icon: ImageVector,
    title: String,
    subtitle: String,
    gradient: List<Color>,
    onClick: () -> Unit
) {
    Card(
        onClick = onClick,
        modifier = modifier.height(100.dp),
        shape = RoundedCornerShape(20.dp),
        colors = CardDefaults.cardColors(containerColor = Color.Transparent)
    ) {
        Box(
            modifier = Modifier
                .fillMaxSize()
                .background(Brush.linearGradient(gradient))
                .padding(16.dp)
        ) {
            Column(
                modifier = Modifier.fillMaxSize(),
                verticalArrangement = Arrangement.SpaceBetween
            ) {
                Icon(icon, contentDescription = null, tint = Color.White, modifier = Modifier.size(28.dp))
                Column {
                    Text(title, style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.SemiBold, color = Color.White)
                    Text(subtitle, style = MaterialTheme.typography.bodySmall, color = Color.White.copy(alpha = 0.8f))
                }
            }
        }
    }
}

@Composable
fun HostsSection(
    hosts: List<HostInfo>,
    isSearching: Boolean,
    onHostSelected: (HostInfo) -> Unit,
    onRefresh: () -> Unit
) {
    Column(modifier = Modifier.fillMaxSize()) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Text("Available Hosts", style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.SemiBold)
            if (hosts.isNotEmpty()) {
                Surface(shape = RoundedCornerShape(12.dp), color = MaterialTheme.colorScheme.primaryContainer) {
                    Text(
                        "${hosts.size} found",
                        style = MaterialTheme.typography.labelMedium,
                        modifier = Modifier.padding(horizontal = 12.dp, vertical = 4.dp),
                        color = MaterialTheme.colorScheme.onPrimaryContainer
                    )
                }
            }
        }

        Spacer(modifier = Modifier.height(12.dp))

        AnimatedContent(
            targetState = when {
                isSearching -> "searching"
                hosts.isEmpty() -> "empty"
                else -> "list"
            },
            transitionSpec = { fadeIn(tween(300)) togetherWith fadeOut(tween(300)) },
            label = "hosts_content"
        ) { state ->
            when (state) {
                "searching" -> SearchingState()
                "empty" -> EmptyState(onRefresh = onRefresh)
                "list" -> HostsList(hosts = hosts, onHostSelected = onHostSelected)
            }
        }
    }
}

@Composable
fun SearchingState() {
    Box(
        modifier = Modifier.fillMaxSize().padding(vertical = 48.dp),
        contentAlignment = Alignment.Center
    ) {
        Column(horizontalAlignment = Alignment.CenterHorizontally) {
            val infiniteTransition = rememberInfiniteTransition(label = "pulse")
            val scale by infiniteTransition.animateFloat(
                initialValue = 0.8f, targetValue = 1.2f,
                animationSpec = infiniteRepeatable(tween(1000), RepeatMode.Reverse),
                label = "scale"
            )

            Box(
                modifier = Modifier
                    .size(80.dp)
                    .scale(scale)
                    .clip(CircleShape)
                    .background(MaterialTheme.colorScheme.primary.copy(alpha = 0.1f)),
                contentAlignment = Alignment.Center
            ) {
                Icon(Icons.Filled.Wifi, contentDescription = null, modifier = Modifier.size(40.dp), tint = MaterialTheme.colorScheme.primary)
            }

            Spacer(modifier = Modifier.height(24.dp))
            Text("Searching for hosts...", style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.Medium)
            Spacer(modifier = Modifier.height(4.dp))
            Text("Looking for stream-linux on your network", style = MaterialTheme.typography.bodyMedium, color = MaterialTheme.colorScheme.onSurfaceVariant, textAlign = TextAlign.Center)
        }
    }
}

@Composable
fun EmptyState(onRefresh: () -> Unit) {
    Box(modifier = Modifier.fillMaxSize().padding(vertical = 32.dp), contentAlignment = Alignment.Center) {
        Column(horizontalAlignment = Alignment.CenterHorizontally) {
            Box(
                modifier = Modifier.size(100.dp).clip(CircleShape).background(MaterialTheme.colorScheme.surfaceVariant),
                contentAlignment = Alignment.Center
            ) {
                Icon(Icons.Outlined.DesktopAccessDisabled, contentDescription = null, modifier = Modifier.size(48.dp), tint = MaterialTheme.colorScheme.onSurfaceVariant)
            }

            Spacer(modifier = Modifier.height(24.dp))
            Text("No hosts found", style = MaterialTheme.typography.titleLarge, fontWeight = FontWeight.SemiBold)
            Spacer(modifier = Modifier.height(8.dp))
            Text("Make sure stream-linux is running\non your computer", style = MaterialTheme.typography.bodyMedium, color = MaterialTheme.colorScheme.onSurfaceVariant, textAlign = TextAlign.Center)

            Spacer(modifier = Modifier.height(24.dp))
            Button(onClick = onRefresh, shape = RoundedCornerShape(12.dp)) {
                Icon(Icons.Filled.Refresh, contentDescription = null, modifier = Modifier.size(18.dp))
                Spacer(modifier = Modifier.width(8.dp))
                Text("Try Again")
            }
        }
    }
}

@Composable
fun HostsList(hosts: List<HostInfo>, onHostSelected: (HostInfo) -> Unit) {
    LazyColumn(
        verticalArrangement = Arrangement.spacedBy(12.dp),
        contentPadding = PaddingValues(bottom = 24.dp)
    ) {
        items(hosts, key = { "${it.address}:${it.port}" }) { host ->
            ModernHostCard(host = host, onClick = { onHostSelected(host) })
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun ModernHostCard(host: HostInfo, onClick: () -> Unit) {
    val scale by animateFloatAsState(targetValue = 1f, animationSpec = spring(stiffness = Spring.StiffnessHigh), label = "scale")
    
    // USB connection gets special colors - bright blue/cyan for emphasis
    val isUSB = host.connectionType == ConnectionType.USB || host.isUSB
    
    // Colors based on connection type and host status
    val connectionColor = if (isUSB) Color(0xFF00BCD4) else Color(0xFF4CAF50)  // Cyan for USB, Green for WiFi
    
    val statusColor = when {
        isUSB -> Color(0xFF00BCD4)  // Cyan - USB connection (always best)
        host.isActive && !host.hasClients -> Color(0xFF4CAF50)  // Green - Active and available
        host.isActive && host.hasClients -> Color(0xFFFF9800)   // Orange - Active but has clients
        else -> MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = 0.5f)  // Gray - Unknown/inactive
    }
    
    val statusText = when {
        isUSB -> "USB"
        host.isActive && !host.hasClients -> "Streaming"
        host.isActive && host.hasClients -> "In Use"
        else -> "Detected"
    }
    
    // USB connections get a special highlight border
    val cardColors = if (isUSB) {
        CardDefaults.cardColors(
            containerColor = Color(0xFF00BCD4).copy(alpha = 0.08f)
        )
    } else {
        CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface)
    }

    Card(
        onClick = onClick,
        modifier = Modifier.fillMaxWidth().scale(scale),
        shape = RoundedCornerShape(20.dp),
        colors = cardColors,
        elevation = CardDefaults.cardElevation(defaultElevation = if (isUSB) 4.dp else 2.dp),
        border = if (isUSB) androidx.compose.foundation.BorderStroke(2.dp, Color(0xFF00BCD4).copy(alpha = 0.5f)) else null
    ) {
        Row(
            modifier = Modifier.fillMaxWidth().padding(16.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            Box(
                modifier = Modifier
                    .size(56.dp)
                    .clip(RoundedCornerShape(16.dp))
                    .background(
                        Brush.linearGradient(
                            colors = if (isUSB) listOf(
                                Color(0xFF00BCD4).copy(alpha = 0.3f),
                                Color(0xFF00BCD4).copy(alpha = 0.15f)
                            ) else if (host.isActive) listOf(
                                statusColor.copy(alpha = 0.2f),
                                statusColor.copy(alpha = 0.1f)
                            ) else listOf(
                                MaterialTheme.colorScheme.primary.copy(alpha = 0.15f),
                                MaterialTheme.colorScheme.secondary.copy(alpha = 0.15f)
                            )
                        )
                    ),
                contentAlignment = Alignment.Center
            ) {
                Icon(
                    imageVector = when {
                        isUSB -> Icons.Filled.Usb  // USB icon
                        host.isActive -> Icons.Filled.Cast
                        else -> Icons.Filled.Computer
                    },
                    contentDescription = null,
                    modifier = Modifier.size(32.dp),
                    tint = if (isUSB) Color(0xFF00BCD4) else if (host.isActive) statusColor else MaterialTheme.colorScheme.primary
                )
            }

            Spacer(modifier = Modifier.width(16.dp))

            Column(modifier = Modifier.weight(1f)) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Text(
                        host.name,
                        style = MaterialTheme.typography.titleMedium,
                        fontWeight = FontWeight.SemiBold,
                        maxLines = 1,
                        overflow = TextOverflow.Ellipsis,
                        modifier = Modifier.weight(1f, fill = false)
                    )
                    Spacer(modifier = Modifier.width(8.dp))
                    // Status badge
                    Surface(
                        shape = RoundedCornerShape(6.dp),
                        color = statusColor.copy(alpha = 0.15f)
                    ) {
                        Row(
                            modifier = Modifier.padding(horizontal = 8.dp, vertical = 2.dp),
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            if (isUSB) {
                                // Lightning bolt for USB
                                Text(
                                    "⚡",
                                    style = MaterialTheme.typography.labelSmall,
                                    modifier = Modifier.padding(end = 2.dp)
                                )
                            }
                            Text(
                                statusText,
                                style = MaterialTheme.typography.labelSmall,
                                fontWeight = FontWeight.Medium,
                                color = statusColor
                            )
                        }
                    }
                }
                Spacer(modifier = Modifier.height(4.dp))
                Row(verticalAlignment = Alignment.CenterVertically) {
                    // Connection type icon
                    Icon(
                        imageVector = if (isUSB) Icons.Filled.Usb else Icons.Outlined.Lan,
                        contentDescription = null,
                        modifier = Modifier.size(14.dp),
                        tint = if (isUSB) Color(0xFF00BCD4) else MaterialTheme.colorScheme.onSurfaceVariant
                    )
                    Spacer(modifier = Modifier.width(4.dp))
                    Text(
                        text = if (isUSB) "USB • Ultra-low latency" else "${host.address}:${host.port}",
                        style = MaterialTheme.typography.bodySmall,
                        color = if (isUSB) Color(0xFF00BCD4) else MaterialTheme.colorScheme.onSurfaceVariant
                    )
                }
                
                // Quality indicator for USB
                if (isUSB) {
                    Spacer(modifier = Modifier.height(4.dp))
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        // Signal strength bars for USB (all full)
                        repeat(4) {
                            Box(
                                modifier = Modifier
                                    .width(4.dp)
                                    .height(8.dp + (it * 3).dp)
                                    .padding(horizontal = 1.dp)
                                    .clip(RoundedCornerShape(1.dp))
                                    .background(Color(0xFF00BCD4))
                            )
                        }
                        Spacer(modifier = Modifier.width(6.dp))
                        Text(
                            "Best quality",
                            style = MaterialTheme.typography.labelSmall,
                            color = Color(0xFF00BCD4).copy(alpha = 0.8f)
                        )
                    }
                }
            }

            // Status indicator dot with animation
            val infiniteTransition = rememberInfiniteTransition(label = "pulse")
            val alpha by infiniteTransition.animateFloat(
                initialValue = 0.4f,
                targetValue = 1f,
                animationSpec = infiniteRepeatable(
                    animation = tween(if (isUSB) 500 else 1000),  // Faster pulse for USB
                    repeatMode = RepeatMode.Reverse
                ),
                label = "alpha"
            )
            
            if (isUSB || host.isActive) {
                Box(
                    modifier = Modifier
                        .size(12.dp)
                        .clip(CircleShape)
                        .background(statusColor.copy(alpha = alpha))
                )
            } else {
                Box(
                    modifier = Modifier
                        .size(12.dp)
                        .clip(CircleShape)
                        .background(statusColor)
                )
            }
            Spacer(modifier = Modifier.width(8.dp))
            Icon(Icons.Filled.ChevronRight, contentDescription = "Connect", tint = if (isUSB) Color(0xFF00BCD4) else MaterialTheme.colorScheme.onSurfaceVariant)
        }
    }
}
