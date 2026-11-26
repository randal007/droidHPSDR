// droidHPSDR - GPL v3 or later.
//
// M5 round 2/3 (Randal's picks 2026-11-25): the rest of deskHPSDR's Display
// menu in a tabbed sheet (E1 Levels, E3 Averaging + peaks & hold), its Screen
// menu (E6), and the info bar placed in the empty gaps beside the VFO pill
// (E8: nothing moves, nothing gets smaller). Pictures: tools/mockups/layouts6.js, layouts7.js.
package io.github.randal007.droidhpsdr

import android.content.Context
import android.content.SharedPreferences
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableLongStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Rect
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.toArgb
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import java.time.ZoneOffset
import java.time.ZonedDateTime
import kotlin.math.roundToInt

/** The display settings added in M5 round 2, saved like the rest (SharedPreferences "radio"). */
internal object Disp {
    private lateinit var prefs: SharedPreferences

    // Levels (E1): deskHPSDR's panadapter high / low / step and waterfall high / low, or automatic
    val panAuto = MutableStateFlow(true)
    val panHi = MutableStateFlow(-40)
    val panLo = MutableStateFlow(-140)
    val panStep = MutableStateFlow(10)
    val panMargin = MutableStateFlow(8)          // automatic: dB the bottom sits below the noise floor
    val wfAuto = MutableStateFlow(true)
    val wfHi = MutableStateFlow(-60)
    val wfLo = MutableStateFlow(-130)

    // Averaging and peaks & hold (E3)
    val avgMode = MutableStateFlow(AVG_LOG)      // deskHPSDR's default: Log recursive
    val peakMode = MutableStateFlow(PEAK_DECAY)
    val holdS = MutableStateFlow(2.0)
    val dropDbS = MutableStateFlow(6.0)
    val peakColor = MutableStateFlow(0)

    // Screen (E6)
    val fftSize = MutableStateFlow(0)            // 0 Auto
    val saveZoom = MutableStateFlow(true)
    val preserve = MutableStateFlow(false)
    val showMeter = MutableStateFlow(true)
    val showVfo = MutableStateFlow(true)
    val showChips = MutableStateFlow(true)
    val showBandBar = MutableStateFlow(true)
    val showInfo = MutableStateFlow(true)
    val showAxis = MutableStateFlow(true)

    // Info bar items (E8)
    val infoClock = MutableStateFlow(true)
    val infoAdc = MutableStateFlow(true)
    val infoRadio = MutableStateFlow(true)

    private val ints = listOf(
        "panHi" to panHi, "panLo" to panLo, "panStep" to panStep, "panMargin" to panMargin, "wfHi" to wfHi,
        "wfLo" to wfLo, "avgMode" to avgMode, "peakMode" to peakMode, "peakColor" to peakColor, "fftSize" to fftSize,
    )
    private val bools = listOf(
        "panAuto" to panAuto, "wfAuto" to wfAuto, "saveZoom" to saveZoom, "preserve" to preserve,
        "showMeter" to showMeter, "showVfo" to showVfo, "showChips" to showChips, "showBandBar" to showBandBar,
        "showInfo" to showInfo, "showAxis" to showAxis, "infoClock" to infoClock, "infoAdc" to infoAdc,
        "infoRadio" to infoRadio,
    )

    fun init(ctx: Context) {
        prefs = ctx.getSharedPreferences("radio", Context.MODE_PRIVATE)
        for ((k, f) in ints) f.value = prefs.getInt("d.$k", f.value)
        for ((k, f) in bools) f.value = prefs.getBoolean("d.$k", f.value)
        holdS.value = prefs.getFloat("d.holdS", holdS.value.toFloat()).toDouble()
        dropDbS.value = prefs.getFloat("d.dropDbS", dropDbS.value.toFloat()).toDouble()
        // before round 2: peak hold was on/off, Av. Time 0 meant no averaging
        if (!prefs.contains("d.peakMode") && !prefs.getBoolean("peakHold", true)) peakMode.value = PEAK_OFF
        if (!prefs.contains("d.avgMode") && prefs.getInt("avgMs", 250) == 0) avgMode.value = AVG_NONE
    }

