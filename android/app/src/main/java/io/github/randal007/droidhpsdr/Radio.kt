// droidHPSDR - GPL v3 or later.
package io.github.randal007.droidhpsdr

import android.content.Context
import android.content.SharedPreferences
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlin.math.roundToLong

/** Upstream deskHPSDR mode numbers (src/mode.h). */
object Modes {
    const val LSB = 0
    const val USB = 1
    const val CWL = 3
    const val CWU = 4
    const val FM = 5
    const val AM = 6
    const val DIGU = 7
    const val DIGL = 9
    const val SAM = 10

    fun name(m: Int) = when (m) {
        0 -> "LSB"; 1 -> "USB"; 2 -> "DSB"; 3 -> "CWL"; 4 -> "CWU"; 5 -> "FM"; 6 -> "AM"
        7 -> "USB-D"; 8 -> "SPEC"; 9 -> "LSB-D"; 10 -> "SAM"; 11 -> "DRM"; else -> "?"
    }

    /** The modes offered in the picker, in deskHPSDR's order of use. */
    val picker = listOf(USB, LSB, CWU, CWL, DIGU, DIGL, AM, SAM, FM)

    /**
     * How far the passband edges may go, per mode: deskHPSDR's own filter
     * tables (src/filter.c) are the yardstick, so no edge goes past the widest
     * preset upstream has for that mode. SSB keeps a 50 Hz low edge for ESSB.
     */
    data class Limits(val minLo: Int, val maxHi: Int, val minWidth: Int)

    fun limits(m: Int): Limits? = when (m) {
        USB -> Limits(50, 5150, 100)
        LSB -> Limits(-5150, -50, 100)
        DIGU -> Limits(0, 5000, 50)
        DIGL -> Limits(-5000, 0, 50)
        CWU, CWL -> Limits(-500, 500, 25)
        AM, SAM, 2 -> Limits(-8000, 8000, 1000)
        else -> null   // FM (set by deviation) and the rest: no edge dragging
    }

    /** The filter a mode starts with until the user changes it. */
    fun defaultFilter(m: Int): Pair<Int, Int>? = when (m) {
        USB -> 150 to 2850
        LSB -> -2850 to -150
        DIGU -> 0 to 3000
        DIGL -> -3000 to 0
        CWU, CWL -> -250 to 250
        AM, SAM, 2 -> -4000 to 4000
        else -> null
    }
}

enum class Phase { IDLE, PREPARING, CONNECTING, RUNNING, ERROR, DEMO }

data class Status(
    val phase: Phase = Phase.IDLE,
    val message: String = "",
    val network: String = "",
    val mode: Int = Modes.DIGU,
    val filterLow: Int = 0,
    val filterHigh: Int = 3000,
    val sampleRate: Int = 192000,
    val meterDbm: Double = -140.0,
    val lostPackets: Long = 0,
    val adcOverloads: Int = 0,
    val noiseFloor: Double = -125.0,
)

/**
 * The app's view of the radio. The frequency is owned here (the UI tunes
 * faster than the radio reports back); everything else is read from the
 * native core while it runs.
 */
object Radio {
    const val PIXELS = 2048
    const val FPS = 30
    const val SAMPLE_RATE = 384000   // default; the HL2 does 48/96/192/384 kHz

    val status = MutableStateFlow(Status())
    val freq = MutableStateFlow(14_074_000L)
    val span = MutableStateFlow(48_000.0)       // Hz across the screen
    val style = MutableStateFlow(0)              // 3D style, see gl_render.c
    val volumeDb = MutableStateFlow(-12.0)       // -40..0
    val lnaDb = MutableStateFlow(20.0)           // -12..48
    val step = MutableStateFlow(10)              // Hz
    val region = MutableStateFlow(2)             // IARU region for the band bar
    val phoneAudio = MutableStateFlow(false)     // also play on the phone

