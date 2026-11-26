// droidHPSDR - GPL v3 or later.
//
// M6 (Randal's pick B, pictures F1/F2 in tools/mockups/layouts8.js): the
// HL2's lines of deskHPSDR's Radio menu as a section of the SDR Device sheet,
// and deskHPSDR's P1 Setup (reboot, EEPROM network settings) on its own page,
// over Ethernet only, behind a warning. See docs/M6-INVENTORY.md.
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
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
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
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

/** The HL2 settings (deskHPSDR keeps them in its props file; we keep them here and send them at every start). */
internal object Hl2 {
    private lateinit var prefs: SharedPreferences
    val codec = MutableStateFlow(true)        // HL2+ (AK4951); false = no codec
    val txOnly = MutableStateFlow(false)      // filter board: N2ADR (LPF RX + TX + HPF) / N2ADR (LPF TX only)
    val hpf = MutableStateFlow(true)          // "+RX: N2ADR HPF 3MHz" (TX only mode)
    val ppm = MutableStateFlow(0.0)           // Freq. Calibration (ppm factor)
    val adcCal = MutableStateFlow(0)          // ADC Gain Calibration (dB)
    val forceIob = MutableStateFlow(false)    // HL2 Force IO Board
    val splitOff = MutableStateFlow(false)    // Disable Split on Band Change
    val region60 = MutableStateFlow(0)        // 60 m channel markers: 0 None (VFO), 1 UK, 2 US, 3 WRC-15, 4 CA
    val beacons = MutableStateFlow(false)     // IARU protected beacon areas (Region 1)
    val contestFree = MutableStateFlow(false) // IARU contest-free areas (Region 1)
    /** The 60 m channels on the spectrum (Hz centre to Hz width), read from deskHPSDR's table. */
    val channels60 = MutableStateFlow<List<Pair<Long, Long>>>(emptyList())

    // Native.hl2Set fields (droid_app.h)
    private const val F_CODEC = 0
    private const val F_FILTER = 1
    private const val F_HPF = 2
    private const val F_PPM = 3
    private const val F_ADC = 4
    private const val F_FORCE = 5
    private const val F_SPLIT_OFF = 6
    private const val F_REGION60 = 7
    private const val F_IARU = 8

    fun init(ctx: Context) {
        prefs = ctx.getSharedPreferences("radio", Context.MODE_PRIVATE)
        codec.value = prefs.getBoolean("hl2.codec", true)
        txOnly.value = prefs.getBoolean("hl2.txOnly", false)
        hpf.value = prefs.getBoolean("hl2.hpf", true)
        ppm.value = prefs.getFloat("hl2.ppm", 0f).toDouble()
        adcCal.value = prefs.getInt("hl2.adcCal", 0)
        forceIob.value = prefs.getBoolean("hl2.forceIob", false)
        splitOff.value = prefs.getBoolean("hl2.splitOff", false)
        region60.value = prefs.getInt("hl2.region60", 0)
        beacons.value = prefs.getBoolean("hl2.beacons", false)
        contestFree.value = prefs.getBoolean("hl2.contestFree", false)
    }

    /** After every start: the radio core begins with deskHPSDR's HL2 defaults. */
    fun apply() {
        Native.hl2Set(F_FORCE, if (forceIob.value) 1.0 else 0.0)
        Native.hl2Set(F_CODEC, if (codec.value) 1.0 else 0.0)
        Native.hl2Set(F_HPF, if (hpf.value) 1.0 else 0.0)
        Native.hl2Set(F_FILTER, if (txOnly.value) 1.0 else 0.0)
        Native.hl2Set(F_PPM, ppm.value)
        Native.hl2Set(F_ADC, adcCal.value.toDouble())
        Native.hl2Set(F_SPLIT_OFF, if (splitOff.value) 1.0 else 0.0)
        Native.hl2Set(F_IARU, Radio.region.value.toDouble())
        Native.hl2Set(F_REGION60, region60.value.toDouble())
        refreshChannels()
    }

    /** The command queue runs them in order: read the table once the region change has been applied. */
    private fun refreshChannels() {
        Thread {
            Thread.sleep(300)
            channels60.value = Native.channels60m().split(";").filter { it.contains(':') }
                .map { it.substringBefore(':').toLong() to it.substringAfter(':').toLong() }
        }.start()
    }

    fun sendIaru() { send(F_IARU, Radio.region.value.toDouble()) }

