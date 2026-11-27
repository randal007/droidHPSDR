// droidHPSDR - GPL v3 or later.
package io.github.randal007.droidhpsdr

import android.Manifest
import android.content.Context
import android.content.SharedPreferences
import android.content.pm.PackageManager
import android.media.AudioDeviceInfo
import android.media.AudioManager
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.setValue
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalLifecycleOwner
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleEventObserver
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlin.math.roundToInt

/**
 * M7, transmit (step 1: SSB voice, drive, PTT, TUNE). The fields are numbered
 * as in droid_app.h (TX_* settings, TXS_* live state); the native side is
 * android/native/app/transmit.c around deskHPSDR's transmitter.c.
 *
 * Randal's picks (docs/M7-INVENTORY.md, pictures m7-round1): one PTT chip,
 * held to transmit; one TX chip that opens the transmit panel (M5b); the
 * meter card becomes the TX meter while on the air (A); TX and PA pages in
 * the menu; mic from the HL2+ jack, the phone or a Bluetooth headset; TX
 * timeout 2 min; VOX off at every start; "Transmit enabled" off until he
 * switches it on.
 */
object Tx {
    const val ENABLED = 0; const val TIMEOUT = 1; const val PTT = 2; const val TUNE = 3; const val DRIVE = 4
    const val TUNE_DRIVE = 5; const val TUNE_USE_DRIVE = 6; const val DRIVE_PER_BAND = 7; const val MIC_GAIN = 8
    const val MIC_BOOST = 9; const val MIC_LOCAL = 10; const val FILTER_LOW = 11; const val FILTER_HIGH = 12
    const val USE_RX_FILTER = 13; const val SWR_PROT = 14; const val SWR_ALARM = 15; const val OUT_OF_BAND = 16
    const val MUTE_RX = 17; const val PA = 18; const val PA_POWER = 19; const val PA_CAL0 = 20
    const val INHIBIT_EN = 44; const val XIT_ON = 45; const val XIT = 46; const val PA_FILE = 47
    const val ON_AIR = 48; const val FWD = 49; const val REV = 50; const val SWR = 51; const val ALC = 52
    const val MIC = 53; const val HIGH_SWR = 54; const val SECONDS = 55; const val TIMEOUTS = 56; const val REFUSED = 57
    const val RADIO_PTT = 58; const val UNDERRUN = 59; const val OVERRUN = 60; const val OUT_OF_BAND_NOW = 61
    const val MAX_W = 62; const val PICO = 63; const val INHIBITED = 64; const val PA_FILE_OK = 65; const val VOX_PEAK = 66
    const val VOX_REFUSED = 67; const val TX_MODE = 68
    /** the live fields, the radio's and never the UI's: [LIVE] until [LIVE_END] */
    private const val LIVE = ON_AIR
    private const val LIVE_END = 72

    // step 2, the TX audio chain (droid_app.h TXA_*); "per mode" ones are kept per mode by deskHPSDR
    const val COMP = 72; const val COMP_LEVEL = 73; const val LEV = 74; const val LEV_GAIN = 75; const val LEV_DECAY = 76
    const val CESSB = 77; const val PHROT = 78; const val PHROT_STAGES = 79; const val PHROT_FREQ = 80
    const val CFC = 81; const val CFC_EQ = 82; const val EQ = 83; const val CTFMODE = 84
    const val DEXP = 85; const val DEXP_TRIGGER = 86; const val DEXP_EXP = 87; const val DEXP_ATTACK = 88
    const val DEXP_RELEASE = 89; const val DEXP_HOLD = 90; const val DEXP_TAU = 91; const val DEXP_HYST = 92
    const val DEXP_FILTER = 93; const val DEXP_FILTER_LOW = 94; const val DEXP_FILTER_HIGH = 95
    const val VOX = 96; const val VOX_THRESH = 97; const val VOX_HANG = 98
    const val MON = 99; const val MON_GAIN = 100; const val MON_POST = 101; const val ADDGAIN = 102; const val ADDGAIN_GAIN = 103
    /** 13 each: [0] added gain / unused / Pre Comp / Post Gain, [1..12] the points */
    const val EQ_GAIN0 = 104; const val EQ_FREQ0 = 117; const val CFC_FREQ0 = 130; const val CFC_LVL0 = 143; const val CFC_POST0 = 156
    // step 4, PureSignal (droid_app.h TXP_* / TXPS_*, puresignal.c = deskHPSDR's ps_menu.c)
    const val PS_ENABLE = 169; const val PS_RESET = 170; const val PS_RESUME = 171; const val PS_TWOTONE = 172
    const val PS_NOISE = 173; const val PS_NOISE_LEVEL = 174; const val PS_AUTO = 175; const val PS_ATT = 176
    const val PS_MON = 177; const val PS_ONESHOT = 178; const val PS_TOLERANCE = 179; const val PS_DELAY = 180
    const val PS_SETPK = 181; const val PS_ZERO_WARN = 182
    /** live, the radio's: [PS_FEEDBACK] until [COUNT] */
    const val PS_FEEDBACK = 183; const val PS_COR_COUNT = 184; const val PS_SLN_CHECK = 185; const val PS_DG_COUNT = 186
    const val PS_CORRECTING = 187; const val PS_STATE = 188; const val PS_GETPK = 189; const val PS_WARN = 190
    const val PS_GEN_REFUSED = 191; const val PS_BAND = 192
    const val COUNT = 193
    /** the audio chain's fields: set only while the radio runs (they belong to the mode it is in), never kept for later */
    private const val AUDIO = COMP

