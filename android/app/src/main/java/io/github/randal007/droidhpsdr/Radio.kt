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

/**
 * M5 receive DSP: deskHPSDR's Noise menu, notch, squelch, RX EQ and DSP menu
 * as numbered fields, the same numbers as android/native/app/dsp.c. Units are
 * the ones deskHPSDR's menus show. NR, NB, ANF, SNB, their settings, the EQ
 * and the CW peak filter are kept per mode by the radio (deskHPSDR's way).
 */
object Dsp {
    const val NR = 0; const val NB = 1; const val ANF = 2; const val SNB = 3
    const val MNF = 4; const val MNF_FREQ = 5; const val MNF_WIDTH = 6; const val NR_POST_AGC = 7
    const val NR2_GAIN = 8; const val NR2_NPE = 9; const val NR2_AE = 10; const val NR2_POST = 11
    const val NR2_POST_LEVEL = 12; const val NR2_POST_RATE = 13; const val NR2_POST_FACTOR = 14
    const val NR2_POST_TAPER = 15; const val NR2_TRAINED_THRESH = 16; const val NR2_TRAINED_T2 = 17
    const val NB2_MODE = 18; const val NB_SLEW = 19; const val NB_LEAD = 20; const val NB_LAG = 21; const val NB_THRESH = 22
    const val NR4_REDUCTION = 23; const val NR4_SMOOTHING = 24; const val NR4_WHITENING = 25
    const val NR4_RESCALE = 26; const val NR4_THRESHOLD = 27
    const val NNR_MODEL = 28; const val NNR_FLOOR = 29
    const val SQL_ON = 30; const val SQL = 31; const val EQ_ON = 32; const val BINAURAL = 33; const val CW_PEAK = 34
    const val LOW_LATENCY = 35; const val FIR_SIZE = 36
    const val EQ_GAIN0 = 37   // 13 gains; [0] = the added frequency-independent gain
    const val EQ_FREQ0 = 50   // 13 frequencies; [0] unused
    // the AGC lines (M4 pick F): gain and hang threshold to set; the lines' levels (dBm) are read-only
    const val AGC_GAIN = 63; const val AGC_HANG_TH = 64; const val AGC_LINE = 65; const val AGC_HANG_LINE = 66
    // deskHPSDR's RX menu, the parts that matter on an HL2; LNA is read-only (auto gain moves it)
    const val AUTOGAIN = 67; const val AUTOGAIN_TIME = 68; const val DIGU_OFFSET = 69; const val DIGL_OFFSET = 70
    const val AUDIO_CHANNEL = 71; const val TEST_AUDIO = 72; const val RESERVE_ON = 73; const val RESERVE_MS = 74
    const val MUTE_RADIO = 75; const val RX_IQ_GAIN = 76; const val RX_IQ_PHASE = 77; const val LNA = 78
    // AGC Automatic (agc_menu.c) and the S-meter reading (meter_menu.c: 1 = Peak, 0 = Average)
    const val AGC_AUTO = 79; const val AGC_AUTO_OFFSET = 80; const val SMETER_PEAK = 81
    const val COUNT = 82

    val NR_NAMES = listOf("Off", "NR", "NR2", "NR3", "NR4", "NNR")
    val NB_NAMES = listOf("Off", "NB", "NB2")

    /** receiver.c's defaults (rx create), for before the radio has said otherwise. */
    val DEFAULTS = DoubleArray(COUNT).also { d ->
        d[MNF_WIDTH] = 80.0; d[NR2_GAIN] = 2.0; d[NR2_AE] = 1.0
        d[NR2_POST_LEVEL] = 15.0; d[NR2_POST_RATE] = 5.0; d[NR2_POST_FACTOR] = 15.0; d[NR2_POST_TAPER] = 12.0
        d[NR2_TRAINED_THRESH] = -0.5; d[NR2_TRAINED_T2] = 0.2
        d[NB_SLEW] = 0.01; d[NB_LEAD] = 0.01; d[NB_LAG] = 0.01; d[NB_THRESH] = 30.0
        d[NR4_REDUCTION] = 10.0; d[NR4_RESCALE] = 2.0; d[NR4_THRESHOLD] = -10.0; d[NNR_FLOOR] = -25.0
        d[FIR_SIZE] = 2048.0
        d[RESERVE_MS] = 100.0; d[LNA] = 20.0; d[AGC_AUTO_OFFSET] = -25.0
        d[AGC_GAIN] = 80.0; d[AGC_LINE] = -95.0; d[AGC_HANG_LINE] = -105.0   // levels: made up until the radio says
        listOf(0.0, 50.0, 100.0, 200.0, 500.0, 1000.0, 1500.0, 2000.0, 2500.0, 3000.0, 5000.0, 6000.0, 8000.0)
            .forEachIndexed { i, f -> d[EQ_FREQ0 + i] = f }
    }