    fun set(f: MutableStateFlow<Int>, v: Int) {
        f.value = v
        ints.firstOrNull { it.second === f }?.let { prefs.edit().putInt("d.${it.first}", v).apply() }
        if (f === fftSize) Radio.setDisplay()
    }

    fun set(f: MutableStateFlow<Boolean>, v: Boolean) {
        f.value = v
        bools.firstOrNull { it.second === f }?.let { prefs.edit().putBoolean("d.${it.first}", v).apply() }
    }

    fun setHold(s: Double) { holdS.value = s; prefs.edit().putFloat("d.holdS", s.toFloat()).apply() }
    fun setDrop(d: Double) { dropDbS.value = d; prefs.edit().putFloat("d.dropDbS", d.toFloat()).apply() }

    /** 2D spectrum bottom and top (dBm): automatic follows the noise floor, like deskHPSDR's autoscale. */
    fun panLevels(nf: Double): Pair<Double, Double> = if (panAuto.value) {
        val lo = nf - panMargin.value
        lo to lo + PAN_RANGE
    } else panLo.value.toDouble() to maxOf(panHi.value, panLo.value + 10).toDouble()

    /** Waterfall colour range (dBm). */
    fun wfLevels(nf: Double): Pair<Double, Double> = if (wfAuto.value) (nf - 2.5) to (nf + 48.0)
    else wfLo.value.toDouble() to maxOf(wfHi.value, wfLo.value + 10).toDouble()

    /** 3D: the floor (dBm) and the dB to a full ridge. Manual levels apply to the 3D view too. */
    fun levels3d(style: Int, nf: Double): Pair<Double, Double> = if (panAuto.value) {
        SpectrumView.floor3d(style, nf) to SpectrumView.RANGE_3D.toDouble()
    } else panLo.value.toDouble() to maxOf(panHi.value - panLo.value, 10).toDouble()

    fun peakArgb(): Int = PEAK_COLORS[peakColor.value.coerceIn(0, PEAK_COLORS.size - 1)].toArgb()

    const val PAN_RANGE = 70.0
    const val AVG_NONE = 0
    const val AVG_LOG = 3
    const val PEAK_OFF = 0
    const val PEAK_HOLD = 1
    const val PEAK_DECAY = 2
    val PEAK_COLORS = listOf(Color(0xFFFFD27A), Color(0xFFFF7B7B), Color(0xFF3EE07A), Color(0xFF3FD0FF), Color(0xFFFFFFFF))
    val FFT_SIZES = listOf(0, 16384, 32768, 65536, 131072, 262144)
}

// ---------------------------------------------------------------- the Display sheet (tabs, E1/E3)

private val TABS = listOf("Look", "Levels", "Peaks", "Averaging", "Info bar", "Speed")

/** deskHPSDR's Display menu, the phone's way: it grew long, so it has tabs now (E1). */
@Composable
internal fun DisplaySheet() {
    var tab by remember { mutableStateOf(0) }
    SheetTitle("Display")
    Seg(TABS, tab, h = 30) { tab = it }
    when (tab) {
        0 -> LookTab()
        1 -> LevelsTab()
        2 -> PeaksTab()
        3 -> AveragingTab()
        4 -> InfoTab()
        else -> SpeedTab()
    }
}