    /** deskHPSDR's band numbers (band.h) of the HL2's HF bands, for the PA page */
    val PA_BANDS = listOf("160m" to 2, "80m" to 3, "60m" to 4, "40m" to 5, "30m" to 6, "20m" to 7, "17m" to 8,
        "15m" to 9, "12m" to 10, "10m" to 11, "6m" to 13)
    /** pa_power_list[] (radio.c) and the indexes the PA page offers */
    val METER_SCALES = listOf("1 W" to 0, "5 W" to 1, "10 W" to 2, "100 W" to 9)
    val TIMEOUTS_MIN = listOf(1, 2, 3, 5, 10, 0)
    val MIC_SOURCES = listOf("HL2+ jack", "Phone", "Bluetooth")

    val v = MutableStateFlow(DoubleArray(COUNT).also {
        it[TIMEOUT] = 120.0; it[DRIVE] = 50.0; it[TUNE_DRIVE] = 2.0; it[DRIVE_PER_BAND] = 1.0; it[SWR_ALARM] = 3.0
        it[FILTER_LOW] = 100.0; it[FILTER_HIGH] = 2900.0; it[MUTE_RX] = 1.0; it[PA] = 1.0; it[PA_POWER] = 1.0
        it[MAX_W] = 5.0; it[SWR] = 1.0; it[ALC] = -100.0; it[MIC] = -100.0; it[VOX_HANG] = 250.0; it[VOX_THRESH] = 0.001
        it[MON_GAIN] = -6.0; it[MON_POST] = 1.0
        it[PS_TOLERANCE] = 2.0; it[PS_DELAY] = 150.0; it[PS_SETPK] = 0.24; it[PS_ZERO_WARN] = 1.0
        for (b in 0 until 24) it[PA_CAL0 + b] = 38.8
    })
    /** the safety switch (TX page): off until Randal turns it on; kept across starts */
    val enabled = MutableStateFlow(false)
    /** 0 HL2+ jack, 1 phone, 2 Bluetooth */
    val micSource = MutableStateFlow(0)
    /** a message for the screen (refusals, the timeout), with when it was set */
    val note = MutableStateFlow("" to 0L)
    /** PureSignal was switched on with TX ATT 0 (deskHPSDR's warning dialog) */
    val psWarn = MutableStateFlow(false)

