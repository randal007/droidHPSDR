// droidHPSDR - GPL v3 or later.
package io.github.randal007.droidhpsdr

/** The one bridge into the native core (android/native/app/jni_bridge.c). */
object Native {
    init {
        System.loadLibrary("droidhpsdr")
    }

    /** WDSP's FFT wisdom; slow the first time. Returns 1 when it had to build it. */
    external fun wisdom(dir: String): Int
    external fun wisdomStatus(): String

    /** Discovery and stream start. Returns "" when streaming, else the reason. */
    external fun start(
        freq: Long, mode: Int, lnaDb: Double, codec: Boolean,
        sampleRate: Int, pixels: Int, fps: Int, ip: String?,
    ): String

    external fun stop()
    external fun setFrequency(hz: Long)
    external fun setMode(mode: Int)
    external fun setFilter(low: Int, high: Int)
    external fun setVolumeDb(db: Double)
    external fun setLnaDb(db: Double)
    /** Also play the receiver on the phone (deskHPSDR's local audio); the HL2+ speaker keeps playing. */
    external fun setPhoneAudio(on: Boolean)
    /** How old a spectrum row's data is, so rows drawn while tuning land at the right frequency. */
    external fun setRowDelayMs(ms: Int)

    /** [running, freq, mode, filterLow, filterHigh, sampleRate, meterDbm, lostPackets, adcOverloads, lna, volume] */
    external fun status(out: DoubleArray)

    /** The made-up 20 m band (demo.c), for the display without a radio. */
    external fun demoStart()
    external fun demoStop()
    external fun demoSetFrequency(hz: Long)

    // GL thread only
    external fun glInit()
    external fun glResize(w: Int, h: Int)
    external fun glDraw(
        lo: Double, hi: Double, split: Double, style: Int,
        floorDb: Float, rangeDb: Float, wfLo: Float, wfHi: Float,
    )
}
