// droidHPSDR - GPL v3 or later.
//
// M5, receive DSP (Randal's picks from m5-round1, 2026-11-24): the Noise
// drawer from the right edge (D1) with a settings page per NR/NB kind (D8),
// the RX equalizer graph (D7), deskHPSDR's DSP menu, and the bottom row the
// user puts together (D9, deskHPSDR's Toolbar menu). The notch (D5) and the
// squelch line (D6) live on the spectrum, in Screen.kt's SpectrumOverlay.
// The radio side is android/native/app/dsp.c.
package io.github.randal007.droidhpsdr

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.gestures.detectDragGesturesAfterLongPress
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Slider
import androidx.compose.material3.SliderDefaults
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.key
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.drawText
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.rememberTextMeasurer
import androidx.compose.ui.unit.IntOffset
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlin.math.abs
import kotlin.math.log10
import kotlin.math.pow
import kotlin.math.roundToInt

// ---------------------------------------------------------------- small controls

/** One row of choices, the selected one lit. */
@Composable
internal fun Seg(items: List<String>, selected: Int, enabled: Boolean = true, h: Int = 34, onPick: (Int) -> Unit) {
    Row(
        Modifier.fillMaxWidth().height(h.dp).clip(RoundedCornerShape(10.dp)).background(Color(0x0FFFFFFF)).padding(2.dp),
        horizontalArrangement = Arrangement.spacedBy(2.dp),
    ) {
        items.forEachIndexed { i, t ->
            val on = i == selected
            Box(
                Modifier.weight(1f).fillMaxHeight().clip(RoundedCornerShape(8.dp))
                    .background(if (on) Color(0x333FD0FF) else Color.Transparent)
                    .clickable(enabled = enabled) { onPick(i) },
                contentAlignment = Alignment.Center,
            ) {
                Text(t, color = if (!enabled) Dim.copy(alpha = 0.4f) else if (on) Acc else Dim, fontSize = 12.sp,
                    fontWeight = FontWeight.Medium, maxLines = 1)
            }
        }
    }
}

/** A labelled on/off switch (the tiles in the pictures). */
@Composable
internal fun Toggle(label: String, on: Boolean, modifier: Modifier = Modifier, enabled: Boolean = true, sub: String = "",
                    onChange: (Boolean) -> Unit) {
    Row(
        modifier.height(40.dp).clip(RoundedCornerShape(11.dp)).background(Color(0x0DFFFFFF))
            .clickable(enabled = enabled) { onChange(!on) }.padding(horizontal = 12.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.SpaceBetween,
    ) {
        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            Text(label, color = if (enabled) Txt else Dim.copy(alpha = 0.5f), fontSize = 13.sp, fontWeight = FontWeight.Medium,
                maxLines = 1)
            if (sub.isNotEmpty()) Text(sub, color = Dim, fontSize = 9.5.sp, maxLines = 1)
        }
        Box(Modifier.width(30.dp).height(18.dp).clip(RoundedCornerShape(9.dp))
            .background(if (on && enabled) Acc.copy(alpha = 0.55f) else Color(0x29FFFFFF))) {
            Box(Modifier.padding(2.dp).offset(x = if (on) 12.dp else 0.dp).size(14.dp).clip(CircleShape)
                .background(if (on && enabled) Color.White else Color(0xFF9AA6BB)))
        }
    }
}

/** A setting with a slider: name on the left, value on the right. */
@Composable
internal fun Setting(label: String, value: Double, range: ClosedFloatingPointRange<Float>, step: Double,
                     fmt: (Double) -> String, enabled: Boolean = true, set: (Double) -> Unit) {
    Column {
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
            Text(label, color = Dim, fontSize = 11.sp)
            Text(fmt(value), color = if (enabled) Txt else Dim, fontSize = 12.sp, fontWeight = FontWeight.Medium)
        }
        Slider(
            value = value.toFloat().coerceIn(range), enabled = enabled, valueRange = range,
            onValueChange = { v -> set((v / step).roundToInt() * step) },
            modifier = Modifier.height(30.dp),
            colors = SliderDefaults.colors(thumbColor = Acc, activeTrackColor = Acc),
        )
    }
}

@Composable
internal fun SmallButton(t: String, modifier: Modifier = Modifier, on: Boolean = false, onClick: () -> Unit) {
    Box(
        modifier.height(32.dp).clip(RoundedCornerShape(10.dp))
            .background(if (on) Color(0x293FD0FF) else Color(0xFF0F1624))
            .border(1.dp, if (on) Acc.copy(alpha = 0.5f) else Line, RoundedCornerShape(10.dp))
            .clickable(onClick = onClick).padding(horizontal = 12.dp),
        contentAlignment = Alignment.Center,
    ) { Text(t, color = if (on) Acc else Txt, fontSize = 12.sp, fontWeight = FontWeight.Medium, maxLines = 1) }
}

private fun on(d: DoubleArray, f: Int) = d[f] != 0.0
private fun set(f: Int, v: Boolean) = Radio.setDsp(f, if (v) 1.0 else 0.0)
private fun i(d: DoubleArray, f: Int) = d[f].roundToInt()

/** Puts the named fields back to receiver.c's defaults. */
private fun defaults(vararg fields: Int) = fields.forEach { Radio.setDsp(it, Dsp.DEFAULTS[it]) }

