package com.streamlinux.client.ui

import android.Manifest
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Bundle
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.camera.core.*
import androidx.camera.lifecycle.ProcessCameraProvider
import androidx.camera.view.PreviewView
import androidx.compose.animation.core.*
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.ArrowBack
import androidx.compose.material.icons.filled.FlashOff
import androidx.compose.material.icons.filled.FlashOn
import androidx.compose.material.icons.filled.QrCodeScanner
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.BlendMode
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalLifecycleOwner
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.viewinterop.AndroidView
import androidx.core.content.ContextCompat
import com.google.mlkit.vision.barcode.BarcodeScanning
import com.google.mlkit.vision.common.InputImage
import com.streamlinux.client.R
import com.streamlinux.client.network.LANDiscovery
import com.streamlinux.client.ui.theme.StreamLinuxTheme
import java.util.concurrent.ExecutorService
import java.util.concurrent.Executors

/**
 * Modern QR Code scanning activity with visual guides and animations
 */
class QRScanActivity : ComponentActivity() {

    private lateinit var cameraExecutor: ExecutorService
    private var camera: Camera? = null

    private val requestPermissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestPermission()
    ) { isGranted ->
        if (!isGranted) {
            Toast.makeText(this, getString(R.string.camera_permission_required), Toast.LENGTH_SHORT).show()
            finish()
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        cameraExecutor = Executors.newSingleThreadExecutor()

        // Check camera permission
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.CAMERA)
            != PackageManager.PERMISSION_GRANTED) {
            requestPermissionLauncher.launch(Manifest.permission.CAMERA)
        }

        setContent {
            StreamLinuxTheme {
                QRScanScreen(
                    onQRCodeScanned = { data -> handleQRCode(data) },
                    onBack = { finish() },
                    onCameraReady = { cam -> camera = cam },
                    cameraExecutor = cameraExecutor
                )
            }
        }
    }

    private fun handleQRCode(data: String) {
        val hostInfo = LANDiscovery.parseHostInfo(data)
        
        if (hostInfo != null) {
            val intent = Intent(this, StreamActivity::class.java).apply {
                putExtra(StreamActivity.EXTRA_HOST_ADDRESS, hostInfo.address)
                putExtra(StreamActivity.EXTRA_HOST_PORT, hostInfo.port)
                putExtra(StreamActivity.EXTRA_HOST_NAME, hostInfo.name)
                putExtra(StreamActivity.EXTRA_HOST_TOKEN, hostInfo.token)
            }
            startActivity(intent)
            finish()
        } else {
            Toast.makeText(this, getString(R.string.invalid_qr_code), Toast.LENGTH_SHORT).show()
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        cameraExecutor.shutdown()
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun QRScanScreen(
    onQRCodeScanned: (String) -> Unit,
    onBack: () -> Unit,
    onCameraReady: (Camera?) -> Unit,
    cameraExecutor: ExecutorService
) {
    val context = LocalContext.current
    val lifecycleOwner = LocalLifecycleOwner.current
    var scanned by remember { mutableStateOf(false) }
    var flashEnabled by remember { mutableStateOf(false) }
    var camera by remember { mutableStateOf<Camera?>(null) }

    Box(modifier = Modifier.fillMaxSize()) {
        // Camera Preview
        AndroidView(
            factory = { ctx ->
                PreviewView(ctx).apply {
                    implementationMode = PreviewView.ImplementationMode.COMPATIBLE
                }
            },
            modifier = Modifier.fillMaxSize(),
            update = { previewView ->
                val cameraProviderFuture = ProcessCameraProvider.getInstance(context)
                
                cameraProviderFuture.addListener({
                    val cameraProvider = cameraProviderFuture.get()
                    
                    val preview = Preview.Builder().build().also {
                        it.setSurfaceProvider(previewView.surfaceProvider)
                    }
                    
                    val imageAnalysis = ImageAnalysis.Builder()
                        .setBackpressureStrategy(ImageAnalysis.STRATEGY_KEEP_ONLY_LATEST)
                        .build()
                    
                    val scanner = BarcodeScanning.getClient()
                    
                    imageAnalysis.setAnalyzer(cameraExecutor) { imageProxy ->
                        @androidx.camera.core.ExperimentalGetImage
                        val mediaImage = imageProxy.image
                        if (mediaImage != null && !scanned) {
                            val image = InputImage.fromMediaImage(
                                mediaImage,
                                imageProxy.imageInfo.rotationDegrees
                            )
                            
                            scanner.process(image)
                                .addOnSuccessListener { barcodes ->
                                    for (barcode in barcodes) {
                                        barcode.rawValue?.let { value ->
                                            if (!scanned) {
                                                scanned = true
                                                onQRCodeScanned(value)
                                            }
                                        }
                                    }
                                }
                                .addOnCompleteListener {
                                    imageProxy.close()
                                }
                        } else {
                            imageProxy.close()
                        }
                    }
                    
                    val cameraSelector = CameraSelector.DEFAULT_BACK_CAMERA
                    
                    try {
                        cameraProvider.unbindAll()
                        camera = cameraProvider.bindToLifecycle(
                            lifecycleOwner,
                            cameraSelector,
                            preview,
                            imageAnalysis
                        )
                        onCameraReady(camera)
                    } catch (e: Exception) {
                        // Handle error
                    }
                }, ContextCompat.getMainExecutor(context))
            }
        )
        
        // Scanning Overlay
        ScanningOverlay(
            modifier = Modifier.fillMaxSize()
        )
        
        // Top Bar with gradient
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .background(
                    Brush.verticalGradient(
                        colors = listOf(
                            Color.Black.copy(alpha = 0.7f),
                            Color.Transparent
                        )
                    )
                )
                .statusBarsPadding()
                .padding(8.dp)
        ) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                IconButton(
                    onClick = onBack,
                    modifier = Modifier
                        .clip(CircleShape)
                        .background(Color.White.copy(alpha = 0.2f))
                ) {
                    Icon(
                        Icons.Default.ArrowBack,
                        contentDescription = stringResource(R.string.back),
                        tint = Color.White
                    )
                }
                
                Text(
                    text = stringResource(R.string.scan_qr_code),
                    style = MaterialTheme.typography.titleLarge,
                    fontWeight = FontWeight.Bold,
                    color = Color.White
                )
                
                // Flash toggle
                IconButton(
                    onClick = {
                        flashEnabled = !flashEnabled
                        camera?.cameraControl?.enableTorch(flashEnabled)
                    },
                    modifier = Modifier
                        .clip(CircleShape)
                        .background(Color.White.copy(alpha = 0.2f))
                ) {
                    Icon(
                        if (flashEnabled) Icons.Default.FlashOn else Icons.Default.FlashOff,
                        contentDescription = if (flashEnabled) "Flash On" else "Flash Off",
                        tint = if (flashEnabled) Color(0xFFFFD700) else Color.White
                    )
                }
            }
        }
        
        // Bottom instruction card
        Box(
            modifier = Modifier
                .align(Alignment.BottomCenter)
                .fillMaxWidth()
                .background(
                    Brush.verticalGradient(
                        colors = listOf(
                            Color.Transparent,
                            Color.Black.copy(alpha = 0.8f)
                        )
                    )
                )
                .navigationBarsPadding()
                .padding(24.dp)
        ) {
            Column(
                horizontalAlignment = Alignment.CenterHorizontally,
                modifier = Modifier.fillMaxWidth()
            ) {
                Card(
                    modifier = Modifier.fillMaxWidth(),
                    shape = RoundedCornerShape(20.dp),
                    colors = CardDefaults.cardColors(
                        containerColor = Color.White.copy(alpha = 0.15f)
                    )
                ) {
                    Row(
                        modifier = Modifier
                            .fillMaxWidth()
                            .padding(16.dp),
                        verticalAlignment = Alignment.CenterVertically,
                        horizontalArrangement = Arrangement.spacedBy(16.dp)
                    ) {
                        Box(
                            modifier = Modifier
                                .size(48.dp)
                                .clip(CircleShape)
                                .background(
                                    Brush.linearGradient(
                                        colors = listOf(
                                            Color(0xFF6C63FF),
                                            Color(0xFF00D9FF)
                                        )
                                    )
                                ),
                            contentAlignment = Alignment.Center
                        ) {
                            Icon(
                                Icons.Default.QrCodeScanner,
                                contentDescription = null,
                                tint = Color.White,
                                modifier = Modifier.size(24.dp)
                            )
                        }
                        
                        Column(modifier = Modifier.weight(1f)) {
                            Text(
                                text = stringResource(R.string.scan_qr_title),
                                style = MaterialTheme.typography.titleMedium,
                                fontWeight = FontWeight.Bold,
                                color = Color.White
                            )
                            Text(
                                text = stringResource(R.string.scan_qr_subtitle),
                                style = MaterialTheme.typography.bodySmall,
                                color = Color.White.copy(alpha = 0.7f)
                            )
                        }
                    }
                }
            }
        }
    }
}