    private lateinit var prefs: SharedPreferences
    private lateinit var appCtx: Context
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Default)
    @Volatile private var lastSetMs = 0L
    /** settings changed while the radio was stopped: sent at the next start (code review) */
    private val pending = mutableSetOf<Int>()
    private var pttJob: Job? = null
    /** the finger is down; checked and sent under [pttLock], so a repeat can't follow the release */
    private var pttHeld = false
    private val pttLock = Any()
    private var lastTimeouts = 0
    private var lastVoxRefused = 0
    private var lastPsWarn = 0
    private var lastGenRefused = 0
    /** VOX as the radio last reported it: only its own switch-off (timeout, stop) overrides a fresh tap (code review) */
    private var nativeVox = 0.0
    private var lastRefusedAt = 0.0

    fun init(ctx: Context) {
        if (::prefs.isInitialized) return
        appCtx = ctx.applicationContext
        prefs = ctx.getSharedPreferences("tx", Context.MODE_PRIVATE)
        enabled.value = prefs.getBoolean("enabled", false)
        micSource.value = prefs.getInt("micSource", 0)
    }

    private val running get() = Radio.running && !Radio.playback.value

    fun set(field: Int, value: Double) {
        if (field >= AUDIO && !running) return   // the drawer says "start the radio"
        val a = v.value.copyOf()
        a[field] = value
        v.value = a
        lastSetMs = System.currentTimeMillis()
        if (running) Native.txSet(field, value)
        // events and the VFO's XIT are never kept for later: they'd land on the radio at the next start
        else if (field != PTT && field != TUNE && field != PA_FILE && field != XIT && field != XIT_ON && field < LIVE)   // not AUDIO either
            synchronized(pending) { pending.add(field) }
    }

    fun setEnabled(on: Boolean) {
        enabled.value = on
        prefs.edit().putBoolean("enabled", on).apply()
        if (running) Native.txSet(ENABLED, if (on) 1.0 else 0.0)
    }

    /** After every start: the safety switch and the mic (the rest comes back from deskHPSDR's props file). */
    fun apply() {
        Native.txSet(ENABLED, if (enabled.value) 1.0 else 0.0)
        val a = v.value
        synchronized(pending) { pending.forEach { Native.txSet(it, a[it]) }; pending.clear() }
        lastSetMs = System.currentTimeMillis()   // the poll doesn't overwrite them with the props' values meanwhile
        applyMic()
    }

    /** The radio stopped (service): no Bluetooth call mode left behind, no on-air state on the screen. */
    fun stopped() {
        runCatching { appCtx.getSystemService(AudioManager::class.java).clearCommunicationDevice() }
        clearLive()
    }

    private fun clearLive() {
        val a = v.value.copyOf()
        for (i in LIVE until LIVE_END) if (i != MAX_W && i != TIMEOUTS) a[i] = 0.0
        a[PTT] = 0.0; a[TUNE] = 0.0; a[SWR] = 1.0; a[ALC] = -100.0; a[MIC] = -100.0; a[VOX] = 0.0   // the radio switches VOX off when it stops
        // PureSignal's status, and Two Tone / Noise (stopping ends them); the counters stay for the next run's compare
        for (i in PS_FEEDBACK..PS_GETPK) a[i] = 0.0
        a[PS_TWOTONE] = 0.0; a[PS_NOISE] = 0.0
        if (!a.contentEquals(v.value)) v.value = a
    }

    // ---- the microphone: HL2+ jack, the phone's, or a Bluetooth headset ----

    fun hasMicPermission(ctx: Context) =
        ctx.checkSelfPermission(Manifest.permission.RECORD_AUDIO) == PackageManager.PERMISSION_GRANTED

    fun setMicSource(src: Int) {
        micSource.value = src
        prefs.edit().putInt("micSource", src).apply()
        applyMic()
    }

    private fun applyMic() {
        val am = appCtx.getSystemService(AudioManager::class.java)
        val src = micSource.value
        if (src == 0 || !hasMicPermission(appCtx)) {
            am.clearCommunicationDevice()
            if (running) Native.txSet(MIC_LOCAL, 0.0)
            if (src != 0) note.value = "The phone mic needs the microphone permission" to System.currentTimeMillis()
            RadioService.micInUse(appCtx)   // the service drops the microphone type again
            return
        }
        var id = 0
        if (src == 2) {
            // a Bluetooth headset's mic: make it Android's communication device (SCO or LE audio)
            val bt = am.availableCommunicationDevices.firstOrNull {
                it.type == AudioDeviceInfo.TYPE_BLUETOOTH_SCO || it.type == AudioDeviceInfo.TYPE_BLE_HEADSET
            }
            if (bt == null || !am.setCommunicationDevice(bt)) {
                note.value = "No Bluetooth headset connected: using the phone's mic" to System.currentTimeMillis()
            } else {
                id = am.getDevices(AudioManager.GET_DEVICES_INPUTS).firstOrNull {
                    (it.type == AudioDeviceInfo.TYPE_BLUETOOTH_SCO || it.type == AudioDeviceInfo.TYPE_BLE_HEADSET) &&
                        it.productName == bt.productName
                }?.id ?: 0
            }
        } else {
            am.clearCommunicationDevice()
            id = am.getDevices(AudioManager.GET_DEVICES_INPUTS).firstOrNull { it.type == AudioDeviceInfo.TYPE_BUILTIN_MIC }?.id ?: 0
        }
        Native.micRoute(id, src == 2 && id != 0)
        if (running) {
            // a new device: close and open the input again
            Native.txSet(MIC_LOCAL, 0.0)
            Native.txSet(MIC_LOCAL, 1.0)
        }
        RadioService.micInUse(appCtx)   // Android stops a background mic unless the service says it uses one
    }

    // ---- PTT: held. The chip repeats it every 250 ms while the finger is down;
    //      the native side goes back to receive if the repeats stop (transmit.c) ----

    fun pttDown() {
        if (pttJob?.isActive == true) return
        synchronized(pttLock) { pttHeld = true }
        pttJob = scope.launch {
            while (isActive) {
                synchronized(pttLock) { if (pttHeld && running) Native.txSet(PTT, 1.0) }
                delay(250)
            }
        }
    }

    fun pttUp() {
        // the release goes after any repeat already on its way (the native queue keeps PTT events in order)
        synchronized(pttLock) {
            val was = pttHeld
            pttHeld = false
            if (was && running) Native.txSet(PTT, 0.0)
        }
        pttJob?.cancel()
        pttJob = null
    }

    fun tune(on: Boolean) = set(TUNE, if (on) 1.0 else 0.0)

    /** Radio's poll loop (every 50 ms while running). */
    fun poll(buf: DoubleArray) {
        if (!running) { clearLive(); return }
        if (Native.txGet(buf) != COUNT) return
        val fresh = System.currentTimeMillis() - lastSetMs > 800
        val a = if (fresh) buf.copyOf() else v.value.copyOf().also {
            for (i in LIVE until LIVE_END) it[i] = buf[i]
            it[PTT] = buf[PTT]; it[TUNE] = buf[TUNE]
            if (nativeVox != 0.0 && buf[VOX] == 0.0) it[VOX] = 0.0   // the timeout switched VOX off
            for (i in PS_FEEDBACK until COUNT) it[i] = buf[i]
            if (buf[PS_AUTO] != 0.0) it[PS_ATT] = buf[PS_ATT]   // Auto Attenuate moves it
        }
        nativeVox = buf[VOX]
        val vr = a[VOX_REFUSED].toInt()
        if (vr > lastVoxRefused) note.value = "Out of band: VOX can't key the radio here" to System.currentTimeMillis()
        lastVoxRefused = vr
        val pw = a[PS_WARN].toInt()
        if (pw > lastPsWarn && a[PS_ZERO_WARN] != 0.0) psWarn.value = true
        lastPsWarn = pw
        val gr = a[PS_GEN_REFUSED].toInt()
        if (gr > lastGenRefused) note.value = refusalOf(a[REFUSED].toInt()).ifEmpty { "Two Tone / Noise can't transmit now" } to
            System.currentTimeMillis()
        lastGenRefused = gr
        val t = a[TIMEOUTS].toInt()
        if (t > lastTimeouts) note.value = "TX timeout (${timeoutLabel(a[TIMEOUT])}): back to receive" +
            (if (v.value[VOX] != 0.0) ", VOX off" else "") to System.currentTimeMillis()
        lastTimeouts = t
        if (!a.contentEquals(v.value)) v.value = a
    }

    /** Why the last PTT/TUNE didn't transmit (transmit.c TXR_*); read once per press. */
    fun refusal(): String = refusalOf(v.value[REFUSED].toInt())

    private fun refusalOf(r: Int): String = when (r) {
        1 -> "Transmit is off: turn it on in ☰ → TX"
        2 -> "External TX inhibit input (IO1) is active: see ☰ → TX"
        3 -> "CW transmit comes with M7 step 3: switch to SSB"
        4 -> "Out of band: no transmit here"
        5 -> "TX timeout: let go of PTT, then press again"
        else -> ""
    }

    fun timeoutLabel(s: Double) = if (s <= 0) "off" else "${(s / 60).roundToInt()} min"
    /** deskHPSDR's drive slider on an HL2 reads watts as drive / 20 (sliders.c). */
    fun watts(drive: Double) = drive / 20.0
}

