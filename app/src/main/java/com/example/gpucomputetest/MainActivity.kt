package com.example.gpucomputetest

import android.os.Bundle
import android.content.res.AssetManager
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material3.Text // Keep Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
// DELETED: import com.example.gpucomputetest.ui.theme.GpuComputeTestTheme

class MainActivity : ComponentActivity() {

    // --- Native (JNI) Functions ---
    private external fun initJNI(assetManager: AssetManager)
    private external fun stringFromJNI(): String
    private external fun cleanup()

    // --- Activity Lifecycle ---
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        initJNI(assets)
        val computeResult = stringFromJNI()

        // --- SIMPLIFIED setContent ---
        setContent {
            // No theme or surface needed, just show the text
            Greeting(computeResult)
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        cleanup()
    }

    companion object {
        init {
            System.loadLibrary("gpucomputetest")
        }
    }
}

// --- Composable UI ---
@Composable
fun Greeting(message: String, modifier: Modifier = Modifier) {
    // We use a simple Box to center the text on the screen
    Box(
        modifier = modifier.fillMaxSize(),
        contentAlignment = Alignment.Center
    ) {
        Text(
            text = message
        )
    }
}