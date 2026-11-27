// droidHPSDR - GPL v3 or later.
//
// M7 step 4, PureSignal (Randal's picks from m7-round3, 2026-11-26): the
// PureSignal drawer from the right edge (S1, like the TX audio drawer) with
// everything of deskHPSDR's PS menu, AmpView as deskHPSDR draws it (S4: one
// graph, magnitude and phase together), and the rarely-changed settings
// (S6, without the sample-rate switch: deskHPSDR runs PS at the receivers'
// rate, and Thetis does 384 kHz on an HL2 too). The PS chip opens it. The
// radio side is android/native/app/puresignal.c (ps_menu.c's lines).
package io.github.randal007.droidhpsdr

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
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
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.PathEffect
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.drawText
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.rememberTextMeasurer
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlinx.coroutines.delay
import kotlin.math.atan2
import kotlin.math.roundToInt

/** deskHPSDR's AmpView data layout (droid_app.h PSD_*) */
private object PsD {
    const val SAMPLES = 4096; const val POINTS = 512
    const val X = 3; const val YM = X + SAMPLES; const val YC = YM + SAMPLES; const val YS = YC + SAMPLES
    const val XM_COR = YS + SAMPLES; const val YM_COR = XM_COR + POINTS; const val XA_COR = YM_COR + POINTS
    const val YA_COR = XA_COR + POINTS; const val COUNT = YA_COR + POINTS
}

private val FbBlue = Color(0xFF4A8CFF)
private val FbYellow = Color(0xFFFFD250)
private val PsRed = Color(0xFFFF4D4D)

private val live get() = Radio.running && !Radio.playback.value
private fun on(v: DoubleArray, f: Int) = v[f] != 0.0
private fun setOn(f: Int, b: Boolean) = Tx.set(f, if (b) 1.0 else 0.0)

/** ps_menu.c's colours for the feedback level: blue > 181, green > 128, yellow > 90, red below. */
internal fun psFeedbackColor(level: Int) = when {
    level > 181 -> FbBlue
    level > 128 -> Grn
    level > 90 -> FbYellow
    else -> PsRed
}

private val PS_STATES = listOf("RESET", "WAIT", "MOXDELAY", "SETUP", "COLLECT", "MOXCHECK", "CALC", "DELAY", "STAYON", "TURNON")

/** What the PS chip shows: Off, On, or the feedback level with ✓ while correcting. */
internal fun psLabel(v: DoubleArray): String {
    if (!on(v, Tx.PS_ENABLE)) return "Off"
    val fb = v[Tx.PS_FEEDBACK].toInt()
    return if (fb <= 0) "On" else "$fb" + if (on(v, Tx.PS_CORRECTING)) " ✓" else ""
}

@Composable
private fun StatusDot(on: Boolean, text: String) {
    Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
        Box(Modifier.size(9.dp).clip(CircleShape).background(if (on) Grn else PsRed))
        Text(text, color = Txt, fontSize = 12.sp)
    }
}

/** The feedback level on deskHPSDR's colours, 0..256, the "optimal 140..165" band framed. */
@Composable
private fun FeedbackBar(level: Int, modifier: Modifier = Modifier) {
    Canvas(modifier.fillMaxWidth().height(16.dp)) {
        val w = size.width; val h = size.height
        fun x(v: Int) = v / 256f * w
        listOf(Triple(0, 90, PsRed), Triple(90, 128, FbYellow), Triple(128, 181, Grn), Triple(181, 256, FbBlue)).forEach { (a, b, c) ->
            drawRect(c.copy(alpha = 0.16f), Offset(x(a), 0f), Size(x(b) - x(a), h))
        }
        if (level > 0) drawRect(psFeedbackColor(level).copy(alpha = 0.9f), Offset.Zero, Size(x(level.coerceAtMost(256)), h))
        drawRect(Color.White.copy(alpha = 0.8f), Offset(x(140), -1f), Size(x(165) - x(140), h + 2f), style = Stroke(1.5.dp.toPx()))
    }
}