// ---------------------------------------------------------------- the PTT chip

/** Hold to transmit (Randal's pick): ON AIR with the time left before the timeout. */
@Composable
internal fun PttChip(modifier: Modifier = Modifier) {
    val v by Tx.v.collectAsState()
    val enabled by Tx.enabled.collectAsState()
    val onAir = v[Tx.ON_AIR] != 0.0
    val held = v[Tx.PTT] != 0.0 || v[Tx.RADIO_PTT] != 0.0
    val lifecycle = LocalLifecycleOwner.current.lifecycle
    val ui = rememberCoroutineScope()
    // the app going to the background with the finger down, or the chip leaving the screen: release
    DisposableEffect(lifecycle) {
        val obs = LifecycleEventObserver { _, e -> if (e == Lifecycle.Event.ON_PAUSE) Tx.pttUp() }
        lifecycle.addObserver(obs)
        onDispose { lifecycle.removeObserver(obs); Tx.pttUp() }
    }
    val shape = RoundedCornerShape(13.dp)
    val base = modifier.height(46.dp).width(132.dp).clip(shape)
    val look = when {
        onAir -> base.background(Color(0xEBFF4D4D)).border(1.dp, Color(0xCCFFC8C8), shape)
        !enabled -> base.glass(13)
        else -> base.glass(13).border(1.5.dp, Color(0xCCFF4D4D), shape)
    }
    Column(
        look.pointerInput(Unit) {
            detectTapGestures(onPress = {
                Tx.pttDown()
                // refused (transmit off, out of band, CW, timeout): say why once the radio has answered
                val check = ui.launch {
                    delay(400)
                    if (Tx.v.value[Tx.ON_AIR] == 0.0) Tx.refusal().takeIf { it.isNotEmpty() }?.let { Tx.note.value = it to System.currentTimeMillis() }
                }
                tryAwaitRelease()
                Tx.pttUp()
                if (check.isActive) {   // a quick tap: still say why, after the radio answered
                    check.cancel()
                    ui.launch {
                        delay(300)
                        Tx.refusal().takeIf { it.isNotEmpty() }?.let { Tx.note.value = it to System.currentTimeMillis() }
                    }
                }
            })
        },
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.Center,
    ) {
        when {
            onAir -> {
                val limit = v[Tx.TIMEOUT]
                val left = if (limit > 0) (limit - v[Tx.SECONDS]).coerceAtLeast(0.0).toInt() else -1
                Text(if (v[Tx.TUNE] != 0.0) "TUNE" else if (held) "ON AIR" else "TX", color = Color.White, fontSize = 10.sp)
                Text(if (left >= 0) "%d:%02d".format(left / 60, left % 60) else "on", color = Color.White,
                    fontSize = 15.sp, fontWeight = FontWeight.SemiBold)
            }
            !enabled -> {
                Text("PTT", color = Dim, fontSize = 10.sp)
                Text("🔒 TX off", color = Dim, fontSize = 13.sp, fontWeight = FontWeight.Medium)
            }
            else -> {
                Text("HOLD", color = Color(0xFFFF9D9D), fontSize = 10.sp, letterSpacing = 0.4.sp)
                Text("PTT", color = Color(0xFFFF6B6B), fontSize = 17.sp, fontWeight = FontWeight.SemiBold, letterSpacing = 1.sp)
            }
        }
    }
}

