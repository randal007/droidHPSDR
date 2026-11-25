// droidHPSDR - GPL v3 or later.
//
// The operating screen: layout 1 ("glass cockpit") from the round-1
// mock-ups, picked by Randal on 2026-11-23, with the X6100 band bar and its
// segment tag. Sizes are dp at the S26 Ultra's 832 x 384 dp landscape.
package io.github.randal007.droidhpsdr

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

private val Bg = Color(0xFF05070D)
private val Glass = Color(0xA80A101E)
private val Line = Color(0x1AFFFFFF)
private val Txt = Color(0xFFE9EEF7)
private val Dim = Color(0xFF8B97AD)
private val Acc = Color(0xFF3FD0FF)
private val Amb = Color(0xFFFFB341)
private val Grn = Color(0xFF3EE07A)
private val Red = Color(0xFFFF5A50)

private val STYLE_NAMES = listOf("X6100 look 3", "AetherSDR curtains", "Lit terrain", "Neon wire")

private enum class Sheet { BAND, MODE, AF, LNA, FILTER, MENU }

private fun Modifier.glass(r: Int = 14) =
    clip(RoundedCornerShape(r.dp)).background(Glass).border(1.dp, Line, RoundedCornerShape(r.dp))

@Composable
fun Screen(onStart: () -> Unit, onStop: () -> Unit) {
    MaterialTheme(colorScheme = darkColorScheme(primary = Acc, background = Bg, surface = Bg)) {
        val status by Radio.status.collectAsState()
        val freq by Radio.freq.collectAsState()
        val style by Radio.style.collectAsState()
        var sheet by remember { mutableStateOf<Sheet?>(null) }
        var flash by remember { mutableStateOf("" to 0L) }

        Box(Modifier.fillMaxSize().background(Bg)) {
            AndroidView(factory = { SpectrumView(it) }, modifier = Modifier.fillMaxSize())
            SpectrumOverlay(Modifier.fillMaxSize())

            MeterCard(status.meterDbm, Modifier.align(Alignment.TopStart).padding(10.dp))
            VfoPill(freq, status, { sheet = Sheet.MODE }, { sheet = Sheet.FILTER },
                Modifier.align(Alignment.TopCenter).padding(top = 8.dp))
            SegmentTag(freq, Modifier.align(Alignment.TopCenter).padding(top = 76.dp))
            Flash(flash, Modifier.align(Alignment.TopEnd).padding(top = 66.dp, end = 12.dp))

            Row(Modifier.align(Alignment.TopEnd).padding(12.dp), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                RoundButton("3D") {
                    Radio.setStyle(style + 1)
                    flash = "3D: ${STYLE_NAMES[Radio.style.value]}" to System.currentTimeMillis()
                }
                RoundButton("☰") { sheet = Sheet.MENU }
            }

            ChipRow(status, { sheet = it }, Modifier.align(Alignment.BottomStart).padding(10.dp))

            if (status.phase == Phase.DEMO) {
                Text("DEMO · no radio", color = Amb, fontSize = 12.sp, fontWeight = FontWeight.Bold,
                    modifier = Modifier.align(Alignment.TopStart).padding(start = 12.dp, top = 112.dp)
                        .glass(8).padding(horizontal = 8.dp, vertical = 3.dp))
            } else if (status.phase != Phase.RUNNING) {
                StatusCard(status, onStart, Modifier.align(Alignment.Center))
            }
            sheet?.let { SheetHost(it, status, onStart, onStop) { sheet = null } }
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
    val measurer = rememberTextMeasurer()
    val scope = rememberCoroutineScope()
    val fling = remember { arrayOfNulls<Job>(1) }

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
                val xa = ((f0 + st.filterLow - lo) / hzpx).toFloat()
                val xb = ((f0 + st.filterHigh - lo) / hzpx).toFloat()
                val dA = abs(down.position.x - xa)
                val dB = abs(down.position.x - xb)
                // 0 = tune, 1 = low edge, 2 = high edge
                val edge = when {
                    minOf(dA, dB) > edgeTol -> 0
                    dA < dB -> 1
                    else -> 2
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
                            else -> Radio.tune(f0 - dx * hzpx)
                        }
                        c.consume()
                    }
                }
                if (!moved) {
                    // tap: put that signal in the middle of the passband
                    val f = lo + down.position.x * hzpx
                    Radio.tune(f - (st.filterLow + st.filterHigh) / 2.0)
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
        val splitY = h * SpectrumView.SPLIT
        val lo = freq - span / 2
        val hzpx = span / w
        fun x(f: Double) = ((f - lo) / hzpx).toFloat()

        // passband and VFO line
        val pa = x(freq + status.filterLow.toDouble())
        val pb = x(freq + status.filterHigh.toDouble())
        drawRect(Color(0x2078C8FF), Offset(pa, 0f), Size(pb - pa, h))
        drawLine(Color(0x668CD2FF), Offset(pa, 0f), Offset(pa, h), 1.dp.toPx())
        drawLine(Color(0x668CD2FF), Offset(pb, 0f), Offset(pb, h), 1.dp.toPx())
        drawLine(Color(0xFFFF4D4D), Offset(x(freq.toDouble()), 0f), Offset(x(freq.toDouble()), h), 1.2.dp.toPx())

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

/** S-units the deskHPSDR way: S9 = -73 dBm, 6 dB per S-unit, then +10 dB segments. */
private fun sMeter(dbm: Double): Pair<String, Double> {
    if (dbm <= -73.0) {
        val s = ((dbm + 127.0) / 6.0).coerceAtLeast(0.0)
        return "S${s.toInt().coerceIn(0, 9)}" to s
    }
    val over = dbm + 73.0
    return "S9+${(over / 10).toInt() * 10}" to 9.0 + over / 10.0
}

@Composable
private fun VfoPill(freq: Long, st: Status, onMode: () -> Unit, onFilter: () -> Unit, modifier: Modifier) {
    Row(
        modifier.height(62.dp).glass().padding(horizontal = 18.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(10.dp),
    ) {
        Box(Modifier.clip(RoundedCornerShape(6.dp)).background(Amb).padding(horizontal = 7.dp, vertical = 1.dp)) {
            Text("A", color = Color(0xFF1A1000), fontSize = 11.sp, fontWeight = FontWeight.Bold)
        }
        FreqText(freq, 34)
        Column(verticalArrangement = Arrangement.spacedBy(3.dp)) {
            Tag(Modes.name(st.mode), Acc, Color(0x293FD0FF), onMode)
            Tag(widthLabel(st.filterHigh - st.filterLow), Txt, Color(0x14FFFFFF), onFilter)
        }
    }
}

@Composable
private fun FreqText(freq: Long, size: Int) {
    val mhz = freq / 1_000_000
    val khz = (freq / 1000) % 1000
    val hz = freq % 1000
    Text(
        buildAnnotatedString {
            withStyle(SpanStyle(color = Txt, fontSize = size.sp, fontWeight = FontWeight.Medium)) {
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

private fun widthLabel(hz: Int) = if (hz >= 1000) "%.1fk".format(hz / 1000.0) else "$hz"

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

@Composable
private fun RoundButton(label: String, onClick: () -> Unit) {
    Box(
        Modifier.size(44.dp).clip(CircleShape).background(Glass).border(1.dp, Line, CircleShape).clickable(onClick = onClick),
        contentAlignment = Alignment.Center,
    ) { Text(label, color = Txt, fontSize = if (label.length > 1) 13.sp else 18.sp, fontWeight = FontWeight.SemiBold) }
}

// ---------------------------------------------------------------- bottom chips

@Composable
private fun ChipRow(st: Status, open: (Sheet) -> Unit, modifier: Modifier) {
    val freq by Radio.freq.collectAsState()
    val vol by Radio.volumeDb.collectAsState()
    val lna by Radio.lnaDb.collectAsState()
    val step by Radio.step.collectAsState()
    val span by Radio.span.collectAsState()
    Row(modifier, horizontalArrangement = Arrangement.spacedBy(7.dp)) {
        Chip("BAND", BandPlan.band(freq)?.name ?: "GEN") { open(Sheet.BAND) }
        Chip("AF", "${((vol + 40) / 40 * 100).roundToInt()}") { open(Sheet.AF) }
        Chip("LNA", "%+.0f dB".format(lna)) { open(Sheet.LNA) }
        val phone by Radio.phoneAudio.collectAsState()
        Chip("PHONE", if (phone) "On" else "Off", on = phone) { Radio.setPhoneAudio(!phone) }
        Chip("FILTER", widthLabel(st.filterHigh - st.filterLow)) { open(Sheet.FILTER) }
        Chip("STEP", if (step >= 1000) "${step / 1000} kHz" else "$step Hz") {
            val steps = listOf(1, 10, 50, 100, 500, 1000)
            Radio.setStep(steps[(steps.indexOf(step) + 1) % steps.size])
        }
        Chip("SPAN", "${(span / 1000).roundToInt()} kHz") {
            val spans = listOf(6_000.0, 12_000.0, 24_000.0, 48_000.0, 96_000.0, 182_000.0)
            Radio.setSpan(spans.firstOrNull { it > span * 1.05 } ?: spans.first())
        }
    }
}

@Composable
private fun Chip(label: String, value: String, on: Boolean = false, onClick: () -> Unit) {
    Column(
        Modifier.height(46.dp).widthIn(min = 70.dp).glass(13)
            .then(if (on) Modifier.border(1.dp, Acc.copy(alpha = 0.55f), RoundedCornerShape(13.dp)) else Modifier)
            .clickable(onClick = onClick).padding(horizontal = 12.dp),
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
private fun SheetHost(which: Sheet, st: Status, onStart: () -> Unit, onStop: () -> Unit, close: () -> Unit) {
    Box(Modifier.fillMaxSize().background(Color(0x66000000)).clickable(onClick = close)) {
        Column(
            Modifier.align(Alignment.BottomCenter).widthIn(max = 560.dp).fillMaxWidth()
                .clip(RoundedCornerShape(topStart = 20.dp, topEnd = 20.dp)).background(Color(0xF20C111E))
                .border(1.dp, Line, RoundedCornerShape(topStart = 20.dp, topEnd = 20.dp))
                .clickable(enabled = false) {}.padding(16.dp),
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
                Sheet.MENU -> MenuSheet(st, onStart, onStop, close)
            }
        }
    }
}

@Composable
private fun SheetTitle(t: String) = Text(t, color = Txt, fontSize = 17.sp, fontWeight = FontWeight.Medium)

@Composable
private fun GridButtons(items: List<String>, selected: String?, cols: Int, onClick: (Int) -> Unit) {
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

@Composable
private fun BandSheet(close: () -> Unit) {
    val freq by Radio.freq.collectAsState()
    val region by Radio.region.collectAsState()
    Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
        SheetTitle("Band")
        Text("IARU Region $region", color = Dim, fontSize = 11.sp)
    }
    val bands = BandPlan.bands
    GridButtons(bands.map { it.name }, BandPlan.band(freq)?.name, 6) {
        Radio.tune(bands[it].home.toDouble())
        close()
    }
}

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

/** deskHPSDR's filter presets for the mode, as low/high edges. Dragging the passband edges also works. */
private fun presets(mode: Int): List<Pair<String, Pair<Int, Int>>> {
    fun ssb(w: Int, lsb: Boolean) = if (lsb) -(150 + w) to -150 else 150 to 150 + w
    fun dig(w: Int, lsb: Boolean) = if (lsb) -w to 0 else 0 to w
    fun sym(w: Int) = -w / 2 to w / 2
    return when (mode) {
        Modes.USB, Modes.LSB -> listOf(1800, 2100, 2400, 2700, 2900, 3300, 3800, 4400)
            .map { widthLabel(it) to ssb(it, mode == Modes.LSB) } +
            // deskHPSDR's ESSB presets: 50 Hz low edge
            listOf(2900, 3400).map { w ->
                "ESSB ${widthLabel(w)}" to if (mode == Modes.LSB) -(50 + w) to -50 else 50 to 50 + w
            }
        Modes.DIGU, Modes.DIGL -> listOf(500, 1000, 1500, 2000, 2500, 3000, 4000, 5000)
            .map { widthLabel(it) to dig(it, mode == Modes.DIGL) }
        Modes.CWU, Modes.CWL -> listOf(50, 100, 250, 400, 500, 600, 800, 1000).map { widthLabel(it) to sym(it) }
        else -> listOf(4000, 5200, 6600, 8000, 10000, 12000, 16000).map { widthLabel(it) to sym(it) }
    }
}

@Composable
private fun FilterSheet(st: Status, close: () -> Unit) {
    SheetTitle("Filter · ${Modes.name(st.mode)}")
    val p = presets(st.mode)
    GridButtons(p.map { it.first }, widthLabel(st.filterHigh - st.filterLow), 4) {
        Radio.setFilter(p[it].second.first, p[it].second.second)
        close()
    }
    Text("Or drag the passband edges on the spectrum.", color = Dim, fontSize = 11.sp)
}

@Composable
private fun MenuSheet(st: Status, onStart: () -> Unit, onStop: () -> Unit, close: () -> Unit) {
    val region by Radio.region.collectAsState()
    SheetTitle("droidHPSDR")
    Text(
        when (st.phase) {
            Phase.RUNNING -> "Receiving on ${st.network} · ${st.sampleRate / 1000} kHz · lost packets ${st.lostPackets} · ADC overloads ${st.adcOverloads}"
            else -> "${st.phase.name.lowercase()}: ${st.message}"
        },
        color = Dim, fontSize = 12.sp,
    )
    Text("Band plan region", color = Dim, fontSize = 11.sp)
    GridButtons(listOf("Region 1", "Region 2", "Region 3"), "Region $region", 3) { Radio.setRegion(it + 1) }
    val delay by Radio.rowDelayMs.collectAsState()
    Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
        Text("Tuning delay (test): spectrum rows are this old", color = Dim, fontSize = 11.sp)
        Text("$delay ms", color = Acc, fontSize = 12.sp, fontWeight = FontWeight.Medium)
    }
    Slider(
        value = delay.toFloat(), onValueChange = { Radio.setRowDelay(it.roundToInt()) }, valueRange = 0f..400f,
        colors = SliderDefaults.colors(thumbColor = Acc, activeTrackColor = Acc),
    )
    GridButtons(
        listOf(if (st.phase == Phase.RUNNING) "Stop radio" else "Start radio",
            if (st.phase == Phase.DEMO) "Stop demo" else "Demo band"), null, 2,
    ) {
        when (it) {
            0 -> if (st.phase == Phase.RUNNING) onStop() else onStart()
            else -> if (st.phase == Phase.DEMO) Radio.stopDemo() else Radio.startDemo()
        }
        close()
    }
    Text("Receive only: transmit is not built yet (M7).", color = Dim, fontSize = 11.sp)
}