/** Upstream's squelch is 0..100; in AM, SAM and CW it is AM squelch, a level -160..0 dBm (receiver.c rx_set_squelch). */
internal fun sqlDbm(v: Double) = v / 100.0 * 160.0 - 160.0
internal fun sqlIsLevel(mode: Int) = mode == Modes.AM || mode == Modes.SAM || mode == Modes.CWU || mode == Modes.CWL
internal fun sqlLabel(mode: Int, v: Double) = when {
    sqlIsLevel(mode) -> "%.0f dBm".format(sqlDbm(v))
    mode == Modes.FM -> "%.0f (FM)".format(v)
    else -> "%.0f (voice)".format(v)
}

// ---------------------------------------------------------------- the Noise drawer (D1)

/** deskHPSDR's Noise menu as a drawer from the right edge; the fine settings open as a page (D8). */
@Composable
internal fun NoiseDrawer(st: Status, close: () -> Unit) {
    val d by Radio.dsp.collectAsState()
    var page by remember { mutableStateOf<String?>(null) }
    val mode = st.mode
    val nrOk = Dsp.nrAllowed(mode)
    val anfOk = Dsp.anfAllowed(mode)
    Box(Modifier.fillMaxSize().background(Color(0x55000000)).clickable(onClick = close)) {
        if (page != null) {
            SettingsPage(page!!, d, mode) { page = null }
            return@Box
        }
        Column(
            Modifier.align(Alignment.CenterEnd).fillMaxHeight().width(340.dp)
                .clip(RoundedCornerShape(topStart = 18.dp, bottomStart = 18.dp)).background(Color(0xF0090C16))
                .border(1.dp, Line, RoundedCornerShape(topStart = 18.dp, bottomStart = 18.dp))
                .clickable(enabled = false) {}.verticalScroll(rememberScrollState()).padding(14.dp),
            verticalArrangement = Arrangement.spacedBy(7.dp),
        ) {
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween, verticalAlignment = Alignment.CenterVertically) {
                SheetTitle("Noise")
                Text("${Modes.name(mode)} · kept per mode", color = Dim, fontSize = 10.sp)
            }
            Label("Noise reduction")
            Seg(Dsp.NR_NAMES, i(d, Dsp.NR), enabled = nrOk) { Radio.setDsp(Dsp.NR, it.toDouble()) }
            Label("Noise blanker")
            Seg(Dsp.NB_NAMES, i(d, Dsp.NB)) { Radio.setDsp(Dsp.NB, it.toDouble()) }
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                Toggle("ANF", on(d, Dsp.ANF) && anfOk, Modifier.weight(1f), enabled = anfOk) { set(Dsp.ANF, it) }
                Toggle("SNB", on(d, Dsp.SNB) && nrOk, Modifier.weight(1f), enabled = nrOk) { set(Dsp.SNB, it) }
                Toggle("MNF", on(d, Dsp.MNF), Modifier.weight(1f)) { notchOnOff(it, st) }
            }
            if (on(d, Dsp.MNF)) {
                Setting("Notch width (double-tap the spectrum to move it)", d[Dsp.MNF_WIDTH], 10f..1500f, 10.0,
                    { "%.0f Hz".format(it) }) { Radio.setDsp(Dsp.MNF_WIDTH, it) }
            }
            Label("NR / NR2 / ANF position")
            Seg(listOf("Pre AGC", "Post AGC"), i(d, Dsp.NR_POST_AGC), enabled = nrOk, h = 30) { Radio.setDsp(Dsp.NR_POST_AGC, it.toDouble()) }
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                val nr = i(d, Dsp.NR)
                val nrPage = when (nr) { 2 -> "NR2"; 4 -> "NR4"; 5 -> "NNR"; else -> null }
                if (nrPage != null && nrOk) SmallButton("$nrPage settings ›", Modifier.weight(1f)) { page = nrPage }
                SmallButton("NB settings ›", Modifier.weight(1f)) { page = "NB" }
            }
            Label("Squelch")
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp), verticalAlignment = Alignment.CenterVertically) {
                Toggle("SQL", on(d, Dsp.SQL_ON), Modifier.width(96.dp)) { set(Dsp.SQL_ON, it) }
                Text("drag the green line on the spectrum, or:", color = Dim, fontSize = 10.sp)
            }
            Setting("Level", d[Dsp.SQL], 0f..100f, 1.0, { sqlLabel(mode, it) }) { Radio.setDsp(Dsp.SQL, it) }
            if (mode == Modes.CWU || mode == Modes.CWL) {
                Toggle("CW audio peak filter", on(d, Dsp.CW_PEAK)) { set(Dsp.CW_PEAK, it) }
            }
            if (!nrOk) Text("No noise reduction in the digital modes (deskHPSDR).", color = Amb, fontSize = 10.5.sp)
            else if (!anfOk) Text("No ANF in CW (deskHPSDR).", color = Dim, fontSize = 10.5.sp)
        }
    }
}