// ---------------------------------------------------------------- the TX meter (pick A)

/** While on the air the meter card turns into this: thick PWR bar, SWR, ALC, MIC (Randal's X6100 pick). */
@Composable
internal fun TxMeterCard(modifier: Modifier) {
    val v by Tx.v.collectAsState()
    val maxW = v[Tx.MAX_W].coerceAtLeast(1.0)
    val fwd = v[Tx.FWD]
    Column(modifier.width(236.dp).glass().border(1.dp, Color(0x80FF4D4D), RoundedCornerShape(14.dp))
        .padding(horizontal = 11.dp, vertical = 9.dp)) {
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween, verticalAlignment = Alignment.CenterVertically) {
            Text(if (fwd < 10) "%.1f W".format(fwd) else "%.0f W".format(fwd), color = Txt, fontSize = 22.sp, fontWeight = FontWeight.Medium)
            Text("HL2 out · ${maxW.roundToInt()} W scale", color = Dim, fontSize = 10.5.sp)
        }
        Spacer(Modifier.height(4.dp))
        SegBar(fwd / maxW, 15, 24) { p -> if (p < 0.8) Grn else if (p < 0.92) Amb else Red }
        Row(Modifier.fillMaxWidth().padding(top = 2.dp), horizontalArrangement = Arrangement.SpaceBetween) {
            for (i in 0..5) {
                val w = maxW * i / 5
                val t = if (maxW < 5) "%.1f".format(w) else "%.0f".format(w)
                Text(if (i == 5) "$t W" else t, color = Dim, fontSize = 8.5.sp)
            }
        }
        val swr = v[Tx.SWR]
        MeterRow("SWR", "%.1f".format(swr), ((swr - 1.0) / 2.0).coerceIn(0.0, 1.0)) { p -> if (p < 0.4) Grn else if (p < 0.6) Amb else Red }
        val alc = v[Tx.ALC]
        MeterRow("ALC", if (alc < -90) "—" else "%.0f dB".format(alc), ((alc + 30.0) / 30.0).coerceIn(0.0, 1.0)) { p -> if (p < 0.9) Acc else Red }
        val mic = v[Tx.MIC]
        MeterRow("MIC", if (mic < -90) "—" else "%.0f dB".format(mic), ((mic + 40.0) / 40.0).coerceIn(0.0, 1.0)) { p -> if (p < 0.85) Acc else Red }
    }
}

@Composable
private fun MeterRow(name: String, value: String, frac: Double, col: (Double) -> Color) {
    Spacer(Modifier.height(6.dp))
    Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
        Text(name, color = Dim, fontSize = 9.5.sp, letterSpacing = 0.8.sp)
        Text(value, color = Txt, fontSize = 12.sp, fontWeight = FontWeight.Medium)
    }
    Spacer(Modifier.height(3.dp))
    SegBar(frac, 7, 24, col)
}

