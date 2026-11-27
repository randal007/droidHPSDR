// droidHPSDR - GPL v3 or later.
//
// M7 step 3, CW (Randal's picks from the CW pictures, 2026-11-27): the CW
// drawer from the right edge (C1, like PureSignal's) with what you touch on
// the air, the settings page behind it (C6, all of deskHPSDR's CW menu plus
// the PTT delay), and a CW row in the TX meter card while the paddle sends
// (C4, Transmit.kt). The keyer is the HL2+'s own, paddle in the KEY jack; the
// radio side is android/native/app/cw.c (cw_menu.c's lines, and the gate that
// arms the radio's keyer only while transmitting is allowed).
package io.github.randal007.droidhpsdr

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
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlin.math.roundToInt

private val CwRed = Color(0xFFFF4D4D)
private val live get() = Radio.running && !Radio.playback.value
private fun setOn(f: Int, b: Boolean) = Tx.set(f, if (b) 1.0 else 0.0)

internal val CW_MODES = listOf("Straight", "Iambic A", "Iambic B")

internal fun isCwMode(mode: Int) = mode == Modes.CWU || mode == Modes.CWL

/** Why the radio's keyer isn't armed (cw.c, transmit.c TXR_*). */
internal fun cwLockText(r: Int): String = when (r) {
    1 -> "Transmit enabled is off (☰ → TX), or the demo band is playing"
    2 -> "The external TX inhibit input (IO1) is active"
    4 -> "Out of band: no transmit here"
    5 -> "The TX timeout switched the keyer off"
    else -> ""
}

@Composable
private fun CwDot(ok: Boolean, text: String) {
    Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
        Box(Modifier.size(9.dp).clip(CircleShape).background(if (ok) Grn else Amb))
        Text(text, color = Txt, fontSize = 12.sp)
    }
}

/** A big −/value/+ stepper (thumb-sized), for the speed. */
@Composable
private fun Stepper(value: String, unit: String, enabled: Boolean, step: (Int) -> Unit) {
    Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        BigButton("−", enabled) { step(-1) }
        Column(Modifier.weight(1f), horizontalAlignment = Alignment.CenterHorizontally) {
            Text(value, color = if (enabled) Txt else Dim, fontSize = 24.sp, fontWeight = FontWeight.SemiBold)
            Text(unit, color = Dim, fontSize = 10.sp)
        }
        BigButton("+", enabled) { step(1) }
    }
}

@Composable
private fun BigButton(t: String, enabled: Boolean, onClick: () -> Unit) {
    Box(
        Modifier.width(52.dp).height(44.dp).clip(RoundedCornerShape(12.dp)).background(Color(0xFF0F1624))
            .border(1.dp, Line, RoundedCornerShape(12.dp)).clickable(enabled = enabled, onClick = onClick),
        contentAlignment = Alignment.Center,
    ) { Text(t, color = if (enabled) Txt else Dim, fontSize = 22.sp) }
}

/** The status line of the drawer: ready, why not, or not a CW mode. */
@Composable
private fun CwStatus(v: DoubleArray) {
    when {
        !live -> Text("Start the radio to use CW.", color = Amb, fontSize = 10.5.sp)
        v[Tx.CW_NOW] == 0.0 -> CwDot(false, "Not a CW mode")
        v[Tx.CW_ARMED] != 0.0 -> CwDot(true, "Paddle ready")
        else -> CwDot(false, "Paddle off")
    }
}

// ---------------------------------------------------------------- the drawer (C1)

