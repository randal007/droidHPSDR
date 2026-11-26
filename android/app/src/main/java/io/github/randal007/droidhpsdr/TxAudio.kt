// droidHPSDR - GPL v3 or later.
//
// M7 step 2, the TX audio chain (Randal's picks from m7-round2, 2026-11-26):
// the TX audio drawer from the right edge (N1, like the Noise drawer) with
// every switch of deskHPSDR's chain, and its pages: the CFC curves (N4), the
// TX equalizer graph (N5), VOX + noise gate + monitor with a live mic bar
// (N6). The chips PROC, VOX and MON open it. The radio side is
// android/native/app/transmit.c (tx_audio_apply), the same lines as
// deskHPSDR's tx_menu.c / equalizer_menu.c / vox_menu.c.
package io.github.randal007.droidhpsdr

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.awaitEachGesture
import androidx.compose.foundation.gestures.awaitFirstDown
import androidx.compose.foundation.gestures.drag
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
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
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.layout.onSizeChanged
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.drawText
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.rememberTextMeasurer
import androidx.compose.ui.unit.IntSize
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlin.math.abs
import kotlin.math.log10
import kotlin.math.pow
import kotlin.math.roundToInt

private val TxOrange = Color(0xFFFF9A4A)
private val CfcCyan = Color(0xFF3FD0FF)
private val GatePurple = Color(0xFFB48CFF)

private fun on(v: DoubleArray, f: Int) = v[f] != 0.0
private fun setOn(f: Int, b: Boolean) = Tx.set(f, if (b) 1.0 else 0.0)
private val live get() = Radio.running && !Radio.playback.value

/** vox_threshold is linear (vox_menu.c's 0..1000 / 1000); the page shows it in dB like the mic bar. */
internal fun linToDb(x: Double) = if (x <= 1e-6) -120.0 else 20 * log10(x)
internal fun dbToLin(db: Double) = 10.0.pow(db / 20)

/** The mode the settings belong to: the TX VFO's (with split it isn't the RX one). */
private fun txModeName(v: DoubleArray, st: Status) = Modes.name(if (live) v[Tx.TX_MODE].toInt() else st.mode)

/**
 * A drag that only starts on something to drag: [start] says whether the
 * finger came down on one. Otherwise the touch is left alone, so the page
 * around it still scrolls (code review: detectDragGestures took every drag).
 */
private fun Modifier.dragOn(enabled: Boolean, start: (Offset) -> Boolean, move: (Offset) -> Unit, end: () -> Unit) =
    pointerInput(enabled) {
        if (!enabled) return@pointerInput
        awaitEachGesture {
            val down = awaitFirstDown(requireUnconsumed = false)
            if (!start(down.position)) return@awaitEachGesture
            down.consume()
            drag(down.id) { ch -> move(ch.position); ch.consume() }
            end()
        }
    }

/** The VOX chip's tap: VOX keys the radio from the mic, so it says when it can't. */
internal fun voxToggle() {
    val v = Tx.v.value
    if (!live) { Tx.note.value = "Start the radio to use VOX" to System.currentTimeMillis(); return }
    val turnOn = !on(v, Tx.VOX)
    setOn(Tx.VOX, turnOn)
    if (turnOn && !Tx.enabled.value) Tx.note.value = "VOX is on, but Transmit is off: turn it on in ☰ → TX" to System.currentTimeMillis()
}

/** What the PROC chip shows: the processor level, or CFC / leveler when only those run. */
internal fun procLabel(v: DoubleArray) = when {
    on(v, Tx.COMP) -> "%.0f dB".format(v[Tx.COMP_LEVEL])
    on(v, Tx.CFC) -> "CFC"
    on(v, Tx.LEV) -> "LEV"
    else -> "Off"
}

// ---------------------------------------------------------------- the drawer (N1)

