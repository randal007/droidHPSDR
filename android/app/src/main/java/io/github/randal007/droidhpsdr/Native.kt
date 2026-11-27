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
        sampleRate: Int, pixels: Int, fps: Int, ip: String?, propsDir: String,
    ): String

    external fun stop()
    external fun setFrequency(hz: Long)
    external fun setMode(mode: Int)
    external fun setFilter(low: Int, high: Int)
    external fun setVolumeDb(db: Double)
    external fun setLnaDb(db: Double)
    /** Also play the receiver on the phone (deskHPSDR's local audio); the HL2+ speaker keeps playing. */
    external fun setPhoneAudio(on: Boolean)
    /** Upstream's spectrum zoom, 1..16 (more detail when pinched in). */
    external fun setZoom(zoom: Int)
    /** Rows per second, FFT window (Screen menu order), detector (0 Peak, 1 Average, 2 Rosenfell, 3 Sample),
     *  FFT size (Screen menu: 0 Auto, 16384 .. 262144). */
    external fun setDisplay(fps: Int, window: Int, detector: Int, fftSize: Int)
    /** How old a spectrum row's data is, so rows drawn while tuning land at the right frequency. */
    external fun setRowDelayMs(ms: Int)

    /** See jni_bridge.c FN(status) for the order; STATUS_N values. */
    external fun status(out: DoubleArray)

    // M4: deskHPSDR's own vfo.c / store.c calls (radio_core.c)
    external fun filterPreset(index: Int)       // 0..9 presets, 10 Var1, 11 Var2
    external fun band(band: Int)                // same band again = next band-stack entry
    external fun bandstack(entry: Int)
    external fun vfoOp(op: Int)                 // 0 A>B, 1 B>A, 2 A<>B
    external fun rit(on: Boolean, hz: Long)
    external fun ctun(on: Boolean)
    external fun lock(on: Boolean)
    external fun step(hz: Int)
    external fun agc(agc: Int)                  // 0 Off, 1 Long, 2 Slow, 3 Medium, 4 Fast
    external fun memory(store: Boolean, slot: Int)
    external fun save()                         // write deskHPSDR's props file

    // M5: receive DSP fields, numbered as in dsp.c (Dsp in Radio.kt)
    external fun dspSet(field: Int, value: Double)
    external fun dspGet(out: DoubleArray): Int

    // M7: transmit fields, numbered as in droid_app.h TX_* / TXS_* (Tx in Transmit.kt)
    external fun txSet(field: Int, value: Double)
    external fun txGet(out: DoubleArray): Int
    /** The phone mic's AAudio device id (0 = default) and whether it is a Bluetooth headset. */
    external fun micRoute(device: Int, bluetooth: Boolean)
    /** M7 step 4: PureSignal's AmpView data (puresignal.c, Ps.D_* offsets); returns the floats written, 0 if none. */
    external fun psDisp(out: FloatArray): Int

    // deskHPSDR's tables, one entry per line, fields split by '|'
    external fun filters(mode: Int): String     // title|low|high
    external fun bands(): String                // title|fmin|fmax (index = band number)
    external fun bandstackEntries(band: Int): String   // first line current; freq|mode|filter
    external fun memories(): String             // freq|mode

    /** The made-up 20 m band (demo.c), for the display without a radio. */
    external fun demoStart()
    external fun demoStop()
    external fun demoSetFrequency(hz: Long)
    external fun demoSetFps(fps: Int)
    /** The demo band from an IQ recording (playback.c): "" when playing, else why not. */
    external fun startPlayback(path: String, centreHz: Long, mode: Int, pixels: Int, fps: Int, propsDir: String): String

    // M6: the HL2's settings (field: droid_app.h HL2_*), IO board status "iob|pico|lpf|gateware",
    // and HL2 setup over UDP 1025 (blocking: call off the main thread; "" = done, else the error)
    external fun hl2Set(field: Int, value: Double)
    /** ANT menu: a band's RX antenna (0 ANT1 .. 3 EXT1 ..), deskHPSDR's band numbers. */
    external fun bandRxAnt(band: Int): Int
    external fun setBandRxAnt(band: Int, ant: Int)
    external fun ritStep(hz: Int)
    /** The 60 m channels of the chosen region: "freq:width;..." ("" for none). */
    external fun channels60m(): String
    external fun hl2Info(): String
    external fun hl2Reboot(): String
    external fun hl2Dhcp(): String
    external fun hl2Mac(a: Int, b: Int): String

    // GL thread only
    external fun glInit()
    external fun glResize(w: Int, h: Int)
    external fun glDraw(
        lo: Double, hi: Double, split: Double, style: Int,
        floorDb: Float, rangeDb: Float, wfLo: Float, wfHi: Float,
        flat: Boolean, panLo: Float, panHi: Float, wfTheme: Int, depthRows: Int,
        peakHold: Boolean, holdS: Float, dropDbS: Float, avgRows: Int,
        avgMode: Int, gridDb: Float, preserve: Boolean, peakArgb: Int, bgAlpha: Float,
    )
    /** The 2D view's background picture: a texture made in the GL context (0 = none) and its size. */
    external fun glSetBackground(tex: Int, w: Int, h: Int)

    /** Peak labels the way deskHPSDR finds them: out = [f0, dB0, f1, dB1, ...]; returns how many. */
    external fun peaks(
        viewLo: Double, viewHi: Double, pbLo: Double, pbHi: Double, num: Int, divider: Int,
        hideNoise: Boolean, percentile: Double, out: DoubleArray,
    ): Int
}
