// droidHPSDR - GPL v3 or later.
//
// The operating screen: layout 1 ("glass cockpit") from the round-1
// mock-ups, picked by Randal on 2026-11-23, with the X6100 band bar and its
// segment tag. Sizes are dp at the S26 Ultra's 832 x 384 dp landscape.
package io.github.randal007.droidhpsdr

import android.app.Activity
import android.content.Intent
import androidx.activity.compose.BackHandler
import androidx.compose.foundation.layout.BoxWithConstraints
import android.net.Uri
import android.os.PowerManager
import android.provider.Settings
import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.combinedClickable
import androidx.compose.foundation.gestures.detectDragGesturesAfterLongPress
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.BasicTextField
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.hapticfeedback.HapticFeedbackType
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalHapticFeedback
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.core.AnimationState
import androidx.compose.animation.core.animateDecay
import androidx.compose.animation.core.exponentialDecay
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.awaitEachGesture
import androidx.compose.foundation.gestures.awaitFirstDown
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Slider
import androidx.compose.material3.SliderDefaults
import androidx.compose.material3.Text
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.input.pointer.util.VelocityTracker
import androidx.compose.ui.text.SpanStyle
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.buildAnnotatedString
import androidx.compose.ui.text.drawText
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.rememberTextMeasurer
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.withStyle
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.viewinterop.AndroidView
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlin.math.abs
import kotlin.math.ceil
import kotlin.math.roundToInt
import kotlin.math.roundToLong

internal val Bg = Color(0xFF05070D)
internal val Glass = Color(0xA80A101E)
internal val Line = Color(0x1AFFFFFF)
internal val Txt = Color(0xFFE9EEF7)
internal val Dim = Color(0xFF8B97AD)
internal val Acc = Color(0xFF3FD0FF)
internal val Amb = Color(0xFFFFB341)
internal val Grn = Color(0xFF3EE07A)
internal val Red = Color(0xFFFF5A50)


internal enum class Sheet { BAND, MODE, AF, LNA, FILTER, MENU, DISPLAY, FREQ, VFO, AGC, SDR, DISCOVERY, ABOUT, NOISE, EQ, DSP, TOOLBAR, RX }

internal fun Modifier.glass(r: Int = 14) =
    clip(RoundedCornerShape(r.dp)).background(Glass).border(1.dp, Line, RoundedCornerShape(r.dp))

@Composable
fun Screen(onStart: () -> Unit, onStop: () -> Unit) {
    MaterialTheme(colorScheme = darkColorScheme(primary = Acc, background = Bg, surface = Bg)) {
        val status by Radio.status.collectAsState()
        val freq by Radio.freq.collectAsState()
        val flat by Radio.flat.collectAsState()
        var sheet by remember { mutableStateOf<Sheet?>(null) }
        // deskHPSDR: a menu opened from the main menu goes back to it when closed
        var fromMenu by remember { mutableStateOf(false) }
        val dismiss = { sheet = if (fromMenu && sheet != Sheet.MENU) Sheet.MENU else null; fromMenu = false }
        val done = { sheet = null; fromMenu = false }
        BackHandler(enabled = sheet != null) { dismiss() }
        var flash by remember { mutableStateOf("" to 0L) }

        Box(Modifier.fillMaxSize().background(Bg)) {
            AndroidView(factory = { SpectrumView(it) }, modifier = Modifier.fillMaxSize())
            SpectrumOverlay(Modifier.fillMaxSize())

            MeterCard(status.meterDbm, Modifier.align(Alignment.TopStart).padding(10.dp))
            VfoPill(freq, status, { sheet = Sheet.MODE }, { sheet = Sheet.FILTER }, { sheet = Sheet.FREQ }, { sheet = Sheet.VFO },
                Modifier.align(Alignment.TopCenter).padding(top = 8.dp))
            SegmentTag(freq, Modifier.align(Alignment.TopCenter).padding(top = 76.dp))
            Flash(flash, Modifier.align(Alignment.TopEnd).padding(top = 66.dp, end = 12.dp))

            Row(Modifier.align(Alignment.TopEnd).padding(12.dp), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                // tap: 2D/3D; hold: the display settings
                RoundButton(if (flat) "2D" else "3D", onLongClick = { sheet = Sheet.DISPLAY }) {
                    Radio.setFlat(!flat)
                    flash = (if (!flat) "2D spectrum" else "3D: ${SpectrumView.STYLES[Radio.style.value]}") +
                        " · hold for 3D looks and settings" to System.currentTimeMillis()
                }
                RoundButton("☰") { sheet = Sheet.MENU; fromMenu = false }
            }

            ChipRow(status, { sheet = it; fromMenu = false }, Modifier.align(Alignment.BottomStart).padding(10.dp))

            if (status.phase == Phase.DEMO) {
                Text("DEMO · no radio", color = Amb, fontSize = 12.sp, fontWeight = FontWeight.Bold,
                    modifier = Modifier.align(Alignment.TopStart).padding(start = 12.dp, top = 112.dp)
                        .glass(8).padding(horizontal = 8.dp, vertical = 3.dp))
            } else if (status.phase != Phase.RUNNING) {
                StatusCard(status, onStart, Modifier.align(Alignment.Center))
            }
            when (sheet) {
                Sheet.VFO -> VfoPanel(freq, status, dismiss)
                Sheet.MENU -> MainMenu(status, onStart, done) { sheet = it; fromMenu = true }
                Sheet.NOISE -> NoiseDrawer(status, dismiss)
                Sheet.EQ -> EqPanel(status, dismiss)
                Sheet.TOOLBAR -> ToolbarEditor(status, dismiss)
                null -> {}
                else -> SheetHost(sheet!!, status, onStart, onStop, dismiss, done)
            }
        }
    }
}

// ---------------------------------------------------------------- spectrum overlay