@Composable
fun ScanningOverlay(modifier: Modifier = Modifier) {
    val infiniteTransition = rememberInfiniteTransition(label = "scan")
    
    // Animated scan line
    val scanLinePosition by infiniteTransition.animateFloat(
        initialValue = 0f,
        targetValue = 1f,
        animationSpec = infiniteRepeatable(
            animation = tween(2000, easing = LinearEasing),
            repeatMode = RepeatMode.Reverse
        ),
        label = "scanLine"
    )
    
    // Pulsing corners
    val cornerAlpha by infiniteTransition.animateFloat(
        initialValue = 0.5f,
        targetValue = 1f,
        animationSpec = infiniteRepeatable(
            animation = tween(1000),
            repeatMode = RepeatMode.Reverse
        ),
        label = "cornerAlpha"
    )

    Canvas(modifier = modifier) {
        val scanAreaSize = size.minDimension * 0.65f
        val scanAreaLeft = (size.width - scanAreaSize) / 2
        val scanAreaTop = (size.height - scanAreaSize) / 2
        
        // Dark overlay outside scan area
        drawRect(
            color = Color.Black.copy(alpha = 0.6f),
            size = size
        )
        
        // Clear center (scan area)
        drawRoundRect(
            color = Color.Transparent,
            topLeft = Offset(scanAreaLeft, scanAreaTop),
            size = Size(scanAreaSize, scanAreaSize),
            cornerRadius = CornerRadius(24.dp.toPx()),
            blendMode = BlendMode.Clear
        )
        
        // Border around scan area
        drawRoundRect(
            color = Color.White.copy(alpha = 0.3f),
            topLeft = Offset(scanAreaLeft, scanAreaTop),
            size = Size(scanAreaSize, scanAreaSize),
            cornerRadius = CornerRadius(24.dp.toPx()),
            style = Stroke(width = 2.dp.toPx())
        )
        
        // Animated corner accents
        val cornerLength = 40.dp.toPx()
        val cornerWidth = 4.dp.toPx()
        val cornerColor = Color(0xFF6C63FF).copy(alpha = cornerAlpha)
        val cornerRadius = 24.dp.toPx()
        
        // Top-left corner
        drawPath(
            path = Path().apply {
                moveTo(scanAreaLeft, scanAreaTop + cornerLength)
                lineTo(scanAreaLeft, scanAreaTop + cornerRadius)
                quadraticBezierTo(scanAreaLeft, scanAreaTop, scanAreaLeft + cornerRadius, scanAreaTop)
                lineTo(scanAreaLeft + cornerLength, scanAreaTop)
            },
            color = cornerColor,
            style = Stroke(width = cornerWidth)
        )
        
        // Top-right corner
        drawPath(
            path = Path().apply {
                moveTo(scanAreaLeft + scanAreaSize - cornerLength, scanAreaTop)
                lineTo(scanAreaLeft + scanAreaSize - cornerRadius, scanAreaTop)
                quadraticBezierTo(scanAreaLeft + scanAreaSize, scanAreaTop, scanAreaLeft + scanAreaSize, scanAreaTop + cornerRadius)
                lineTo(scanAreaLeft + scanAreaSize, scanAreaTop + cornerLength)
            },
            color = cornerColor,
            style = Stroke(width = cornerWidth)
        )
        
        // Bottom-left corner
        drawPath(
            path = Path().apply {
                moveTo(scanAreaLeft, scanAreaTop + scanAreaSize - cornerLength)
                lineTo(scanAreaLeft, scanAreaTop + scanAreaSize - cornerRadius)
                quadraticBezierTo(scanAreaLeft, scanAreaTop + scanAreaSize, scanAreaLeft + cornerRadius, scanAreaTop + scanAreaSize)
                lineTo(scanAreaLeft + cornerLength, scanAreaTop + scanAreaSize)
            },
            color = cornerColor,
            style = Stroke(width = cornerWidth)
        )
        
        // Bottom-right corner
        drawPath(
            path = Path().apply {
                moveTo(scanAreaLeft + scanAreaSize - cornerLength, scanAreaTop + scanAreaSize)
                lineTo(scanAreaLeft + scanAreaSize - cornerRadius, scanAreaTop + scanAreaSize)
                quadraticBezierTo(scanAreaLeft + scanAreaSize, scanAreaTop + scanAreaSize, scanAreaLeft + scanAreaSize, scanAreaTop + scanAreaSize - cornerRadius)
                lineTo(scanAreaLeft + scanAreaSize, scanAreaTop + scanAreaSize - cornerLength)
            },
            color = cornerColor,
            style = Stroke(width = cornerWidth)
        )
        
        // Animated scan line
        val scanLineY = scanAreaTop + (scanAreaSize * scanLinePosition)
        drawLine(
            brush = Brush.horizontalGradient(
                colors = listOf(
                    Color.Transparent,
                    Color(0xFF00D9FF),
                    Color(0xFF6C63FF),
                    Color(0xFF00D9FF),
                    Color.Transparent
                ),
                startX = scanAreaLeft,
                endX = scanAreaLeft + scanAreaSize
            ),
            start = Offset(scanAreaLeft + 20.dp.toPx(), scanLineY),
            end = Offset(scanAreaLeft + scanAreaSize - 20.dp.toPx(), scanLineY),
            strokeWidth = 3.dp.toPx()
        )
    }
}