/** MNF on: like deskHPSDR's MNF action, a notch with no place yet goes in the middle of the passband. */
internal fun notchOnOff(v: Boolean, st: Status) {
    val d = Radio.dsp.value
    if (v) {
        val f = Radio.freq.value.toDouble()
        val cf = d[Dsp.MNF_FREQ]
        if (cf <= 0 || abs(cf - f) > st.sampleRate / 2.0) Radio.setDsp(Dsp.MNF_FREQ, f + (st.filterLow + st.filterHigh) / 2.0)
    }
    set(Dsp.MNF, v)
}

/** The fine settings of one NR/NB kind, every one deskHPSDR's Noise menu has (D8). */
@Composable
private fun SettingsPage(which: String, d: DoubleArray, mode: Int, back: () -> Unit) {
    Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
        Column(
            Modifier.widthIn(max = 640.dp).fillMaxWidth().padding(8.dp).clip(RoundedCornerShape(18.dp))
                .background(Color(0xF20C111E)).border(1.dp, Line, RoundedCornerShape(18.dp))
                .clickable(enabled = false) {}.verticalScroll(rememberScrollState()).padding(14.dp),
            verticalArrangement = Arrangement.spacedBy(6.dp),
        ) {
            Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                SmallButton("‹ Noise", onClick = back)
                SheetTitle("$which settings · ${Modes.name(mode)}")
                Spacer(Modifier.weight(1f))
                SmallButton("Defaults") {
                    when (which) {
                        "NR2" -> defaults(Dsp.NR2_GAIN, Dsp.NR2_NPE, Dsp.NR2_AE, Dsp.NR2_POST, Dsp.NR2_POST_LEVEL,
                            Dsp.NR2_POST_RATE, Dsp.NR2_POST_FACTOR, Dsp.NR2_POST_TAPER, Dsp.NR2_TRAINED_THRESH, Dsp.NR2_TRAINED_T2)
                        "NB" -> defaults(Dsp.NB2_MODE, Dsp.NB_SLEW, Dsp.NB_LEAD, Dsp.NB_LAG, Dsp.NB_THRESH)
                        "NR4" -> defaults(Dsp.NR4_REDUCTION, Dsp.NR4_SMOOTHING, Dsp.NR4_WHITENING, Dsp.NR4_RESCALE, Dsp.NR4_THRESHOLD)
                        "NNR" -> defaults(Dsp.NNR_MODEL, Dsp.NNR_FLOOR)
                    }
                }
            }
            val int = { v: Double -> "%.0f".format(v) }
            Row(horizontalArrangement = Arrangement.spacedBy(14.dp)) {
                Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(5.dp)) {
                    when (which) {
                        "NR2" -> {
                            Label("Gain method")
                            Seg(listOf("Linear", "Log", "Gamma", "Trained"), i(d, Dsp.NR2_GAIN), h = 30) { Radio.setDsp(Dsp.NR2_GAIN, it.toDouble()) }
                            Label("Noise estimate (NPE method)")
                            Seg(listOf("OSMS", "MMSE", "NSTAT"), i(d, Dsp.NR2_NPE), h = 30) { Radio.setDsp(Dsp.NR2_NPE, it.toDouble()) }
                            Toggle("Artifact elimination", on(d, Dsp.NR2_AE)) { set(Dsp.NR2_AE, it) }
                            val trained = i(d, Dsp.NR2_GAIN) == 3
                            Setting("Trained threshold", d[Dsp.NR2_TRAINED_THRESH], -5f..5f, 0.1, { "%.1f".format(it) }, trained) {
                                Radio.setDsp(Dsp.NR2_TRAINED_THRESH, it)
                            }
                            Setting("Trained T2", d[Dsp.NR2_TRAINED_T2], 0.02f..0.3f, 0.01, { "%.2f".format(it) }, trained) {
                                Radio.setDsp(Dsp.NR2_TRAINED_T2, it)
                            }
                        }
                        "NB" -> {
                            Label("NB2 mode")
                            Seg(listOf("Zero", "S&H", "Mean", "Hold S", "Interp."), i(d, Dsp.NB2_MODE), h = 30) {
                                Radio.setDsp(Dsp.NB2_MODE, it.toDouble())
                            }
                            Setting("Threshold", d[Dsp.NB_THRESH], 15f..500f, 1.0, int) { Radio.setDsp(Dsp.NB_THRESH, it) }
                        }
                        "NR4" -> {
                            Setting("Reduction", d[Dsp.NR4_REDUCTION], 0f..20f, 1.0, { "%.0f dB".format(it) }) { Radio.setDsp(Dsp.NR4_REDUCTION, it) }
                            Setting("Smoothing", d[Dsp.NR4_SMOOTHING], 0f..100f, 1.0, { "%.0f %%".format(it) }) { Radio.setDsp(Dsp.NR4_SMOOTHING, it) }
                            Setting("Whitening", d[Dsp.NR4_WHITENING], 0f..100f, 1.0, { "%.0f %%".format(it) }) { Radio.setDsp(Dsp.NR4_WHITENING, it) }
                        }
                        "NNR" -> {
                            Label("Model (built into WDSP 2.11)")
                            Seg(listOf("Standard", "Premium"), i(d, Dsp.NNR_MODEL), h = 30) { Radio.setDsp(Dsp.NNR_MODEL, it.toDouble()) }
                        }
                    }
                }
                Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(5.dp)) {
                    when (which) {
                        "NR2" -> {
                            Toggle("Post-processing", on(d, Dsp.NR2_POST)) { set(Dsp.NR2_POST, it) }
                            val p = on(d, Dsp.NR2_POST)
                            Setting("Post level", d[Dsp.NR2_POST_LEVEL], 0f..100f, 1.0, int, p) { Radio.setDsp(Dsp.NR2_POST_LEVEL, it) }
                            Setting("Post rate", d[Dsp.NR2_POST_RATE], 0f..100f, 1.0, int, p) { Radio.setDsp(Dsp.NR2_POST_RATE, it) }
                            Setting("Post factor", d[Dsp.NR2_POST_FACTOR], 0f..100f, 1.0, int, p) { Radio.setDsp(Dsp.NR2_POST_FACTOR, it) }
                            Setting("Post taper", d[Dsp.NR2_POST_TAPER], 0f..100f, 1.0, int, p) { Radio.setDsp(Dsp.NR2_POST_TAPER, it) }
                        }
                        "NB" -> {
                            val ms = { v: Double -> "%.3f ms".format(v) }
                            Setting("Slew time", d[Dsp.NB_SLEW], 0f..0.1f, 0.001, ms) { Radio.setDsp(Dsp.NB_SLEW, it) }
                            Setting("Lead time", d[Dsp.NB_LEAD], 0f..0.1f, 0.001, ms) { Radio.setDsp(Dsp.NB_LEAD, it) }
                            Setting("Lag time", d[Dsp.NB_LAG], 0f..0.1f, 0.001, ms) { Radio.setDsp(Dsp.NB_LAG, it) }
                        }
                        "NR4" -> {
                            Setting("Noise rescale", d[Dsp.NR4_RESCALE], 0f..12f, 0.1, { "%.1f dB".format(it) }) { Radio.setDsp(Dsp.NR4_RESCALE, it) }
                            Setting("Post filter threshold", d[Dsp.NR4_THRESHOLD], -10f..10f, 0.1, { "%.1f dB".format(it) }) {
                                Radio.setDsp(Dsp.NR4_THRESHOLD, it)
                            }
                        }
                        "NNR" -> {
                            Setting("Mask floor", d[Dsp.NNR_FLOOR], -50f..-10f, 1.0, { "%.0f dB".format(it) }) { Radio.setDsp(Dsp.NNR_FLOOR, it) }
                        }
                    }
                }
            }
        }
    }
}