/** VFO line, passband, frequency axis and band bar over the GL view, plus touch tuning. */
@Composable
private fun SpectrumOverlay(modifier: Modifier) {
    val freq by Radio.freq.collectAsState()
    val span by Radio.span.collectAsState()
    val status by Radio.status.collectAsState()
    val region by Radio.region.collectAsState()
    val peaks by Radio.peaks.collectAsState()
    val split by Radio.split.collectAsState()
    val flat by Radio.flat.collectAsState()
    val style by Radio.style.collectAsState()
    val dsp by Radio.dsp.collectAsState()
    val measurer = rememberTextMeasurer()
    val scope = rememberCoroutineScope()
    val fling = remember { arrayOfNulls<Job>(1) }
    // a single tap waits this long for a second one: double-tap puts the notch there (M5, D5)
    val tap = remember { arrayOfNulls<Job>(1) }
    val lastTap = remember { floatArrayOf(-1f, 0f, 0f) }   // uptime ms (as float), x, y

    /** Where the squelch line is (px from the top), as the peak labels place a level. */
    fun sqlY(splitY: Float, nf: Double, dbm: Double): Float = if (Radio.flat.value) {
        val lo2 = nf + SpectrumView.PAN_LO
        val hi2 = nf + SpectrumView.PAN_HI
        splitY * (1 - ((dbm - lo2) / (hi2 - lo2)).coerceIn(0.0, 1.0)).toFloat()
    } else {
        val s = ((dbm - SpectrumView.floor3d(Radio.style.value, nf)) / SpectrumView.RANGE_3D).coerceIn(0.0, 1.0)
        splitY * (1 - SpectrumView.ridge3d(Radio.style.value, s)).toFloat()
    }
    // Lines on the spectrum (squelch, AGC) stay between the VFO pill (top) and the frequency axis.
    fun levelY(splitY: Float, top: Float, gap: Float, nf: Double, dbm: Double) =
        sqlY(splitY, nf, dbm).coerceIn(top, splitY - gap)
    /** The level a height stands for: levelY backwards (it only falls as the level rises). */
    fun dbmAt(splitY: Float, top: Float, gap: Float, nf: Double, y: Float): Double {
        var a = -170.0; var b = 10.0
        repeat(32) { val m = (a + b) / 2; if (levelY(splitY, top, gap, nf, m) > y) a = m else b = m }
        return (a + b) / 2
    }
    /** Squelch: in AM a real level (dBm); in the other modes upstream's 0..100 as a height. */
    fun sqlLineY(mode: Int, v: Double, splitY: Float, top: Float, gap: Float, nf: Double): Float =
        if (mode == Modes.AM || mode == Modes.SAM) levelY(splitY, top, gap, nf, sqlDbm(v))
        else top + (1 - v / 100.0).toFloat() * (splitY - gap - top)
    fun sqlAt(mode: Int, y: Float, splitY: Float, top: Float, gap: Float, nf: Double): Double =
        if (mode == Modes.AM || mode == Modes.SAM) ((dbmAt(splitY, top, gap, nf, y) + 160.0) / 160.0 * 100.0).coerceIn(0.0, 100.0)
        else ((1 - (y - top) / (splitY - gap - top)) * 100.0).coerceIn(0.0, 100.0)

    Canvas(
        modifier.pointerInput(Unit) {
            val slop = viewConfiguration.touchSlop
            val edgeTol = 22.dp.toPx()
            awaitEachGesture {
                val down = awaitFirstDown(requireUnconsumed = false)
                fling[0]?.cancel()
                val w = size.width.toFloat()
                val sp0 = Radio.span.value
                val hzpx = sp0 / w
                val f0 = Radio.freq.value.toDouble()
                val st = Radio.status.value
                val lo = f0 - sp0 / 2
                val ritHz = if (st.ritOn) st.rit else 0L
                val xa = ((f0 + ritHz + st.filterLow - lo) / hzpx).toFloat()
                val xb = ((f0 + ritHz + st.filterHigh - lo) / hzpx).toFloat()
                val dA = abs(down.position.x - xa)
                val dB = abs(down.position.x - xb)
                val d0 = Radio.dsp.value
                val splitY = size.height * Radio.split.value
                val top = 90.dp.toPx(); val gap = 12.dp.toPx(); val nf = Radio.displayFloor
                // the notch (D5): drag it to move it, drag an edge (when it is wide enough) for its width,
                // push it off the top to remove it
                val notchOn = d0[Dsp.MNF] != 0.0 && d0[Dsp.MNF_FREQ] > 0
                val nx = ((d0[Dsp.MNF_FREQ] - lo) / hzpx).toFloat()
                val nHalfReal = (d0[Dsp.MNF_WIDTH] / 2 / hzpx).toFloat()
                val nHalf = maxOf(nHalfReal, 16.dp.toPx())
                val notchEdge = nHalfReal >= 20.dp.toPx() && abs(abs(down.position.x - nx) - nHalfReal) < 12.dp.toPx()
                // the squelch line (D6): drag it up and down inside the passband
                val sqlOn = d0[Dsp.SQL_ON] != 0.0
                val sy = sqlLineY(st.mode, d0[Dsp.SQL], splitY, top, gap, nf)
                // the AGC lines (M4 pick F): grab their handles on the left
                val agcY = levelY(splitY, top, gap, nf, d0[Dsp.AGC_LINE])
                val hangY = levelY(splitY, top, gap, nf, d0[Dsp.AGC_HANG_LINE])
                val handleW = 190.dp.toPx()
                // 0 = tune, 1 = low edge, 2 = high edge, 3 = RIT (RIT on: drag the middle of the passband),
                // 5 = notch, 6 = squelch line, 7 = AGC gain line, 8 = AGC hang threshold line
                val edge = when {
                    sqlOn && abs(down.position.y - sy) < 16.dp.toPx() &&
                        down.position.x > minOf(xa, xb) - 20.dp.toPx() && down.position.x < maxOf(xa, xb) + 20.dp.toPx() -> 6
                    st.agc != 0 && down.position.x < handleW && abs(down.position.y - agcY) < 16.dp.toPx() -> 7
                    (st.agc == 1 || st.agc == 2) && down.position.x < handleW && abs(down.position.y - hangY) < 16.dp.toPx() -> 8
                    notchOn && abs(down.position.x - nx) < nHalf -> 5
                    minOf(dA, dB) <= edgeTol -> if (dA < dB) 1 else 2
                    st.ritOn && down.position.x > xa && down.position.x < xb -> 3
                    else -> 0
                }
                val vt = VelocityTracker()
                vt.addPosition(down.uptimeMillis, down.position)
                var moved = false
                var pinch = false
                var startDist = 0f
                var startSpan = sp0
                while (true) {
                    val ev = awaitPointerEvent()
                    val pressed = ev.changes.filter { it.pressed }
                    if (pressed.isEmpty()) break
                    if (pressed.size >= 2) {
                        val d = (pressed[0].position - pressed[1].position).getDistance()
                        if (!pinch) {
                            pinch = true; moved = true; startDist = d; startSpan = Radio.span.value
                        } else if (d > 0f) {
                            Radio.setSpan(startSpan * startDist / d)
                        }
                        ev.changes.forEach { it.consume() }
                        continue
                    }
                    if (pinch) continue
                    val c = pressed[0]
                    vt.addPosition(c.uptimeMillis, c.position)
                    val dx = c.position.x - down.position.x
                    if (!moved && (c.position - down.position).getDistance() > slop) moved = true
                    if (moved) {
                        when (edge) {
                            1 -> Radio.setFilter((st.filterLow + dx * hzpx).roundToInt(), st.filterHigh)
                            2 -> Radio.setFilter(st.filterLow, (st.filterHigh + dx * hzpx).roundToInt())
                            3 -> Radio.rit(true, ((st.rit + dx * hzpx) / 10).roundToLong() * 10)
                            5 -> if (c.position.y - down.position.y < -70.dp.toPx() && c.position.y < splitY * 0.3f) {
                                Radio.setDsp(Dsp.MNF, 0.0)   // pushed off the top: gone
                            } else if (Radio.dsp.value[Dsp.MNF] != 0.0) {
                                if (notchEdge) {
                                    val wHz = (2 * abs(c.position.x - nx) * hzpx).coerceIn(10.0, 15000.0)
                                    Radio.setDsp(Dsp.MNF_WIDTH, (wHz / 10).roundToInt() * 10.0)
                                } else {
                                    Radio.setDsp(Dsp.MNF_FREQ, (d0[Dsp.MNF_FREQ] + dx * hzpx).roundToLong().toDouble())
                                }
                            }
                            6 -> Radio.setDsp(Dsp.SQL, sqlAt(st.mode, c.position.y, splitY, top, gap, nf).roundToInt().toDouble())
                            7 -> {
                                // more AGC gain = a lower line (WDSP's AGC top and its knee)
                                val target = dbmAt(splitY, top, gap, nf, c.position.y)
                                val g = (d0[Dsp.AGC_GAIN] - (target - d0[Dsp.AGC_LINE])).coerceIn(-20.0, 120.0).roundToInt().toDouble()
                                Radio.setDsp(Dsp.AGC_GAIN, g)
                                Radio.setDsp(Dsp.AGC_LINE, d0[Dsp.AGC_LINE] - (g - d0[Dsp.AGC_GAIN]))   // until the radio says
                            }
                            8 -> {
                                val th = (d0[Dsp.AGC_HANG_TH] - (c.position.y - down.position.y) / 3.dp.toPx()).coerceIn(0.0, 100.0)
                                Radio.setDsp(Dsp.AGC_HANG_TH, th.roundToInt().toDouble())
                                Radio.setDsp(Dsp.AGC_HANG_LINE, dbmAt(splitY, top, gap, nf, c.position.y))
                            }
                            else -> Radio.tune(f0 - dx * hzpx)
                        }
                        c.consume()
                    }
                }
                if (!moved) {
                    val f = lo + down.position.x * hzpx
                    val t = down.uptimeMillis.toFloat()
                    val again = tap[0]?.isActive == true && t - lastTap[0] < 300f &&
                        (down.position - Offset(lastTap[1], lastTap[2])).getDistance() < 40.dp.toPx()
                    if (again) {
                        // double-tap: the manual notch goes here (deskHPSDR: right-click on the panadapter)
                        tap[0]?.cancel()
                        lastTap[0] = -1f
                        Radio.setDsp(Dsp.MNF_FREQ, f)
                        Radio.setDsp(Dsp.MNF, 1.0)
                    } else {
                        // tap: put that signal in the middle of the passband
                        lastTap[0] = t; lastTap[1] = down.position.x; lastTap[2] = down.position.y
                        tap[0] = scope.launch {
                            delay(260)
                            Radio.tune(f - (st.filterLow + st.filterHigh) / 2.0)
                        }
                    }
                } else if (!pinch && edge == 0) {
                    val v = vt.calculateVelocity().x
                    if (abs(v) > 400f) {
                        fling[0] = scope.launch {
                            var acc = Radio.freq.value.toDouble()
                            var last = 0f
                            AnimationState(0f, v).animateDecay(exponentialDecay(frictionMultiplier = 1.4f)) {
                                acc -= (value - last) * Radio.span.value / w
                                last = value
                                Radio.tune(acc)
                            }
                        }
                    }
                }
            }
        },
    ) {
        val w = size.width
        val h = size.height
        val splitY = h * split
        val lo = freq - span / 2
        val hzpx = span / w
        fun x(f: Double) = ((f - lo) / hzpx).toFloat()

        // passband and VFO line
        val rit = if (status.ritOn) status.rit.toDouble() else 0.0   // RIT moves what you hear
        val pa = x(freq + rit + status.filterLow.toDouble())
        val pb = x(freq + rit + status.filterHigh.toDouble())
        drawRect(Color(0x2078C8FF), Offset(pa, 0f), Size(pb - pa, h))
        drawLine(Color(0x668CD2FF), Offset(pa, 0f), Offset(pa, h), 1.dp.toPx())
        drawLine(Color(0x668CD2FF), Offset(pb, 0f), Offset(pb, h), 1.dp.toPx())
        drawLine(Color(0xFFFF4D4D), Offset(x(freq.toDouble()), 0f), Offset(x(freq.toDouble()), h), 1.2.dp.toPx())

        // the manual notch (MNF): a red band, its width as wide as the notch
        if (dsp[Dsp.MNF] != 0.0 && dsp[Dsp.MNF_FREQ] > 0) {
            val cx = x(dsp[Dsp.MNF_FREQ])
            val half = maxOf((dsp[Dsp.MNF_WIDTH] / 2 / hzpx).toFloat(), 1.5.dp.toPx())
            drawRect(Color(0x38FF4D4D), Offset(cx - half, 0f), Size(2 * half, h))
            drawLine(Color(0xD9FF5A5A), Offset(cx - half, 0f), Offset(cx - half, h), 1.5.dp.toPx())
            drawLine(Color(0xD9FF5A5A), Offset(cx + half, 0f), Offset(cx + half, h), 1.5.dp.toPx())
        }

        // the squelch line across the passband, with its level (dBm only in AM: see sqlLabel)
        val top = 90.dp.toPx(); val gap = 12.dp.toPx()
        // deskHPSDR's AGC lines (rx_panadapter.c): the AGC gain knee, and for Long/Slow the hang threshold
        fun hline(y: Float, col: Color, text: String) {
            val dash = 12.dp.toPx()
            var xx = 12.dp.toPx()
            while (xx < w - 12.dp.toPx()) {
                drawLine(col.copy(alpha = 0.85f), Offset(xx, y), Offset(xx + dash, y), 1.8.dp.toPx()); xx += dash * 1.7f
            }
            val t = measurer.measure(text, TextStyle(color = col, fontSize = 10.5.sp, fontWeight = FontWeight.SemiBold))
            val bx = 12.dp.toPx()
            drawRoundRect(Color(0xE008101E), Offset(bx, y - t.size.height / 2 - 5.dp.toPx()),
                Size(t.size.width + 18.dp.toPx(), t.size.height + 10.dp.toPx()), CornerRadius(13.dp.toPx()))
            drawRoundRect(col.copy(alpha = 0.6f), Offset(bx, y - t.size.height / 2 - 5.dp.toPx()),
                Size(t.size.width + 18.dp.toPx(), t.size.height + 10.dp.toPx()), CornerRadius(13.dp.toPx()),
                style = androidx.compose.ui.graphics.drawscope.Stroke(1.dp.toPx()))
            drawText(t, topLeft = Offset(bx + 9.dp.toPx(), y - t.size.height / 2))
        }
        if (status.agc != 0) {
            val nf = Radio.displayFloor
            if (status.agc == 1 || status.agc == 2) {
                hline(levelY(splitY, top, gap, nf, dsp[Dsp.AGC_HANG_LINE]), Color(0xFFFF7F50),
                    "AGC-Th %.0f dBm  ↕".format(dsp[Dsp.AGC_HANG_LINE]))
            }
            hline(levelY(splitY, top, gap, nf, dsp[Dsp.AGC_LINE]), Amb, "AGC-G %.0f dBm  ↕".format(dsp[Dsp.AGC_LINE]))
        }
        if (dsp[Dsp.SQL_ON] != 0.0) {
            val sy = sqlLineY(status.mode, dsp[Dsp.SQL], splitY, top, gap, Radio.displayFloor)
            val dash = 10.dp.toPx()
            var xx = minOf(pa, pb)
            while (xx < maxOf(pa, pb)) {
                drawLine(Grn, Offset(xx, sy), Offset(minOf(xx + dash, maxOf(pa, pb)), sy), 3.dp.toPx())
                xx += dash * 1.8f
            }
            val t = measurer.measure("SQL " + sqlLabel(status.mode, dsp[Dsp.SQL]) + "  drag ↕",
                TextStyle(color = Txt, fontSize = 10.5.sp, fontWeight = FontWeight.Medium))
            val bx = (maxOf(pa, pb) + 8.dp.toPx()).coerceAtMost(w - t.size.width - 16.dp.toPx())
            drawRoundRect(Color(0xCC08101E), Offset(bx, sy - t.size.height / 2 - 4.dp.toPx()),
                Size(t.size.width + 16.dp.toPx(), t.size.height + 8.dp.toPx()), CornerRadius(12.dp.toPx()))
            drawText(t, topLeft = Offset(bx + 8.dp.toPx(), sy - t.size.height / 2))
        }

        // frequency axis between the 3D view and the waterfall
        val axisH = 16.dp.toPx()
        drawRect(Color(0xB802050C), Offset(0f, splitY - axisH / 2), Size(w, axisH))
        val step = niceStep(span)
        var f = ceil(lo / step) * step
        val axisStyle = TextStyle(color = Color(0xD8E1EBFA), fontSize = 10.sp)
        while (f <= lo + span) {
            val xx = x(f)
            if (xx > 18.dp.toPx() && xx < w - 18.dp.toPx()) {
                val label = measurer.measure(fmtAxis(f, step), axisStyle)
                drawText(label, topLeft = Offset(xx - label.size.width / 2, splitY - label.size.height / 2))
            }
            f += step
        }

        // band bar: red outside the bands, the plan's segments inside
        bandBar(region, lo, lo + span, splitY + axisH / 2, ::x, measurer)

        // peak labels (deskHPSDR's "Peak Labels on Panadapter"), over the 2D trace or the front 3D ridge
        val nf = Radio.displayFloor
        val ls = TextStyle(color = Txt, fontSize = 9.5.sp, fontWeight = FontWeight.SemiBold)
        for ((pf, pdb) in peaks) {
            val px = x(pf)
            val top = if (flat) {
                val lo2 = nf + SpectrumView.PAN_LO
                val hi2 = nf + SpectrumView.PAN_HI
                splitY * (1 - ((pdb - lo2) / (hi2 - lo2)).coerceIn(0.0, 1.0)).toFloat()
            } else {
                val s = ((pdb - SpectrumView.floor3d(style, nf)) / SpectrumView.RANGE_3D).coerceIn(0.0, 1.0)
                splitY * (1 - SpectrumView.ridge3d(style, s)).toFloat()
            }
            // below S1 deskHPSDR's table says "no signal": not worth a label
            if (Radio.labelsSmeter.value && SMeter.index(freq, pdb) == 0) continue
            val text = if (Radio.labelsSmeter.value) SMeter.label(freq, pdb) else "%.0f dBm".format(pdb)
            val t = measurer.measure(text, ls)
            val w = t.size.width + 10.dp.toPx()
            val h = t.size.height + 3.dp.toPx()
            val y = (top - h - 6.dp.toPx()).coerceAtLeast(70.dp.toPx())
            val cx = px.coerceIn(w / 2 + 2.dp.toPx(), size.width - w / 2 - 2.dp.toPx())   // keep it on screen
            drawRoundRect(Color(0xCC08101E), Offset(cx - w / 2, y), Size(w, h), CornerRadius(h / 2))
            drawLine(Color(0x66FFFFFF), Offset(px, y + h), Offset(px, top - 2.dp.toPx()), 1.dp.toPx())
            drawText(t, topLeft = Offset(cx - t.size.width / 2, y + 1.5.dp.toPx()))
        }
    }
}

