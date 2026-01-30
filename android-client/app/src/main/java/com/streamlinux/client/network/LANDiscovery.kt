package com.streamlinux.client.network

import android.content.Context
import android.util.Log
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.async
import kotlinx.coroutines.awaitAll
import kotlinx.coroutines.coroutineScope
import kotlinx.coroutines.withContext
import kotlinx.coroutines.withTimeout
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.HttpURLConnection
import java.net.InetAddress
import java.net.InetSocketAddress
import java.net.Socket
import java.net.URL
import org.json.JSONObject

/**
 * Connection type for host
 */
enum class ConnectionType {
    USB,    // Connected via USB (ADB port forwarding) - fastest, most stable
    WIFI,   // Connected via WiFi LAN - good for wireless
    UNKNOWN // Connection type not determined
}

/**
 * Host information discovered via LAN, USB, or QR code
 */
data class HostInfo(
    val name: String,
    val address: String,
    val port: Int,
    val fingerprint: String = "",
    val isActive: Boolean = false,      // True if host is actively streaming
    val hasClients: Boolean = false,    // True if host already has connected clients
    val connectionType: ConnectionType = ConnectionType.UNKNOWN,  // USB or WiFi
    val isUSB: Boolean = false,         // Quick check for USB connection
    val token: String? = null           // Optional auth token from QR code
) {
    /**
     * Get display name with connection type indicator
     */
    fun getDisplayName(): String {
        return when (connectionType) {
            ConnectionType.USB -> "⚡ $name (USB)"
            ConnectionType.WIFI -> "📶 $name"
            ConnectionType.UNKNOWN -> name
        }
    }
    
    /**
     * Get connection quality description
     */
    fun getConnectionQuality(): String {
        return when (connectionType) {
            ConnectionType.USB -> "USB • Ultra-low latency"
            ConnectionType.WIFI -> "WiFi • ${address}"
            ConnectionType.UNKNOWN -> address
        }
    }
}

/**
 * LAN and USB discovery for finding stream-linux hosts
 */
object LANDiscovery {
    private const val TAG = "LANDiscovery"
    private const val DISCOVERY_PORT = 54321
    private const val MULTICAST_ADDRESS = "239.255.42.42"
    private const val DISCOVERY_TIMEOUT_MS = 3000L
    private const val DISCOVERY_MESSAGE = "STREAMLINUX_DISCOVER"
    private const val HTTP_TIMEOUT_MS = 2000
    private const val USB_CHECK_TIMEOUT_MS = 500
    
    // USB connection uses localhost with ADB port forwarding
    private const val USB_ADDRESS = "127.0.0.1"

    /**
     * Discover hosts on the local network AND via USB
     * USB connections are prioritized as they're faster and more stable
     */
    suspend fun discover(): List<HostInfo> = coroutineScope {
        val allHosts = mutableMapOf<String, HostInfo>()  // Key by address to avoid duplicates
        
        // Run all discovery methods in parallel
        val usbDeferred = async { discoverViaUSB() }
        val udpDeferred = async { discoverViaUDP() }
        val httpDeferred = async { discoverActiveHosts() }
        
        try {
            // USB discovery first (highest priority)
            val usbHost = usbDeferred.await()
            if (usbHost != null) {
                // USB connection found - add with special key to keep it separate
                allHosts["USB:${USB_ADDRESS}"] = usbHost
                Log.d(TAG, "✓ USB host found: ${usbHost.name}")
            }
            
            // Collect results from network methods
            val udpHosts = udpDeferred.await()
            val httpHosts = httpDeferred.await()
            
            // Add UDP discovered hosts
            for (host in udpHosts) {
                allHosts[host.address] = host.copy(connectionType = ConnectionType.WIFI)
            }
            
            // Merge/update with HTTP discovered hosts (they have isActive=true)
            for (host in httpHosts) {
                val existingHost = allHosts[host.address]
                if (existingHost != null) {
                    // Update existing host with active status
                    allHosts[host.address] = existingHost.copy(
                        isActive = host.isActive,
                        hasClients = host.hasClients,
                        name = if (host.name.isNotEmpty()) host.name else existingHost.name
                    )
                } else {
                    allHosts[host.address] = host.copy(connectionType = ConnectionType.WIFI)
                }
            }
        } catch (e: Exception) {
            Log.e(TAG, "Discovery error", e)
        }
        
        // Return sorted: USB first, then active hosts, then by name
        allHosts.values.sortedWith(
            compareByDescending<HostInfo> { it.connectionType == ConnectionType.USB }
                .thenByDescending { it.isActive }
                .thenBy { it.name.lowercase() }
        )
    }
    