@Composable
internal fun CwDrawer(close: () -> Unit) {
    val v by Tx.v.collectAsState()
    var page by remember { mutableStateOf(false) }
    Box(Modifier.fillMaxSize().background(Color(0x55000000)).clickable(onClick = close)) {
        if (page) { CwSettingsPage(v) { page = false }; return@Box }
        val en = live
        val lock = v[Tx.CW_LOCK].toInt()
        Column(
            Modifier.align(Alignment.CenterEnd).fillMaxHeight().width(340.dp)
                .clip(RoundedCornerShape(topStart = 18.dp, bottomStart = 18.dp)).background(Color(0xF0090C16))
                .border(1.dp, Line, RoundedCornerShape(topStart = 18.dp, bottomStart = 18.dp))
                .clickable(enabled = false) {}.verticalScroll(rememberScrollState()).padding(14.dp),
            verticalArrangement = Arrangement.spacedBy(6.dp),
        ) {
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween, verticalAlignment = Alignment.CenterVertically) {
                SheetTitle("CW keyer")
                CwStatus(v)
            }
            if (en && v[Tx.CW_NOW] == 0.0) Text("The radio's keyer works only in CWU / CWL.", color = Dim, fontSize = 10.5.sp)
            else if (en && v[Tx.CW_ARMED] == 0.0 && lock != 0) Text(cwLockText(lock), color = Amb, fontSize = 10.5.sp)
            if (en && lock == 5) {
                Box(
                    Modifier.fillMaxWidth().height(40.dp).clip(RoundedCornerShape(11.dp)).background(Color(0xFF0F1624))
                        .border(1.dp, CwRed.copy(alpha = 0.7f), RoundedCornerShape(11.dp)).clickable { Tx.set(Tx.CW_REARM, 1.0) },
                    contentAlignment = Alignment.Center,
                ) { Text("Arm the keyer again", color = Txt, fontSize = 13.sp, fontWeight = FontWeight.Medium) }
            }
            Label("SPEED")
            Stepper("${v[Tx.CW_SPEED].roundToInt()}", "WPM", en) { d ->
                Tx.set(Tx.CW_SPEED, (v[Tx.CW_SPEED].roundToInt() + d).coerceIn(1, 60).toDouble())
            }
            Label("PADDLE")
            Seg(CW_MODES, v[Tx.CW_MODE].roundToInt(), en) { Tx.set(Tx.CW_MODE, it.toDouble()) }
            Setting("Pitch · sidetone and receive offset", v[Tx.CW_PITCH], 100f..1000f, 10.0, { "%.0f Hz".format(it) }, en) {
                Tx.set(Tx.CW_PITCH, it)
            }
            Setting("Sidetone level", v[Tx.CW_SIDE_LEVEL], 0f..127f, 1.0, { "%.0f".format(it) }, en) { Tx.set(Tx.CW_SIDE_LEVEL, it) }
            Setting("Hang time · back to receive after", v[Tx.CW_HANG], 0f..1000f, 10.0, { "%.0f ms".format(it) }, en) {
                Tx.set(Tx.CW_HANG, it)
            }
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                ZeroBeatButton(v, Modifier.weight(1f))
                SmallButton("Settings ›", Modifier.weight(1f)) { page = true }
            }
            Text("The paddle in the KEY jack keys the HL2+ itself; the app arms its keyer only while transmitting " +
                "is allowed (Transmit enabled, band edges, TX inhibit, the TX timeout). Sidetone comes from the " +
                "HL2+ speaker and phones.", color = Dim, fontSize = 10.sp)
        }
    }
}

/** actions.c CW_ZERO_BEAT: listen, find the strongest tone near the pitch, retune onto it. */
@Composable
private fun ZeroBeatButton(v: DoubleArray, modifier: Modifier = Modifier) {
    val busy = v[Tx.CW_ZB_ACTIVE] != 0.0
    SmallButton(if (busy) "Listening…" else "Zero beat", modifier, on = busy) {
        when {
            !live -> Tx.note.value = "Start the radio first" to System.currentTimeMillis()
            v[Tx.CW_NOW] == 0.0 -> Tx.note.value = "Zero beat works in CWU / CWL" to System.currentTimeMillis()
            v[Tx.ON_AIR] != 0.0 -> Tx.note.value = "Zero beat works while receiving" to System.currentTimeMillis()
            !busy -> Tx.set(Tx.CW_ZB_START, 1.0)
        }
    }
}

// ---------------------------------------------------------------- settings (C6)