// ---------------------------------------------------------------- RX equalizer (D7)

private const val EQ_FMIN = 30.0
private const val EQ_FMAX = 16000.0

/** WDSP's RX equalizer: 12 points to drag (up/down gain, sideways frequency) and the added gain. */
@Composable
internal fun EqPanel(st: Status, close: () -> Unit) {
    val d by Radio.dsp.collectAsState()
    val dRef by rememberUpdatedState(d)
    val digi = !Dsp.nrAllowed(st.mode)
    var sel by remember { mutableIntStateOf(-1) }
    val measurer = rememberTextMeasurer()
    Box(Modifier.fillMaxSize().background(Color(0x66000000)).clickable(onClick = close), contentAlignment = Alignment.Center) {
        Column(
            Modifier.widthIn(max = 700.dp).fillMaxWidth().padding(8.dp).clip(RoundedCornerShape(18.dp))
                .background(Color(0xF20C111E)).border(1.dp, Line, RoundedCornerShape(18.dp))
                .clickable(enabled = false) {}.padding(14.dp),
            verticalArrangement = Arrangement.spacedBy(6.dp),
        ) {
            Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                SheetTitle("RX equalizer · ${Modes.name(st.mode)}")
                Spacer(Modifier.weight(1f))
                Toggle("Enable", on(d, Dsp.EQ_ON) && !digi, Modifier.width(116.dp), enabled = !digi) { set(Dsp.EQ_ON, it) }
                SmallButton("Flat") { for (k in 1..12) Radio.setDsp(Dsp.EQ_GAIN0 + k, 0.0) }
            }
            Canvas(
                Modifier.fillMaxWidth().height(170.dp).pointerInput(Unit) {
                    val padL = 34.dp.toPx(); val padR = 10.dp.toPx(); val padT = 8.dp.toPx(); val padB = 18.dp.toPx()
                    fun gx(f: Double) = padL + (log10(f / EQ_FMIN) / log10(EQ_FMAX / EQ_FMIN)).toFloat() * (size.width - padL - padR)
                    fun gy(g: Double) = padT + ((20 - g) / 40).toFloat() * (size.height - padT - padB)
                    fun fx(x: Float) = EQ_FMIN * 10.0.pow(((x - padL) / (size.width - padL - padR)).toDouble() * log10(EQ_FMAX / EQ_FMIN))
                    fun gyInv(y: Float) = 20 - (y - padT) / (size.height - padT - padB) * 40.0
                    detectDragGestures(
                        onDragStart = { p ->
                            val dd = dRef
                            sel = (1..12).minByOrNull { k ->
                                (Offset(gx(dd[Dsp.EQ_FREQ0 + k]), gy(dd[Dsp.EQ_GAIN0 + k])) - p).getDistance()
                            }?.takeIf { k ->
                                (Offset(gx(dd[Dsp.EQ_FREQ0 + k]), gy(dd[Dsp.EQ_GAIN0 + k])) - p).getDistance() < 36.dp.toPx()
                            } ?: -1
                        },
                        onDragEnd = { sel = -1 },
                        onDragCancel = { sel = -1 },
                    ) { c, _ ->
                        val k = sel
                        if (k < 1) return@detectDragGestures
                        val g = gyInv(c.position.y).coerceIn(-20.0, 20.0).roundToInt().toDouble()
                        if (g != dRef[Dsp.EQ_GAIN0 + k]) Radio.setDsp(Dsp.EQ_GAIN0 + k, g)
                        // sideways: keep the points in order, 10 Hz apart (equalizer_menu.c)
                        val lo = if (k > 1) dRef[Dsp.EQ_FREQ0 + k - 1] + 10 else 10.0
                        val hi = if (k < 12) dRef[Dsp.EQ_FREQ0 + k + 1] - 10 else EQ_FMAX
                        val f = (fx(c.position.x) / 10).roundToInt() * 10.0
                        val fc = f.coerceIn(lo, hi)
                        if (abs(fc - dRef[Dsp.EQ_FREQ0 + k]) >= 10) Radio.setDsp(Dsp.EQ_FREQ0 + k, fc)
                        c.consume()
                    }
                },
            ) {
                val padL = 34.dp.toPx(); val padR = 10.dp.toPx(); val padT = 8.dp.toPx(); val padB = 18.dp.toPx()
                fun gx(f: Double) = padL + (log10(f / EQ_FMIN) / log10(EQ_FMAX / EQ_FMIN)).toFloat() * (size.width - padL - padR)
                fun gy(g: Double) = padT + ((20 - g) / 40).toFloat() * (size.height - padT - padB)
                val ts = TextStyle(color = Dim, fontSize = 9.5.sp)
                for (g in listOf(-20, -10, 0, 10, 20)) {
                    drawLine(Color(if (g == 0) 0x33FFFFFF else 0x12FFFFFF), Offset(padL, gy(g.toDouble())), Offset(size.width - padR, gy(g.toDouble())))
                    val t = measurer.measure(if (g > 0) "+$g" else "$g", ts)
                    drawText(t, topLeft = Offset(2.dp.toPx(), gy(g.toDouble()) - t.size.height / 2))
                }
                for (f in listOf(50.0, 100.0, 300.0, 1000.0, 3000.0, 8000.0)) {
                    val t = measurer.measure(if (f >= 1000) "${(f / 1000).toInt()}k" else "${f.toInt()}", ts)
                    drawText(t, topLeft = Offset(gx(f) - t.size.width / 2, size.height - t.size.height))
                }
                val col = if (on(d, Dsp.EQ_ON) && !digi) Acc else Dim
                val pts = (1..12).map { Offset(gx(d[Dsp.EQ_FREQ0 + it]), gy(d[Dsp.EQ_GAIN0 + it])) }
                val path = Path().apply { moveTo(pts[0].x, pts[0].y); pts.drop(1).forEach { lineTo(it.x, it.y) } }
                drawPath(path, col, style = Stroke(2.4.dp.toPx()))
                pts.forEachIndexed { k, p ->
                    val big = k + 1 == sel
                    drawCircle(if (big) col else Color(0xFF0B1220), if (big) 9.dp.toPx() else 6.dp.toPx(), p)
                    drawCircle(col, if (big) 9.dp.toPx() else 6.dp.toPx(), p, style = Stroke(2.dp.toPx()))
                }
                if (sel >= 1) {
                    val f = d[Dsp.EQ_FREQ0 + sel]
                    val t = measurer.measure(
                        "%s  %+.0f dB".format(if (f >= 1000) "%.2f kHz".format(f / 1000) else "%.0f Hz".format(f), d[Dsp.EQ_GAIN0 + sel]),
                        TextStyle(color = Txt, fontSize = 11.sp))
                    val p = pts[sel - 1]
                    drawText(t, topLeft = Offset((p.x - t.size.width / 2).coerceIn(padL, size.width - t.size.width), (p.y - 30.dp.toPx()).coerceAtLeast(0f)))
                }
            }
            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                Box(Modifier.weight(1f)) {
                    Setting("Added gain (all frequencies)", d[Dsp.EQ_GAIN0], -20f..20f, 1.0, { "%+.0f dB".format(it) }) {
                        Radio.setDsp(Dsp.EQ_GAIN0, it)
                    }
                }
                Text(if (digi) "deskHPSDR keeps the EQ off in the digital modes." else "drag a point: ↕ gain, ↔ frequency",
                    color = if (digi) Amb else Dim, fontSize = 10.5.sp, modifier = Modifier.weight(1f))
            }
        }
    }
}