private fun DrawScope.bandBar(
    region: Int, lo: Double, hi: Double, y: Float, x: (Double) -> Float,
    measurer: androidx.compose.ui.text.TextMeasurer,
) {
    val barH = 4.dp.toPx()
    drawRect(BandPlan.OUT_OF_BAND.copy(alpha = 0.85f), Offset(0f, y), Size(size.width, barH))
    for (b in BandPlan.bands) {
        if (b.hiHz < lo || b.loHz > hi) continue
        val a = x(b.loHz.toDouble()).coerceAtLeast(0f)
        val e = x(b.hiHz.toDouble()).coerceAtMost(size.width)
        drawRect(Color(0xFF2A3346), Offset(a, y), Size(e - a, barH))
    }
    val ls = TextStyle(fontSize = 8.5.sp, fontWeight = FontWeight.SemiBold)
    for (s in BandPlan.segments(region, lo.toLong(), hi.toLong())) {
        val a = x(s.loHz.toDouble()).coerceAtLeast(0f)
        val e = x(s.hiHz.toDouble()).coerceAtMost(size.width)
        drawRect(s.kind.color, Offset(a + 0.5f, y), Size(e - a - 1f, barH))
        if (e - a > 44.dp.toPx()) {
            drawText(measurer.measure(s.kind.label.uppercase(), ls.copy(color = s.kind.color)),
                topLeft = Offset(a + 5.dp.toPx(), y + barH + 2.dp.toPx()))
        }
    }
}