    /** deskHPSDR: no noise reduction (NR, SNB) in the digital modes. */
    fun nrAllowed(mode: Int) = mode != Modes.DIGU && mode != Modes.DIGL
    /** receiver.c rx_anf_allowed(): no ANF in CW and the digital modes. */
    fun anfAllowed(mode: Int) = nrAllowed(mode) && mode != Modes.CWU && mode != Modes.CWL
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
    // M4: VFO state from deskHPSDR's vfo.c
    val freqB: Long = 0,
    val modeB: Int = Modes.USB,
    val ritOn: Boolean = false,
    val rit: Long = 0,
    val ctun: Boolean = false,
    val locked: Boolean = false,
    val split: Boolean = false,
    val band: Int = -1,
    val bandstack: Int = 0,
    val filter: Int = 0,
    val agc: Int = 3,
    val rows: Long = 0,
    val ritStep: Int = 10,
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
    val DEFAULT_CHIPS = listOf("NR", "BAND", "AF", "LNA", "PHONE", "FILTER", "AGC", "STEP", "SPAN", "LEVELS")

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

    /** M5: the receive DSP fields (Dsp), read from the radio while it runs. */
    val dsp = MutableStateFlow(Dsp.DEFAULTS.copyOf())
    @Volatile private var lastDspMs = 0L

    fun setDsp(field: Int, v: Double) {
        val a = dsp.value.copyOf()
        a[field] = v
        dsp.value = a
        lastDspMs = System.currentTimeMillis()
        if (running) Native.dspSet(field, v)
    }

    /** deskHPSDR's Meter Type: analog needle instead of the digital bar (M4 pick G). */
    val analogMeter = MutableStateFlow(false)
    fun setAnalogMeter(on: Boolean) { analogMeter.value = on; prefs.edit().putBoolean("analogMeter", on).apply() }

    /** The bottom row (deskHPSDR's Toolbar menu, the phone's way): chip ids, see ChipSpec. */
    val chips = MutableStateFlow(DEFAULT_CHIPS)

    fun setChips(l: List<String>) {
        chips.value = l
        prefs.edit().putString("chips", l.joinToString(",")).apply()
    }

    /** The display's smoothed noise floor (dBm), written by the renderer, read by the labels. */
    @Volatile var displayFloor = -125.0

    // where the radio is: wired (APIPA, see CLAUDE.md) and on WiFi (its DHCP address)
    /** The radio's sample rate: how wide a band it hears at once (more to scroll into). */
    val sampleRate = MutableStateFlow(SAMPLE_RATE)

    val wiredIp = MutableStateFlow("169.254.19.221")
    val wifiIp = MutableStateFlow("")