@Composable
private fun CwSettingsPage(v: DoubleArray, back: () -> Unit) {
    val en = live
    Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
        Column(
            Modifier.widthIn(max = 760.dp).fillMaxWidth().padding(8.dp).clip(RoundedCornerShape(18.dp))
                .background(Color(0xF80A0E1A)).border(1.dp, Line, RoundedCornerShape(18.dp))
                .clickable(enabled = false) {}.verticalScroll(rememberScrollState()).padding(14.dp),
            verticalArrangement = Arrangement.spacedBy(6.dp),
        ) {
            Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                SmallButton("‹ CW", onClick = back)
                SheetTitle("CW settings")
                Spacer(Modifier.weight(1f))
                CwStatus(v)
            }
            if (!en) Text("Start the radio to use these.", color = Amb, fontSize = 10.5.sp)
            Row(horizontalArrangement = Arrangement.spacedBy(16.dp)) {
                Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                    Label("KEYER")
                    Seg(CW_MODES, v[Tx.CW_MODE].roundToInt(), en) { Tx.set(Tx.CW_MODE, it.toDouble()) }
                    Setting("Speed", v[Tx.CW_SPEED], 1f..60f, 1.0, { "%.0f WPM".format(it) }, en) { Tx.set(Tx.CW_SPEED, it) }
                    Setting("Weight", v[Tx.CW_WEIGHT], 0f..100f, 1.0, { "%.0f".format(it) }, en) { Tx.set(Tx.CW_WEIGHT, it) }
                    Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                        Toggle("Letter spacing", v[Tx.CW_SPACING] != 0.0, Modifier.weight(1f), enabled = en) { setOn(Tx.CW_SPACING, it) }
                        Toggle("Keys reversed", v[Tx.CW_REVERSED] != 0.0, Modifier.weight(1f), enabled = en) { setOn(Tx.CW_REVERSED, it) }
                    }
                    Label("SIDETONE")
                    Setting("Pitch · also the receive offset", v[Tx.CW_PITCH], 100f..1000f, 10.0, { "%.0f Hz".format(it) }, en) {
                        Tx.set(Tx.CW_PITCH, it)
                    }
                    Setting("Level", v[Tx.CW_SIDE_LEVEL], 0f..127f, 1.0, { "%.0f".format(it) }, en) { Tx.set(Tx.CW_SIDE_LEVEL, it) }
                }
                Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                    Label("TIMING")
                    Setting("PTT delay · RF after the amplifier's PTT", v[Tx.CW_PTT_DELAY], 0f..255f, 1.0, { "%.0f ms".format(it) }, en) {
                        Tx.set(Tx.CW_PTT_DELAY, it)
                    }
                    val cap = 900 / v[Tx.CW_SPEED].roundToInt().coerceAtLeast(1)
                    Text("deskHPSDR caps it at 900 / speed: $cap ms at ${v[Tx.CW_SPEED].roundToInt()} WPM", color = Dim, fontSize = 9.5.sp)
                    Setting("Hang time · break-in delay", v[Tx.CW_HANG], 0f..1000f, 10.0, { "%.0f ms".format(it) }, en) {
                        Tx.set(Tx.CW_HANG, it)
                    }
                    Label("ZERO BEAT")
                    Setting("Correction", v[Tx.CW_ZB_CORR], -50f..50f, 1.0, { "%+.0f Hz".format(it) }, en) { Tx.set(Tx.CW_ZB_CORR, it) }
                    ZeroBeatButton(v, Modifier.width(140.dp))
                }
            }
        }
    }
}

// ---------------------------------------------------------------- the TX meter card's CW row (C4)

/** While the paddle sends: speed and mode, and PTT lit while the radio reports it (key down, then the hang time). */
@Composable
internal fun CwMeterRow(v: DoubleArray) {
    Spacer(Modifier.height(8.dp))
    Box(Modifier.fillMaxWidth().height(1.dp).background(Color(0x1AFFFFFF)))
    Spacer(Modifier.height(7.dp))
    Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween, verticalAlignment = Alignment.CenterVertically) {
        Text("CW · ${v[Tx.CW_SPEED].roundToInt()} WPM · ${CW_MODES.getOrElse(v[Tx.CW_MODE].roundToInt()) { "?" }.uppercase()}",
            color = Dim, fontSize = 9.5.sp, letterSpacing = 0.8.sp)
        val key = v[Tx.RADIO_PTT] != 0.0
        Box(
            Modifier.clip(RoundedCornerShape(6.dp)).background(if (key) CwRed.copy(alpha = 0.9f) else Color(0x12FFFFFF))
                .padding(horizontal = 8.dp, vertical = 3.dp),
        ) { Text("PTT", color = if (key) Color.White else Dim, fontSize = 10.5.sp, fontWeight = FontWeight.SemiBold) }
    }
}