@Composable
private fun LookTab() {
    val flat by Radio.flat.collectAsState()
    val style by Radio.style.collectAsState()
    val theme by Radio.wfTheme.collectAsState()
    val depth by Radio.depthRows.collectAsState()
    val split by Radio.split.collectAsState()
    Label("3D look")
    GridButtons(SpectrumView.STYLES, if (flat) null else SpectrumView.STYLES[style], 4) { Radio.setStyle(it); Radio.setFlat(false) }
    GridButtons(listOf("2D spectrum"), if (flat) "2D spectrum" else null, 1) { Radio.setFlat(true) }
    Label("Waterfall colours")
    GridButtons(SpectrumView.WF_THEMES, SpectrumView.WF_THEMES[theme], 6) { Radio.setWfTheme(it) }
    Label("3D history (rows)")
    val depths = listOf(40, 80, 120, 160)
    GridButtons(depths.map { "$it" }, "$depth", 4) { Radio.setDepthRows(depths[it]) }
    Label("Spectrum / waterfall split")
    val splits = listOf(0.35f, 0.45f, 0.5f, 0.6f)
    GridButtons(splits.map { "${(it * 100).roundToInt()} %" }, "${(split * 100).roundToInt()} %", 4) { Radio.setSplit(splits[it]) }
}

/** E1: automatic (follows the noise floor) or deskHPSDR's manual high / low / step, spectrum and waterfall. */
@Composable
private fun LevelsTab() {
    val pa by Disp.panAuto.collectAsState()
    val ph by Disp.panHi.collectAsState()
    val pl by Disp.panLo.collectAsState()
    val ps by Disp.panStep.collectAsState()
    val pm by Disp.panMargin.collectAsState()
    val wa by Disp.wfAuto.collectAsState()
    val wh by Disp.wfHi.collectAsState()
    val wl by Disp.wfLo.collectAsState()
    val dbm = { v: Double -> "%.0f dBm".format(v) }
    Row(horizontalArrangement = Arrangement.spacedBy(14.dp)) {
        Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(4.dp)) {
            Label("SPECTRUM")
            Toggle("Automatic", pa, Modifier.fillMaxWidth(), sub = "follows the noise floor") { Disp.set(Disp.panAuto, it) }
            Setting("HIGH", ph.toDouble(), -100f..0f, 5.0, dbm, !pa) { Disp.set(Disp.panHi, maxOf(it.toInt(), pl + 10)) }
            Setting("LOW", pl.toDouble(), -160f..-60f, 5.0, dbm, !pa) { Disp.set(Disp.panLo, minOf(it.toInt(), ph - 10)) }
            Setting("STEP (grid)", ps.toDouble(), 5f..20f, 5.0, { "%.0f dB".format(it) }) { Disp.set(Disp.panStep, it.toInt()) }
            Setting("NOISE MARGIN", pm.toDouble(), 0f..30f, 1.0, { "%.0f dB".format(it) }, pa) { Disp.set(Disp.panMargin, it.toInt()) }
        }
        Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(4.dp)) {
            Label("WATERFALL")
            Toggle("Automatic", wa, Modifier.fillMaxWidth()) { Disp.set(Disp.wfAuto, it) }
            Setting("HIGH", wh.toDouble(), -100f..0f, 5.0, dbm, !wa) { Disp.set(Disp.wfHi, maxOf(it.toInt(), wl + 10)) }
            Setting("LOW", wl.toDouble(), -160f..-60f, 5.0, dbm, !wa) { Disp.set(Disp.wfLo, minOf(it.toInt(), wh - 10)) }
            Text("Automatic greys out HIGH / LOW, like deskHPSDR. Manual spectrum levels also set the 3D view.",
                color = Dim, fontSize = 10.5.sp)
        }
    }
}

