// droidHPSDR - GPL v3 or later.
package io.github.randal007.droidhpsdr

import android.Manifest
import android.content.pm.PackageManager
import android.media.AudioManager
import android.os.Bundle
import android.view.WindowManager
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat

class MainActivity : ComponentActivity() {
    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<String>, grantResults: IntArray) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if (checkSelfPermission(LOCAL_NETWORK) == PackageManager.PERMISSION_GRANTED) {
            RadioService.start(this)
        } else {
            Radio.setPhase(Phase.ERROR, "droidHPSDR needs the Local network permission to reach the HL2")
        }
    }

    /** Going to the background: write deskHPSDR's props file now (the stream keeps running). */
    override fun onStop() {
        super.onStop()
        Radio.save()
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        Radio.init(this)

        // full screen: the waterfall is the screen
        WindowCompat.setDecorFitsSystemWindows(window, false)
        WindowInsetsControllerCompat(window, window.decorView).apply {
            hide(WindowInsetsCompat.Type.systemBars())
            systemBarsBehavior = WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
        }
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        // the phone's volume keys set the phone audio's level
        volumeControlStream = AudioManager.STREAM_MUSIC

        // Local network access (Android 17) is needed to reach the HL2 at all;
        // the radio starts once it is answered. Receive only: starting never transmits.
        // `adb shell am start ... --ez demo true` opens on the demo band (display work without a radio)
        if (intent.getBooleanExtra("demo", false)) {
            Radio.startDemo()
        } else {
            val wanted = arrayOf(LOCAL_NETWORK, Manifest.permission.POST_NOTIFICATIONS)
                .filter { checkSelfPermission(it) != PackageManager.PERMISSION_GRANTED }
            if (wanted.isEmpty()) RadioService.start(this) else requestPermissions(wanted.toTypedArray(), 1)
        }

        setContent {
            Screen(onStart = { RadioService.start(this) }, onStop = { RadioService.stop(this) })
        }
    }
}

private const val LOCAL_NETWORK = "android.permission.ACCESS_LOCAL_NETWORK"