    private fun send(f: Int, v: Double) { if (Radio.running) Native.hl2Set(f, v) }
    fun setCodec(on: Boolean) { codec.value = on; prefs.edit().putBoolean("hl2.codec", on).apply(); send(F_CODEC, if (on) 1.0 else 0.0) }
    fun setTxOnly(on: Boolean) { txOnly.value = on; prefs.edit().putBoolean("hl2.txOnly", on).apply(); send(F_FILTER, if (on) 1.0 else 0.0) }
    fun setHpf(on: Boolean) { hpf.value = on; prefs.edit().putBoolean("hl2.hpf", on).apply(); send(F_HPF, if (on) 1.0 else 0.0) }
    fun setPpm(v: Double) { ppm.value = v; prefs.edit().putFloat("hl2.ppm", v.toFloat()).apply(); send(F_PPM, v) }
    fun setAdcCal(v: Int) { adcCal.value = v; prefs.edit().putInt("hl2.adcCal", v).apply(); send(F_ADC, v.toDouble()) }
    fun setSplitOff(on: Boolean) { splitOff.value = on; prefs.edit().putBoolean("hl2.splitOff", on).apply(); send(F_SPLIT_OFF, if (on) 1.0 else 0.0) }
    fun setRegion60(r: Int) {
        region60.value = r
        prefs.edit().putInt("hl2.region60", r).apply()
        if (Radio.running) { Native.hl2Set(F_REGION60, r.toDouble()); refreshChannels() }
    }
    fun setBeacons(on: Boolean) { beacons.value = on; prefs.edit().putBoolean("hl2.beacons", on).apply() }
    fun setContestFree(on: Boolean) { contestFree.value = on; prefs.edit().putBoolean("hl2.contestFree", on).apply() }
    fun setForceIob(on: Boolean) { forceIob.value = on; prefs.edit().putBoolean("hl2.forceIob", on).apply(); send(F_FORCE, if (on) 1.0 else 0.0) }
}

/** What old_protocol.c's IO board detection found. */
private data class IoInfo(val iob: Boolean, val pico: Boolean, val lpf: String, val gateware: Int)

private fun ioInfo(): IoInfo {
    val p = Native.hl2Info().split("|")
    return IoInfo(p[0] == "1", p[1] == "1", p.getOrElse(2) { "" }, p.getOrElse(3) { "0" }.toIntOrNull() ?: 0)
}

/** deskHPSDR's SDR Device (Radio) menu, the HL2's lines (picture F1). */
@Composable
internal fun SdrDeviceSheet(st: Status, openSetup: () -> Unit) {
    val region by Radio.region.collectAsState()
    val codec by Hl2.codec.collectAsState()
    val txOnly by Hl2.txOnly.collectAsState()
    val hpf by Hl2.hpf.collectAsState()
    val ppm by Hl2.ppm.collectAsState()
    val adc by Hl2.adcCal.collectAsState()
    val force by Hl2.forceIob.collectAsState()
    var io by remember { mutableStateOf(IoInfo(false, false, "", 0)) }
    LaunchedEffect(st.phase) { while (true) { io = ioInfo(); delay(1000) } }
    SheetTitle("SDR Device")
    Row(horizontalArrangement = Arrangement.spacedBy(14.dp)) {
        Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(5.dp)) {
            Label("BAND PLAN REGION")
            Seg(listOf("Region 1", "Region 2", "Region 3"), region - 1, h = 30) { Radio.setRegion(it + 1) }
            val r60 by Hl2.region60.collectAsState()
            val bea by Hl2.beacons.collectAsState()
            val cf by Hl2.contestFree.collectAsState()
            Label("60 M CHANNEL MARKERS")
            Seg(listOf("None", "UK", "US", "WRC-15", "CA"), r60, h = 30) { Hl2.setRegion60(it) }
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                Toggle("Beacon areas", bea, Modifier.weight(1f), enabled = region == 1, sub = "Region 1") { Hl2.setBeacons(it) }
                Toggle("Contest-free", cf, Modifier.weight(1f), enabled = region == 1, sub = "Region 1") { Hl2.setContestFree(it) }
            }
            Label("HL2 · IO BOARD")
            IoStatus(st, io, force)
            Toggle("Force IO board", force, Modifier.fillMaxWidth(), sub = "if it isn't found") { Hl2.setForceIob(it) }
            Label("HL2+ AUDIO CODEC")
            Seg(listOf("HL2+ (AK4951)", "None"), if (codec) 0 else 1, h = 30) { Hl2.setCodec(it == 0) }
            val delayMs by Radio.rowDelayMs.collectAsState()
            Setting("Tuning delay (test): spectrum rows are this old", delayMs.toDouble(), 0f..400f, 5.0,
                { "%.0f ms".format(it) }) { Radio.setRowDelay(it.toInt()) }
        }
        Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(5.dp)) {
            Label("FILTER BOARD")
            Seg(listOf("RX + TX + HPF", "TX only"), if (txOnly) 1 else 0, h = 30) { Hl2.setTxOnly(it == 1) }
            Toggle("3 MHz HPF on receive", hpf, Modifier.fillMaxWidth(), enabled = txOnly, sub = "TX only mode") { Hl2.setHpf(it) }
            Label("CALIBRATION")
            Setting("FREQUENCY", ppm, -10f..10f, 0.1, { "%+.1f ppm".format(it) }) { Hl2.setPpm(it) }
            Setting("ADC GAIN (S-meter and spectrum)", adc.toDouble(), -20f..20f, 1.0, { "%+.0f dB".format(it) }) {
                Hl2.setAdcCal(it.toInt())
            }
            Box(
                Modifier.fillMaxWidth().padding(top = 4.dp).clip(RoundedCornerShape(10.dp))
                    .border(1.dp, Amb.copy(alpha = 0.5f), RoundedCornerShape(10.dp))
                    .clickable(onClick = openSetup).padding(vertical = 8.dp),
                contentAlignment = Alignment.Center,
            ) { Text("HL2 setup: reboot, network  ›", color = Amb, fontSize = 13.sp, fontWeight = FontWeight.Medium) }
            Text("Ethernet only · behind a warning", color = Dim, fontSize = 10.sp)
        }
    }
}