// ---------------------------------------------------------------- deskHPSDR's DSP menu (fft_menu.c)

@Composable
internal fun DspSheet(st: Status) {
    val d by Radio.dsp.collectAsState()
    SheetTitle("DSP")
    Label("WDSP FIR filter type (RX)")
    Seg(listOf("Linear Phase", "Low Latency"), i(d, Dsp.LOW_LATENCY)) { Radio.setDsp(Dsp.LOW_LATENCY, it.toDouble()) }
    Label("WDSP FIR filter size (NC): more = steeper skirts, more CPU; deskHPSDR's default 2048")
    val sizes = listOf(2048, 4096, 8192, 16384)
    Seg(sizes.map { "$it" }, sizes.indexOf(i(d, Dsp.FIR_SIZE))) { Radio.setDsp(Dsp.FIR_SIZE, sizes[it].toDouble()) }
    Toggle("Binaural (I and Q on left and right)", on(d, Dsp.BINAURAL),
        sub = "phone audio in stereo; off with NNR") { set(Dsp.BINAURAL, it) }
    if (st.mode == Modes.CWU || st.mode == Modes.CWL) {
        Toggle("CW audio peak filter", on(d, Dsp.CW_PEAK)) { set(Dsp.CW_PEAK, it) }
    }
    Label("RX I/Q correction (manual): balances the I and Q channels to cancel a mirror image")
    Setting("I/Q gain", d[Dsp.RX_IQ_GAIN], -5f..5f, 0.01, { "%+.2f dB".format(it) }) { Radio.setDsp(Dsp.RX_IQ_GAIN, it) }
    Setting("I/Q phase", d[Dsp.RX_IQ_PHASE], -20f..20f, 0.05, { "%+.2f°".format(it) }) { Radio.setDsp(Dsp.RX_IQ_PHASE, it) }
    Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(10.dp)) {
        SmallButton("Reset I/Q") { Radio.setDsp(Dsp.RX_IQ_GAIN, 0.0); Radio.setDsp(Dsp.RX_IQ_PHASE, 0.0) }
        Text("The HL2 mixes digitally, so it should need none: leave at 0 unless a mirror shows.",
            color = Dim, fontSize = 10.5.sp)
    }
}

