package com.streamlinux.client.settings

import android.content.Context
import android.content.SharedPreferences
import androidx.core.content.edit
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow

/**
 * Centralized app settings with real persistence
 */
class AppSettings private constructor(context: Context) {
    
    companion object {
        private const val PREFS_NAME = "streamlinux_settings"
        
        // Video settings keys
        const val KEY_PREFER_H265 = "prefer_h265"
        const val KEY_MAX_BITRATE = "max_bitrate"
        const val KEY_RESOLUTION = "resolution"
        
        // Network settings keys
        const val KEY_AUTO_RECONNECT = "auto_reconnect"
        const val KEY_LOW_LATENCY_MODE = "low_latency_mode"
        const val KEY_BUFFER_SIZE = "buffer_size"
        const val KEY_LAST_HOST_IP = "last_host_ip"
        const val KEY_LAST_HOST_PORT = "last_host_port"
        const val KEY_SAVED_HOSTS = "saved_hosts"
        
        // Appearance keys
        const val KEY_THEME = "theme"
        const val KEY_KEEP_SCREEN_ON = "keep_screen_on"
        const val KEY_IMMERSIVE_MODE = "immersive_mode"
        
        // Developer settings keys
        const val KEY_SHOW_STATS = "show_stats"
        const val KEY_VERBOSE_LOGGING = "verbose_logging"
        
        // Bitrate options (in kbps)
        val BITRATE_OPTIONS = mapOf(
            "Auto" to 0,
            "5 Mbps" to 5000,
            "10 Mbps" to 10000,
            "20 Mbps" to 20000,
            "30 Mbps" to 30000
        )
        
        // Buffer size options (in ms)
        val BUFFER_OPTIONS = mapOf(
            "Low" to 100,
            "Normal" to 300,
            "High" to 500
        )
        
        // Resolution options
        val RESOLUTION_OPTIONS = listOf("Auto", "1080p", "720p", "480p")
        
        // Theme options
        val THEME_OPTIONS = listOf("System", "Light", "Dark")
        
        @Volatile
        private var instance: AppSettings? = null
        
        fun getInstance(context: Context): AppSettings {
            return instance ?: synchronized(this) {
                instance ?: AppSettings(context.applicationContext).also {
                    instance = it
                }
            }
        }
    }
    
    private val prefs: SharedPreferences = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
    
    // === VIDEO SETTINGS ===
    
    private val _preferH265 = MutableStateFlow(prefs.getBoolean(KEY_PREFER_H265, false))
    val preferH265: StateFlow<Boolean> = _preferH265.asStateFlow()
    
    fun setPreferH265(value: Boolean) {
        prefs.edit { putBoolean(KEY_PREFER_H265, value) }
        _preferH265.value = value
    }
    
    private val _maxBitrate = MutableStateFlow(prefs.getString(KEY_MAX_BITRATE, "Auto") ?: "Auto")
    val maxBitrate: StateFlow<String> = _maxBitrate.asStateFlow()
    
    fun setMaxBitrate(value: String) {
        prefs.edit { putString(KEY_MAX_BITRATE, value) }
        _maxBitrate.value = value
    }
    
    fun getMaxBitrateKbps(): Int = BITRATE_OPTIONS[_maxBitrate.value] ?: 0
    
    private val _resolution = MutableStateFlow(prefs.getString(KEY_RESOLUTION, "Auto") ?: "Auto")
    val resolution: StateFlow<String> = _resolution.asStateFlow()
    
    fun setResolution(value: String) {
        prefs.edit { putString(KEY_RESOLUTION, value) }
        _resolution.value = value
    }
    
    // === NETWORK SETTINGS ===
    
    private val _autoReconnect = MutableStateFlow(prefs.getBoolean(KEY_AUTO_RECONNECT, true))
    val autoReconnect: StateFlow<Boolean> = _autoReconnect.asStateFlow()
    
    fun setAutoReconnect(value: Boolean) {
        prefs.edit { putBoolean(KEY_AUTO_RECONNECT, value) }
        _autoReconnect.value = value
    }
    