/** deskHPSDR's TX audio chain on one drawer from the right edge; [start] = "VOX" opens the VOX page. */
@Composable
internal fun TxAudioDrawer(st: Status, start: String?, close: () -> Unit) {
    val v by Tx.v.collectAsState()
    var page by remember { mutableStateOf(start) }
    Box(Modifier.fillMaxSize().background(Color(0x55000000)).clickable(onClick = close)) {
        when (page) {
            "CFC" -> { CfcPage(v, st) { page = null }; return@Box }
            "EQ" -> { TxEqPage(v, st) { page = null }; return@Box }
            "VOX" -> { VoxGatePage(v) { page = null }; return@Box }
        }
        Column(
            Modifier.align(Alignment.CenterEnd).fillMaxHeight().width(340.dp)
                .clip(RoundedCornerShape(topStart = 18.dp, bottomStart = 18.dp)).background(Color(0xF0090C16))
                .border(1.dp, Line, RoundedCornerShape(topStart = 18.dp, bottomStart = 18.dp))
                .clickable(enabled = false) {}.verticalScroll(rememberScrollState()).padding(14.dp),
            verticalArrangement = Arrangement.spacedBy(6.dp),
        ) {
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween, verticalAlignment = Alignment.CenterVertically) {
                SheetTitle("TX audio")
                Text("${txModeName(v, st)} · kept per mode", color = Dim, fontSize = 10.sp)
            }
            if (!live) Text("Start the radio to change these: they belong to the mode it is in.", color = Amb, fontSize = 10.5.sp)
            val en = live
            Toggle("Speech processor", on(v, Tx.COMP), Modifier.fillMaxWidth(), enabled = en) { setOn(Tx.COMP, it) }
            Setting("LEVEL", v[Tx.COMP_LEVEL], 0f..20f, 1.0, { "%.0f dB".format(it) }, en && on(v, Tx.COMP)) { Tx.set(Tx.COMP_LEVEL, it) }
            Toggle("Leveler", on(v, Tx.LEV), Modifier.fillMaxWidth(), enabled = en) { setOn(Tx.LEV, it) }
            Setting("GAIN", v[Tx.LEV_GAIN], 0f..15f, 1.0, { "%.0f dB".format(it) }, en && on(v, Tx.LEV)) { Tx.set(Tx.LEV_GAIN, it) }
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                Toggle("CESSB", on(v, Tx.CESSB), Modifier.weight(1f), enabled = en, sub = "auto") { setOn(Tx.CESSB, it) }
                Toggle("Phase rot.", on(v, Tx.PHROT), Modifier.weight(1f), enabled = en) { setOn(Tx.PHROT, it) }
            }
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                Toggle("CFC", on(v, Tx.CFC), Modifier.weight(1f), enabled = en) { setOn(Tx.CFC, it) }
                Toggle("TX EQ", on(v, Tx.EQ), Modifier.weight(1f), enabled = en) { setOn(Tx.EQ, it) }
            }
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                Toggle("Noise gate", on(v, Tx.DEXP), Modifier.weight(1f), enabled = en) { setOn(Tx.DEXP, it) }
                Toggle("Monitor", on(v, Tx.MON), Modifier.weight(1f), enabled = en) { setOn(Tx.MON, it) }
            }
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                SmallButton("CFC ›", Modifier.weight(1f)) { page = "CFC" }
                SmallButton("EQ ›", Modifier.weight(1f)) { page = "EQ" }
                SmallButton("VOX / Gate ›", Modifier.weight(1.3f)) { page = "VOX" }
            }
            Label("MORE (deskHPSDR's TX menu)")
            Setting("Leveler decay", v[Tx.LEV_DECAY], 0f..500f, 1.0, { "%.0f ms".format(it) }, en && on(v, Tx.LEV)) { Tx.set(Tx.LEV_DECAY, it) }
            val ph = en && on(v, Tx.PHROT)
            Setting("Phase rotator stages", v[Tx.PHROT_STAGES], 1f..15f, 1.0, { "%.0f".format(it) }, ph) { Tx.set(Tx.PHROT_STAGES, it) }
            Setting("Phase rotator corner", v[Tx.PHROT_FREQ], 1f..500f, 1.0, { "%.0f Hz".format(it) }, ph) { Tx.set(Tx.PHROT_FREQ, it) }
            Toggle("Local mic preamp", on(v, Tx.ADDGAIN), Modifier.fillMaxWidth(), enabled = en, sub = "extra mic gain") { setOn(Tx.ADDGAIN, it) }
            Setting("Preamp gain", v[Tx.ADDGAIN_GAIN], 1f..20f, 1.0, { "%.0f dB".format(it) }, en && on(v, Tx.ADDGAIN)) { Tx.set(Tx.ADDGAIN_GAIN, it) }
            Toggle("TX EQ ctfmode", on(v, Tx.CTFMODE), Modifier.fillMaxWidth(), enabled = en, sub = "WDSP") { setOn(Tx.CTFMODE, it) }
        }
    }
}