private fun niceStep(span: Double): Double {
    for (s in doubleArrayOf(100.0, 200.0, 250.0, 500.0, 1e3, 2e3, 2.5e3, 5e3, 1e4, 2e4, 2.5e4, 5e4, 1e5))
        if (span / s <= 7) return s
    return 1e5
}

private fun fmtAxis(f: Double, step: Double): String {
    val d = if (step % 10_000 == 0.0) 2 else if (step % 1000 == 0.0) 3 else 4
    return "%.${d}f".format(f / 1e6)
}

// ---------------------------------------------------------------- top widgets

@Composable
private fun MeterCard(dbm: Double, modifier: Modifier) {
    val (label, level) = sMeter(dbm)
    Column(modifier.width(168.dp).glass().padding(horizontal = 11.dp, vertical = 9.dp)) {
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween, verticalAlignment = Alignment.CenterVertically) {
            Text(label, color = Txt, fontSize = 20.sp, fontWeight = FontWeight.Medium)
            Text("%.0f dBm".format(dbm), color = Dim, fontSize = 11.sp)
        }
        Spacer(Modifier.height(6.dp))
        Canvas(Modifier.fillMaxWidth().height(9.dp)) {
            val n = 15
            val gap = 2.dp.toPx()
            val sw = (size.width - gap * (n - 1)) / n
            for (i in 0 until n) {
                val fill = (level - i).coerceIn(0.0, 1.0).toFloat()
                val x0 = i * (sw + gap)
                drawRect(Color(0x14FFFFFF), Offset(x0, 0f), Size(sw, size.height))
                if (fill > 0f) drawRect(if (i < 9) Grn else Red, Offset(x0, 0f), Size(sw * fill, size.height))
            }
        }
        Spacer(Modifier.height(3.dp))
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
            for ((t, red) in listOf("S1" to false, "3" to false, "5" to false, "7" to false, "9" to false,
                "+20" to true, "+40" to true, "+60" to true)) {
                Text(t, color = if (red) Color(0xFFFF8A80) else Dim, fontSize = 8.5.sp)
            }
        }
    }
}

/** The label from deskHPSDR's S-unit table (SMeter.kt); the bar: 6 dB per S-unit, then 10 dB segments. */
private fun sMeter(dbm: Double): Pair<String, Double> {
    val f = Radio.freq.value
    val s9 = if (f > 30_000_000L) -93.0 else -73.0
    val label = SMeter.label(f, dbm).let { if (it == "no signal") "S0" else if (it == "out of range") "S9+" else it }
    val level = if (dbm <= s9) ((dbm - s9) / 6.0 + 9.0).coerceAtLeast(0.0) else 9.0 + (dbm - s9) / 10.0
    return label to level
}

/**
 * The main VFO pill (M4 picks): VFO A big, VFO B small under it; on the right
 * the mode and filter, then a column with only the switches that are on (RIT,
 * split, lock, CTUN) - it makes the pill wider, never taller. Tap the pill for
 * the VFO panel, tap the digits to type a frequency, hold them and slide to tune.
 */
@Composable
private fun VfoPill(
    freq: Long, st: Status, onMode: () -> Unit, onFilter: () -> Unit, onFreq: () -> Unit, onPanel: () -> Unit,
    modifier: Modifier,
) {
    Row(
        modifier.height(74.dp).glass().clickable(onClick = onPanel).padding(horizontal = 16.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(10.dp),
    ) {
        Column(verticalArrangement = Arrangement.spacedBy(2.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                Box(Modifier.clip(RoundedCornerShape(6.dp)).background(Amb).padding(horizontal = 7.dp, vertical = 1.dp)) {
                    Text("A", color = Color(0xFF1A1000), fontSize = 11.sp, fontWeight = FontWeight.Bold)
                }
                val haptic = LocalHapticFeedback.current
                Box(
                    Modifier.pointerInput(Unit) {
                        // tap: type a frequency (deskHPSDR's VFO number pad)
                        detectTapGestures(onTap = { onFreq() })
                    }.pointerInput(Unit) {
                        // hold, then drag sideways: one step per 8 dp
                        val px = 8.dp.toPx()
                        var start = 0L
                        var acc = 0f
                        detectDragGesturesAfterLongPress(
                            onDragStart = {
                                haptic.performHapticFeedback(HapticFeedbackType.LongPress)
                                start = Radio.freq.value
                                acc = 0f
                            },
                        ) { change, drag ->
                            change.consume()
                            acc += drag.x
                            Radio.tune(start + (acc / px).toInt() * Radio.step.value.toDouble())
                        }
                    },
                ) { FreqText(freq, 32) }
            }
            if (st.freqB > 0) {
                Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    Text("B", color = Dim, fontSize = 10.sp, fontWeight = FontWeight.Bold,
                        modifier = Modifier.width(24.dp), textAlign = TextAlign.Center)
                    FreqText(st.freqB, 13, dim = true)
                }
            }
        }
        Column(verticalArrangement = Arrangement.spacedBy(3.dp)) {
            Tag(Modes.name(st.mode), Acc, Color(0x293FD0FF), onMode)
            Tag(widthLabel(st.filterHigh - st.filterLow), Txt, Color(0x14FFFFFF), onFilter)
        }
        val on = buildList {
            if (st.ritOn) add("RIT %+d".format(st.rit) to Amb)
            if (st.split) add("SPLIT · TX B" to Color(0xFFC9A8FF))
            if (st.ctun) add("CTUN" to Acc)
            if (st.locked) add("🔒 LOCK" to Dim)
        }
        if (on.isNotEmpty()) {
            Box(Modifier.width(1.dp).height(46.dp).background(Line))
            Column(verticalArrangement = Arrangement.spacedBy(2.dp)) {
                on.forEach { (t, c) -> Text(t, color = c, fontSize = 10.sp, fontWeight = FontWeight.Bold) }
            }
        }
    }
}