@Composable
private fun PeaksTab() {
    val labels by Radio.peakLabels.collectAsState()
    val pass by Radio.labelsPassband.collectAsState()
    val smeter by Radio.labelsSmeter.collectAsState()
    val hide by Radio.labelsHideNoise.collectAsState()
    val pct by Radio.labelsPercentile.collectAsState()
    val ign by Radio.labelsIgnore.collectAsState()
    Label("Peak labels: how many")
    val counts = listOf(0, 1, 2, 3, 5, 8)
    GridButtons(counts.map { if (it == 0) "Off" else "$it" }, if (labels == 0) "Off" else "$labels", 6) { Radio.setPeakLabels(counts[it]) }
    GridButtons(listOf("S-units", "dBm"), if (smeter) "S-units" else "dBm", 2) { Radio.setLabelsSmeter(it == 0) }
    GridButtons(listOf("Whole view", "Passband only"), if (pass) "Passband only" else "Whole view", 2) { Radio.setLabelsPassband(it == 1) }
    Label("Hide peaks below the noise floor (percentile)")
    val pcts = listOf(0, 60, 70, 80, 90)
    GridButtons(pcts.map { if (it == 0) "Off" else "$it %" }, if (!hide) "Off" else "$pct %", 5) {
        if (it == 0) Radio.setLabelsHideNoise(false) else { Radio.setLabelsHideNoise(true); Radio.setLabelsPercentile(pcts[it]) }
    }
    Label("Ignore adjacent peaks (screen width ÷)")
    val ignores = listOf(10, 20, 30, 40)
    GridButtons(ignores.map { "$it" }, "$ign", 4) { Radio.setLabelsIgnore(ignores[it]) }
    Text("Peak hold (the line that holds the highest points): Averaging tab.", color = Dim, fontSize = 10.5.sp)
}

/** E3: deskHPSDR's four averaging kinds with Av. Time and the detector; peaks & hold for the 2D line. */
@Composable
private fun AveragingTab() {
    val mode by Disp.avgMode.collectAsState()
    val avg by Radio.avgMs.collectAsState()
    val det by Radio.detector.collectAsState()
    val pk by Disp.peakMode.collectAsState()
    val hold by Disp.holdS.collectAsState()
    val drop by Disp.dropDbS.collectAsState()
    val col by Disp.peakColor.collectAsState()
    Row(horizontalArrangement = Arrangement.spacedBy(14.dp)) {
        Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(5.dp)) {
            Label("AVERAGING")
            Seg(listOf("None", "Recursive", "Time window", "Log rec."), mode, h = 32) { Disp.set(Disp.avgMode, it) }
            Setting("AV. TIME", maxOf(avg, 30).toDouble(), 30f..1000f, 10.0, { "%.0f ms".format(it) }, mode != Disp.AVG_NONE) {
                Radio.setAvgMs(it.toInt())
            }
            Label("DETECTOR")
            // order of the radio's numbers: 0 Peak, 1 Average, 2 Rosenfell, 3 Sample
            Seg(listOf("Peak", "Average", "Rosenfell", "Sample"), det, h = 32) { Radio.setDisplay(detector = it) }
            Text("Log recursive: steady noise floor, signals still jump out (deskHPSDR's default).",
                color = Dim, fontSize = 10.5.sp)
        }
        Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(5.dp)) {
            Label("PEAKS & HOLD (2D LINE)")
            Seg(listOf("Off", "Hold", "Decay"), pk, h = 32) { Disp.set(Disp.peakMode, it) }
            Setting("HOLD TIME", hold, 0.5f..10f, 0.5, { "%.1f s".format(it) }, pk == Disp.PEAK_DECAY) { Disp.setHold(it) }
            Setting("DROP", drop, 1f..30f, 1.0, { "%.0f dB/s".format(it) }, pk == Disp.PEAK_DECAY) { Disp.setDrop(it) }
            Label("LINE COLOUR")
            Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                Disp.PEAK_COLORS.forEachIndexed { i, c ->
                    Box(
                        Modifier.size(28.dp).clip(CircleShape).background(c.copy(alpha = if (i == col) 1f else 0.75f))
                            .border(if (i == col) 2.dp else 0.dp, if (i == col) Color.White else Color.Transparent, CircleShape)
                            .clickable { Disp.set(Disp.peakColor, i) },
                    )
                }
            }
            if (pk == Disp.PEAK_HOLD) Text("Hold keeps the highest point until you switch it off or on again.", color = Dim, fontSize = 10.5.sp)
        }
    }
}

