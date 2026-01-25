package com.streamlinux.client

import android.view.Surface
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

/**
 * Native decoder wrapper for JNI calls
 * 
 * Provides Kotlin interface to native video/audio decoding functionality
 */
class NativeDecoder {

    companion object {
        private const val TAG = "NativeDecoder"
        private var libraryLoaded = false

        // Load native library - disabled for UI testing
        init {
            try {
                System.loadLibrary("streamlinux_client")
                libraryLoaded = true
            } catch (e: UnsatisfiedLinkError) {
                android.util.Log.w(TAG, "Native library not available - running in UI test mode")
                libraryLoaded = false
            }
        }
    }

    // Native methods - implemented in streamlinux_jni.cpp

    /**
     * Initialize the native decoder session
     * 
     * @param surface Surface to render video to
     * @param videoWidth Expected video width
     * @param videoHeight Expected video height
     * @param sps H.264 Sequence Parameter Set
     * @param pps H.264 Picture Parameter Set
     * @param audioSampleRate Audio sample rate (e.g., 48000)
     * @param audioChannels Number of audio channels (e.g., 2)
     * @return true if initialization succeeded
     */
    external fun initialize(
        surface: Surface?,
        videoWidth: Int,
        videoHeight: Int,
        sps: ByteArray?,
        pps: ByteArray?,
        audioSampleRate: Int,
        audioChannels: Int
    ): Boolean

    /**
     * Decode a video frame
     * 
     * @param data H.264 NAL unit data
     * @param pts Presentation timestamp in microseconds
     * @param isKeyFrame Whether this is a keyframe (IDR)
     * @return true if decode was queued successfully
     */
    external fun decodeVideoFrame(data: ByteArray, pts: Long, isKeyFrame: Boolean): Boolean

    /**
     * Decode audio samples
     * 
     * @param data PCM audio samples (16-bit signed)
     * @param pts Presentation timestamp in microseconds
     */
    external fun decodeAudioFrame(data: ShortArray, pts: Long)

    /**
     * Release all native resources
     */
    external fun release()

    /**
     * Get current video latency
     * 
     * @return Video latency in microseconds
     */
    external fun getVideoLatency(): Long

    /**
     * Get current audio latency
     * 
     * @return Audio latency in microseconds
     */
    external fun getAudioLatency(): Long

    /**
     * Check if session is connected
     * 
     * @return true if connected and running
     */
    external fun isConnected(): Boolean

    // Kotlin convenience methods

    /**
     * Initialize decoder on IO dispatcher
     */
    suspend fun initializeAsync(
        surface: Surface?,
        videoWidth: Int,
        videoHeight: Int,
        sps: ByteArray?,
        pps: ByteArray?,
        audioSampleRate: Int = 48000,
        audioChannels: Int = 2
    ): Boolean = withContext(Dispatchers.IO) {
        initialize(surface, videoWidth, videoHeight, sps, pps, audioSampleRate, audioChannels)
    }

    /**
     * Get A/V sync status
     * 
     * @return Pair of (video latency, audio latency) in milliseconds
     */
    fun getSyncStatus(): Pair<Long, Long> {
        val videoLatency = getVideoLatency() / 1000
        val audioLatency = getAudioLatency() / 1000
        return Pair(videoLatency, audioLatency)
    }

    /**
     * Get A/V drift
     * 
     * @return Drift between audio and video in milliseconds (positive = video ahead)
     */
    fun getAVDrift(): Long {
        return (getVideoLatency() - getAudioLatency()) / 1000
    }
}

/**
 * Extension function to decode video frame from ByteBuffer
 */
fun NativeDecoder.decodeVideoFrame(
    buffer: java.nio.ByteBuffer,
    pts: Long,
    isKeyFrame: Boolean
): Boolean {
    val data = ByteArray(buffer.remaining())
    buffer.get(data)
    return decodeVideoFrame(data, pts, isKeyFrame)
}