    // display (deskHPSDR's Display menu, the phone's way)
    val flat = MutableStateFlow(false)           // 2D spectrum instead of 3D
    val wfTheme = MutableStateFlow(0)            // waterfall colours, see WF_THEMES
    val depthRows = MutableStateFlow(80)         // 3D history depth
    val peakHold = MutableStateFlow(true)        // 2D peak-hold line
    // peak labels: deskHPSDR's Display menu settings and defaults, except
    // "as S-meter values", which Randal wants on by default
    val peakLabels = MutableStateFlow(3)         // Number of Peaks to label, 0 = off
    val labelsPassband = MutableStateFlow(false) // Peak Labels in Passband Only
    val labelsSmeter = MutableStateFlow(true)    // Peak Labels as S-Meter values
    val labelsHideNoise = MutableStateFlow(true) // Hide Peaks Below Noise Floor
    val labelsPercentile = MutableStateFlow(80)  // Panadapter Noise Floor Percentile
    val labelsIgnore = MutableStateFlow(20)      // Panadapter Ignore Adjacent Peaks (width divider)
    val avgMs = MutableStateFlow(250)            // 2D averaging, deskHPSDR's Av. Time (ms)
    val peaks = MutableStateFlow<List<Pair<Double, Double>>>(emptyList())   // (Hz, dBm)
    val split = MutableStateFlow(0.5f)           // share of the height for the 3D/2D view
    val fps = MutableStateFlow(FPS)              // spectrum rows per second
    val window = MutableStateFlow(5)             // FFT window: 5 = Kaiser, upstream's default
    val detector = MutableStateFlow(1)           // 1 = Average, upstream's default

    /** The display's smoothed noise floor (dBm), written by the renderer, read by the labels. */
    @Volatile var displayFloor = -125.0

    // where the radio is: wired (APIPA, see CLAUDE.md) and on WiFi (its DHCP address)
    /** The radio's sample rate: how wide a band it hears at once (more to scroll into). */
    val sampleRate = MutableStateFlow(SAMPLE_RATE)

    val wiredIp = MutableStateFlow("169.254.19.221")
    val wifiIp = MutableStateFlow("")