@Composable
private fun InfoTab() {
    val show by Disp.showInfo.collectAsState()
    val clock by Disp.infoClock.collectAsState()
    val adc by Disp.infoAdc.collectAsState()
    val radio by Disp.infoRadio.collectAsState()
    Text("Beside the VFO, in the gaps that are already there: nothing moves and nothing gets smaller.",
        color = Dim, fontSize = 11.sp)
    Toggle("Show the info bar", show, Modifier.fillMaxWidth()) { Disp.set(Disp.showInfo, it) }
    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        Toggle("UTC clock", clock, Modifier.weight(1f), enabled = show) { Disp.set(Disp.infoClock, it) }
        Toggle("ADC overload alarm", adc, Modifier.weight(1f), enabled = show) { Disp.set(Disp.infoAdc, it) }
    }
    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        Toggle("Radio and address", radio, Modifier.weight(1f), enabled = show) { Disp.set(Disp.infoRadio, it) }
        Toggle("Solar data", false, Modifier.weight(1f), enabled = false, sub = "M8") {}
    }
}

@Composable
private fun SpeedTab() {
    val fps by Radio.fps.collectAsState()
    Label("Frames per second (spectrum rows)")
    val rates = listOf(15, 20, 30, 60)
    GridButtons(rates.map { "$it" }, "$fps", 4) { Radio.setDisplay(fps = rates[it]) }
    Text("More rows a second: smoother, more battery. FFT window and size: ☰ → Screen.", color = Dim, fontSize = 10.5.sp)
}

// ---------------------------------------------------------------- the Screen sheet (E6)

/** deskHPSDR's Screen menu, the phone's way: FFT size and window, zoom, and which parts show. */
@Composable
internal fun ScreenSheet() {
    val fft by Disp.fftSize.collectAsState()
    val win by Radio.window.collectAsState()
    val save by Disp.saveZoom.collectAsState()
    val keep by Disp.preserve.collectAsState()
    val rate by Radio.sampleRate.collectAsState()
    val flags = listOf(
        "Meter" to Disp.showMeter, "VFO pill" to Disp.showVfo, "Bottom row" to Disp.showChips,
        "Band plan bar" to Disp.showBandBar, "Info bar" to Disp.showInfo, "Frequency axis" to Disp.showAxis,
    )
    SheetTitle("Screen")
    Row(horizontalArrangement = Arrangement.spacedBy(14.dp)) {
        Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(5.dp)) {
            Label("FFT SIZE (finer = narrower bins, slower)")
            val names = Disp.FFT_SIZES.map { if (it == 0) "Auto" else "${it / 1024}k" }
            Seg(names.take(3), Disp.FFT_SIZES.indexOf(fft).takeIf { it < 3 } ?: -1, h = 30) { Disp.set(Disp.fftSize, Disp.FFT_SIZES[it]) }
            Seg(names.drop(3), Disp.FFT_SIZES.indexOf(fft).takeIf { it >= 3 }?.minus(3) ?: -1, h = 30) {
                Disp.set(Disp.fftSize, Disp.FFT_SIZES[it + 3])
            }
            Text(
                if (fft == 0) "Auto: deskHPSDR picks the size from the screen width and zoom"
                else "%d kHz ÷ %dk = %.1f Hz per bin".format(rate / 1000, fft / 1024, rate.toDouble() / fft),
                color = Dim, fontSize = 10.5.sp,
            )
            Label("FFT WINDOW")
            val windows = listOf("Rect", "B-H 4", "Hann", "Flat-top", "Hamming", "Kaiser", "B-H 7")
            GridButtons(windows, windows[win], 4) { Radio.setDisplay(window = it) }
            Label("ZOOM")
            Toggle("Save zoom level", save, Modifier.fillMaxWidth(), sub = "start at the last span") { Disp.set(Disp.saveZoom, it) }
            Toggle("Preserve narrow peaks", keep, Modifier.fillMaxWidth(), sub = "2D: a thin carrier stays visible") {
                Disp.set(Disp.preserve, it)
            }
        }
        Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(5.dp)) {
            Label("SHOW ON SCREEN")
            flags.forEach { (t, f) ->
                val on by f.collectAsState()
                Toggle(t, on, Modifier.fillMaxWidth()) { Disp.set(f, it) }
            }
            Text("Full screen: always.", color = Dim, fontSize = 10.5.sp)
        }
    }
}