/** Tap the pill: the VFO panel drops open under it (round 2, picture 3). */
@Composable
private fun VfoPanel(freq: Long, st: Status, close: () -> Unit) {
    Box(Modifier.fillMaxSize().background(Color(0x55000000)).clickable(onClick = close)) {
        Column(
            Modifier.align(Alignment.TopCenter).padding(top = 8.dp).width(440.dp).glass(18)
                .background(Color(0xE6090D18)).clickable(enabled = false) {}.padding(14.dp),
            verticalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                Box(Modifier.clip(RoundedCornerShape(6.dp)).background(Amb).padding(horizontal = 7.dp, vertical = 1.dp)) {
                    Text("A", color = Color(0xFF1A1000), fontSize = 11.sp, fontWeight = FontWeight.Bold)
                }
                FreqText(freq, 30)
                Tag(Modes.name(st.mode), Acc, Color(0x293FD0FF)) {}
            }
            Row(
                Modifier.clip(RoundedCornerShape(8.dp)).clickable { Radio.vfoOp(2) }.padding(vertical = 2.dp),
                verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(10.dp),
            ) {
                Text("B", color = Dim, fontSize = 12.sp, fontWeight = FontWeight.Bold, modifier = Modifier.width(26.dp),
                    textAlign = TextAlign.Center)
                FreqText(st.freqB, 20, dim = true)
                Tag(Modes.name(st.modeB), Txt, Color(0x14FFFFFF)) { Radio.vfoOp(2) }
                Text("tap B to make it active", color = Dim, fontSize = 10.sp)
            }
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                PanelButton("A›B", modifier = Modifier.weight(1f)) { Radio.vfoOp(0) }
                PanelButton("B›A", modifier = Modifier.weight(1f)) { Radio.vfoOp(1) }
                PanelButton("A⇄B", modifier = Modifier.weight(1f)) { Radio.vfoOp(2) }
                PanelButton("SPLIT", st.split, "with transmit (M7)", Modifier.weight(1f), enabled = false) {}
            }
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                PanelButton("RIT", st.ritOn, "%+d Hz".format(st.rit), Modifier.weight(1f)) { Radio.rit(!st.ritOn) }
                PanelButton("RIT 0", modifier = Modifier.weight(1f)) { Radio.rit(st.ritOn, 0) }
                PanelButton("CTUN", st.ctun, modifier = Modifier.weight(1f)) { Radio.ctun(!st.ctun) }
                PanelButton("LOCK", st.locked, modifier = Modifier.weight(1f)) { Radio.lock(!st.locked) }
                val step by Radio.step.collectAsState()
                PanelButton("STEP", false, if (step >= 1000) "${step / 1000} kHz" else "$step Hz", Modifier.weight(1f)) {
                    val steps = listOf(1, 10, 50, 100, 500, 1000)
                    Radio.setStep(steps[(steps.indexOf(step) + 1) % steps.size])
                }
            }
            Text("RIT on: drag the middle of the passband · tap outside to close", color = Dim, fontSize = 10.sp,
                modifier = Modifier.fillMaxWidth(), textAlign = TextAlign.Center)
        }
    }
}

@Composable
private fun PanelButton(
    label: String, on: Boolean = false, sub: String? = null, modifier: Modifier = Modifier, enabled: Boolean = true,
    onClick: () -> Unit,
) {
    Column(
        modifier.height(42.dp).clip(RoundedCornerShape(12.dp))
            .background(if (on) Color(0x293FD0FF) else Color(0xFF0F1624))
            .border(1.dp, if (on) Acc.copy(alpha = 0.5f) else Line, RoundedCornerShape(12.dp))
            .clickable(enabled = enabled, onClick = onClick),
        horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.Center,
    ) {
        Text(label, color = if (!enabled) Dim else if (on) Acc else Txt, fontSize = 13.sp, fontWeight = FontWeight.Medium)
        if (sub != null) Text(sub, color = Dim, fontSize = 9.5.sp)
    }
}

@Composable
private fun FreqText(freq: Long, size: Int, dim: Boolean = false) {
    val mhz = freq / 1_000_000
    val khz = (freq / 1000) % 1000
    val hz = freq % 1000
    Text(
        buildAnnotatedString {
            withStyle(SpanStyle(color = if (dim) Dim else Txt, fontSize = size.sp, fontWeight = FontWeight.Medium)) {
                append("%d.%03d".format(mhz, khz))
            }
            withStyle(SpanStyle(color = Dim, fontSize = (size * 0.6).sp)) { append(".%03d".format(hz)) }
        },
    )
}

@Composable
private fun Tag(text: String, fg: Color, bg: Color, onClick: () -> Unit) {
    Box(
        Modifier.widthIn(min = 52.dp).clip(RoundedCornerShape(6.dp)).background(bg).clickable(onClick = onClick)
            .padding(horizontal = 7.dp, vertical = 2.dp),
        contentAlignment = Alignment.Center,
    ) { Text(text, color = fg, fontSize = 11.sp, fontWeight = FontWeight.SemiBold) }
}

internal fun widthLabel(hz: Int) = if (hz >= 1000) "%.1fk".format(hz / 1000.0) else "$hz"

