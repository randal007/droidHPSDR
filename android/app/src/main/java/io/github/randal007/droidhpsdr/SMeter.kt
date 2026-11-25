// droidHPSDR - GPL v3 or later.
//
// S-units exactly as deskHPSDR's panadapter labels them (src/rx_panadapter.c
// lowlimitsHF/uplimitsHF, lowlimitsUKW/uplimitsUKW, dbm2smeter): S9 = -73 dBm
// up to 30 MHz, -93 dBm above, then +5 dB steps to +30 and +10 dB steps.
package io.github.randal007.droidhpsdr

object SMeter {
    private val lowHF = intArrayOf(-200, -121, -115, -109, -103, -97, -91, -85, -79, -73, -68, -63, -58, -53, -48, -43, -33, -23, -13)
    private val upHF = intArrayOf(-122, -116, -110, -104, -98, -92, -86, -80, -74, -69, -64, -59, -54, -49, -44, -34, -24, -14, 0)
    private val lowUKW = intArrayOf(-200, -141, -135, -129, -123, -117, -111, -105, -99, -93, -88, -83, -78, -73, -68, -63, -53, -43, -33)
    private val upUKW = intArrayOf(-142, -136, -130, -124, -118, -112, -106, -100, -94, -89, -84, -79, -74, -69, -64, -54, -44, -34, 0)
    private val names = listOf(
        "no signal", "S1", "S2", "S3", "S4", "S5", "S6", "S7", "S8", "S9", "S9+5", "S9+10", "S9+15",
        "S9+20", "S9+25", "S9+30", "S9+40", "S9+50", "S9+60", "out of range",
    )

    /** deskHPSDR's get_SWert(): the index into the table for this level. */
    fun index(freqHz: Long, dbm: Double): Int {
        val d = dbm.toInt()
        val (lo, up) = if (freqHz > 30_000_000L) lowUKW to upUKW else lowHF to upHF
        for (i in lo.indices) if (d >= lo[i] && d <= up[i]) return i
        return names.size - 1
    }

    fun label(freqHz: Long, dbm: Double): String = names[index(freqHz, dbm)]
}