// ---------------------------------------------------------------- deskHPSDR's RX menu (rx_menu.c)

/** The RX menu's items that matter on an HL2 (Randal, 2026-11-24); the others are on the "later" list. */
@Composable
internal fun RxSheet(st: Status) {
    val d by Radio.dsp.collectAsState()
    val ctx = androidx.compose.ui.platform.LocalContext.current
    val phone by Radio.phoneAudio.collectAsState()
    SheetTitle("RX")
    val rate by Radio.sampleRate.collectAsState()
    Label("Sample rate: how wide a band the radio hears at once (restarts the stream)")
    val rates = listOf(96000, 192000, 384000)
    Seg(rates.map { "${it / 1000} kHz" }, rates.indexOf(rate)) {
        if (rates[it] != rate) {
            Radio.setSampleRate(rates[it])
            if (st.phase == Phase.RUNNING) RadioService.restart(ctx)
        }
    }
    Label("HL2 ADC auto gain: the radio sets its own LNA gain, lowering it when the ADC overloads")
    Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
        Toggle("Auto gain RxPGA", on(d, Dsp.AUTOGAIN), Modifier.weight(1f)) { set(Dsp.AUTOGAIN, it) }
        Toggle("Time-regulated", on(d, Dsp.AUTOGAIN_TIME), Modifier.weight(1f), enabled = on(d, Dsp.AUTOGAIN),
            sub = "re-checks every 30 s") { set(Dsp.AUTOGAIN_TIME, it) }
    }
    Label("Digital modes: dial offset, so the frequency shown is the one in use")
    Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
        Box(Modifier.weight(1f)) {
            Setting("DIGU offset", d[Dsp.DIGU_OFFSET], 0f..4000f, 10.0, { "%.0f Hz".format(it) }) { Radio.setDsp(Dsp.DIGU_OFFSET, it) }
        }
        Box(Modifier.weight(1f)) {
            Setting("DIGL offset", d[Dsp.DIGL_OFFSET], 0f..4000f, 10.0, { "%.0f Hz".format(it) }) { Radio.setDsp(Dsp.DIGL_OFFSET, it) }
        }
    }
    Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
        SmallButton("Offset RTTY (1500 / 2210)") { Radio.setDsp(Dsp.DIGU_OFFSET, 1500.0); Radio.setDsp(Dsp.DIGL_OFFSET, 2210.0) }
        SmallButton("Offset off") { Radio.setDsp(Dsp.DIGU_OFFSET, 0.0); Radio.setDsp(Dsp.DIGL_OFFSET, 0.0) }
    }
    Label("Audio")
    Seg(listOf("Stereo", "Left only", "Right only"), i(d, Dsp.AUDIO_CHANNEL)) { Radio.setDsp(Dsp.AUDIO_CHANNEL, it.toDouble()) }
    Row(horizontalArrangement = Arrangement.spacedBy(6.dp), verticalAlignment = Alignment.CenterVertically) {
        Toggle("Mute audio", on(d, Dsp.MUTE_RADIO), Modifier.weight(1f), sub = "HL2+ and phone") { set(Dsp.MUTE_RADIO, it) }
        SmallButton("Test audio", Modifier.weight(1f)) { Radio.setDsp(Dsp.TEST_AUDIO, 1.0) }
    }
    if (!phone) Text("Test audio plays on the phone: turn PHONE on first.", color = Dim, fontSize = 10.5.sp)
    Label("RX audio network reserve: a bigger audio buffer, against dropouts on WiFi")
    Row(horizontalArrangement = Arrangement.spacedBy(12.dp), verticalAlignment = Alignment.CenterVertically) {
        Toggle("Reserve", on(d, Dsp.RESERVE_ON), Modifier.width(120.dp)) { set(Dsp.RESERVE_ON, it) }
        Box(Modifier.weight(1f)) {
            Setting("Audio reserve", d[Dsp.RESERVE_MS], 5f..500f, 5.0, { "%.0f ms".format(it) }, on(d, Dsp.RESERVE_ON)) {
                Radio.setDsp(Dsp.RESERVE_MS, it)
            }
        }
    }
}