    /**
     * Check for USB connection via ADB port forwarding
     * This checks if localhost:54321 is available (ADB forward from host)
     */
    private suspend fun discoverViaUSB(): HostInfo? = withContext(Dispatchers.IO) {
        try {
            // Try to connect to localhost - if ADB forwarding is set up, this will work
            val socket = Socket()
            socket.connect(InetSocketAddress(USB_ADDRESS, DISCOVERY_PORT), USB_CHECK_TIMEOUT_MS)
            socket.close()
            
            // Connection successful - USB forwarding is active
            Log.d(TAG, "USB connection detected on localhost:$DISCOVERY_PORT")
            
            // Try to get host info from the server
            val hostName = try {
                val url = URL("http://$USB_ADDRESS:$DISCOVERY_PORT/hosts")
                val connection = url.openConnection() as HttpURLConnection
                connection.connectTimeout = USB_CHECK_TIMEOUT_MS
                connection.readTimeout = USB_CHECK_TIMEOUT_MS
                
                if (connection.responseCode == HttpURLConnection.HTTP_OK) {
                    val response = connection.inputStream.bufferedReader().readText()
                    val json = JSONObject(response)
                    val hostsArray = json.optJSONArray("hosts")
                    if (hostsArray != null && hostsArray.length() > 0) {
                        hostsArray.getJSONObject(0).optString("name", "Linux Host")
                    } else {
                        "Linux Host (USB)"
                    }
                } else {
                    "Linux Host (USB)"
                }
            } catch (e: Exception) {
                "Linux Host (USB)"
            }
            
            return@withContext HostInfo(
                name = hostName,
                address = USB_ADDRESS,
                port = DISCOVERY_PORT,
                isActive = true,
                connectionType = ConnectionType.USB,
                isUSB = true
            )
        } catch (e: Exception) {
            // No USB connection available
            Log.d(TAG, "No USB connection: ${e.message}")
            return@withContext null
        }
    }

    /**
     * Discover hosts via UDP broadcast
     */
    private suspend fun discoverViaUDP(): List<HostInfo> = withContext(Dispatchers.IO) {
        val hosts = mutableListOf<HostInfo>()
        
        try {
            withTimeout(DISCOVERY_TIMEOUT_MS) {
                // Send discovery broadcast
                val socket = DatagramSocket()
                socket.broadcast = true
                socket.soTimeout = DISCOVERY_TIMEOUT_MS.toInt()
                
                val message = DISCOVERY_MESSAGE.toByteArray()
                val broadcastAddress = InetAddress.getByName("255.255.255.255")
                val packet = DatagramPacket(message, message.size, broadcastAddress, DISCOVERY_PORT)
                socket.send(packet)
                
                // Listen for responses
                val buffer = ByteArray(1024)
                val receivePacket = DatagramPacket(buffer, buffer.size)
                
                try {
                    while (true) {
                        socket.receive(receivePacket)
                        val response = String(receivePacket.data, 0, receivePacket.length)
                        
                        try {
                            val host = parseHostInfo(response, receivePacket.address.hostAddress ?: "")
                            if (host != null && !hosts.any { it.address == host.address }) {
                                hosts.add(host)
                            }
                        } catch (e: Exception) {
                            // Invalid response, ignore
                        }
                    }
                } catch (e: Exception) {
                    // Timeout or socket closed
                }
                
                socket.close()
            }
        } catch (e: Exception) {
            Log.d(TAG, "UDP discovery: ${e.message}")
        }
        
        hosts
    }

    /**
     * Query signaling server for active hosts
     * Tries common local addresses and cached server address
     */
    private suspend fun discoverActiveHosts(): List<HostInfo> = withContext(Dispatchers.IO) {
        val hosts = mutableListOf<HostInfo>()
        
        // Get addresses in local network range first (most likely to succeed)
        val localAddresses = getLocalNetworkRange()
        
        // Then add common gateway addresses as fallback
        val serverAddresses = localAddresses + listOf(
            "192.168.1.1:54321",
            "192.168.0.1:54321",
            "10.0.0.1:54321",
            "10.0.0.9:54321",  // Common Linux host address
            "192.168.1.100:54321",
            "192.168.0.100:54321"
        )
        
        // Try addresses in parallel for faster discovery
        val uniqueAddresses = serverAddresses.distinct().take(20)
        
        val jobs = uniqueAddresses.map { address ->
            async {
                try {
                    queryHostsFromServer("http://$address/hosts")
                } catch (e: Exception) {
                    emptyList()
                }
            }
        }
        
        // Wait for all queries and collect results
        val results = jobs.awaitAll()
        for (result in results) {
            if (result.isNotEmpty()) {
                hosts.addAll(result)
            }
        }
        
        hosts.distinctBy { it.address }
    }

