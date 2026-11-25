// droidHPSDR - GPL v3 or later.
//
// M4 leftovers: deskHPSDR's Meter menu (meter type digital/analog, S-meter
// peak/average; M4 pick G, picture m4-round1 V7) and the rest of its AGC menu
// (AGC gain, hang threshold, AGC Automatic with its offset).
package io.github.randal007.droidhpsdr

import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.tween
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.combinedClickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.drawText
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.rememberTextMeasurer
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlin.math.cos
import kotlin.math.sin

/** S-meter level (0 = S0 .. 9 = S9 .. 15 = S9+60) to the needle angle, degrees from 3 o'clock, up positive. */
private fun needleDeg(level: Float) = 155f - level.coerceIn(0f, 15f) / 15f * 130f

/** The analog S-meter (pick G): an arc, a needle that moves smoothly, Peak/Avg under it. Hold: Meter menu. */
@OptIn(ExperimentalFoundationApi::class)
@Composable
internal fun AnalogMeter(dbm: Double, modifier: Modifier, onHold: () -> Unit) {
    val (label, level) = sMeter(dbm)
    val d by Radio.dsp.collectAsState()
    // a short glide, so the needle moves like a needle instead of jumping (no flicker)
    val lv by animateFloatAsState(level.toFloat(), tween(180), label = "needle")
    val measurer = rememberTextMeasurer()
    Column(
        modifier.width(200.dp).glass().combinedClickable(onClick = {}, onLongClick = onHold)
            .padding(horizontal = 10.dp, vertical = 8.dp),
        verticalArrangement = Arrangement.spacedBy(4.dp),
    ) {
        Canvas(Modifier.fillMaxWidth().height(78.dp)) {
            val cx = size.width / 2
            val cy = size.height * 1.02f
            val r = size.width * 0.44f
            fun at(deg: Float, rr: Float) = Offset(cx + rr * cos(Math.toRadians(deg.toDouble())).toFloat(),
                cy - rr * sin(Math.toRadians(deg.toDouble())).toFloat())
            // green to S9, red above: like the digital bar
            val s9 = needleDeg(9f)
            drawArc(Grn, -155f, 155f - s9, false, Offset(cx - r, cy - r), Size(2 * r, 2 * r), style = Stroke(3.dp.toPx(), cap = StrokeCap.Round))
            drawArc(Red, -s9, s9 - needleDeg(15f), false, Offset(cx - r, cy - r), Size(2 * r, 2 * r), style = Stroke(3.dp.toPx(), cap = StrokeCap.Round))
            val ts = TextStyle(color = Dim, fontSize = 9.sp)
            for ((t, lvl) in listOf("1" to 1f, "3" to 3f, "5" to 5f, "7" to 7f, "9" to 9f, "+20" to 11f, "+40" to 13f, "+60" to 15f)) {
                val p = at(needleDeg(lvl), r - 12.dp.toPx())
                val m = measurer.measure(t, if (lvl > 9f) ts.copy(color = Color(0xFFFF8A80)) else ts)
                drawText(m, topLeft = Offset(p.x - m.size.width / 2, p.y - m.size.height / 2))
            }
            drawLine(Color.White, at(needleDeg(lv), 6.dp.toPx()), at(needleDeg(lv), r + 3.dp.toPx()), 2.dp.toPx(), StrokeCap.Round)
            drawCircle(Color.White, 3.5.dp.toPx(), Offset(cx, cy))
        }
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween, verticalAlignment = Alignment.CenterVertically) {
            Text(label, color = Txt, fontSize = 18.sp, fontWeight = FontWeight.Medium)
            Text("%.0f dBm".format(dbm), color = Dim, fontSize = 11.sp)
        }
        Seg(listOf("Peak", "Avg"), if (d[Dsp.SMETER_PEAK] != 0.0) 0 else 1, h = 28) {
            Radio.setDsp(Dsp.SMETER_PEAK, if (it == 0) 1.0 else 0.0)
        }
    }
}

/** deskHPSDR's Meter menu: meter type and the S-meter reading (TX ALC comes with M7). */
@Composable
internal fun MeterSheet() {
    val d by Radio.dsp.collectAsState()
    val analog by Radio.analogMeter.collectAsState()
    SheetTitle("Meter")
    Label("Meter type")
    Seg(listOf("Digital", "Analog"), if (analog) 1 else 0) { Radio.setAnalogMeter(it == 1) }
    Label("S-meter reading")
    Seg(listOf("Peak", "Average"), if (d[Dsp.SMETER_PEAK] != 0.0) 0 else 1) {
        Radio.setDsp(Dsp.SMETER_PEAK, if (it == 0) 1.0 else 0.0)
    }
    Text("Hold the meter to open this. TX ALC reading comes with transmit (M7).", color = Dim, fontSize = 10.5.sp)
}

/** The rest of deskHPSDR's AGC menu, under the AGC modes. */
@Composable
internal fun AgcMore(st: Status) {
    val d by Radio.dsp.collectAsState()
    val auto = d[Dsp.AGC_AUTO] != 0.0
    Toggle("AGC Automatic", auto, sub = "sets the AGC gain from the noise floor every 5 s") {
        Radio.setDsp(Dsp.AGC_AUTO, if (it) 1.0 else 0.0)
    }
    Setting("Auto offset", d[Dsp.AGC_AUTO_OFFSET], -35f..-15f, 1.0, { "%.0f dB".format(it) }, auto) {
        Radio.setDsp(Dsp.AGC_AUTO_OFFSET, it)
    }
    Setting("AGC gain (or drag the amber AGC-G line)", d[Dsp.AGC_GAIN], -20f..120f, 1.0, { "%.0f dB".format(it) }, !auto) {
        Radio.setDsp(Dsp.AGC_GAIN, it)
    }
    Setting("Hang threshold (Long and Slow; or drag the AGC-Th line)", d[Dsp.AGC_HANG_TH], 0f..100f, 1.0,
        { "%.0f".format(it) }, st.agc == 1 || st.agc == 2) { Radio.setDsp(Dsp.AGC_HANG_TH, it) }
}