    private val _lowLatencyMode = MutableStateFlow(prefs.getBoolean(KEY_LOW_LATENCY_MODE, true))
    val lowLatencyMode: StateFlow<Boolean> = _lowLatencyMode.asStateFlow()
    
    fun setLowLatencyMode(value: Boolean) {
        prefs.edit { putBoolean(KEY_LOW_LATENCY_MODE, value) }
        _lowLatencyMode.value = value
    }
    
    private val _bufferSize = MutableStateFlow(prefs.getString(KEY_BUFFER_SIZE, "Normal") ?: "Normal")
    val bufferSize: StateFlow<String> = _bufferSize.asStateFlow()
    
    fun setBufferSize(value: String) {
        prefs.edit { putString(KEY_BUFFER_SIZE, value) }
        _bufferSize.value = value
    }
    
    fun getBufferSizeMs(): Int = BUFFER_OPTIONS[_bufferSize.value] ?: 300
    
    // Last connection
    var lastHostIp: String
        get() = prefs.getString(KEY_LAST_HOST_IP, "") ?: ""
        set(value) = prefs.edit { putString(KEY_LAST_HOST_IP, value) }
    
    var lastHostPort: Int
        get() = prefs.getInt(KEY_LAST_HOST_PORT, 54321)
        set(value) = prefs.edit { putInt(KEY_LAST_HOST_PORT, value) }
    
    // Saved hosts (JSON format)
    var savedHostsJson: String
        get() = prefs.getString(KEY_SAVED_HOSTS, "[]") ?: "[]"
        set(value) = prefs.edit { putString(KEY_SAVED_HOSTS, value) }
    
    // === APPEARANCE SETTINGS ===
    
    private val _theme = MutableStateFlow(prefs.getString(KEY_THEME, "System") ?: "System")
    val theme: StateFlow<String> = _theme.asStateFlow()
    
    fun setTheme(value: String) {
        prefs.edit { putString(KEY_THEME, value) }
        _theme.value = value
    }
    
    private val _keepScreenOn = MutableStateFlow(prefs.getBoolean(KEY_KEEP_SCREEN_ON, true))
    val keepScreenOn: StateFlow<Boolean> = _keepScreenOn.asStateFlow()
    
    fun setKeepScreenOn(value: Boolean) {
        prefs.edit { putBoolean(KEY_KEEP_SCREEN_ON, value) }
        _keepScreenOn.value = value
    }
    
    private val _immersiveMode = MutableStateFlow(prefs.getBoolean(KEY_IMMERSIVE_MODE, true))
    val immersiveMode: StateFlow<Boolean> = _immersiveMode.asStateFlow()
    
    fun setImmersiveMode(value: Boolean) {
        prefs.edit { putBoolean(KEY_IMMERSIVE_MODE, value) }
        _immersiveMode.value = value
    }
    
    // === DEVELOPER SETTINGS ===
    
    private val _showStats = MutableStateFlow(prefs.getBoolean(KEY_SHOW_STATS, false))
    val showStats: StateFlow<Boolean> = _showStats.asStateFlow()
    
    fun setShowStats(value: Boolean) {
        prefs.edit { putBoolean(KEY_SHOW_STATS, value) }
        _showStats.value = value
    }
    
    private val _verboseLogging = MutableStateFlow(prefs.getBoolean(KEY_VERBOSE_LOGGING, false))
    val verboseLogging: StateFlow<Boolean> = _verboseLogging.asStateFlow()
    
    fun setVerboseLogging(value: Boolean) {
        prefs.edit { putBoolean(KEY_VERBOSE_LOGGING, value) }
        _verboseLogging.value = value
    }
    
    // === UTILITY ===
    
    fun resetToDefaults() {
        prefs.edit { clear() }
        
        // Reset all flows
        _preferH265.value = false
        _maxBitrate.value = "Auto"
        _resolution.value = "Auto"
        _autoReconnect.value = true
        _lowLatencyMode.value = true
        _bufferSize.value = "Normal"
        _theme.value = "System"
        _keepScreenOn.value = true
        _immersiveMode.value = true
        _showStats.value = false
        _verboseLogging.value = false
    }
}
