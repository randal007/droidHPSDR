// droidHPSDR - GPL v3 or later.
//
// The amateur band plans for the band bar, ported from the X6100 band bar
// (OpenX6100 src/band_plan.c, Copyright (c) 2026 Randal Warren VE7NHW,
// LGPL-2.1-or-later): the same simplified IARU tables and colours.
package io.github.randal007.droidhpsdr

import androidx.compose.ui.graphics.Color

enum class SegKind(val label: String, val color: Color) {
    CW("CW", Color(40, 110, 200)),
    DIGITAL("Digital", Color(40, 170, 90)),
    PHONE("SSB", Color(210, 150, 40)),
    CHANNEL("Channel", Color(255, 190, 60)),   // a 60 m channel (SDR Device → 60 m channel markers)
}

data class Segment(val loHz: Long, val hiHz: Long, val kind: SegKind)

object BandPlan {
    val OUT_OF_BAND = Color(255, 60, 60)

    private val C = SegKind.CW
    private val D = SegKind.DIGITAL
    private val P = SegKind.PHONE

    // kHz x 10, so 60 m's half kHz fit
    private fun t(vararg s: Triple<Int, Int, SegKind>) = s.map { Segment(it.first * 100L, it.second * 100L, it.third) }
    private fun s(lo: Int, hi: Int, k: SegKind) = Triple(lo, hi, k)

    /** Simplified from the IARU Region 1 HF band plan and its 6 m plan. */
    private val region1 = t(
        s(18100, 18380, C), s(18380, 18430, D), s(18430, 20000, P),
        s(35000, 35700, C), s(35700, 36000, D), s(36000, 38000, P),
        s(53515, 53540, C), s(53540, 53660, P), s(53660, 53665, D),
        s(70000, 70400, C), s(70400, 70500, D), s(70500, 72000, P),
        s(101000, 101300, C), s(101300, 101500, D),
        s(140000, 140700, C), s(140700, 141120, D), s(141120, 143500, P),
        s(180680, 180950, C), s(180950, 181110, D), s(181110, 181680, P),
        s(210000, 210700, C), s(210700, 211510, D), s(211510, 214500, P),
        s(248900, 249150, C), s(249150, 249310, D), s(249310, 249900, P),
        s(280000, 280700, C), s(280700, 282250, D), s(282250, 297000, P),
        s(500000, 501000, C), s(501000, 503000, P), s(503000, 504000, D), s(504000, 520000, P),
    )

    /** Simplified from the IARU Region 2 band plan. */
    private val region2 = t(
        s(18000, 18100, D), s(18100, 18400, C), s(18400, 18500, D), s(18500, 20000, P),
        s(35000, 35700, C), s(35700, 36000, D), s(36000, 40000, P),
        s(53515, 53540, C), s(53540, 53660, P), s(53660, 53665, D),
        s(70000, 70400, C), s(70400, 70600, D), s(70600, 73000, P),
        s(101000, 101300, C), s(101300, 101500, D),
        s(140000, 140700, C), s(140700, 141120, D), s(141120, 143500, P),
        s(180680, 180950, C), s(180950, 181110, D), s(181110, 181680, P),
        s(210000, 210700, C), s(210700, 211500, D), s(211500, 214500, P),
        s(248900, 249150, C), s(249150, 249310, D), s(249310, 249900, P),
        s(280000, 280700, C), s(280700, 283000, D), s(283000, 297000, P),
        s(500000, 501000, C), s(501000, 503000, P), s(503000, 504000, D), s(504000, 540000, P),
    )

    /** Simplified from the IARU Region 3 band plan. */
    private val region3 = t(
        s(18000, 18300, C), s(18300, 18400, D), s(18400, 20000, P),
        s(35000, 35700, C), s(35700, 36000, D), s(36000, 39000, P),
        s(53515, 53540, C), s(53540, 53660, P), s(53660, 53665, D),
        s(70000, 70300, C), s(70300, 70400, D), s(70400, 72000, P),
        s(101000, 101300, C), s(101300, 101500, D),
        s(140000, 140700, C), s(140700, 141120, D), s(141120, 143500, P),
        s(180680, 180950, C), s(180950, 181110, D), s(181110, 181680, P),
        s(210000, 210700, C), s(210700, 211510, D), s(211510, 214500, P),
        s(248900, 249150, C), s(249150, 249310, D), s(249310, 249900, P),
        s(280000, 280700, C), s(280700, 282250, D), s(282250, 297000, P),
        s(500000, 501000, C), s(501000, 503000, P), s(503000, 504000, D), s(504000, 540000, P),
    )