/** Two Tone / Noise: they key the radio, so red-edged, red when on. */
@Composable
private fun KeyButton(t: String, on: Boolean, modifier: Modifier = Modifier, onClick: () -> Unit) {
    Box(
        modifier.height(40.dp).clip(RoundedCornerShape(11.dp))
            .background(if (on) PsRed.copy(alpha = 0.85f) else Color(0xFF0F1624))
            .border(1.dp, PsRed.copy(alpha = if (on) 0.9f else 0.6f), RoundedCornerShape(11.dp))
            .clickable(onClick = onClick),
        contentAlignment = Alignment.Center,
    ) { Text(t, color = if (on) Color.White else Txt, fontSize = 13.sp, fontWeight = FontWeight.Medium) }
}

// ---------------------------------------------------------------- the drawer (S1)

@Composable
internal fun PsDrawer(close: () -> Unit) {
    val v by Tx.v.collectAsState()
    var page by remember { mutableStateOf<String?>(null) }
    Box(Modifier.fillMaxSize().background(Color(0x55000000)).clickable(onClick = close)) {
        when (page) {
            "AMP" -> { AmpViewPage(v) { page = null }; return@Box }
            "SET" -> { PsSettingsPage(v) { page = null }; return@Box }
        }
        val en = live
        val ps = on(v, Tx.PS_ENABLE)
        val fb = v[Tx.PS_FEEDBACK].toInt()
        Column(
            Modifier.align(Alignment.CenterEnd).fillMaxHeight().width(340.dp)
                .clip(RoundedCornerShape(topStart = 18.dp, bottomStart = 18.dp)).background(Color(0xF0090C16))
                .border(1.dp, Line, RoundedCornerShape(topStart = 18.dp, bottomStart = 18.dp))
                .clickable(enabled = false) {}.verticalScroll(rememberScrollState()).padding(14.dp),
            verticalArrangement = Arrangement.spacedBy(6.dp),
        ) {
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween, verticalAlignment = Alignment.CenterVertically) {
                SheetTitle("PureSignal")
                if (en && ps) StatusDot(on(v, Tx.PS_CORRECTING), "Correcting")
            }
            if (!en) Text("Start the radio to use PureSignal.", color = Amb, fontSize = 10.5.sp)
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp), verticalAlignment = Alignment.CenterVertically) {
                Toggle("PureSignal", ps, Modifier.weight(1f), enabled = en) { setOn(Tx.PS_ENABLE, it) }
                SmallButton("OFF") { if (en && ps) Tx.set(Tx.PS_RESET, 1.0) }
                SmallButton("Restart") { if (en && ps) Tx.set(Tx.PS_RESUME, 1.0) }
            }
            Label(if (ps && fb > 0) "FEEDBACK LEVEL · $fb" else "FEEDBACK LEVEL · aim 140-165")
            FeedbackBar(if (ps) fb else 0)
            if (ps) Text(
                "state ${PS_STATES.getOrElse(v[Tx.PS_STATE].toInt()) { "?" }}   cal. ${v[Tx.PS_COR_COUNT].toInt()}   " +
                    "GetPk %.3f".format(v[Tx.PS_GETPK]),
                color = Dim, fontSize = 10.5.sp,
            )
            Label("CALIBRATE · keys the radio until switched off")
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp), verticalAlignment = Alignment.CenterVertically) {
                KeyButton("Two Tone", on(v, Tx.PS_TWOTONE), Modifier.weight(1f)) {
                    if (!en) Tx.note.value = "Start the radio first" to System.currentTimeMillis()
                    else setOn(Tx.PS_TWOTONE, !on(v, Tx.PS_TWOTONE))
                }
                KeyButton("Noise", on(v, Tx.PS_NOISE), Modifier.weight(1f)) {
                    if (!en) Tx.note.value = "Start the radio first" to System.currentTimeMillis()
                    else setOn(Tx.PS_NOISE, !on(v, Tx.PS_NOISE))
                }
                Toggle("Auto att.", on(v, Tx.PS_AUTO), Modifier.weight(1.2f), enabled = en) { setOn(Tx.PS_AUTO, it) }
            }
            val auto = on(v, Tx.PS_AUTO)
            Setting(if (auto) "TX ATT · per band · set by Auto att." else "TX ATT · per band", v[Tx.PS_ATT], -29f..31f, 1.0,
                { "%.0f dB".format(it) }, en && ps && !auto) { Tx.set(Tx.PS_ATT, it) }
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                Toggle("MON", on(v, Tx.PS_MON), Modifier.weight(1f), enabled = en, sub = "feedback") { setOn(Tx.PS_MON, it) }
                Toggle("OneShot", on(v, Tx.PS_ONESHOT), Modifier.weight(1f), enabled = en) { setOn(Tx.PS_ONESHOT, it) }
            }
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                SmallButton("AmpView ›", Modifier.weight(1f)) { page = "AMP" }
                SmallButton("Settings ›", Modifier.weight(1f)) { page = "SET" }
            }
            Text("MON shows the feedback on the TX spectrum while transmitting. Two Tone and Noise follow " +
                "Transmit enabled, the band edges and the TX timeout; they stop when you go back to receive.",
                color = Dim, fontSize = 10.sp)
        }
    }
}