/** The X6100 band bar's tag: band and segment, shown for 2 s whenever either changes. */
@Composable
private fun SegmentTag(freq: Long, modifier: Modifier) {
    val region by Radio.region.collectAsState()
    val key = BandPlan.tagKey(region, freq)
    var visible by remember { mutableStateOf(false) }
    LaunchedEffect(key) {
        visible = true
        delay(2000)
        visible = false
    }
    val tag = BandPlan.tag(region, freq)
    val seg = BandPlan.segmentAt(region, freq)
    AnimatedVisibility(visible, modifier, enter = fadeIn(tween(120)), exit = fadeOut(tween(500))) {
        Row(
            Modifier.glass(20).border(1.dp, tag.color.copy(alpha = 0.55f), RoundedCornerShape(20.dp))
                .padding(horizontal = 14.dp, vertical = 6.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            Box(Modifier.size(9.dp).clip(CircleShape).background(tag.color))
            Text(tag.text, color = tag.color, fontSize = 13.sp, fontWeight = FontWeight.SemiBold)
            if (seg != null) {
                Text("%.3f – %.3f".format(seg.loHz / 1e6, seg.hiHz / 1e6), color = Dim, fontSize = 11.sp)
            }
        }
    }
}

@Composable
private fun Flash(flash: Pair<String, Long>, modifier: Modifier) {
    var visible by remember { mutableStateOf(false) }
    LaunchedEffect(flash) {
        if (flash.first.isEmpty()) return@LaunchedEffect
        visible = true
        delay(1800)
        visible = false
    }
    AnimatedVisibility(visible, modifier, enter = fadeIn(), exit = fadeOut(tween(400))) {
        Text(flash.first, color = Txt, fontSize = 12.sp, modifier = Modifier.glass(10).padding(horizontal = 10.dp, vertical = 5.dp))
    }
}

@OptIn(ExperimentalFoundationApi::class)
@Composable
private fun RoundButton(label: String, onLongClick: (() -> Unit)? = null, onClick: () -> Unit) {
    Box(
        Modifier.size(44.dp).clip(CircleShape).background(Glass).border(1.dp, Line, CircleShape)
            .combinedClickable(onClick = onClick, onLongClick = onLongClick),
        contentAlignment = Alignment.Center,
    ) { Text(label, color = Txt, fontSize = if (label.length > 1) 13.sp else 18.sp, fontWeight = FontWeight.SemiBold) }
}

// ---------------------------------------------------------------- bottom chips

/** The bottom row: the chips Randal picked (deskHPSDR's Toolbar menu, the phone's way; see Dsp.kt). */
@Composable
private fun ChipRow(st: Status, open: (Sheet) -> Unit, modifier: Modifier) {
    val ids by Radio.chips.collectAsState()
    Row(modifier, horizontalArrangement = Arrangement.spacedBy(7.dp)) {
        ids.forEach { ChipFor(it, st, open) }
    }
}

@OptIn(ExperimentalFoundationApi::class)
@Composable
internal fun Chip(
    label: String, value: String, on: Boolean = false, modifier: Modifier = Modifier,
    onLongClick: (() -> Unit)? = null, onClick: () -> Unit,
) {
    Column(
        modifier.height(46.dp).widthIn(min = 70.dp).glass(13)
            .then(if (on) Modifier.border(1.dp, Acc.copy(alpha = 0.55f), RoundedCornerShape(13.dp)) else Modifier)
            .combinedClickable(onClick = onClick, onLongClick = onLongClick).padding(horizontal = 12.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.Center,
    ) {
        Text(label, color = Dim, fontSize = 10.sp, letterSpacing = 0.4.sp)
        Text(value, color = if (on) Acc else Txt, fontSize = 15.sp, fontWeight = FontWeight.Medium)
    }
}

// ---------------------------------------------------------------- status and sheets

@Composable
private fun StatusCard(st: Status, onStart: () -> Unit, modifier: Modifier) {
    Column(
        modifier.widthIn(max = 420.dp).glass(18).padding(18.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        val title = when (st.phase) {
            Phase.IDLE -> "Radio stopped"
            Phase.PREPARING -> "Getting ready"
            Phase.CONNECTING -> "Connecting"
            Phase.ERROR -> "Not connected"
            Phase.RUNNING, Phase.DEMO -> ""
        }
        Text(title, color = if (st.phase == Phase.ERROR) Red else Txt, fontSize = 17.sp, fontWeight = FontWeight.Medium)
        if (st.message.isNotEmpty()) Text(st.message, color = Dim, fontSize = 12.sp)
        if (st.phase == Phase.ERROR || st.phase == Phase.IDLE) {
            Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                CardButton(if (st.phase == Phase.ERROR) "Try again" else "Start", Acc, onStart)
                CardButton("Demo band", Amb) { Radio.startDemo() }
            }
        }
    }
}

@Composable
private fun CardButton(text: String, color: Color, onClick: () -> Unit) {
    Text(
        text, color = color, fontSize = 14.sp, fontWeight = FontWeight.SemiBold,
        modifier = Modifier.clip(RoundedCornerShape(10.dp)).background(color.copy(alpha = 0.16f))
            .clickable(onClick = onClick).padding(horizontal = 18.dp, vertical = 8.dp),
    )
}

@Composable
private fun SheetHost(
    which: Sheet, st: Status, onStart: () -> Unit, onStop: () -> Unit, dismiss: () -> Unit, close: () -> Unit,
) {
    Box(Modifier.fillMaxSize().background(Color(0x66000000)).clickable(onClick = dismiss)) {
        Column(
            Modifier.align(Alignment.BottomCenter).widthIn(max = 560.dp).fillMaxWidth()
                .clip(RoundedCornerShape(topStart = 20.dp, topEnd = 20.dp)).background(Color(0xF20C111E))
                .border(1.dp, Line, RoundedCornerShape(topStart = 20.dp, topEnd = 20.dp))
                .clickable(enabled = false) {}.heightIn(max = 330.dp)
                .verticalScroll(rememberScrollState()).padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(10.dp),
        ) {
            when (which) {
                Sheet.BAND -> BandSheet(close)
                Sheet.MODE -> ModeSheet(st, close)
                Sheet.AF -> SliderSheet("AF (HL2+ speaker)", Radio.volumeDb.collectAsState().value, -40f..0f,
                    { "${((it + 40) / 40 * 100).roundToInt()}" }) { Radio.setVolume(it) }
                Sheet.LNA -> SliderSheet("LNA gain", Radio.lnaDb.collectAsState().value, -12f..48f,
                    { "%+.0f dB".format(it) }) { Radio.setLna(it.roundToInt().toDouble()) }
                Sheet.FILTER -> FilterSheet(st, close)
                Sheet.SDR -> SdrDeviceSheet(st)
                Sheet.DISCOVERY -> DiscoverySheet(st, onStart, onStop, close)
                Sheet.ABOUT -> AboutSheet()
                Sheet.DSP -> DspSheet(st)
                Sheet.RX -> RxSheet(st)
                Sheet.NOISE, Sheet.EQ, Sheet.TOOLBAR -> {}
                Sheet.DISPLAY -> DisplaySheet()
                Sheet.FREQ -> FreqSheet(close)
                Sheet.AGC -> AgcSheet(st, close)
                Sheet.VFO, Sheet.MENU -> {}
            }
        }
    }
}

@Composable
private fun IpField(value: String, hint: String, modifier: Modifier, set: (String) -> Unit) {
    var text by remember(value) { mutableStateOf(value) }
    Box(
        modifier.height(40.dp).clip(RoundedCornerShape(10.dp)).background(Color(0xFF0F1624))
            .border(1.dp, Line, RoundedCornerShape(10.dp)).padding(horizontal = 12.dp),
        contentAlignment = Alignment.CenterStart,
    ) {
        if (text.isEmpty()) Text(hint, color = Dim.copy(alpha = 0.6f), fontSize = 13.sp)
        BasicTextField(
            value = text,
            onValueChange = { t -> text = t.filter { it.isDigit() || it == '.' }; set(text) },
            singleLine = true,
            textStyle = TextStyle(color = Txt, fontSize = 14.sp),
            cursorBrush = SolidColor(Acc),
            keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Decimal),
            modifier = Modifier.fillMaxWidth(),
        )
    }
}

@Composable
internal fun SheetTitle(t: String) = Text(t, color = Txt, fontSize = 17.sp, fontWeight = FontWeight.Medium)

@Composable
internal fun GridButtons(items: List<String>, selected: String?, cols: Int, onClick: (Int) -> Unit) {
    Column(verticalArrangement = Arrangement.spacedBy(7.dp)) {
        items.chunked(cols).forEachIndexed { r, row ->
            Row(horizontalArrangement = Arrangement.spacedBy(7.dp)) {
                row.forEachIndexed { c, label ->
                    val on = label == selected
                    Box(
                        Modifier.weight(1f).height(42.dp).clip(RoundedCornerShape(12.dp))
                            .background(if (on) Color(0x293FD0FF) else Color(0xFF0F1624))
                            .border(1.dp, if (on) Acc.copy(alpha = 0.5f) else Line, RoundedCornerShape(12.dp))
                            .clickable { onClick(r * cols + c) },
                        contentAlignment = Alignment.Center,
                    ) { Text(label, color = if (on) Acc else Txt, fontSize = 14.sp, fontWeight = FontWeight.Medium) }
                }
                repeat(cols - row.size) { Spacer(Modifier.weight(1f)) }
            }
        }
    }
}

/** Band sheet (M4 pick E): deskHPSDR's bands, this band's stack, memories M0-M9. */
@OptIn(ExperimentalFoundationApi::class)
@Composable
private fun BandSheet(close: () -> Unit) {
    val st by Radio.status.collectAsState()
    var refresh by remember { mutableStateOf(0) }
    Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
        SheetTitle("Band")
        Text("tap the band again for its next stack entry", color = Dim, fontSize = 11.sp)
    }
    if (!Radio.running) {
        Text("Bands, band stacks and memories come from the radio: start it first.", color = Dim, fontSize = 12.sp)
        return
    }
    val bands = remember(refresh) { Radio.bands() }
    val sel = bands.firstOrNull { it.index == st.band }?.title
    GridButtons(bands.map { it.title }, sel, 7) { Radio.band(bands[it].index) }
    val (cur, stack) = remember(st.band, st.bandstack, st.freq_dummy(), refresh) { Radio.bandstackEntries(st.band) }
    if (stack.isNotEmpty()) {
        Label("${sel ?: ""} band stack")
        Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            stack.forEachIndexed { i, e ->
                val on = i == cur
                Column(
                    Modifier.weight(1f).height(44.dp).clip(RoundedCornerShape(12.dp))
                        .background(if (on) Color(0x293FD0FF) else Color(0xFF0F1624))
                        .border(1.dp, if (on) Acc.copy(alpha = 0.5f) else Line, RoundedCornerShape(12.dp))
                        .clickable { Radio.bandstack(i); refresh++ },
                    horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.Center,
                ) {
                    FreqText(e.freq, 13)
                    Text(Modes.name(e.mode), color = Dim, fontSize = 9.5.sp)
                }
            }
        }
    }
    Label("Memories · tap to recall, hold to store the current frequency")
    val mems = remember(refresh) { Radio.memories() }
    val haptic = LocalHapticFeedback.current
    Row(horizontalArrangement = Arrangement.spacedBy(5.dp)) {
        mems.forEachIndexed { i, (f, _) ->
            Column(
                Modifier.weight(1f).height(42.dp).clip(RoundedCornerShape(10.dp)).background(Color(0xFF0F1624))
                    .border(1.dp, Line, RoundedCornerShape(10.dp))
                    .combinedClickable(
                        onClick = { Radio.memory(false, i); close() },
                        onLongClick = {
                            haptic.performHapticFeedback(HapticFeedbackType.LongPress)
                            Radio.memory(true, i); refresh++
                        },
                    ),
                horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.Center,
            ) {
                Text("M$i", color = Dim, fontSize = 9.sp)
                Text(if (f > 0) "%.3f".format(f / 1e6) else "—", color = Txt, fontSize = 11.sp)
            }
        }
    }
}

