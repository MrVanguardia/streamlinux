package com.streamlinux.client

import android.app.Application
import android.util.Log

/**
 * Application class for Stream Linux Client
 */
class StreamLinuxApp : Application() {

    companion object {
        private const val TAG = "StreamLinuxApp"
        
        lateinit var instance: StreamLinuxApp
            private set
    }

    override fun onCreate() {
        super.onCreate()
        instance = this
        
        Log.d(TAG, "StreamLinux Client initialized")
        
        // Initialize native libraries
        initNativeLibraries()
    }

    private fun initNativeLibraries() {
        try {
            System.loadLibrary("streamlinux_client")
            Log.d(TAG, "Native library loaded successfully")
        } catch (e: UnsatisfiedLinkError) {
            Log.e(TAG, "Failed to load native library", e)
        }
    }
}