/** The pages' frame, with a back button to the drawer. */
@Composable
private fun PsPage(title: String, back: () -> Unit, right: @Composable () -> Unit = {}, body: @Composable () -> Unit) {
    Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
        Column(
            Modifier.widthIn(max = 760.dp).fillMaxWidth().padding(8.dp).clip(RoundedCornerShape(18.dp))
                .background(Color(0xF806080E)).border(1.dp, Line, RoundedCornerShape(18.dp))
                .clickable(enabled = false) {}.verticalScroll(rememberScrollState()).padding(14.dp),
            verticalArrangement = Arrangement.spacedBy(6.dp),
        ) {
            Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                SmallButton("‹ PureSignal", onClick = back)
                SheetTitle(title)
                Spacer(Modifier.weight(1f))
                right()
            }
            if (!live) Text("Start the radio to use these.", color = Amb, fontSize = 10.5.sp)
            body()
        }
    }
}

// ---------------------------------------------------------------- AmpView (S4)

/** ps_menu.c's AmpView: the amplifier's magnitude (blue) and phase (yellow) as PS measures them, and PS's
 *  correction (red, green), 10 times a second. Drawn as deskHPSDR draws it. */
@Composable
private fun AmpViewPage(v: DoubleArray, back: () -> Unit) {
    val buf = remember { FloatArray(PsD.COUNT) }
    var frame by remember { mutableIntStateOf(0) }
    LaunchedEffect(Unit) {
        while (true) {
            if (live && Native.psDisp(buf) > 0) frame++
            delay(100)
        }
    }
    val ps = on(v, Tx.PS_ENABLE)
    val fb = v[Tx.PS_FEEDBACK].toInt()
    PsPage("AmpView", back, right = {
        if (ps) Row(horizontalArrangement = Arrangement.spacedBy(14.dp), verticalAlignment = Alignment.CenterVertically) {
            StatusDot(on(v, Tx.PS_CORRECTING), "Correcting")
            Text("feedback $fb", color = if (fb > 0) psFeedbackColor(fb) else Dim, fontSize = 12.sp, fontWeight = FontWeight.Medium)
            Text("GetPk %.3f".format(v[Tx.PS_GETPK]), color = Dim, fontSize = 11.sp)
        }
    }) {
        val tm = rememberTextMeasurer()
        val lab = TextStyle(color = Color(0xFFF2B3A6), fontSize = 10.sp)
        Canvas(Modifier.fillMaxWidth().height(300.dp)) {
            if (frame < 0) return@Canvas   // reading it redraws on new data
            val left = 40.dp.toPx(); val right = 44.dp.toPx(); val top = 6.dp.toPx(); val bottom = 22.dp.toPx()
            val gw = size.width - left - right; val gh = size.height - top - bottom
            fun mx(x: Float) = left + gw * x.coerceIn(0f, 1f)
            fun my(m: Float) = top + gh * (1f - m.coerceIn(0f, 1f))
            fun py(deg: Float) = top + gh * (0.5f - deg.coerceIn(-180f, 180f) / 360f)
            drawRect(Color(0xFF050505), Offset(left, top), Size(gw, gh))
            for (i in 0..5) {
                val f = i / 5f
                drawLine(Color(0xFF616161), Offset(left + gw * f, top), Offset(left + gw * f, top + gh), 1f)
                drawLine(Color(0xFF616161), Offset(left, top + gh * f), Offset(left + gw, top + gh * f), 1f)
                drawText(tm, "%.1f".format(f), Offset(left + gw * f - 8.dp.toPx(), top + gh + 4.dp.toPx()), lab)
                drawText(tm, "%.1f".format(f), Offset(left - 26.dp.toPx(), top + gh * (1 - f) - 7.dp.toPx()), lab)
                drawText(tm, "${-180 + i * 72}", Offset(left + gw + 6.dp.toPx(), top + gh * (1 - f) - 7.dp.toPx()), lab)
            }
            drawRect(Color(0xFFBFBFBF), Offset(left, top), Size(gw, gh), style = Stroke(1f))
            // Thetis AmpView's reference lines: unity magnitude and zero phase
            drawLine(Color(0xFFA6A6A6), Offset(left, top + gh), Offset(left + gw, top), 1f)
            drawLine(Color(0xFF737373), Offset(left, top + gh / 2), Offset(left + gw, top + gh / 2), 1f,
                pathEffect = PathEffect.dashPathEffect(floatArrayOf(4.dp.toPx(), 4.dp.toPx())))
            val ns = buf[0].toInt().coerceIn(0, PsD.SAMPLES)
            val cp = buf[1].toInt().coerceIn(0, PsD.POINTS)
            val ref = buf[2]
            val dot = 1.6.dp.toPx()
            // amplifier magnitude: input magnitude versus measured output magnitude (ps_menu.c)
            for (i in 0 until ns) {
                val x = buf[PsD.X + i]
                drawRect(Color(0xFF33A6FF), Offset(mx(buf[PsD.YM + i] * x), my(x)), Size(dot, dot))
            }
            // amplifier phase, from the measured cosine / sine parts
            for (i in 0 until ns) {
                var ph = Math.toDegrees(atan2(buf[PsD.YS + i].toDouble(), buf[PsD.YC + i].toDouble())).toFloat() - ref
                while (ph > 180f) ph -= 360f
                while (ph < -180f) ph += 360f
                drawRect(Color(0xFFFFCC26), Offset(mx(buf[PsD.X + i]), py(ph)), Size(dot, dot))
            }
            if (cp > 1) {
                val mag = Path(); val phs = Path()
                for (i in 0 until cp) {
                    val xm = buf[PsD.XM_COR + i]
                    val p1 = Offset(mx(xm), my(buf[PsD.YM_COR + i] * xm))   // ym_cor is a gain, not an output
                    val p2 = Offset(mx(buf[PsD.XA_COR + i]), py(buf[PsD.YA_COR + i]))
                    if (i == 0) { mag.moveTo(p1.x, p1.y); phs.moveTo(p2.x, p2.y) } else { mag.lineTo(p1.x, p1.y); phs.lineTo(p2.x, p2.y) }
                }
                drawPath(mag, Color(0xFFF23359), style = Stroke(1.6.dp.toPx()))
                drawPath(phs, Color(0xFF33D94D), style = Stroke(1.6.dp.toPx()))
            }
            // the legend, as Thetis names them
            val lx = left + gw - 210.dp.toPx(); val ly = top + gh - 48.dp.toPx()
            listOf(Triple("Mag amp", Color(0xFF33A6FF), 0), Triple("Mag corr", Color(0xFFF23359), 1),
                Triple("Phs amp", Color(0xFFFFCC26), 2), Triple("Phs corr", Color(0xFF33D94D), 3)).forEach { (t, c, k) ->
                val x0 = lx + (k % 2) * 104.dp.toPx(); val y0 = ly + (k / 2) * 22.dp.toPx()
                drawLine(c, Offset(x0, y0), Offset(x0 + 20.dp.toPx(), y0), 2.dp.toPx())
                drawText(tm, t, Offset(x0 + 26.dp.toPx(), y0 - 7.dp.toPx()), TextStyle(color = Color(0xFFD9D9D9), fontSize = 10.sp))
            }
        }
        val ns = buf[0].toInt(); val cp = buf[1].toInt()
        Text("input magnitude →   ·   samples $ns   ·   correction points $cp   ·   phase reference %.1f°".format(buf[2]) +
            if (ps) "" else "   ·   PureSignal is off", color = Dim, fontSize = 10.sp)
    }
}