private fun Status.freq_dummy() = rows / 30   // re-read the stack about once a second

@Composable
private fun ModeSheet(st: Status, close: () -> Unit) {
    SheetTitle("Mode")
    GridButtons(Modes.picker.map { Modes.name(it) }, Modes.name(st.mode), 5) {
        Radio.setMode(Modes.picker[it])
        close()
    }
}

@Composable
private fun SliderSheet(title: String, value: Double, range: ClosedFloatingPointRange<Float>, fmt: (Double) -> String,
                        set: (Double) -> Unit) {
    Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
        SheetTitle(title)
        Text(fmt(value), color = Acc, fontSize = 17.sp, fontWeight = FontWeight.Medium)
    }
    Slider(
        value = value.toFloat(), onValueChange = { set(it.toDouble()) }, valueRange = range,
        colors = SliderDefaults.colors(thumbColor = Acc, activeTrackColor = Acc),
    )
}

/** deskHPSDR's own filter table for the mode (filter.c): its presets, then Var1/Var2. */
@Composable
private fun FilterSheet(st: Status, close: () -> Unit) {
    SheetTitle("Filter · ${Modes.name(st.mode)}")
    if (!Radio.running) {
        Text("Start the radio first.", color = Dim, fontSize = 12.sp)
        return
    }
    val list = remember(st.mode) { Radio.filters(st.mode) }
    val shown = list.withIndex().filter { it.value.first.isNotBlank() }
    val label = { t: Triple<String, Int, Int> ->
        if (t.first.startsWith("Var")) "${t.first} ${widthLabel(t.third - t.second)}" else t.first
    }
    GridButtons(shown.map { label(it.value) }, list.getOrNull(st.filter)?.let(label), 4) {
        Radio.filterPreset(shown[it].index)
        close()
    }
    Text("Or drag the passband edges on the spectrum (that sets Var1).", color = Dim, fontSize = 11.sp)
}

/** AGC (deskHPSDR's AGC menu: the modes; gain and hang threshold come with the AGC line). */
@Composable
private fun AgcSheet(st: Status, close: () -> Unit) {
    SheetTitle("AGC")
    GridButtons(AGC_NAMES, AGC_NAMES.getOrNull(st.agc), 5) { Radio.agc(it); close() }
}

internal val AGC_NAMES = listOf("Off", "Long", "Slow", "Med", "Fast")

/** deskHPSDR's VFO number pad: type a frequency, finish with MHz or kHz. */
@Composable
private fun FreqSheet(close: () -> Unit) {
    var text by remember { mutableStateOf("") }
    Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
        SheetTitle("Frequency")
        Spacer(Modifier.weight(1f))
        Text(text.ifEmpty { "type, then MHz or kHz" }, color = if (text.isEmpty()) Dim else Txt,
            fontSize = if (text.isEmpty()) 13.sp else 26.sp, fontWeight = FontWeight.Medium)
    }
    fun enter(scale: Double) {
        val v = text.toDoubleOrNull() ?: return
        Radio.tune(v * scale, exact = true)
        close()
    }
    val keys = listOf("1", "2", "3", "MHz", "4", "5", "6", "kHz", "7", "8", "9", "⌫", ".", "0", "Clear", "Cancel")
    GridButtons(keys, null, 4) {
        when (val k = keys[it]) {
            "MHz" -> enter(1e6)
            "kHz" -> enter(1e3)
            "⌫" -> text = text.dropLast(1)
            "Clear" -> text = ""
            "Cancel" -> close()
            "." -> if (!text.contains('.')) text += "."
            else -> if (text.length < 12) text += k
        }
    }
}

/** deskHPSDR's Display menu, the phone's way: the 3D looks first. */
@Composable
private fun DisplaySheet() {
    val flat by Radio.flat.collectAsState()
    val style by Radio.style.collectAsState()
    val theme by Radio.wfTheme.collectAsState()
    val depth by Radio.depthRows.collectAsState()
    val hold by Radio.peakHold.collectAsState()
    val labels by Radio.peakLabels.collectAsState()
    val pass by Radio.labelsPassband.collectAsState()
    val smeter by Radio.labelsSmeter.collectAsState()
    val hide by Radio.labelsHideNoise.collectAsState()
    val pct by Radio.labelsPercentile.collectAsState()
    val ign by Radio.labelsIgnore.collectAsState()
    val avg by Radio.avgMs.collectAsState()
    val split by Radio.split.collectAsState()
    val fps by Radio.fps.collectAsState()
    val win by Radio.window.collectAsState()
    val det by Radio.detector.collectAsState()
    SheetTitle("Display")
    Label("3D look")
    GridButtons(SpectrumView.STYLES, if (flat) null else SpectrumView.STYLES[style], 4) { Radio.setStyle(it); Radio.setFlat(false) }
    GridButtons(listOf("2D spectrum"), if (flat) "2D spectrum" else null, 1) { Radio.setFlat(true) }
    Label("Waterfall colours")
    GridButtons(SpectrumView.WF_THEMES, SpectrumView.WF_THEMES[theme], 6) { Radio.setWfTheme(it) }
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
    Label("2D averaging (Av. Time)")
    val avgs = listOf(0, 120, 250, 500)
    GridButtons(avgs.map { if (it == 0) "Off" else "$it ms" }, if (avg == 0) "Off" else "$avg ms", 4) { Radio.setAvgMs(avgs[it]) }
    Label("Peak hold (2D)")
    GridButtons(listOf("On", "Off"), if (hold) "On" else "Off", 2) { Radio.setPeakHold(it == 0) }
    Label("3D history (rows)")
    val depths = listOf(40, 80, 120, 160)
    GridButtons(depths.map { "$it" }, "$depth", 4) { Radio.setDepthRows(depths[it]) }
    Label("Spectrum / waterfall split")
    val splits = listOf(0.35f, 0.45f, 0.5f, 0.6f)
    GridButtons(splits.map { "${(it * 100).roundToInt()} %" }, "${(split * 100).roundToInt()} %", 4) { Radio.setSplit(splits[it]) }
    Label("Frames per second (spectrum rows)")
    val rates = listOf(15, 20, 30, 60)
    GridButtons(rates.map { "$it" }, "$fps", 4) { Radio.setDisplay(fps = rates[it]) }
    Label("FFT window")
    val windows = listOf("Rectangular", "Blackman-Harris 4", "Hann", "Flat-top", "Hamming", "Kaiser", "Blackman-Harris 7")
    GridButtons(windows, windows[win], 4) { Radio.setDisplay(window = it) }
    Label("Detector")
    val dets = listOf("Peak", "Average", "Rosenfell", "Sample")
    GridButtons(dets, dets[det], 4) { Radio.setDisplay(detector = it) }
}

@Composable
internal fun Label(t: String) = Text(t, color = Dim, fontSize = 11.sp)

/** One button of the main menu: opens [sheet], or isn't built yet ([later] = the milestone that brings it). */
private class MenuItem(val label: String, val sheet: Sheet? = null, val later: String? = null)

/**
 * deskHPSDR's main menu (src/new_menu.c), same columns and order so a
 * deskHPSDR user finds things: radio, VFO, RX, TX, RX+TX, control.
 * Diversity is left out (it needs RX2); P2 ADC/DDC and Saturn never show on an HL2.
 */
private val MENU_COLUMNS = listOf(
    listOf(MenuItem("SDR Device", Sheet.SDR), MenuItem("Screen", later = "M5"), MenuItem("Display", Sheet.DISPLAY),
        MenuItem("Meter", later = "M4"), MenuItem("XVTR", later = "M6"), MenuItem("Discovery", Sheet.DISCOVERY)),
    listOf(MenuItem("VFO", Sheet.VFO), MenuItem("Band", Sheet.BAND), MenuItem("BandStack", Sheet.BAND),
        MenuItem("Mode", Sheet.MODE), MenuItem("Memory", Sheet.BAND)),
    listOf(MenuItem("RX", Sheet.RX), MenuItem("RX Filter", Sheet.FILTER), MenuItem("Noise", Sheet.NOISE),
        MenuItem("AGC", Sheet.AGC)),
    listOf(MenuItem("TX", later = "M7"), MenuItem("PA", later = "M6"), MenuItem("VOX", later = "M7"),
        MenuItem("PS", later = "M7"), MenuItem("CW", later = "M7")),
    listOf(MenuItem("DSP", Sheet.DSP), MenuItem("WDSP EQ", Sheet.EQ), MenuItem("Ant", later = "M6"),
        MenuItem("OC Output", later = "M6"), MenuItem("Extras", later = "M8")),
    listOf(MenuItem("Toolbar", Sheet.TOOLBAR), MenuItem("CAT/TCI", later = "M8"), MenuItem("MIDI", later = "M8")),
)