/** The pages' frame: a panel in the middle with a back button, like the Noise drawer's settings pages. */
@Composable
private fun Page(title: String, back: () -> Unit, right: @Composable () -> Unit = {}, body: @Composable () -> Unit) {
    Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
        Column(
            Modifier.widthIn(max = 700.dp).fillMaxWidth().padding(8.dp).clip(RoundedCornerShape(18.dp))
                .background(Color(0xF50A0E1A)).border(1.dp, Line, RoundedCornerShape(18.dp))
                .clickable(enabled = false) {}.verticalScroll(rememberScrollState()).padding(14.dp),
            verticalArrangement = Arrangement.spacedBy(6.dp),
        ) {
            Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                SmallButton("‹ TX audio", onClick = back)
                SheetTitle(title)
                Spacer(Modifier.weight(1f))
                right()
            }
            if (!live) Text("Start the radio to change these.", color = Amb, fontSize = 10.5.sp)
            body()
        }
    }
}

// ---------------------------------------------------------------- a graph of points to drag (N4, N5)

private const val G_FMIN = 30.0
private const val G_FMAX = 16000.0

/** The graph's scales, the same for drawing and for the finger. */
private class Geo(val w: Float, val h: Float, val padL: Float, val padR: Float, val padT: Float, val padB: Float,
                  val ymin: Double, val ymax: Double) {
    fun gx(f: Double) = padL + (log10(f.coerceAtLeast(G_FMIN) / G_FMIN) / log10(G_FMAX / G_FMIN)).toFloat() * (w - padL - padR)
    fun gy(g: Double) = padT + ((ymax - g) / (ymax - ymin)).toFloat() * (h - padT - padB)
    fun fx(x: Float) = G_FMIN * 10.0.pow(((x - padL) / (w - padL - padR)).toDouble() * log10(G_FMAX / G_FMIN))
    fun gyInv(y: Float) = ymax - (y - padT) / (h - padT - padB) * (ymax - ymin)
}

/** One curve of points 1..12: frequencies from [freq0], values from [val0] (fields of Tx.v). */
private class Curve(val freq0: Int, val val0: Int, val color: Color, val lo: Double, val hi: Double)

/**
 * Points to drag, as the RX equalizer (Dsp.kt EqPanel): up/down sets the
 * value, sideways the frequency (kept in order, 10 Hz apart, as
 * equalizer_menu.c). Curves on one graph share the y scale [ymin]..[ymax].
 */