@Composable
private fun SegBar(frac: Double, h: Int, n: Int, col: (Double) -> Color) {
    Canvas(Modifier.fillMaxWidth().height(h.dp)) {
        val gap = 2.dp.toPx()
        val sw = (size.width - gap * (n - 1)) / n
        for (i in 0 until n) {
            val fill = (frac * n - i).coerceIn(0.0, 1.0).toFloat()
            val x0 = i * (sw + gap)
            drawRect(Color(0x14FFFFFF), Offset(x0, 0f), Size(sw, size.height))
            if (fill > 0f) drawRect(col(i / (n - 1.0)), Offset(x0, 0f), Size(sw * fill, size.height))
        }
    }
}

// ---------------------------------------------------------------- warnings (M8)

/** High SWR (drive dropped to 0), the timeout, a refused PTT: briefly, over the spectrum. */
@Composable
internal fun TxWarnings(modifier: Modifier) {
    val v by Tx.v.collectAsState()
    val note by Tx.note.collectAsState()
    var now by androidx.compose.runtime.remember { androidx.compose.runtime.mutableLongStateOf(System.currentTimeMillis()) }
    androidx.compose.runtime.LaunchedEffect(note) { while (true) { now = System.currentTimeMillis(); delay(500) } }
    Column(modifier, horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.spacedBy(6.dp)) {
        if (v[Tx.HIGH_SWR] != 0.0) {
            Text("HIGH SWR %.1f%s".format(v[Tx.SWR], if (v[Tx.SWR_PROT] != 0.0) " · drive set to 0" else ""),
                color = Color.White, fontSize = 12.sp, fontWeight = FontWeight.SemiBold,
                modifier = Modifier.clip(RoundedCornerShape(8.dp)).background(Color(0xEBDC2828)).padding(horizontal = 10.dp, vertical = 3.dp))
        }
        if (v[Tx.ON_AIR] != 0.0 && (v[Tx.UNDERRUN] != 0.0 || v[Tx.OVERRUN] != 0.0)) {
            Text(if (v[Tx.UNDERRUN] != 0.0) "TX underrun" else "TX overrun", color = Amb, fontSize = 11.sp,
                modifier = Modifier.glass(8).padding(horizontal = 8.dp, vertical = 2.dp))
        }
        if (note.first.isNotEmpty() && now - note.second < 4000) {
            Text(note.first, color = Txt, fontSize = 12.5.sp,
                modifier = Modifier.clip(RoundedCornerShape(12.dp)).background(Color(0xE61E1406))
                    .border(1.dp, Amb.copy(alpha = 0.7f), RoundedCornerShape(12.dp)).padding(horizontal = 14.dp, vertical = 8.dp))
        }
    }
}

// ---------------------------------------------------------------- the transmit panel (M5b)