    private lateinit var prefs: SharedPreferences
    private lateinit var appCtx: Context
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Default)
    private var poll: Job? = null

    fun init(ctx: Context) {
        if (::prefs.isInitialized) return
        appCtx = ctx.applicationContext
        prefs = ctx.getSharedPreferences("radio", Context.MODE_PRIVATE)
        freq.value = prefs.getLong("freq", 14_074_000L)
        Disp.init(ctx)
        Hl2.init(ctx)
        // Screen → Save zoom level: off starts at the default span
        span.value = if (Disp.saveZoom.value) prefs.getFloat("span", 48_000f).toDouble() else 48_000.0
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
        analogMeter.value = prefs.getBoolean("analogMeter", false)
        prefs.getString("chips", null)?.let { c -> chips.value = c.split(",").filter { it.isNotBlank() } }
        // the LEVELS chip is new (it replaced the dB scale, E2): once, at the end of a saved row
        if (!prefs.getBoolean("chipsLevelsAdded", false)) {
            if ("LEVELS" !in chips.value) setChips(chips.value + "LEVELS")
            prefs.edit().putBoolean("chipsLevelsAdded", true).apply()
        }
    }

    val running get() = status.value.phase == Phase.RUNNING

    @Volatile private var lastTuneMs = 0L

    fun tune(hz: Double, exact: Boolean = false) {
        if (status.value.locked) return   // deskHPSDR's Lock: no tuning
        val st = if (exact) 1 else step.value.coerceAtLeast(1)
        val f = ((hz / st).roundToLong() * st).coerceIn(100_000L, 61_440_000L)
        if (f == freq.value) return
        lastTuneMs = System.currentTimeMillis()
        freq.value = f
        if (running) Native.setFrequency(f)
        if (status.value.phase == Phase.DEMO) Native.demoSetFrequency(f)
        if (!playback.value) prefs.edit().putLong("freq", f).apply()   // tuning inside a recording isn't the radio's frequency
    }

    /** deskHPSDR's mode change: the mode brings back its own filter, AGC and step (vfo.c). */
    fun setMode(m: Int) {
        status.value = status.value.copy(mode = m)
        if (running) Native.setMode(m) else applyModeFilter()
        prefs.edit().putInt("mode", m).apply()
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
        span.value = hz.coerceIn(1_500.0, rate() * 0.95)
        if (!playback.value) prefs.edit().putFloat("span", span.value.toFloat()).apply()   // a recording's width isn't the radio's
        applyZoom()
    }

    /**
     * Upstream's zoom to match the span: about PIXELS analyzer points across
     * the screen (sharp), while display_bridge.c keeps up to 8192 of them per
     * row, so ~4 screens of real data to drag into instead of black.
     */
    /** The sample rate the stream runs at: just after a restart the poll hasn't caught up yet. */
    private fun rate() = if (running && !playback.value) sampleRate.value.toDouble() else status.value.sampleRate.toDouble()

    fun applyZoom() {
        if (!running) return
        val z = (rate() / span.value).toInt().coerceIn(1, 16)
        Native.setZoom(z)
    }

    fun setStyle(s: Int) {
        style.value = ((s % 4) + 4) % 4
        prefs.edit().putInt("style", style.value).apply()
    }

    @Volatile private var lastStepMs = 0L

    fun setStep(s: Int) {
        lastStepMs = System.currentTimeMillis()
        step.value = s
        if (running) Native.step(s)
        prefs.edit().putInt("step", s).apply()
    }

    // ---- M4: deskHPSDR's VFO, band and memory operations (vfo.c, store.c) ----
    fun filterPreset(i: Int) { if (running) Native.filterPreset(i) }
    fun band(b: Int) { if (running) { lastTuneMs = 0; Native.band(b) } }
    fun bandstack(i: Int) { if (running) { lastTuneMs = 0; Native.bandstack(i) } }
    /** deskHPSDR's RIT/XIT step (Radio menu), 1, 10 or 100 Hz, kept per mode. */
    fun setRitStep(hz: Int) {
        if (running) Native.ritStep(hz)
        status.value = status.value.copy(ritStep = hz)
    }
    fun vfoOp(op: Int) { if (running) { lastTuneMs = 0; Native.vfoOp(op) } }
    fun rit(on: Boolean, hz: Long = status.value.rit) {
        status.value = status.value.copy(ritOn = on, rit = hz)
        if (running) Native.rit(on, hz)
    }
    fun ctun(on: Boolean) { if (running) Native.ctun(on) }
    fun lock(on: Boolean) { status.value = status.value.copy(locked = on); if (running) Native.lock(on) }
    fun agc(a: Int) { status.value = status.value.copy(agc = a); if (running) Native.agc(a) }
    fun memory(store: Boolean, slot: Int) { if (running) { lastTuneMs = 0; Native.memory(store, slot) } }
    fun save() { if (running) Native.save() }

    /** deskHPSDR's filter table for a mode: (title, low, high); index = filter number. */
    fun filters(mode: Int): List<Triple<String, Int, Int>> =
        Native.filters(mode).lines().filter { it.isNotBlank() }.map {
            val p = it.split('|'); Triple(p[0], p[1].toInt(), p[2].toInt())
        }

    data class UpBand(val index: Int, val title: String, val fmin: Long, val fmax: Long)

    /** deskHPSDR's bands the HL2 can tune (index = upstream band number). */
    fun bands(): List<UpBand> = Native.bands().lines().filter { it.isNotBlank() }.mapIndexed { i, l ->
        val p = l.split('|'); UpBand(i, p[0], p[1].toLong(), p[2].toLong())
    }.filter { it.fmin in 1..61_440_000L && it.title.isNotBlank() }

    data class StackEntry(val freq: Long, val mode: Int, val filter: Int)

    fun bandstackEntries(band: Int): Pair<Int, List<StackEntry>> {
        val l = Native.bandstackEntries(band).lines().filter { it.isNotBlank() }
        if (l.isEmpty()) return 0 to emptyList()
        return l[0].toInt() to l.drop(1).map { val p = it.split('|'); StackEntry(p[0].toLong(), p[1].toInt(), p[2].toInt()) }
    }

    fun memories(): List<Pair<Long, Int>> = Native.memories().lines().filter { it.isNotBlank() }.map {
        val p = it.split('|'); p[0].toLong() to p[1].toInt()
    }

    @Volatile private var lastPhoneMs = 0L

    fun setPhoneAudio(on: Boolean) {
        lastPhoneMs = System.currentTimeMillis()
        phoneAudio.value = on
        if (running) Native.setPhoneAudio(on)
        if (!playback.value) prefs.edit().putBoolean("phoneAudio", on).apply()   // the demo's PHONE isn't the radio's
    }

    fun setFlat(on: Boolean) { flat.value = on; prefs.edit().putBoolean("flat", on).apply() }
    fun setWfTheme(t: Int) { wfTheme.value = t; prefs.edit().putInt("wfTheme", t).apply() }
    fun setDepthRows(n: Int) { depthRows.value = n; prefs.edit().putInt("depthRows", n).apply() }
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
        if (running) Native.setDisplay(fps, window, detector, Disp.fftSize.value)
        Native.demoSetFps(fps)   // the demo band follows the speed too
    }

    fun setRegion(r: Int) {
        region.value = r
        prefs.edit().putInt("region", r).apply()
        Hl2.sendIaru()   // deskHPSDR's band stacks (80 m, 40 m) follow the IARU region
    }

    fun setPhase(p: Phase, msg: String = "", network: String? = null) {
        if (p != Phase.DEMO && status.value.phase == Phase.DEMO) Native.demoStop()
        status.value = status.value.copy(phase = p, message = msg, network = network ?: status.value.network)
        if (p == Phase.RUNNING || p == Phase.DEMO) startPolling() else poll?.cancel()
    }

    /** The demo band is playing an IQ recording (playback.c) instead of the radio. */
    val playback = MutableStateFlow(false)
    private var freqBeforePlayback = 0L
    private var spanBeforePlayback = 48_000.0
    private var statusBeforePlayback: Status? = null
    @Volatile private var playbackGen = 0
    private val playbackLock = Any()
    private var pbRows = -1L
    private var pbQuietSince = 0L

    /**
     * The demo's IQ recording: the first .wav in the app's own folder on the phone
     * (Android/data/io.github.randal007.droidhpsdr/files). Its centre comes from the
     * file name, as SDR++ and SDR# write it (..._14225000Hz_...).
     */
    fun iqFile(): java.io.File? =
        appCtx.getExternalFilesDir(null)?.listFiles { f -> f.name.endsWith(".wav", ignoreCase = true) }
            ?.sortedBy { it.name }?.firstOrNull()

    /** The demo band: the saved IQ recording if there is one, else the made-up band (display work, no sound). */
    fun startDemo() {
        if (running || status.value.phase == Phase.CONNECTING) return
        val file = iqFile()
        if (file != null) { startPlayback(file); return }
        Native.demoSetFrequency(freq.value)
        Native.demoSetFps(fps.value)
        Native.demoStart()
        setPhase(Phase.DEMO, "made-up 20 m band, no radio")
        applyModeFilter()
    }

    private fun startPlayback(file: java.io.File) {
        val centre = Regex("(\\d{6,9})Hz", RegexOption.IGNORE_CASE).find(file.name)?.groupValues?.get(1)?.toLong()
            ?: freq.value
        val mode = if (centre >= 10_000_000) Modes.USB else Modes.LSB
        // claimed before the file opens: a Stop or a radio start meanwhile has to find it
        freqBeforePlayback = freq.value
        spanBeforePlayback = span.value
        statusBeforePlayback = status.value
        playback.value = true
        val gen = ++playbackGen
        setPhase(Phase.CONNECTING, "IQ recording ${file.name}")
        scope.launch {
            val err = Native.startPlayback(file.absolutePath, centre, mode, PIXELS, fps.value, appCtx.filesDir.absolutePath)
            synchronized(playbackLock) {
                if (gen != playbackGen) return@launch   // stopped while opening: stopDemo stopped it
                if (err.isNotEmpty()) { playback.value = false; setPhase(Phase.ERROR, err); return@launch }
                startedPlayback(centre)
            }
        }
    }

    private fun startedPlayback(centre: Long) {
        run {
            pbRows = -1
            pbQuietSince = System.currentTimeMillis()
            lastTuneMs = 0
            freq.value = centre
            val a = DoubleArray(27)
            Native.status(a)   // the recording's own sample rate, before the span is fitted to it
            status.value = status.value.copy(sampleRate = a[5].toInt())
            Native.setVolumeDb(volumeDb.value)
            setPhase(Phase.RUNNING, "", "IQ recording")
            Native.setPhoneAudio(true)   // there's no HL2+ speaker: the recording plays on the phone
            phoneAudio.value = true      // the PHONE chip shows it; not saved (setPhoneAudio's pref stays the radio's)
            lastPhoneMs = System.currentTimeMillis()
            setSpan(span.value)
            setDisplay()
        }
    }

    fun stopDemo() {
        synchronized(playbackLock) {
            if (playback.value) {
                playbackGen++
                Native.stop()   // also waits for a recording that is still opening (native life lock)
                playback.value = false
                // the radio's frequency, span, mode/filter display and PHONE, as before the recording
                freq.value = freqBeforePlayback
                span.value = spanBeforePlayback
                statusBeforePlayback?.let { status.value = it }
                phoneAudio.value = prefs.getBoolean("phoneAudio", false)
                setPhase(Phase.IDLE)
                return
            }
        }
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
            val a = DoubleArray(27)
            val pk = DoubleArray(64)
            val dv = DoubleArray(Dsp.COUNT)
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
                        freqB = a[13].toLong(), modeB = a[14].toInt(), ritOn = a[15] != 0.0, rit = a[16].toLong(),
                        ctun = a[17] != 0.0, locked = a[18] != 0.0, split = a[19] != 0.0, band = a[21].toInt(),
                        bandstack = a[22].toInt(), filter = a[23].toInt(), agc = a[24].toInt(), rows = a[25].toLong(), ritStep = a[26].toInt(),
                    )
                    // The radio moves the frequency itself on band changes, A/B swaps and memory
                    // recalls: follow it, unless the user is tuning right now.
                    val nf = a[1].toLong()
                    if (nf != freq.value && System.currentTimeMillis() - lastTuneMs > 400) {
                        freq.value = nf
                        if (!playback.value) prefs.edit().putLong("freq", nf).apply()   // not a recording's
                    }
                    // phone audio can fail to open, or upstream's watchdog turns it off: the switch follows
                    // (not during a recording: it turns phone audio on itself, PHONE stays the user's)
                    val pa = a[12] != 0.0
                    if (!playback.value && pa != phoneAudio.value && System.currentTimeMillis() - lastPhoneMs > 1500) {
                        phoneAudio.value = pa
                    }
                    // each mode keeps its own step (vfo.c mode_settings): the STEP chip follows
                    val ns = a[20].toInt()
                    if (ns > 0 && ns != step.value && System.currentTimeMillis() - lastStepMs > 400) {
                        step.value = ns
                        prefs.edit().putInt("step", ns).apply()
                    }
                }
                // a recording that stops producing rows (file gone, read error): say so, don't sit frozen
                if (playback.value && status.value.phase == Phase.RUNNING) {
                    val now = System.currentTimeMillis()
                    if (status.value.rows != pbRows) { pbRows = status.value.rows; pbQuietSince = now }
                    else if (now - pbQuietSince > 3000) {
                        stopDemo()
                        setPhase(Phase.ERROR, "The recording stopped playing: the file can't be read")
                    }
                }
                if (++tick % 4 == 0) pollPeaks(pk)
                // the radio changes these itself on a mode change (per-mode settings)
                if (tick % 6 == 0 && running && System.currentTimeMillis() - lastDspMs > 800 &&
                    Native.dspGet(dv) == Dsp.COUNT) {
                    if (!dv.contentEquals(dsp.value)) dsp.value = dv.copyOf()
                    // HL2 auto gain moves the LNA gain itself: the LNA chip follows
                    if (dv[Dsp.AUTOGAIN] != 0.0 && dv[Dsp.LNA] != lnaDb.value) lnaDb.value = dv[Dsp.LNA]
                }
                delay(50)
            }
        }
    }
}