@Composable
private fun PointGraph(v: DoubleArray, curves: List<Curve>, ymin: Double, ymax: Double, enabled: Boolean) {
    val vRef by rememberUpdatedState(v)
    var sel by remember { mutableStateOf<Pair<Int, Int>?>(null) }   // curve, point
    val measurer = rememberTextMeasurer()
    val span = ymax - ymin
    val dens = LocalDensity.current
    var sz by remember { mutableStateOf(IntSize.Zero) }
    val hitPx = with(dens) { 36.dp.toPx() }
    val geo = Geo(sz.width.toFloat(), sz.height.toFloat(), with(dens) { 34.dp.toPx() }, with(dens) { 10.dp.toPx() },
        with(dens) { 8.dp.toPx() }, with(dens) { 18.dp.toPx() }, ymin, ymax)
    val geoNow by rememberUpdatedState(geo)   // the drag keeps its lambdas: they read the current size through this
    Canvas(
        Modifier.fillMaxWidth().height(190.dp).onSizeChanged { sz = it }.dragOn(enabled,
            start = { p ->
                val d = vRef
                var best: Pair<Int, Int>? = null
                var bestD = hitPx
                curves.forEachIndexed { ci, c ->
                    for (k in 1..12) {
                        val dist = (Offset(geoNow.gx(d[c.freq0 + k]), geoNow.gy(d[c.val0 + k])) - p).getDistance()
                        if (dist < bestD) { bestD = dist; best = ci to k }
                    }
                }
                sel = best
                best != null
            },
            move = { p ->
                val (ci, k) = sel ?: return@dragOn
                val c = curves[ci]
                val d = vRef
                val g = geoNow.gyInv(p.y).coerceIn(c.lo, c.hi).roundToInt().toDouble()
                if (g != d[c.val0 + k]) Tx.set(c.val0 + k, g)
                val lo = if (k > 1) d[c.freq0 + k - 1] + 10 else 10.0
                val hi = if (k < 12) d[c.freq0 + k + 1] - 10 else G_FMAX
                // neighbours not 20 Hz apart (or not loaded yet): leave the frequency (code review: coerceIn threw)
                if (lo <= hi) {
                    val f = ((geoNow.fx(p.x) / 10).roundToInt() * 10.0).coerceIn(lo, hi)
                    if (abs(f - d[c.freq0 + k]) >= 10) Tx.set(c.freq0 + k, f)
                }
            },
            end = { sel = null },
        ),
    ) {
        val padL = 34.dp.toPx(); val padR = 10.dp.toPx(); val padT = 8.dp.toPx(); val padB = 18.dp.toPx()
        fun gx(f: Double) = padL + (log10(f.coerceAtLeast(G_FMIN) / G_FMIN) / log10(G_FMAX / G_FMIN)).toFloat() * (size.width - padL - padR)
        fun gy(g: Double) = padT + ((ymax - g) / span).toFloat() * (size.height - padT - padB)
        val ts = TextStyle(color = Dim, fontSize = 9.5.sp)
        val step = if (span > 30) 10 else 5
        var g = (ymin / step).roundToInt() * step
        while (g <= ymax) {
            drawLine(Color(if (g == 0) 0x33FFFFFF else 0x12FFFFFF), Offset(padL, gy(g.toDouble())), Offset(size.width - padR, gy(g.toDouble())))
            val t = measurer.measure(if (g > 0) "+$g" else "$g", ts)
            drawText(t, topLeft = Offset(2.dp.toPx(), gy(g.toDouble()) - t.size.height / 2))
            g += step
        }
        for (f in listOf(50.0, 100.0, 300.0, 1000.0, 3000.0, 8000.0)) {
            val t = measurer.measure(if (f >= 1000) "${(f / 1000).toInt()}k" else "${f.toInt()}", ts)
            drawText(t, topLeft = Offset(gx(f) - t.size.width / 2, size.height - t.size.height))
        }
        curves.forEachIndexed { ci, c ->
            val col = if (enabled) c.color else c.color.copy(alpha = 0.4f)
            val pts = (1..12).map { Offset(gx(v[c.freq0 + it]), gy(v[c.val0 + it])) }
            val path = Path().apply { moveTo(pts[0].x, pts[0].y); pts.drop(1).forEach { lineTo(it.x, it.y) } }
            drawPath(path, col, style = Stroke(2.4.dp.toPx()))
            pts.forEachIndexed { k, p ->
                val big = sel == ci to k + 1
                drawCircle(if (big) col else Color(0xFF0B1220), if (big) 9.dp.toPx() else 6.dp.toPx(), p)
                drawCircle(col, if (big) 9.dp.toPx() else 6.dp.toPx(), p, style = Stroke(2.dp.toPx()))
            }
        }
        sel?.let { (ci, k) ->
            val c = curves[ci]
            val f = v[c.freq0 + k]
            val t = measurer.measure(
                "%s  %+.0f dB".format(if (f >= 1000) "%.2f kHz".format(f / 1000) else "%.0f Hz".format(f), v[c.val0 + k]),
                TextStyle(color = Txt, fontSize = 11.sp))
            val p = Offset(gx(f), gy(v[c.val0 + k]))
            drawText(t, topLeft = Offset((p.x - t.size.width / 2).coerceIn(padL, size.width - t.size.width), (p.y - 30.dp.toPx()).coerceAtLeast(0f)))
        }
    }
}