// ---------------------------------------------------------------- the info bar in the gaps (E8)

/**
 * The info bar (E8): the clock in the gap left of the VFO pill, the ADC
 * alarm and the radio right of it. The gaps are measured from where the
 * meter, the pill and the buttons really are; a gap too narrow (portrait)
 * stays empty instead of covering anything.
 */
@Composable
internal fun InfoGaps(st: Status, meter: Rect?, pill: Rect?, buttons: Rect?) {
    val show by Disp.showInfo.collectAsState()
    val clock by Disp.infoClock.collectAsState()
    val adc by Disp.infoAdc.collectAsState()
    val radio by Disp.infoRadio.collectAsState()
    if (!show || pill == null) return
    val d = LocalDensity.current
    var now by remember { mutableStateOf(utcNow()) }
    LaunchedEffect(Unit) { while (true) { now = utcNow(); delay(1000) } }
    // the ADC alarm stays red 2 s after the last overload the radio counted
    var lastOvl by remember { mutableLongStateOf(0L) }
    var seen by remember { mutableStateOf(st.adcOverloads) }
    if (st.adcOverloads != seen) {
        if (st.adcOverloads > seen) lastOvl = System.currentTimeMillis()
        seen = st.adcOverloads
    }
    var tick by remember { mutableLongStateOf(0L) }
    LaunchedEffect(lastOvl) { if (lastOvl > 0) { delay(2100); tick++ } }
    val overload = tick >= 0 && System.currentTimeMillis() - lastOvl < 2000

    fun gap(a: Float, b: Float, content: @Composable () -> Unit): @Composable () -> Unit = {
        val w = with(d) { (b - a).toDp() }
        if (w >= 56.dp) {
            Box(Modifier.offset(x = with(d) { a.toDp() }, y = with(d) { pill.top.toDp() }).width(w)
                .padding(top = 8.dp), contentAlignment = Alignment.TopCenter) { content() }
        }
    }
    val left = meter?.right ?: 0f
    val right = buttons?.left ?: (pill.right + with(d) { 200.dp.toPx() })
    if (clock) gap(left, pill.left) {
        Column(horizontalAlignment = Alignment.CenterHorizontally) {
            Text(now, color = Txt, fontSize = 15.sp, fontWeight = FontWeight.Medium, style = shadow)
            Text("UTC", color = Dim, fontSize = 10.5.sp, style = shadow)
        }
    }()
    val live = st.phase == Phase.RUNNING
    if (adc || radio) gap(pill.right, right) {
        Column(horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.spacedBy(3.dp)) {
            if (adc && live) {
                if (overload) {
                    Text("ADC OVERLOAD", color = Color.White, fontSize = 10.5.sp, fontWeight = FontWeight.SemiBold,
                        modifier = Modifier.clip(RoundedCornerShape(8.dp)).background(Color(0xE6DC2828)).padding(horizontal = 7.dp))
                    Text("lower the LNA", color = Color(0xFFFFD0D0), fontSize = 9.5.sp, style = shadow)
                } else Text("ADC OK", color = Grn, fontSize = 10.5.sp, style = shadow)
            }
            if (radio) {
                val ip = if (st.network == "Ethernet") Radio.wiredIp.value else Radio.wifiIp.value
                Text(
                    when (st.phase) {
                        Phase.RUNNING -> "HL2 $ip"
                        Phase.DEMO -> "demo band"
                        else -> "no radio"
                    },
                    color = Dim, fontSize = 9.5.sp, style = shadow, textAlign = TextAlign.Center,
                )
            }
        }
    }()
}

private val shadow = androidx.compose.ui.text.TextStyle(
    shadow = androidx.compose.ui.graphics.Shadow(Color.Black, androidx.compose.ui.geometry.Offset(0f, 1f), 6f),
)

private fun utcNow(): String = ZonedDateTime.now(ZoneOffset.UTC).let { "%02d:%02d".format(it.hour, it.minute) }