/** The ☰ button: deskHPSDR's main menu grid. A menu opened from here comes back here when closed. */
@Composable
private fun MainMenu(st: Status, onStart: () -> Unit, close: () -> Unit, open: (Sheet) -> Unit) {
    val ctx = LocalContext.current
    var note by remember { mutableStateOf("") }
    Box(Modifier.fillMaxSize().background(Color(0x66000000)).clickable(onClick = close)) {
        BoxWithConstraints(Modifier.align(Alignment.Center).padding(6.dp)) {
            val narrow = maxWidth < 600.dp
            Column(
                Modifier.widthIn(max = 820.dp).fillMaxWidth().glass(18).background(Color(0xF20C111E))
                    .clickable(enabled = false) {}.verticalScroll(rememberScrollState()).padding(12.dp),
                verticalArrangement = Arrangement.spacedBy(6.dp),
            ) {
                Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.SpaceBetween) {
                    SheetTitle("droidHPSDR - Menu")
                    Text(
                        when (st.phase) {
                            Phase.RUNNING -> "${st.network} · ${st.sampleRate / 1000} kHz · lost ${st.lostPackets} · ADC overloads ${st.adcOverloads}"
                            Phase.DEMO -> "demo band, no radio"
                            else -> "${st.phase.name.lowercase()}${if (st.message.isNotEmpty()) ": ${st.message}" else ""}"
                        },
                        color = Dim, fontSize = 11.sp,
                    )
                }
                // first row: Close / Restart Protocol / Iconify
                Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    MenuButton("Close", Modifier.weight(1f), onClick = close)
                    if (st.phase == Phase.RUNNING) {
                        MenuButton("Restart Protocol P1", Modifier.weight(1f)) { RadioService.restart(ctx); close() }
                    } else {
                        MenuButton("Start radio", Modifier.weight(1f)) { onStart(); close() }
                    }
                    // the radio keeps running in the background (foreground service)
                    MenuButton("Iconify", Modifier.weight(1f)) { (ctx as? Activity)?.moveTaskToBack(true); close() }
                }
                Box(Modifier.fillMaxWidth().height(1.dp).background(Line))
                val press: (MenuItem) -> Unit = { item ->
                    if (item.sheet != null) open(item.sheet)
                    else note = "${item.label} isn't built yet: it comes with ${item.later}"
                }
                val blocks = if (narrow) MENU_COLUMNS.chunked(3) else listOf(MENU_COLUMNS)
                blocks.forEach { cols ->
                    Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                        cols.forEach { col ->
                            Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                                col.forEach { item ->
                                    MenuButton(item.label, Modifier.fillMaxWidth(), later = item.later) { press(item) }
                                }
                            }
                        }
                    }
                }
                Box(Modifier.fillMaxWidth().height(1.dp).background(Line))
                Row(horizontalArrangement = Arrangement.spacedBy(6.dp), verticalAlignment = Alignment.CenterVertically) {
                    Text(note, color = Amb, fontSize = 11.sp, modifier = Modifier.weight(1f))
                    MenuButton("About", Modifier.weight(1f)) { open(Sheet.ABOUT) }
                    Text("Receive only: transmit comes with M7", color = Dim, fontSize = 10.sp,
                        modifier = Modifier.weight(1f), textAlign = TextAlign.End)
                }
            }
        }
    }
}

@Composable
private fun MenuButton(label: String, modifier: Modifier, later: String? = null, onClick: () -> Unit) {
    Row(
        modifier.height(32.dp).clip(RoundedCornerShape(10.dp)).background(Color(0xFF0F1624))
            .border(1.dp, Line, RoundedCornerShape(10.dp)).clickable(onClick = onClick),
        horizontalArrangement = Arrangement.spacedBy(6.dp, Alignment.CenterHorizontally),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text(label, color = if (later == null) Txt else Dim.copy(alpha = 0.7f), fontSize = 13.sp,
            fontWeight = FontWeight.Medium, maxLines = 1)
        if (later != null) Text(later, color = Dim.copy(alpha = 0.5f), fontSize = 9.sp)
    }
}

/** deskHPSDR's SDR Device (Radio) menu: the parts built so far. The HL2 settings come with M6. */
@Composable
private fun SdrDeviceSheet(st: Status) {
    SheetTitle("SDR Device")
    Text("The HL2's own settings (IO board, HL2+, PA) come with M6. Sample rate: ☰ → RX.", color = Dim, fontSize = 11.sp)
    val region by Radio.region.collectAsState()
    Label("Band plan region")
    GridButtons(listOf("Region 1", "Region 2", "Region 3"), "Region $region", 3) { Radio.setRegion(it + 1) }
    val delay by Radio.rowDelayMs.collectAsState()
    Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
        Label("Tuning delay (test): spectrum rows are this old")
        Text("$delay ms", color = Acc, fontSize = 12.sp, fontWeight = FontWeight.Medium)
    }
    Slider(
        value = delay.toFloat(), onValueChange = { Radio.setRowDelay(it.roundToInt()) }, valueRange = 0f..400f,
        colors = SliderDefaults.colors(thumbColor = Acc, activeTrackColor = Acc),
    )
}

/** deskHPSDR's Discovery: where the radio is, start/stop, and the demo band. */
@Composable
private fun DiscoverySheet(st: Status, onStart: () -> Unit, onStop: () -> Unit, close: () -> Unit) {
    val ctx = LocalContext.current
    SheetTitle("Discovery")
    val wired by Radio.wiredIp.collectAsState()
    val wifi by Radio.wifiIp.collectAsState()
    Label("Radio address: on the USB Ethernet link / on WiFi")
    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        IpField(wired, "169.254.19.221", Modifier.weight(1f)) { Radio.setWiredIp(it) }
        IpField(wifi, "radio's IP on your WiFi", Modifier.weight(1f)) { Radio.setWifiIp(it) }
    }
    GridButtons(
        listOf(if (st.phase == Phase.RUNNING) "Stop radio" else "Start radio",
            if (st.phase == Phase.DEMO) "Stop demo" else "Demo band"), null, 2,
    ) {
        when (it) {
            0 -> { if (st.phase == Phase.RUNNING) onStop() else onStart(); close() }
            else -> { if (st.phase == Phase.DEMO) Radio.stopDemo() else Radio.startDemo(); close() }
        }
    }
    val pm = ctx.getSystemService(PowerManager::class.java)
    if (!pm.isIgnoringBatteryOptimizations(ctx.packageName)) {
        GridButtons(listOf("Let it run in the background (battery setting)"), null, 1) {
            ctx.startActivity(
                Intent(Settings.ACTION_REQUEST_IGNORE_BATTERY_OPTIMIZATIONS, Uri.parse("package:${ctx.packageName}")),
            )
        }
    }
}

/** deskHPSDR's About. */
@Composable
private fun AboutSheet() {
    val ctx = LocalContext.current
    val version = remember { runCatching { ctx.packageManager.getPackageInfo(ctx.packageName, 0).versionName }.getOrNull() ?: "?" }
    SheetTitle("About droidHPSDR")
    Text("Version $version", color = Txt, fontSize = 13.sp)
    Text(
        "An Android front end for the Hermes Lite 2, built on deskHPSDR by Heiko Amft DL1BZ, " +
            "which is built on piHPSDR by John Melton G0ORX/N6LYT and Christoph van Wüllen DL1YCF. " +
            "DSP: WDSP by Warren Pratt NR0V. GPLv3.",
        color = Dim, fontSize = 12.sp,
    )
    Text("github.com/randal007/droidHPSDR", color = Acc, fontSize = 12.sp)
}