@Composable
private fun IoStatus(st: Status, io: IoInfo, force: Boolean) {
    val live = st.phase == Phase.RUNNING
    val (dot, text, sub) = when {
        !live -> Triple(Dim, "Start the radio to detect it", "")
        io.pico -> Triple(Grn, "Pico found", "IO board controller (your XPA125B firmware)")
        io.iob && force -> Triple(Amb, "Forced on", "detection skipped")
        io.iob -> Triple(Grn, "N2ADR IO board found", "")
        else -> Triple(Red, "No IO board found", "try Force IO board")
    }
    Row(
        Modifier.fillMaxWidth().clip(RoundedCornerShape(11.dp)).background(dot.copy(alpha = 0.08f))
            .border(1.dp, dot.copy(alpha = 0.35f), RoundedCornerShape(11.dp)).padding(horizontal = 10.dp, vertical = 7.dp),
        verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        Box(Modifier.size(8.dp).clip(CircleShape).background(dot))
        Column(Modifier.weight(1f)) {
            Text(text, color = Txt, fontSize = 12.sp)
            if (sub.isNotEmpty()) Text(sub, color = Dim, fontSize = 10.sp)
        }
        if (live && io.pico && io.lpf.isNotEmpty()) Text("LPF ${io.lpf}", color = Dim, fontSize = 10.5.sp)
    }
}