/** From the TX chip: drive, mic, tune drive and TUNE, beside the spectrum like LEVELS (no dimming). */
@Composable
internal fun TxPanel(close: () -> Unit) {
    val v by Tx.v.collectAsState()
    val src by Tx.micSource.collectAsState()
    val ctx = androidx.compose.ui.platform.LocalContext.current
    val ui = rememberCoroutineScope()
    val askMic = androidx.activity.compose.rememberLauncherForActivityResult(
        androidx.activity.result.contract.ActivityResultContracts.RequestPermission(),
    ) { ok -> if (ok) Tx.setMicSource(Tx.micSource.value) else Tx.setMicSource(0) }
    Box(Modifier.fillMaxSize().clickable(interactionSource = null, indication = null, onClick = close)) {
        Column(
            Modifier.align(Alignment.TopEnd).padding(top = 8.dp, end = 8.dp, bottom = 64.dp).width(290.dp)
                .clip(RoundedCornerShape(16.dp)).background(Color(0xEB0A0E1A)).border(1.dp, Line, RoundedCornerShape(16.dp))
                .clickable(enabled = false) {}.verticalScroll(rememberScrollState()).padding(horizontal = 12.dp, vertical = 10.dp),
            verticalArrangement = Arrangement.spacedBy(4.dp),
        ) {
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween, verticalAlignment = Alignment.CenterVertically) {
                Text("Transmit", color = Txt, fontSize = 15.sp, fontWeight = FontWeight.Medium)
                Text("✕", color = Dim, fontSize = 16.sp, modifier = Modifier.clickable(onClick = close).padding(4.dp))
            }
            Setting("DRIVE", v[Tx.DRIVE], 0f..100f, 1.0, { "%.0f %% · ≈%.1f W".format(it, Tx.watts(it)) }) { Tx.set(Tx.DRIVE, it) }
            Toggle("Per band", v[Tx.DRIVE_PER_BAND] != 0.0, Modifier.fillMaxWidth()) { Tx.set(Tx.DRIVE_PER_BAND, if (it) 1.0 else 0.0) }
            Label("MICROPHONE")
            Seg(Tx.MIC_SOURCES, src, h = 30) { s ->
                if (s != 0 && !Tx.hasMicPermission(ctx)) { Tx.micSource.value = s; askMic.launch(android.Manifest.permission.RECORD_AUDIO) }
                else Tx.setMicSource(s)
            }
            Setting("MIC GAIN", v[Tx.MIC_GAIN], -12f..50f, 1.0, { "%+.0f dB".format(it) }) { Tx.set(Tx.MIC_GAIN, it) }
            Toggle("Mic Boost", v[Tx.MIC_BOOST] != 0.0, Modifier.fillMaxWidth(), enabled = src == 0, sub = "+20 dB, HL2+ jack") {
                Tx.set(Tx.MIC_BOOST, if (it) 1.0 else 0.0)
            }
            Spacer(Modifier.height(4.dp))
            Column(Modifier.fillMaxWidth().clip(RoundedCornerShape(12.dp)).border(1.dp, Color(0x99FFD27A), RoundedCornerShape(12.dp))
                .padding(8.dp), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                Label("TUNE")
                val useDrive = v[Tx.TUNE_USE_DRIVE] != 0.0
                Setting("TUNE DRIVE", v[Tx.TUNE_DRIVE], 0f..100f, 1.0, { "%.0f %% · ≈%.1f W".format(it, Tx.watts(it)) }, !useDrive) {
                    Tx.set(Tx.TUNE_DRIVE, it)
                }
                if (useDrive) Text("Follows DRIVE (TX page: Tune uses TX drive)", color = Dim, fontSize = 10.sp)
                val tuning = v[Tx.TUNE] != 0.0
                Box(
                    Modifier.fillMaxWidth().height(34.dp).clip(RoundedCornerShape(12.dp))
                        .background(if (tuning) Color(0xEBFF4D4D) else Color(0xFF0F1624))
                        .border(1.dp, Color(0x99FF4D4D), RoundedCornerShape(12.dp))
                        .clickable {
                            Tx.tune(!tuning)
                            if (!tuning) ui.launch {   // refused (transmit off, out of band, inhibit): say why
                                delay(400)
                                val why = Tx.refusal()
                                if (why.isNotEmpty() && Tx.v.value[Tx.TUNE] == 0.0) Tx.note.value = why to System.currentTimeMillis()
                            }
                        },
                    contentAlignment = Alignment.Center,
                ) { Text(if (tuning) "TUNE · tap to stop" else "TUNE", color = if (tuning) Color.White else Color(0xFFFF8A8A), fontSize = 13.sp, fontWeight = FontWeight.SemiBold) }
            }
        }
    }
}

// ---------------------------------------------------------------- ☰ → TX (M6) and ☰ → PA (M7)

@Composable
internal fun TxSheet() {
    val v by Tx.v.collectAsState()
    val enabled by Tx.enabled.collectAsState()
    SheetTitle("TX")
    Text("deskHPSDR's TX menu · step 1 (voice, drive, TUNE)", color = Dim, fontSize = 11.sp)
    Row(horizontalArrangement = Arrangement.spacedBy(14.dp)) {
        Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(6.dp)) {
            Label("TRANSMIT")
            Toggle("Transmit enabled", enabled, Modifier.fillMaxWidth(), sub = "safety") { Tx.setEnabled(it) }
            Toggle("HL2 5 W PA", v[Tx.PA] != 0.0, Modifier.fillMaxWidth()) { Tx.set(Tx.PA, if (it) 1.0 else 0.0) }
            Label("TX TIMEOUT (minutes)")
            val sel = Tx.TIMEOUTS_MIN.indexOfFirst { it * 60.0 == v[Tx.TIMEOUT] }.let { if (it < 0) 1 else it }
            Seg(Tx.TIMEOUTS_MIN.map { if (it == 0) "Off" else "$it" }, sel, h = 30) { Tx.set(Tx.TIMEOUT, Tx.TIMEOUTS_MIN[it] * 60.0) }
            Text("VOX is always off when the app starts.", color = Dim, fontSize = 10.5.sp)
        }
        Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(6.dp)) {
            Label("PROTECTION")
            Toggle("SWR protection", v[Tx.SWR_PROT] != 0.0, Modifier.fillMaxWidth(), sub = "drive → 0") { Tx.set(Tx.SWR_PROT, if (it) 1.0 else 0.0) }
            Setting("SWR ALARM AT", v[Tx.SWR_ALARM], 1.5f..5f, 0.1, { "%.1f".format(it) }) { Tx.set(Tx.SWR_ALARM, it) }
            Toggle("Transmit out of band", v[Tx.OUT_OF_BAND] != 0.0, Modifier.fillMaxWidth()) { Tx.set(Tx.OUT_OF_BAND, if (it) 1.0 else 0.0) }
            Toggle("Mute RX when TX", v[Tx.MUTE_RX] != 0.0, Modifier.fillMaxWidth()) { Tx.set(Tx.MUTE_RX, if (it) 1.0 else 0.0) }
            // radio_menu.c "Enable external TxInhibit Input": the HL2's IO1 input, active low (grounded = no TX)
            Toggle("External TX inhibit input", v[Tx.INHIBIT_EN] != 0.0, Modifier.fillMaxWidth(), sub = "IO1, grounded = no TX") {
                Tx.set(Tx.INHIBIT_EN, if (it) 1.0 else 0.0)
            }
            if (v[Tx.INHIBIT_EN] != 0.0 && v[Tx.INHIBITED] != 0.0)
                Text("TX inhibit input is active: transmit is blocked", color = Red, fontSize = 10.5.sp)
        }
        Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(6.dp)) {
            Label("TX FILTER")
            Setting("LOW", v[Tx.FILTER_LOW], 0f..1000f, 10.0, { "%.0f Hz".format(it) }) { Tx.set(Tx.FILTER_LOW, it) }
            Setting("HIGH", v[Tx.FILTER_HIGH], 1000f..5000f, 50.0, { "%.0f Hz".format(it) }) { Tx.set(Tx.FILTER_HIGH, it) }
            Toggle("TX uses RX filter", v[Tx.USE_RX_FILTER] != 0.0, Modifier.fillMaxWidth()) { Tx.set(Tx.USE_RX_FILTER, if (it) 1.0 else 0.0) }
            Label("TUNE")
            Setting("TUNE DRIVE", v[Tx.TUNE_DRIVE], 0f..100f, 1.0, { "%.0f %%".format(it) }) { Tx.set(Tx.TUNE_DRIVE, it) }
            Toggle("Tune uses TX drive", v[Tx.TUNE_USE_DRIVE] != 0.0, Modifier.fillMaxWidth()) { Tx.set(Tx.TUNE_USE_DRIVE, if (it) 1.0 else 0.0) }
        }
    }
}