// ---------------------------------------------------------------- CFC (N4)

@Composable
private fun CfcPage(v: DoubleArray, st: Status, back: () -> Unit) {
    val en = live
    Page("CFC · ${txModeName(v, st)}", back, right = {
        Toggle("CFC", on(v, Tx.CFC), Modifier.width(96.dp), enabled = en) { setOn(Tx.CFC, it) }
        Toggle("Post EQ", on(v, Tx.CFC_EQ), Modifier.width(118.dp), enabled = en) { setOn(Tx.CFC_EQ, it) }
        SmallButton("Flat") {
            if (en) for (k in 1..12) { Tx.set(Tx.CFC_LVL0 + k, 0.0); Tx.set(Tx.CFC_POST0 + k, 0.0) }
        }
    }) {
        Row(horizontalArrangement = Arrangement.spacedBy(14.dp)) {
            Text("● compression (0..20 dB)", color = Amb, fontSize = 10.5.sp)
            Text("● post EQ (−20..+20 dB)", color = CfcCyan, fontSize = 10.5.sp)
        }
        PointGraph(v, listOf(
            Curve(Tx.CFC_FREQ0, Tx.CFC_LVL0, Amb, 0.0, 20.0),
            Curve(Tx.CFC_FREQ0, Tx.CFC_POST0, CfcCyan, -20.0, 20.0),
        ), -20.0, 20.0, en)
        Row(horizontalArrangement = Arrangement.spacedBy(14.dp)) {
            Box(Modifier.weight(1f)) {
                Setting("PRE COMP (all frequencies)", v[Tx.CFC_LVL0], 0f..20f, 1.0, { "%.0f dB".format(it) }, en) { Tx.set(Tx.CFC_LVL0, it) }
            }
            Box(Modifier.weight(1f)) {
                Setting("POST GAIN", v[Tx.CFC_POST0], -20f..20f, 1.0, { "%+.0f dB".format(it) }, en) { Tx.set(Tx.CFC_POST0, it) }
            }
        }
        Text("drag a point: ↕ level, ↔ frequency (both curves share the frequencies) · amber: how hard each band is compressed · cyan: the EQ after it",
            color = Dim, fontSize = 10.sp)
    }
}

// ---------------------------------------------------------------- TX equalizer (N5)

@Composable
private fun TxEqPage(v: DoubleArray, st: Status, back: () -> Unit) {
    val en = live
    Page("TX equalizer · ${txModeName(v, st)}", back, right = {
        Toggle("Enable", on(v, Tx.EQ), Modifier.width(116.dp), enabled = en) { setOn(Tx.EQ, it) }
        SmallButton("Flat") { if (en) for (k in 1..12) Tx.set(Tx.EQ_GAIN0 + k, 0.0) }
    }) {
        PointGraph(v, listOf(Curve(Tx.EQ_FREQ0, Tx.EQ_GAIN0, TxOrange, -20.0, 20.0)), -20.0, 20.0, en)
        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(12.dp)) {
            Box(Modifier.weight(1f)) {
                Setting("Added gain (all frequencies)", v[Tx.EQ_GAIN0], -20f..20f, 1.0, { "%+.0f dB".format(it) }, en) { Tx.set(Tx.EQ_GAIN0, it) }
            }
            Text("drag a point: ↕ gain, ↔ frequency · the same graph as the RX equalizer · kept per mode",
                color = Dim, fontSize = 10.5.sp, modifier = Modifier.weight(1f))
        }
    }
}