/** deskHPSDR's P1 Setup for the HL2 (picture F2): Ethernet only, every action asks first. */
@Composable
internal fun Hl2SetupSheet(st: Status, onStart: () -> Unit, onStop: () -> Unit) {
    val scope = rememberCoroutineScope()
    var busy by remember { mutableStateOf("") }
    var result by remember { mutableStateOf("") }
    var ask by remember { mutableStateOf<Pair<String, suspend () -> String>?>(null) }
    var mac by remember { mutableStateOf("") }
    val wired = st.network == "Ethernet"
    val gw = remember(st.phase) { ioInfo().gateware }
    SheetTitle("HL2 setup")
    Text(
        "These change the HL2 itself. Only over the USB Ethernet link, never WiFi. The radio stops while " +
            "they run. Network settings take effect after a reboot.",
        color = Color(0xFFFFB0B0), fontSize = 11.sp,
        modifier = Modifier.fillMaxWidth().clip(RoundedCornerShape(11.dp)).background(Color(0x1FFF4D4D))
            .border(1.dp, Color(0x73FF4D4D), RoundedCornerShape(11.dp)).padding(10.dp),
    )
    Text(
        if (wired) "● Ethernet  ${Radio.wiredIp.value}" + (if (gw > 0) " · gateware ${gw / 10}.${gw % 10}" else "")
        else "Connect the USB Ethernet adapter and start the radio once: setup is Ethernet only.",
        color = if (wired) Grn else Amb, fontSize = 12.sp,
    )
    // every action: ask, stop the radio, run it off the main thread, say what happened
    fun run(question: String, action: suspend () -> String) { ask = question to action }
    Row(horizontalArrangement = Arrangement.spacedBy(14.dp)) {
        Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(6.dp)) {
            Label("REBOOT")
            SmallButton("Reboot HL2, then find it again", Modifier.fillMaxWidth()) {
                if (wired && busy.isEmpty()) run("Reboot the HL2? The radio stops and starts again in about 15 s.") {
                    val e = Native.hl2Reboot()
                    if (e.isEmpty()) {
                        withContext(Dispatchers.Main) { busy = "HL2 rebooting… starting again in 15 s" }
                        delay(15_000)
                        withContext(Dispatchers.Main) { onStart() }
                        "Rebooted. Starting the radio again."
                    } else e
                }
            }
            Label("GATEWARE")
            Text("Updating the gateware from the phone comes as its own step: a failed flash needs a JTAG recovery, " +
                "so it gets tested on its own first.", color = Dim, fontSize = 10.5.sp)
        }
        Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(6.dp)) {
            Label("NETWORK (EEPROM, after a reboot)")
            SmallButton("Prefer DHCP", Modifier.fillMaxWidth()) {
                if (wired && busy.isEmpty()) run("Write \"prefer DHCP\" to the HL2's EEPROM (and switch a static IP off)?") {
                    Native.hl2Dhcp().ifEmpty { "DHCP preference saved. Reboot the HL2 to use it." }
                }
            }
            Label("ALTERNATE MAC (last two bytes, HH:HH)")
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp), verticalAlignment = Alignment.CenterVertically) {
                OutlinedTextField(mac, { mac = it.take(5).uppercase() }, Modifier.weight(1f), singleLine = true,
                    placeholder = { Text("13:DD", color = Dim) },
                    keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Ascii))
                SmallButton("Save", Modifier.padding(start = 2.dp)) {
                    val m = Regex("^([0-9A-F]{2}):([0-9A-F]{2})$").find(mac)
                    if (m == null) result = "Enter the last two MAC bytes as HH:HH (hexadecimal)."
                    else if (wired && busy.isEmpty()) {
                        val a = m.groupValues[1].toInt(16); val b = m.groupValues[2].toInt(16)
                        run("Write the alternate MAC …:$mac to the HL2's EEPROM? A DHCP reservation for the old MAC stops matching.") {
                            Native.hl2Mac(a, b).ifEmpty { "Alternate MAC saved. Reboot the HL2 to use it." }
                        }
                    }
                }
            }
            Label("STATIC IP")
            Text("Not offered: it switches DHCP off, so the HL2 can't be found on WiFi any more.", color = Dim, fontSize = 10.5.sp)
        }
    }
    if (busy.isNotEmpty()) Text(busy, color = Amb, fontSize = 12.sp)
    if (result.isNotEmpty()) Text(result, color = if (result.contains("saved") || result.startsWith("Rebooted")) Grn else Red, fontSize = 12.sp)
    ask?.let { (q, action) ->
        AlertDialog(
            onDismissRequest = { ask = null },
            title = { Text("HL2 setup") },
            text = { Text(q) },
            confirmButton = {
                TextButton({
                    ask = null
                    busy = "Working…"
                    result = ""
                    scope.launch {
                        if (Radio.running) { onStop(); delay(1500) }
                        val r = withContext(Dispatchers.IO) { action() }
                        busy = ""
                        result = r
                    }
                }) { Text("Yes, do it", color = Red) }
            },
            dismissButton = { TextButton({ ask = null }) { Text("Cancel") } },
        )
    }
}

/**
 * deskHPSDR's ANT menu for an HL2 with its IO board (picture H1): per band,
 * the main antenna or the board's separate RX input (any RX antenna but ANT1
 * switches the board's RF mode, old_protocol.c). Kept in the props file.
 */
@Composable
internal fun AntSheet() {
    SheetTitle("Ant")
    if (!Radio.running || Radio.playback.value) {
        Text("Start the radio first: the antennas are deskHPSDR's band settings.", color = Dim, fontSize = 12.sp)
        return
    }
    Text("Your IO board switches receive to its separate RX input on the bands set to \"RX in\". TX antenna comes with M7.",
        color = Dim, fontSize = 11.sp)
    val hf = remember { Radio.bands().filter { it.fmin in 1_800_000L..54_000_000L && it.title.endsWith("m") } }
    var ants by remember { mutableStateOf(hf.associate { it.index to Native.bandRxAnt(it.index) }) }
    hf.chunked(2).forEach { pair ->
        Row(horizontalArrangement = Arrangement.spacedBy(16.dp)) {
            pair.forEach { b ->
                Row(Modifier.weight(1f), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    Text(b.title, color = Txt, fontSize = 12.sp, modifier = Modifier.width(40.dp))
                    Box(Modifier.weight(1f)) {
                        Seg(listOf("ANT1", "RX in"), if ((ants[b.index] ?: 0) == 0) 0 else 1, h = 28) {
                            val a = if (it == 0) 0 else 3   // EXT1: any non-ANT1 choice is the IO board's RX input
                            Native.setBandRxAnt(b.index, a)
                            ants = ants + (b.index to a)
                        }
                    }
                }
            }
            if (pair.size == 1) Box(Modifier.weight(1f))
        }
    }
}