@Composable
internal fun PaSheet() {
    val v by Tx.v.collectAsState()
    SheetTitle("PA")
    Text("deskHPSDR's PA menu", color = Dim, fontSize = 11.sp)
    Row(horizontalArrangement = Arrangement.spacedBy(16.dp)) {
        Column(Modifier.weight(1.4f), verticalArrangement = Arrangement.spacedBy(2.dp)) {
            Label("PA CALIBRATION PER BAND (dB · 38.8 = full 5 W)")
            Tx.PA_BANDS.chunked(2).forEach { pair ->
                Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                    pair.forEach { (name, b) ->
                        Box(Modifier.weight(1f)) {
                            Setting(name, v[Tx.PA_CAL0 + b], 38.8f..70f, 0.1, { "%.1f".format(it) }) { Tx.set(Tx.PA_CAL0 + b, it) }
                        }
                    }
                    if (pair.size == 1) Spacer(Modifier.weight(1f))
                }
            }
        }
        Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(6.dp)) {
            Label("METER FULL SCALE")
            val sel = Tx.METER_SCALES.indexOfFirst { it.second == v[Tx.PA_POWER].toInt() }.coerceAtLeast(0)
            Seg(Tx.METER_SCALES.map { it.first }, sel, h = 30) { Tx.set(Tx.PA_POWER, Tx.METER_SCALES[it].second.toDouble()) }
            Text("The HL2 measures its own output (the XPA125B's input), so 5 W. The amplifier shows its own power and SWR.",
                color = Dim, fontSize = 10.5.sp)
            SmallButton("Reset to 38.8", Modifier.fillMaxWidth()) { Tx.PA_BANDS.forEach { (_, b) -> Tx.set(Tx.PA_CAL0 + b, 38.8) } }
            // pa_menu.c Save / Import with band.c's own file (pa_calibration_<radio>.props, in the app's folder)
            val live = Radio.running && !Radio.playback.value
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                SmallButton("Save", Modifier.weight(1f)) {
                    if (live) Tx.set(Tx.PA_FILE, 1.0) else Tx.note.value = "Start the radio to save or load" to System.currentTimeMillis()
                }
                SmallButton("Load saved", Modifier.weight(1f)) {
                    if (live) Tx.set(Tx.PA_FILE, 2.0) else Tx.note.value = "Start the radio to save or load" to System.currentTimeMillis()
                }
            }
            when (v[Tx.PA_FILE_OK].toInt()) {
                1 -> Text("Saved", color = Grn, fontSize = 10.5.sp)
                -1 -> Text("Could not save the file", color = Red, fontSize = 10.5.sp)
                2 -> Text("Loaded", color = Grn, fontSize = 10.5.sp)
                -2 -> Text("No saved calibration for this radio", color = Amb, fontSize = 10.5.sp)
            }
        }
    }
}