// ---------------------------------------------------------------- VOX, noise gate, monitor (N6)

private const val BAR_MIN = -60.0   // dB, the mic bar's left end

/** The mic level (vox.c's detector, DEXP's peak) with the VOX threshold and the gate trigger to drag. */
@Composable
private fun MicBar(v: DoubleArray, enabled: Boolean) {
    val vRef by rememberUpdatedState(v)
    var drag by remember { mutableIntStateOf(0) }   // 1 VOX, 2 gate
    val measurer = rememberTextMeasurer()
    var bw by remember { mutableStateOf(1f) }
    val hitPx = with(LocalDensity.current) { 40.dp.toPx() }
    Canvas(
        Modifier.fillMaxWidth().height(58.dp).onSizeChanged { bw = it.width.toFloat() }.dragOn(enabled,
            start = { p ->
                fun xOf(db: Double) = ((db - BAR_MIN) / -BAR_MIN).toFloat().coerceIn(0f, 1f) * bw
                val d = vRef
                val dv = abs(p.x - xOf(linToDb(d[Tx.VOX_THRESH])))
                val dg = abs(p.x - xOf(d[Tx.DEXP_TRIGGER]))
                drag = when {
                    dv <= dg && dv < hitPx -> 1
                    dg < hitPx -> 2
                    else -> 0
                }
                drag != 0
            },
            move = { p ->
                val db = BAR_MIN + (p.x / bw).coerceIn(0f, 1f) * -BAR_MIN
                when (drag) {
                    1 -> Tx.set(Tx.VOX_THRESH, dbToLin(db.roundToInt().toDouble()).coerceIn(0.0, 1.0))
                    2 -> Tx.set(Tx.DEXP_TRIGGER, db.roundToInt().toDouble().coerceIn(-40.0, -10.0))
                }
            },
            end = { drag = 0 },
        ),
    ) {
        val top = 16.dp.toPx(); val h = 16.dp.toPx()
        fun xOf(db: Double) = ((db - BAR_MIN) / -BAR_MIN).toFloat().coerceIn(0f, 1f) * size.width
        drawRoundRect(Color(0x12FFFFFF), Offset(0f, top), Size(size.width, h), androidx.compose.ui.geometry.CornerRadius(h / 2))
        val lvl = xOf(linToDb(v[Tx.VOX_PEAK]))
        if (lvl > 0) drawRoundRect(Brush.horizontalGradient(listOf(Color(0x593FD0FF), Color(0xD93FD0FF))), Offset(0f, top), Size(lvl, h),
            androidx.compose.ui.geometry.CornerRadius(h / 2))
        val ts = TextStyle(fontSize = 10.sp)
        fun marker(db: Double, c: Color, label: String, above: Boolean) {
            val x = xOf(db)
            drawLine(c, Offset(x, top - 6.dp.toPx()), Offset(x, top + h + 6.dp.toPx()), strokeWidth = 3.dp.toPx())
            val t = measurer.measure(label, ts.copy(color = c))
            drawText(t, topLeft = Offset((x - t.size.width / 2).coerceIn(0f, size.width - t.size.width),
                if (above) 0f else top + h + 7.dp.toPx()))
        }
        marker(linToDb(v[Tx.VOX_THRESH]), Amb, "VOX %.0f dB".format(linToDb(v[Tx.VOX_THRESH]).coerceAtLeast(BAR_MIN)), true)
        marker(v[Tx.DEXP_TRIGGER], GatePurple, "gate %.0f dB".format(v[Tx.DEXP_TRIGGER]), false)
    }
    Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
        for (t in listOf("−60", "−40", "−20", "0 dB")) Text(t, color = Dim, fontSize = 9.sp)
    }
}

