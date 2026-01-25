package com.streamlinux.client.ui

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.ArrowBack
import androidx.compose.material.icons.outlined.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.streamlinux.client.settings.AppSettings
import com.streamlinux.client.ui.theme.StreamLinuxTheme

/**
 * Settings activity with fully functional settings
 */
class SettingsActivity : ComponentActivity() {

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
                    SettingsScreen(onBack = { finish() })
                }
            }
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun SettingsScreen(onBack: () -> Unit) {
    val context = LocalContext.current
    val settings = remember { AppSettings.getInstance(context) }
    
    // Collect settings as state
    val preferH265 by settings.preferH265.collectAsStateWithLifecycle()
    val autoReconnect by settings.autoReconnect.collectAsStateWithLifecycle()
    val showStats by settings.showStats.collectAsStateWithLifecycle()
    val lowLatencyMode by settings.lowLatencyMode.collectAsStateWithLifecycle()
    val maxBitrate by settings.maxBitrate.collectAsStateWithLifecycle()
    val bufferSize by settings.bufferSize.collectAsStateWithLifecycle()
    val theme by settings.theme.collectAsStateWithLifecycle()
    val keepScreenOn by settings.keepScreenOn.collectAsStateWithLifecycle()
    val immersiveMode by settings.immersiveMode.collectAsStateWithLifecycle()
    val verboseLogging by settings.verboseLogging.collectAsStateWithLifecycle()
    val resolution by settings.resolution.collectAsStateWithLifecycle()
    
    // Snackbar for confirmations
    val snackbarHostState = remember { SnackbarHostState() }
    var showResetDialog by remember { mutableStateOf(false) }

    Scaffold(
        topBar = {
            TopAppBar(
                title = {
                    Text("Ajustes", fontWeight = FontWeight.SemiBold)
                },
                navigationIcon = {
                    IconButton(onClick = onBack) {
                        Icon(Icons.Default.ArrowBack, contentDescription = "Volver")
                    }
                },
                colors = TopAppBarDefaults.topAppBarColors(
                    containerColor = Color.Transparent
                )
            )
        },
        snackbarHost = { SnackbarHost(snackbarHostState) }
    ) { paddingValues ->
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(paddingValues)
                .verticalScroll(rememberScrollState())
                .padding(horizontal = 20.dp)
        ) {
            // Video Settings Section
            SettingsSection(title = "Video", icon = Icons.Outlined.VideoSettings) {
                SettingsSwitch(
                    title = "Preferir H.265 (HEVC)",
                    subtitle = "Mejor calidad con el mismo bitrate",
                    checked = preferH265,
                    onCheckedChange = { settings.setPreferH265(it) }
                )

                Spacer(modifier = Modifier.height(16.dp))

                SettingsLabel("Resolución")
                Spacer(modifier = Modifier.height(8.dp))
                ModernSegmentedButtons(
                    options = AppSettings.RESOLUTION_OPTIONS,
                    selected = resolution,
                    onSelect = { settings.setResolution(it) }
                )

                Spacer(modifier = Modifier.height(16.dp))

                SettingsLabel("Bitrate Máximo")
                Spacer(modifier = Modifier.height(8.dp))
                ModernSegmentedButtons(
                    options = AppSettings.BITRATE_OPTIONS.keys.toList(),
                    selected = maxBitrate,
                    onSelect = { settings.setMaxBitrate(it) }
                )
            }

            Spacer(modifier = Modifier.height(16.dp))

            // Network Settings Section
            SettingsSection(title = "Red", icon = Icons.Outlined.Wifi) {
                SettingsSwitch(
                    title = "Reconexión Automática",
                    subtitle = "Reconectar automáticamente al perder conexión",
                    checked = autoReconnect,
                    onCheckedChange = { settings.setAutoReconnect(it) }
                )

                Spacer(modifier = Modifier.height(16.dp))

                SettingsSwitch(
                    title = "Modo Baja Latencia",
                    subtitle = "Priorizar latencia sobre estabilidad",
                    checked = lowLatencyMode,
                    onCheckedChange = { settings.setLowLatencyMode(it) }
                )

                Spacer(modifier = Modifier.height(16.dp))

                SettingsLabel("Tamaño del Buffer")
                Spacer(modifier = Modifier.height(8.dp))
                ModernSegmentedButtons(
                    options = AppSettings.BUFFER_OPTIONS.keys.toList(),
                    selected = bufferSize,
                    onSelect = { settings.setBufferSize(it) }
                )
                
                Spacer(modifier = Modifier.height(8.dp))
                Text(
                    text = "Buffer actual: ${settings.getBufferSizeMs()}ms",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )
            }

            Spacer(modifier = Modifier.height(16.dp))

            // Display Settings Section
            SettingsSection(title = "Pantalla", icon = Icons.Outlined.Fullscreen) {
                SettingsSwitch(
                    title = "Mantener Pantalla Encendida",
                    subtitle = "Evitar que la pantalla se apague durante el streaming",
                    checked = keepScreenOn,
                    onCheckedChange = { settings.setKeepScreenOn(it) }
                )
                
                Spacer(modifier = Modifier.height(16.dp))
                
                SettingsSwitch(
                    title = "Modo Inmersivo",
                    subtitle = "Ocultar barras del sistema durante el streaming",
                    checked = immersiveMode,
                    onCheckedChange = { settings.setImmersiveMode(it) }
                )
            }

            Spacer(modifier = Modifier.height(16.dp))

            // Appearance Section
            SettingsSection(title = "Apariencia", icon = Icons.Outlined.Palette) {
                SettingsLabel("Tema")
                Spacer(modifier = Modifier.height(8.dp))
                ModernSegmentedButtons(
                    options = AppSettings.THEME_OPTIONS,
                    selected = theme,
                    onSelect = { settings.setTheme(it) }
                )
            }

            Spacer(modifier = Modifier.height(16.dp))

            // Developer Section
            SettingsSection(title = "Desarrollador", icon = Icons.Outlined.Code) {
                SettingsSwitch(
                    title = "Mostrar Estadísticas",
                    subtitle = "Mostrar FPS, bitrate y latencia en pantalla",
                    checked = showStats,
                    onCheckedChange = { settings.setShowStats(it) }
                )
                
                Spacer(modifier = Modifier.height(16.dp))
                
                SettingsSwitch(
                    title = "Registro Detallado",
                    subtitle = "Habilitar logs de depuración",
                    checked = verboseLogging,
                    onCheckedChange = { settings.setVerboseLogging(it) }
                )
            }

            Spacer(modifier = Modifier.height(16.dp))

            // Reset Section
            SettingsSection(title = "Datos", icon = Icons.Outlined.Storage) {
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .clip(RoundedCornerShape(12.dp))
                        .clickable { showResetDialog = true }
                        .padding(vertical = 12.dp),
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Icon(
                        Icons.Outlined.RestartAlt,
                        contentDescription = null,
                        tint = MaterialTheme.colorScheme.error
                    )
                    Spacer(modifier = Modifier.width(12.dp))
                    Column {
                        Text(
                            "Restablecer Ajustes",
                            style = MaterialTheme.typography.bodyLarge,
                            color = MaterialTheme.colorScheme.error
                        )
                        Text(
                            "Volver a los valores predeterminados",
                            style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.onSurfaceVariant
                        )
                    }
                }
            }

            Spacer(modifier = Modifier.height(24.dp))

            // About Section
            Card(
                modifier = Modifier.fillMaxWidth(),
                shape = RoundedCornerShape(20.dp),
                colors = CardDefaults.cardColors(
                    containerColor = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.5f)
                )
            ) {
                Column(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(20.dp),
                    horizontalAlignment = Alignment.CenterHorizontally
                ) {
                    Text(
                        "StreamLinux",
                        style = MaterialTheme.typography.titleLarge,
                        fontWeight = FontWeight.Bold
                    )
                    Spacer(modifier = Modifier.height(4.dp))
                    Text(
                        "Versión 1.0.0",
                        style = MaterialTheme.typography.bodyMedium,
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                    Spacer(modifier = Modifier.height(8.dp))
                    Text(
                        "Streaming de pantalla de baja latencia\nde Linux a Android",
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                        textAlign = androidx.compose.ui.text.style.TextAlign.Center
                    )
                }
            }

            Spacer(modifier = Modifier.height(32.dp))
        }
    }
    
    // Reset confirmation dialog
    if (showResetDialog) {
        AlertDialog(
            onDismissRequest = { showResetDialog = false },
            icon = { Icon(Icons.Outlined.Warning, contentDescription = null) },
            title = { Text("Restablecer Ajustes") },
            text = { Text("¿Estás seguro de que quieres restablecer todos los ajustes a sus valores predeterminados?") },
            confirmButton = {
                TextButton(
                    onClick = {
                        settings.resetToDefaults()
                        showResetDialog = false
                    }
                ) {
                    Text("Restablecer", color = MaterialTheme.colorScheme.error)
                }
            },
            dismissButton = {
                TextButton(onClick = { showResetDialog = false }) {
                    Text("Cancelar")
                }
            }
        )
    }
}