// ---------------------------------------------------------------- settings (S6)

@Composable
private fun PsSettingsPage(v: DoubleArray, back: () -> Unit) {
    val en = live
    PsPage("PureSignal settings", back) {
        Row(horizontalArrangement = Arrangement.spacedBy(16.dp)) {
            Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                Label("STABILITY (PS3 compression check)")
                Seg(listOf("Strict", "Medium", "Relaxed"), v[Tx.PS_TOLERANCE].roundToInt(), en) { Tx.set(Tx.PS_TOLERANCE, it.toDouble()) }
                Text("Strict 0.06 · Medium 0.04 · Relaxed 0.02 (deskHPSDR's default: suits a hard-driven amplifier)",
                    color = Dim, fontSize = 9.5.sp)
                Setting("Noise level (calibration)", v[Tx.PS_NOISE_LEVEL], -12f..3f, 1.0, { "%+.0f dB".format(it) }, en) {
                    Tx.set(Tx.PS_NOISE_LEVEL, it)
                }
                Toggle("Warn when TX ATT is 0 dB", on(v, Tx.PS_ZERO_WARN), Modifier.fillMaxWidth(), enabled = en) { setOn(Tx.PS_ZERO_WARN, it) }
            }
            Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                Setting("TX delay (+ delays TX, − delays RX)", v[Tx.PS_DELAY], -10000f..10000f, 50.0, { "%.0f ns".format(it) }, en) {
                    Tx.set(Tx.PS_DELAY, it)
                }
                Setting("SetPk · the HL2's peak", v[Tx.PS_SETPK], 0.02f..1.0f, 0.001, { "%.3f".format(it) }, en) { Tx.set(Tx.PS_SETPK, it) }
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp), verticalAlignment = Alignment.CenterVertically) {
                    SmallButton("HL2 default 0.240") { if (en) Tx.set(Tx.PS_SETPK, 0.24) }
                    Text("GetPk now %.3f".format(v[Tx.PS_GETPK]), color = Dim, fontSize = 10.sp)
                }
            }
        }
    }
}