    /**
     * Query a specific signaling server for active hosts
     */
    private fun queryHostsFromServer(serverUrl: String): List<HostInfo> {
        val hosts = mutableListOf<HostInfo>()
        
        try {
            val url = URL(serverUrl)
            val connection = url.openConnection() as HttpURLConnection
            connection.requestMethod = "GET"
            connection.connectTimeout = HTTP_TIMEOUT_MS
            connection.readTimeout = HTTP_TIMEOUT_MS
            
            if (connection.responseCode == HttpURLConnection.HTTP_OK) {
                val response = connection.inputStream.bufferedReader().readText()
                val json = JSONObject(response)
                val hostsArray = json.optJSONArray("hosts")
                
                if (hostsArray != null) {
                    for (i in 0 until hostsArray.length()) {
                        val hostJson = hostsArray.getJSONObject(i)
                        // Extract the server address from the URL
                        val serverAddress = url.host
                        
                        hosts.add(HostInfo(
                            name = hostJson.optString("name", "StreamLinux Host"),
                            address = serverAddress,
                            port = url.port.takeIf { it > 0 } ?: DISCOVERY_PORT,
                            isActive = true,
                            hasClients = hostJson.optBoolean("has_clients", false)
                        ))
                    }
                }
            }
            connection.disconnect()
        } catch (e: Exception) {
            // Server not available
        }
        
        return hosts
    }

    /**
     * Get common addresses in the local network range
     * Detects all network interfaces including WiFi
     */
    private fun getLocalNetworkRange(): List<String> {
        val addresses = mutableListOf<String>()
        try {
            // Get all network interfaces
            val interfaces = java.net.NetworkInterface.getNetworkInterfaces()
            while (interfaces.hasMoreElements()) {
                val networkInterface = interfaces.nextElement()
                if (networkInterface.isLoopback || !networkInterface.isUp) continue
                
                for (address in networkInterface.inetAddresses) {
                    if (address is java.net.Inet4Address && !address.isLoopbackAddress) {
                        val hostAddress = address.hostAddress ?: continue
                        if (hostAddress.contains('.')) {
                            val parts = hostAddress.split('.')
                            if (parts.size == 4) {
                                val prefix = "${parts[0]}.${parts[1]}.${parts[2]}"
                                // Add common host addresses in this subnet
                                for (i in listOf(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 100, 101, 102, 50, 150, 200)) {
                                    val addr = "$prefix.$i:$DISCOVERY_PORT"
                                    if (!addresses.contains(addr)) {
                                        addresses.add(addr)
                                    }
                                }
                            }
                        }
                    }
                }
            }
        } catch (e: Exception) {
            Log.e(TAG, "Error getting network range: ${e.message}")
            // Fallback to common ranges
            addresses.addAll(listOf(
                "192.168.1.1:$DISCOVERY_PORT",
                "192.168.0.1:$DISCOVERY_PORT",
                "10.0.0.1:$DISCOVERY_PORT",
                "10.0.0.9:$DISCOVERY_PORT"
            ))
        }
        return addresses
    }

    /**
     * Query hosts from a known server address
     */
    suspend fun discoverFromServer(serverAddress: String, serverPort: Int): List<HostInfo> = 
        withContext(Dispatchers.IO) {
            queryHostsFromServer("http://$serverAddress:$serverPort/hosts")
        }

    /**
     * Parse host info from discovery response or QR code
     */
    fun parseHostInfo(json: String, defaultAddress: String = ""): HostInfo? {
        return try {
            val obj = JSONObject(json)
            
            // Extract token properly - optString returns "" for null, not null
            val tokenValue = if (obj.has("token") && !obj.isNull("token")) {
                obj.getString("token").takeIf { it.isNotEmpty() }
            } else {
                null
            }
            
            // Also try 'host' field for address (some QR formats use 'host')
            val address = obj.optString("address", "").ifEmpty {
                obj.optString("host", defaultAddress)
            }
            
            Log.e(TAG, "PARSED QR - address: $address, port: ${obj.optInt("port")}, token: ${tokenValue?.take(8) ?: "NULL"}...")
            
            HostInfo(
                name = obj.optString("name", "Unknown"),
                address = address,
                port = obj.optInt("port", 0),
                fingerprint = obj.optString("fingerprint", ""),
                isActive = obj.optBoolean("active", false),
                hasClients = obj.optBoolean("has_clients", false),
                token = tokenValue
            )
        } catch (e: Exception) {
            Log.e(TAG, "Error parsing host info: ${e.message}", e)
            null
        }
    }
}