    private lateinit var prefs: SharedPreferences
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Default)
    private var poll: Job? = null

    fun init(ctx: Context) {
        if (::prefs.isInitialized) return
        prefs = ctx.getSharedPreferences("radio", Context.MODE_PRIVATE)
        freq.value = prefs.getLong("freq", 14_074_000L)
        span.value = prefs.getFloat("span", 48_000f).toDouble()
        style.value = prefs.getInt("style", 0)
        volumeDb.value = prefs.getFloat("volume", -12f).toDouble()
        lnaDb.value = prefs.getFloat("lna", 20f).toDouble()
        step.value = prefs.getInt("step", 10)
        region.value = prefs.getInt("region", 2)
        rowDelayMs.value = prefs.getInt("rowDelay", 90)
        phoneAudio.value = prefs.getBoolean("phoneAudio", false)
        flat.value = prefs.getBoolean("flat", false)
        wfTheme.value = prefs.getInt("wfTheme", 0)
        depthRows.value = prefs.getInt("depthRows", 80)
        peakHold.value = prefs.getBoolean("peakHold", true)
        peakLabels.value = prefs.getInt("peakLabels3", 3)
        labelsPassband.value = prefs.getBoolean("labelsPassband", false)
        labelsSmeter.value = prefs.getBoolean("labelsSmeter", true)
        labelsHideNoise.value = prefs.getBoolean("labelsHideNoise", true)
        labelsPercentile.value = prefs.getInt("labelsPercentile", 80)
        labelsIgnore.value = prefs.getInt("labelsIgnore", 20)
        avgMs.value = prefs.getInt("avgMs", 250)
        split.value = prefs.getFloat("split", 0.5f)
        fps.value = prefs.getInt("fps", FPS)
        window.value = prefs.getInt("window", 5)
        detector.value = prefs.getInt("detector", 1)
        sampleRate.value = prefs.getInt("sampleRate", SAMPLE_RATE)
        wiredIp.value = prefs.getString("wiredIp", "169.254.19.221") ?: "169.254.19.221"
        wifiIp.value = prefs.getString("wifiIp", "") ?: ""
        status.value = status.value.copy(mode = prefs.getInt("mode", Modes.DIGU))
    }

    val running get() = status.value.phase == Phase.RUNNING

    fun tune(hz: Double) {
        val st = step.value.coerceAtLeast(1)
        val f = ((hz / st).roundToLong() * st).coerceIn(100_000L, 61_440_000L)
        if (f == freq.value) return
        freq.value = f
        if (running) Native.setFrequency(f)
        if (status.value.phase == Phase.DEMO) Native.demoSetFrequency(f)
        prefs.edit().putLong("freq", f).apply()
    }

    fun setMode(m: Int) {
        status.value = status.value.copy(mode = m)
        if (running) Native.setMode(m)
        prefs.edit().putInt("mode", m).apply()
        applyModeFilter()
    }

    /** The filter this mode had last (deskHPSDR keeps one per mode too), or its default. */
    fun applyModeFilter() {
        val m = status.value.mode
        val saved = prefs.getString("filter.$m", null)?.split(",")?.mapNotNull { it.toIntOrNull() }
        val f = if (saved != null && saved.size == 2) saved[0] to saved[1] else Modes.defaultFilter(m) ?: return
        setFilter(f.first, f.second)
    }

    /** Edges are kept inside the mode's limits (Modes.limits); FM has none to set. */
    fun setFilter(low: Int, high: Int) {
        val m = status.value.mode
        val lim = Modes.limits(m) ?: return
        var lo = low.coerceIn(lim.minLo, lim.maxHi - lim.minWidth)
        var hi = high.coerceIn(lim.minLo + lim.minWidth, lim.maxHi)
        if (hi - lo < lim.minWidth) {
            if (low != status.value.filterLow) lo = hi - lim.minWidth else hi = lo + lim.minWidth
        }
        status.value = status.value.copy(filterLow = lo, filterHigh = hi)
        if (running) Native.setFilter(lo, hi)
        prefs.edit().putString("filter.$m", "$lo,$hi").apply()
    }

    /** Test setting: how old a spectrum row's data is (ms), so tuning doesn't smear. */
    val rowDelayMs = MutableStateFlow(90)

    fun setRowDelay(ms: Int) {
        rowDelayMs.value = ms.coerceIn(0, 400)
        if (running) Native.setRowDelayMs(rowDelayMs.value)
        prefs.edit().putInt("rowDelay", rowDelayMs.value).apply()
    }

    fun setVolume(db: Double) {
        volumeDb.value = db.coerceIn(-40.0, 0.0)
        if (running) Native.setVolumeDb(volumeDb.value)
        prefs.edit().putFloat("volume", volumeDb.value.toFloat()).apply()
    }

    fun setLna(db: Double) {
        lnaDb.value = db.coerceIn(-12.0, 48.0)
        if (running) Native.setLnaDb(lnaDb.value)
        prefs.edit().putFloat("lna", lnaDb.value.toFloat()).apply()
    }

    fun setSpan(hz: Double) {
        span.value = hz.coerceIn(1_500.0, status.value.sampleRate * 0.95)
        prefs.edit().putFloat("span", span.value.toFloat()).apply()
        applyZoom()
    }

    /**
     * Upstream's zoom to match the span: about PIXELS analyzer points across
     * the screen (sharp), while display_bridge.c keeps up to 8192 of them per
     * row, so ~4 screens of real data to drag into instead of black.
     */
    fun applyZoom() {
        if (!running) return
        val z = (status.value.sampleRate / span.value).toInt().coerceIn(1, 16)
        Native.setZoom(z)
    }

    fun setStyle(s: Int) {
        style.value = ((s % 4) + 4) % 4
        prefs.edit().putInt("style", style.value).apply()
    }

    fun setStep(s: Int) {
        step.value = s
        prefs.edit().putInt("step", s).apply()
    }

    fun setPhoneAudio(on: Boolean) {
        phoneAudio.value = on
        if (running) Native.setPhoneAudio(on)
        prefs.edit().putBoolean("phoneAudio", on).apply()
    }

    fun setFlat(on: Boolean) { flat.value = on; prefs.edit().putBoolean("flat", on).apply() }
    fun setWfTheme(t: Int) { wfTheme.value = t; prefs.edit().putInt("wfTheme", t).apply() }
    fun setDepthRows(n: Int) { depthRows.value = n; prefs.edit().putInt("depthRows", n).apply() }
    fun setPeakHold(on: Boolean) { peakHold.value = on; prefs.edit().putBoolean("peakHold", on).apply() }
    fun setPeakLabels(n: Int) { peakLabels.value = n; prefs.edit().putInt("peakLabels3", n).apply() }
    fun setLabelsPassband(on: Boolean) { labelsPassband.value = on; prefs.edit().putBoolean("labelsPassband", on).apply() }
    fun setLabelsSmeter(on: Boolean) { labelsSmeter.value = on; prefs.edit().putBoolean("labelsSmeter", on).apply() }
    fun setLabelsHideNoise(on: Boolean) { labelsHideNoise.value = on; prefs.edit().putBoolean("labelsHideNoise", on).apply() }
    fun setLabelsPercentile(p: Int) { labelsPercentile.value = p; prefs.edit().putInt("labelsPercentile", p).apply() }
    fun setLabelsIgnore(d: Int) { labelsIgnore.value = d; prefs.edit().putInt("labelsIgnore", d).apply() }
    fun setAvgMs(ms: Int) { avgMs.value = ms; prefs.edit().putInt("avgMs", ms).apply() }

    /** Rows the 2D view averages: Av. Time at the current frame rate. */
    fun avgRows(): Int = (avgMs.value * fps.value / 1000.0).toInt().coerceIn(1, 32)
    fun setWiredIp(ip: String) { wiredIp.value = ip.trim(); prefs.edit().putString("wiredIp", ip.trim()).apply() }
    fun setWifiIp(ip: String) { wifiIp.value = ip.trim(); prefs.edit().putString("wifiIp", ip.trim()).apply() }

    /** Saved; RadioService restarts the stream at the new rate (as radio_change_sample_rate does). */
    fun setSampleRate(r: Int) { sampleRate.value = r; prefs.edit().putInt("sampleRate", r).apply() }

    fun setSplit(f: Float) { split.value = f; prefs.edit().putFloat("split", f).apply() }
    fun setDisplay(fps: Int = this.fps.value, window: Int = this.window.value, detector: Int = this.detector.value) {
        this.fps.value = fps; this.window.value = window; this.detector.value = detector
        prefs.edit().putInt("fps", fps).putInt("window", window).putInt("detector", detector).apply()
        if (running) Native.setDisplay(fps, window, detector)
    }

    fun setRegion(r: Int) {
        region.value = r
        prefs.edit().putInt("region", r).apply()
    }

    fun setPhase(p: Phase, msg: String = "", network: String? = null) {
        if (p != Phase.DEMO && status.value.phase == Phase.DEMO) Native.demoStop()
        status.value = status.value.copy(phase = p, message = msg, network = network ?: status.value.network)
        if (p == Phase.RUNNING || p == Phase.DEMO) startPolling() else poll?.cancel()
    }

    /** The made-up band, so the display can be worked on with the radio off. */
    fun startDemo() {
        if (running) return
        Native.demoSetFrequency(freq.value)
        Native.demoStart()
        setPhase(Phase.DEMO, "made-up 20 m band, no radio")
        applyModeFilter()
    }

    fun stopDemo() {
        if (status.value.phase == Phase.DEMO) setPhase(Phase.IDLE)
    }

    /** Peak labels, deskHPSDR's way (display_bridge.c droid_peaks). */
    private fun pollPeaks(buf: DoubleArray) {
        val n = peakLabels.value
        if (n <= 0) { peaks.value = emptyList(); return }
        val f = freq.value.toDouble()
        val st = status.value
        val lo = f - span.value / 2
        val hi = f + span.value / 2
        val (pl, ph) = if (labelsPassband.value) f + st.filterLow to f + st.filterHigh else lo to hi
        val got = Native.peaks(lo, hi, pl, ph, n, labelsIgnore.value, labelsHideNoise.value,
            labelsPercentile.value.toDouble(), buf)
        peaks.value = List(got) { buf[2 * it] to buf[2 * it + 1] }
    }

    private fun startPolling() {
        poll?.cancel()
        poll = scope.launch {
            val a = DoubleArray(13)
            val pk = DoubleArray(64)
            var tick = 0
            while (isActive) {
                Native.status(a)
                if (status.value.phase == Phase.DEMO) {
                    status.value = status.value.copy(meterDbm = a[6], noiseFloor = a[11], sampleRate = 192000)
                } else if (a[0] != 0.0) {
                    status.value = status.value.copy(
                        mode = a[2].toInt(), filterLow = a[3].toInt(), filterHigh = a[4].toInt(),
                        sampleRate = a[5].toInt(), meterDbm = a[6], lostPackets = a[7].toLong(),
                        adcOverloads = a[8].toInt(), noiseFloor = a[11],
                    )
                }
                if (++tick % 4 == 0) pollPeaks(pk)
                delay(50)
            }
        }
    }
}