// ---------------------------------------------------------------- the TX ATT 0 warning

/** radio.c / ps_menu.c's warning when PureSignal goes on with TX ATT 0 dB. */
@Composable
internal fun PsZeroAttWarning() {
    val show by Tx.psWarn.collectAsState()
    if (!show) return
    var dontShow by remember { mutableStateOf(false) }
    val att = Tx.v.value[Tx.PS_ATT].toInt()
    Box(Modifier.fillMaxSize().background(Color(0x66000000)).clickable { Tx.psWarn.value = false }, contentAlignment = Alignment.Center) {
        Column(
            Modifier.width(360.dp).clip(RoundedCornerShape(14.dp)).background(Color(0xF8140A0E))
                .border(1.dp, PsRed.copy(alpha = 0.6f), RoundedCornerShape(14.dp)).clickable(enabled = false) {}.padding(14.dp),
            verticalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            Text("PureSignal TX attenuation", color = PsRed, fontSize = 15.sp, fontWeight = FontWeight.SemiBold)
            Text("TX attenuation is set very low ($att dB). Check the PureSignal feedback level or calibrate again: " +
                "a feedback level that is too high may overload the HL2's ADC. External attenuation in the feedback " +
                "path may make $att dB a valid setting.", color = Dim, fontSize = 11.sp)
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp), verticalAlignment = Alignment.CenterVertically) {
                Toggle("Don't show again", dontShow, Modifier.weight(1f)) { dontShow = it }
                SmallButton("OK") {
                    if (dontShow) Tx.set(Tx.PS_ZERO_WARN, 0.0)
                    Tx.psWarn.value = false
                }
            }
        }
    }
}