@Composable
private fun VoxGatePage(v: DoubleArray, back: () -> Unit) {
    val en = live
    val enabled by Tx.enabled.collectAsState()
    Page("VOX & noise gate", back, right = { Text("VOX is off at every start", color = Dim, fontSize = 10.sp) }) {
        Label("MIC LEVEL · drag the markers (VOX amber, gate purple)")
        MicBar(v, en)
        Row(horizontalArrangement = Arrangement.spacedBy(16.dp)) {
            Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(5.dp)) {
                Label("VOX")
                Toggle("VOX", on(v, Tx.VOX), Modifier.fillMaxWidth(), enabled = en, sub = "keys the radio from the mic") { voxToggle() }
                if (on(v, Tx.VOX) && !enabled) Text("Transmit is off (☰ → TX): VOX can't key the radio", color = Amb, fontSize = 10.sp)
                Setting("HANG", v[Tx.VOX_HANG], 0f..1000f, 50.0, { "%.0f ms".format(it) }, en) { Tx.set(Tx.VOX_HANG, it) }
                Label("MONITOR (hear what you send)")
                Row(horizontalArrangement = Arrangement.spacedBy(6.dp), verticalAlignment = Alignment.CenterVertically) {
                    Toggle("Monitor", on(v, Tx.MON), Modifier.weight(1f), enabled = en) { setOn(Tx.MON, it) }
                    Box(Modifier.weight(1f)) {
                        Seg(listOf("Pre", "Post"), if (on(v, Tx.MON_POST)) 1 else 0, enabled = en, h = 34) { Tx.set(Tx.MON_POST, it.toDouble()) }
                    }
                }
                Setting("VOLUME", v[Tx.MON_GAIN], -40f..0f, 1.0, { "%.0f dB".format(it) }, en) { Tx.set(Tx.MON_GAIN, it) }
                Text("Pre: after mic gain, before WDSP · Post: after the whole chain. On the HL2+ phones and the phone's audio.",
                    color = Dim, fontSize = 10.sp)
            }
            Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(5.dp)) {
                Label("NOISE GATE (DEXP)")
                Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    Toggle("Gate", on(v, Tx.DEXP), Modifier.weight(1f), enabled = en) { setOn(Tx.DEXP, it) }
                    Toggle("Side filter", on(v, Tx.DEXP_FILTER), Modifier.weight(1.2f), enabled = en, sub = "VOX too") { setOn(Tx.DEXP_FILTER, it) }
                }
                Setting("EXPANSION", v[Tx.DEXP_EXP], 0f..30f, 1.0, { "%.0f dB".format(it) }, en) { Tx.set(Tx.DEXP_EXP, it) }
                Setting("ATTACK", v[Tx.DEXP_ATTACK], 1f..250f, 1.0, { "%.0f ms".format(it) }, en) { Tx.set(Tx.DEXP_ATTACK, it) }
                Setting("RELEASE", v[Tx.DEXP_RELEASE], 1f..500f, 1.0, { "%.0f ms".format(it) }, en) { Tx.set(Tx.DEXP_RELEASE, it) }
                Setting("HOLD", v[Tx.DEXP_HOLD], 10f..1500f, 10.0, { "%.0f ms".format(it) }, en) { Tx.set(Tx.DEXP_HOLD, it) }
                Setting("TAU (trigger attack)", v[Tx.DEXP_TAU], 1f..250f, 1.0, { "%.0f ms".format(it) }, en) { Tx.set(Tx.DEXP_TAU, it) }
                Setting("HYSTERESIS", v[Tx.DEXP_HYST], 0.05f..0.95f, 0.01, { "%.2f".format(it) }, en) { Tx.set(Tx.DEXP_HYST, it) }
                val sf = en && on(v, Tx.DEXP_FILTER)
                Setting("SIDE FILTER LOW", v[Tx.DEXP_FILTER_LOW], 0f..1200f, 25.0, { "%.0f Hz".format(it) }, sf) { Tx.set(Tx.DEXP_FILTER_LOW, it) }
                Setting("SIDE FILTER HIGH", v[Tx.DEXP_FILTER_HIGH], 500f..10000f, 25.0, { "%.0f Hz".format(it) }, sf) { Tx.set(Tx.DEXP_FILTER_HIGH, it) }
            }
        }
    }
}