@Composable
fun SettingsSection(
    title: String,
    icon: ImageVector,
    content: @Composable ColumnScope.() -> Unit
) {
    Card(
        modifier = Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(20.dp),
        colors = CardDefaults.cardColors(
            containerColor = MaterialTheme.colorScheme.surface
        ),
        elevation = CardDefaults.cardElevation(defaultElevation = 1.dp)
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(20.dp)
        ) {
            Row(
                verticalAlignment = Alignment.CenterVertically,
                modifier = Modifier.padding(bottom = 16.dp)
            ) {
                Box(
                    modifier = Modifier
                        .size(36.dp)
                        .clip(RoundedCornerShape(10.dp))
                        .background(MaterialTheme.colorScheme.primaryContainer),
                    contentAlignment = Alignment.Center
                ) {
                    Icon(
                        icon,
                        contentDescription = null,
                        tint = MaterialTheme.colorScheme.onPrimaryContainer,
                        modifier = Modifier.size(20.dp)
                    )
                }
                Spacer(modifier = Modifier.width(12.dp))
                Text(
                    text = title,
                    style = MaterialTheme.typography.titleMedium,
                    fontWeight = FontWeight.SemiBold
                )
            }
            content()
        }
    }
}

@Composable
fun SettingsSwitch(
    title: String,
    subtitle: String,
    checked: Boolean,
    onCheckedChange: (Boolean) -> Unit
) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.SpaceBetween,
        verticalAlignment = Alignment.CenterVertically
    ) {
        Column(modifier = Modifier.weight(1f)) {
            Text(
                text = title,
                style = MaterialTheme.typography.bodyLarge
            )
            Text(
                text = subtitle,
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant
            )
        }
        Spacer(modifier = Modifier.width(16.dp))
        Switch(
            checked = checked,
            onCheckedChange = onCheckedChange
        )
    }
}

@Composable
fun SettingsLabel(text: String) {
    Text(
        text = text,
        style = MaterialTheme.typography.bodyMedium,
        fontWeight = FontWeight.Medium
    )
}

@Composable
fun ModernSegmentedButtons(
    options: List<String>,
    selected: String,
    onSelect: (String) -> Unit
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(12.dp))
            .background(MaterialTheme.colorScheme.surfaceVariant),
        horizontalArrangement = Arrangement.SpaceEvenly
    ) {
        options.forEach { label ->
            val isSelected = selected == label
            Box(
                modifier = Modifier
                    .weight(1f)
                    .clip(RoundedCornerShape(12.dp))
                    .background(
                        if (isSelected) MaterialTheme.colorScheme.primary
                        else Color.Transparent
                    )
                    .clickable { onSelect(label) }
                    .padding(vertical = 12.dp),
                contentAlignment = Alignment.Center
            ) {
                Text(
                    text = label,
                    style = MaterialTheme.typography.labelMedium,
                    color = if (isSelected) 
                        MaterialTheme.colorScheme.onPrimary 
                    else 
                        MaterialTheme.colorScheme.onSurfaceVariant
                )
            }
        }
    }
}