// ---------------------------------------------------------------- the bottom row (D9)

/** Every button the bottom row can hold: id to name. deskHPSDR's toolbar holds actions; these are ours. */
internal val ALL_CHIPS = listOf(
    "NR" to "NR", "NB" to "NB", "ANF" to "ANF", "SNB" to "SNB", "NOTCH" to "NOTCH", "SQL" to "SQL", "EQ" to "EQ",
    "BINAURAL" to "BINAURAL", "CWPEAK" to "CW PEAK", "BAND" to "BAND", "MODE" to "MODE", "AF" to "AF", "LNA" to "LNA",
    "PHONE" to "PHONE", "FILTER" to "FILTER", "AGC" to "AGC", "STEP" to "STEP", "SPAN" to "SPAN", "RIT" to "RIT",
    "CTUN" to "CTUN", "LOCK" to "LOCK", "AB" to "A⇄B",
)

/** One chip of the bottom row. Toggles: tap = on/off, hold = their settings. */
@Composable
internal fun ChipFor(id: String, st: Status, open: (Sheet) -> Unit, modifier: Modifier = Modifier) {
    val d by Radio.dsp.collectAsState()
    val freq by Radio.freq.collectAsState()
    val noise = { open(Sheet.NOISE) }
    when (id) {
        "NR" -> {
            val nr = i(d, Dsp.NR); val nb = i(d, Dsp.NB)
            val v = listOfNotNull(Dsp.NR_NAMES[nr].takeIf { nr > 0 }, Dsp.NB_NAMES[nb].takeIf { nb > 0 }).joinToString("+").ifEmpty { "Off" }
            Chip("NR", v, nr > 0 || nb > 0, modifier, noise, noise)
        }
        "NB" -> Chip("NB", Dsp.NB_NAMES[i(d, Dsp.NB)], i(d, Dsp.NB) > 0, modifier, noise) {
            Radio.setDsp(Dsp.NB, ((i(d, Dsp.NB) + 1) % 3).toDouble())
        }
        "ANF" -> Chip("ANF", if (on(d, Dsp.ANF)) "On" else "Off", on(d, Dsp.ANF), modifier, noise) { set(Dsp.ANF, !on(d, Dsp.ANF)) }
        "SNB" -> Chip("SNB", if (on(d, Dsp.SNB)) "On" else "Off", on(d, Dsp.SNB), modifier, noise) { set(Dsp.SNB, !on(d, Dsp.SNB)) }
        "NOTCH" -> Chip("NOTCH", if (on(d, Dsp.MNF)) "%.0f Hz".format(d[Dsp.MNF_WIDTH]) else "Off", on(d, Dsp.MNF), modifier, noise) {
            notchOnOff(!on(d, Dsp.MNF), st)
        }
        "SQL" -> Chip("SQL", if (on(d, Dsp.SQL_ON)) "On" else "Off", on(d, Dsp.SQL_ON), modifier, noise) { set(Dsp.SQL_ON, !on(d, Dsp.SQL_ON)) }
        "EQ" -> Chip("EQ", if (on(d, Dsp.EQ_ON)) "On" else "Off", on(d, Dsp.EQ_ON), modifier, { open(Sheet.EQ) }) { set(Dsp.EQ_ON, !on(d, Dsp.EQ_ON)) }
        "BINAURAL" -> Chip("BINAURAL", if (on(d, Dsp.BINAURAL)) "On" else "Off", on(d, Dsp.BINAURAL), modifier, { open(Sheet.DSP) }) {
            set(Dsp.BINAURAL, !on(d, Dsp.BINAURAL))
        }
        "CWPEAK" -> Chip("CW PEAK", if (on(d, Dsp.CW_PEAK)) "On" else "Off", on(d, Dsp.CW_PEAK), modifier, { open(Sheet.DSP) }) {
            set(Dsp.CW_PEAK, !on(d, Dsp.CW_PEAK))
        }
        "BAND" -> Chip("BAND", BandPlan.band(freq)?.name ?: "GEN", modifier = modifier) { open(Sheet.BAND) }
        "MODE" -> Chip("MODE", Modes.name(st.mode), modifier = modifier) { open(Sheet.MODE) }
        "AF" -> {
            val vol by Radio.volumeDb.collectAsState()
            Chip("AF", "${((vol + 40) / 40 * 100).roundToInt()}", modifier = modifier) { open(Sheet.AF) }
        }
        "LNA" -> {
            val lna by Radio.lnaDb.collectAsState()
            Chip("LNA", "%+.0f dB".format(lna), modifier = modifier) { open(Sheet.LNA) }
        }
        "PHONE" -> {
            val phone by Radio.phoneAudio.collectAsState()
            Chip("PHONE", if (phone) "On" else "Off", phone, modifier) { Radio.setPhoneAudio(!phone) }
        }
        "FILTER" -> Chip("FILTER", widthLabel(st.filterHigh - st.filterLow), modifier = modifier) { open(Sheet.FILTER) }
        "AGC" -> Chip("AGC", AGC_NAMES.getOrElse(st.agc) { "?" }, modifier = modifier) { open(Sheet.AGC) }
        "STEP" -> {
            val step by Radio.step.collectAsState()
            Chip("STEP", if (step >= 1000) "${step / 1000} kHz" else "$step Hz", modifier = modifier) {
                val steps = listOf(1, 10, 50, 100, 500, 1000)
                Radio.setStep(steps[(steps.indexOf(step) + 1) % steps.size])
            }
        }
        "SPAN" -> {
            val span by Radio.span.collectAsState()
            Chip("SPAN", "${(span / 1000).roundToInt()} kHz", modifier = modifier) {
                val spans = listOf(6_000.0, 12_000.0, 24_000.0, 48_000.0, 96_000.0, 182_000.0, 364_000.0)
                    .filter { it <= st.sampleRate * 0.95 }
                Radio.setSpan(spans.firstOrNull { it > span * 1.05 } ?: spans.first())
            }
        }
        "RIT" -> Chip("RIT", if (st.ritOn) "%+d".format(st.rit) else "Off", st.ritOn, modifier, { open(Sheet.VFO) }) { Radio.rit(!st.ritOn) }
        "CTUN" -> Chip("CTUN", if (st.ctun) "On" else "Off", st.ctun, modifier, { open(Sheet.VFO) }) { Radio.ctun(!st.ctun) }
        "LOCK" -> Chip("LOCK", if (st.locked) "On" else "Off", st.locked, modifier, { open(Sheet.VFO) }) { Radio.lock(!st.locked) }
        "AB" -> Chip("A⇄B", "swap", modifier = modifier, onLongClick = { open(Sheet.VFO) }) { Radio.vfoOp(2) }
    }
}