    data class Band(val loHz: Long, val hiHz: Long, val name: String, val home: Long)

    /**
     * The channel set chosen in SDR Device, while it describes what the radio is on (the radio
     * running, not the demo): stale otherwise.
     */
    private fun liveChannels(): List<Pair<Long, Long>> =
        if (Radio.running && !Radio.playback.value) Hl2.channels60.value else emptyList()

    /**
     * The bands, with a frequency to jump to. 60 m: with a channel set, one band per channel (so the
     * bar stays red between them and the BAND chip says 60m only on a channel); else WRC-15's edges.
     */
    val bands: List<Band>
        get() {
            val ch = liveChannels()
            if (ch.isEmpty()) return fixedBands
            val sixty = ch.map { (f, w) -> Band(f - w / 2, f + w / 2, "60m", f) }
            return fixedBands.filter { it.name != "60m" } + sixty
        }

    /** The bands by their widest edges in any region, with a frequency to jump to. */
    private val fixedBands = listOf(
        Band(1_800_000, 2_000_000, "160m", 1_840_000),
        Band(3_500_000, 4_000_000, "80m", 3_573_000),
        Band(5_351_500, 5_366_500, "60m", 5_357_000),   // WRC-15; a chosen channel set replaces it (bands)
        Band(7_000_000, 7_300_000, "40m", 7_074_000),
        Band(10_100_000, 10_150_000, "30m", 10_136_000),
        Band(14_000_000, 14_350_000, "20m", 14_074_000),
        Band(18_068_000, 18_168_000, "17m", 18_100_000),
        Band(21_000_000, 21_450_000, "15m", 21_074_000),
        Band(24_890_000, 24_990_000, "12m", 24_915_000),
        Band(28_000_000, 29_700_000, "10m", 28_074_000),
        Band(50_000_000, 54_000_000, "6m", 50_313_000),
    )

    fun band(hz: Long): Band? = bands.firstOrNull { hz >= it.loHz && hz < it.hiHz }

    fun segments(region: Int, loHz: Long, hiHz: Long): List<Segment> {
        val table = when (region) { 2 -> region2; 3 -> region3; else -> region1 }
        // 60 m: with a channel set chosen (SDR Device), its channels replace the WRC-15 segment, so
        // Canada's or the US's channels show as allowed and the gaps between them as out of band
        val ch = liveChannels()
        val plan = if (ch.isEmpty()) table
        else table.filter { it.hiHz <= 5_250_000L || it.loHz >= 5_450_000L } +
            ch.map { (f, w) -> Segment(f - w / 2, f + w / 2, SegKind.CHANNEL) }
        return plan.filter { it.hiHz > loHz && it.loHz < hiHz }
    }

    fun segmentAt(region: Int, hz: Long): Segment? = segments(region, hz, hz + 1).firstOrNull()

    /** What the tag says for a dial frequency: X6100's band_bar.c tag_show(). */
    data class Tag(val text: String, val color: Color)

    fun tag(region: Int, hz: Long): Tag {
        val band = band(hz)
        val seg = segmentAt(region, hz)
        return when {
            band != null && seg != null -> Tag("${band.name}  ${seg.kind.label}", seg.kind.color)
            band != null -> Tag("Out of the band plan", OUT_OF_BAND)
            else -> Tag("Out of band", OUT_OF_BAND)
        }
    }

    /** Changes when the tag should show again: a new band or a new segment kind. */
    fun tagKey(region: Int, hz: Long): String = "${band(hz)?.name}/${segmentAt(region, hz)?.kind}"
}
