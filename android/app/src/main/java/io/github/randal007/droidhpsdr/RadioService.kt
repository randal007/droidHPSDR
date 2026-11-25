// droidHPSDR - GPL v3 or later.
package io.github.randal007.droidhpsdr

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Context
import android.content.Intent
import android.content.pm.ServiceInfo
import android.net.ConnectivityManager
import android.net.Network
import android.net.NetworkCapabilities
import android.net.wifi.WifiManager
import android.os.IBinder
import android.os.PowerManager
import android.util.Log
import kotlin.concurrent.thread

/**
 * Owns the radio. A foreground service, so the stream (and the HL2+ speaker
 * audio) keeps going with the screen off or the app in the background
 * (CLAUDE.md decision 4). The UI only reads Radio's state.
 *
 * Network: the whole process is bound to the Ethernet network when there is
 * one (USB-C adapter straight to the HL2, 169.254.x.x), else to WiFi.
 * Without that, Android sends unicast to the radio out over WiFi and it is
 * lost (CLAUDE.md "Network setup").
 */
class RadioService : Service() {
    private var wifiLock: WifiManager.WifiLock? = null
    private var wakeLock: PowerManager.WakeLock? = null
    @Volatile private var starting = false

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        Radio.init(this)
        if (intent?.action == ACTION_STOP) {
            shutdown()
            stopSelf()
            return START_NOT_STICKY
        }
        startForeground(1, notification("Starting"), ServiceInfo.FOREGROUND_SERVICE_TYPE_CONNECTED_DEVICE)
        if (!Radio.running && !starting) startRadio()
        return START_STICKY
    }

    private fun startRadio() {
        starting = true
        Radio.stopDemo()
        wakeLock = getSystemService(PowerManager::class.java)
            .newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "droidHPSDR:radio").apply { acquire() }
        thread(name = "radio-start") {
            try {
                val net = bindNetwork()
                if (net == "WiFi") {
                    wifiLock = getSystemService(WifiManager::class.java)
                        .createWifiLock(WifiManager.WIFI_MODE_FULL_LOW_LATENCY, "droidHPSDR").apply { acquire() }
                }

                // WDSP's FFT plans: built once (about 11 s), then read from the app's files.
                Radio.setPhase(Phase.PREPARING, "Preparing FFT plans (first start only)", net)
                val w = thread(name = "wisdom") { Native.wisdom(filesDir.absolutePath) }
                while (w.isAlive) {
                    Radio.setPhase(Phase.PREPARING, Native.wisdomStatus().ifBlank { "Preparing FFT plans" }.trim())
                    Thread.sleep(250)
                }

                // Wired: the HL2 falls back to this APIPA address with no DHCP server
                // (CLAUDE.md "Network setup"). WiFi: broadcast (the radio's DHCP address).
                val ip = if (net == "Ethernet") Radio.wiredIp.value.ifBlank { WIRED_HL2 }
                    else Radio.wifiIp.value.ifBlank { null }
                if (ip == null) {
                    // An app cannot broadcast for the radio (Android hides the interfaces from it).
                    Radio.setPhase(Phase.ERROR, "On WiFi: enter the radio's IP address in the menu (☰)", net)
                    return@thread
                }
                Radio.setPhase(Phase.CONNECTING, "Looking for the HL2 on $net${ip?.let { " at $it" } ?: ""}")
                val s = Radio.status.value
                val err = Native.start(
                    Radio.freq.value, s.mode, Radio.lnaDb.value, true,
                    Radio.SAMPLE_RATE, Radio.PIXELS, Radio.FPS, ip,
                )
                if (err.isEmpty()) {
                    Native.setVolumeDb(Radio.volumeDb.value)
                    Native.setRowDelayMs(Radio.rowDelayMs.value)
                    Native.setPhoneAudio(Radio.phoneAudio.value)
                    Radio.setPhase(Phase.RUNNING, "", net)
                    Radio.applyModeFilter()
                    getSystemService(NotificationManager::class.java).notify(1, notification("Receiving on $net"))
                } else {
                    Radio.setPhase(Phase.ERROR, err)
                    getSystemService(NotificationManager::class.java).notify(1, notification("Not connected: $err"))
                }
            } catch (t: Throwable) {
                Log.e("droidHPSDR", "start failed", t)
                Radio.setPhase(Phase.ERROR, t.message ?: t.toString())
            } finally {
                starting = false
            }
        }
    }

    /** Binds the process to Ethernet if present, else WiFi. Returns which. */
    private fun bindNetwork(): String {
        val cm = getSystemService(ConnectivityManager::class.java)
        fun find(transport: Int): Network? = @Suppress("DEPRECATION") cm.allNetworks.firstOrNull {
            cm.getNetworkCapabilities(it)?.hasTransport(transport) == true
        }
        val eth = find(NetworkCapabilities.TRANSPORT_ETHERNET)
        val net = eth ?: find(NetworkCapabilities.TRANSPORT_WIFI)
        cm.bindProcessToNetwork(net)
        val name = if (eth != null) "Ethernet" else if (net != null) "WiFi" else "no network"
        Log.i("droidHPSDR", "bound to $name ($net)")
        return name
    }

    private fun shutdown() {
        if (Radio.running) Native.stop()
        Radio.setPhase(Phase.IDLE)
        wifiLock?.takeIf { it.isHeld }?.release()
        wakeLock?.takeIf { it.isHeld }?.release()
        stopForeground(STOP_FOREGROUND_REMOVE)
    }

    override fun onDestroy() {
        shutdown()
        super.onDestroy()
    }

    private fun notification(text: String): Notification {
        val nm = getSystemService(NotificationManager::class.java)
        nm.createNotificationChannel(NotificationChannel(CHANNEL, "Radio", NotificationManager.IMPORTANCE_LOW))
        val open = PendingIntent.getActivity(
            this, 0, Intent(this, MainActivity::class.java), PendingIntent.FLAG_IMMUTABLE,
        )
        val stop = PendingIntent.getService(
            this, 1, Intent(this, RadioService::class.java).setAction(ACTION_STOP), PendingIntent.FLAG_IMMUTABLE,
        )
        return Notification.Builder(this, CHANNEL)
            .setSmallIcon(R.drawable.ic_launcher)
            .setContentTitle("droidHPSDR")
            .setContentText(text)
            .setContentIntent(open)
            .setOngoing(true)
            .addAction(Notification.Action.Builder(null, "Stop", stop).build())
            .build()
    }

    companion object {
        const val CHANNEL = "radio"
        const val WIRED_HL2 = "169.254.19.221"
        const val ACTION_STOP = "io.github.randal007.droidhpsdr.STOP"

        fun start(ctx: Context) = ctx.startForegroundService(Intent(ctx, RadioService::class.java))
        fun stop(ctx: Context) = ctx.startService(Intent(ctx, RadioService::class.java).setAction(ACTION_STOP))
    }
}