/** deskHPSDR's Toolbar menu, the phone's way (D9): add buttons from the list, remove with ×, hold and drag to move. */
@OptIn(ExperimentalLayoutApi::class, ExperimentalFoundationApi::class)
@Composable
internal fun ToolbarEditor(st: Status, close: () -> Unit) {
    val ids by Radio.chips.collectAsState()
    Box(Modifier.fillMaxSize().background(Color(0x66000000)).clickable(onClick = close)) {
        Column(
            Modifier.align(Alignment.TopCenter).padding(top = 60.dp).widthIn(max = 680.dp).fillMaxWidth()
                .clip(RoundedCornerShape(18.dp)).background(Color(0xF20C111E)).border(1.dp, Line, RoundedCornerShape(18.dp))
                .clickable(enabled = false) {}.padding(14.dp),
            verticalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
                SheetTitle("Bottom row")
                Spacer(Modifier.weight(1f))
                SmallButton("Reset") { Radio.setChips(Radio.DEFAULT_CHIPS) }
                Spacer(Modifier.width(8.dp))
                SmallButton("Done", on = true, onClick = close)
            }
            Text("Tap a button to add it to the row. In the row: × removes, hold and drag moves it.", color = Dim, fontSize = 11.sp)
            FlowRow(horizontalArrangement = Arrangement.spacedBy(6.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                ALL_CHIPS.filter { it.first !in ids }.forEach { (id, name) ->
                    SmallButton(name) { Radio.setChips(ids + id) }
                }
            }
        }
        // the row itself, in edit mode, where it always is
        Row(Modifier.align(Alignment.BottomStart).padding(10.dp).clickable(enabled = false) {},
            horizontalArrangement = Arrangement.spacedBy(7.dp)) {
            ids.forEach { id -> key(id) {   // state and gesture follow the chip as it moves
                var dx by remember(id) { mutableStateOf(0f) }
                Box(
                    Modifier.offset { IntOffset(dx.roundToInt(), 0) }.pointerInput(id) {
                        val stepPx = 76.dp.toPx()
                        detectDragGesturesAfterLongPress(onDragEnd = { dx = 0f }, onDragCancel = { dx = 0f }) { c, a ->
                            c.consume()
                            dx += a.x
                            val cur = Radio.chips.value
                            val k = cur.indexOf(id)
                            if (dx > stepPx / 2 && k < cur.size - 1) {
                                Radio.setChips(cur.toMutableList().also { it[k] = it[k + 1]; it[k + 1] = id }); dx -= stepPx
                            } else if (dx < -stepPx / 2 && k > 0) {
                                Radio.setChips(cur.toMutableList().also { it[k] = it[k - 1]; it[k - 1] = id }); dx += stepPx
                            }
                        }
                    },
                ) {
                    Chip(ALL_CHIPS.firstOrNull { it.first == id }?.second ?: id, "", onClick = {})
                    Box(
                        Modifier.align(Alignment.TopEnd).offset(x = 5.dp, y = (-6).dp).size(18.dp).clip(CircleShape)
                            .background(Color(0xFFFF4D4D)).clickable { Radio.setChips(Radio.chips.value.filter { it != id }) },
                        contentAlignment = Alignment.Center,
                    ) { Text("×", color = Color.White, fontSize = 12.sp, fontWeight = FontWeight.Bold) }
                }
            } }
        }
    }
}
