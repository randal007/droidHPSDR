// droidHPSDR - GPL v3 or later.
package io.github.randal007.droidhpsdr

import android.content.Context
import android.opengl.GLSurfaceView
import javax.microedition.khronos.egl.EGLConfig
import javax.microedition.khronos.opengles.GL10

/**
 * The GPU surface: 3D spectrum on top, waterfall below, drawn by
 * android/native/app/gl_render.c. Compose draws the text, the band bar and
 * the VFO/passband marks over it (Screen.kt).
 *
 * Drawing never drives the radio: when this surface stops (screen off, app
 * in the background) the stream and the DSP keep running in RadioService.
 */
class SpectrumView(ctx: Context) : GLSurfaceView(ctx) {
    init {
        setEGLContextClientVersion(3)
        preserveEGLContextOnPause = true
        setRenderer(Renderer())
        renderMode = RENDERMODE_CONTINUOUSLY
    }

    private class Renderer : GLSurfaceView.Renderer {
        private var floor = Double.NaN

        override fun onSurfaceCreated(gl: GL10?, config: EGLConfig?) = Native.glInit()
        override fun onSurfaceChanged(gl: GL10?, w: Int, h: Int) = Native.glResize(w, h)

        override fun onDrawFrame(gl: GL10?) {
            val s = Radio.status.value
            // auto levels: follow the noise floor slowly, like deskHPSDR's autoscale
            if (s.phase == Phase.RUNNING || s.phase == Phase.DEMO) {
                floor = if (floor.isNaN()) s.noiseFloor else floor + (s.noiseFloor - floor) * 0.02
            }
            val nf = if (floor.isNaN()) -125.0 else floor
            Radio.displayFloor = nf
            val style = Radio.style.value
            val f = Radio.freq.value.toDouble()
            val half = Radio.span.value / 2
            // levels: automatic (above) or deskHPSDR's manual high / low (Display → Levels)
            val (floor3, range3) = Disp.levels3d(style, nf)
            val (wfLo, wfHi) = Disp.wfLevels(nf)
            val (panLo, panHi) = Disp.panLevels(nf)
            // peaks & hold: Hold never drops, Decay holds then falls (Display → Averaging)
            val pk = Disp.peakMode.value
            Native.glDraw(
                f - half, f + half, Radio.split.value.toDouble(), style,
                floor3.toFloat(), range3.toFloat(), wfLo.toFloat(), wfHi.toFloat(),
                Radio.flat.value, panLo.toFloat(), panHi.toFloat(),
                Radio.wfTheme.value, Radio.depthRows.value,
                pk != Disp.PEAK_OFF, if (pk == Disp.PEAK_HOLD) 1e9f else Disp.holdS.value.toFloat(),
                if (pk == Disp.PEAK_HOLD) 0f else Disp.dropDbS.value.toFloat(), Radio.avgRows(),
                Disp.avgMode.value, Disp.panStep.value.toFloat(), Disp.preserve.value, Disp.peakArgb(),
            )
        }
    }

    companion object {
        // automatic 3D levels, relative to the noise floor (like deskHPSDR's "Related to Noisefloor");
        // the 2D ones are in Disp.panLevels
        const val RANGE_3D = 52f
        fun floor3d(style: Int, nf: Double) = nf + if (style == 0) 1.5 else -2.5

        /** Height of a ridge on the front 3D row, as a fraction of the 3D view (gl_render.c's formula). */
        fun ridge3d(style: Int, s: Double): Double =
            if (style == 0) s * 0.62 else Math.pow(s, 0.6) * 0.46

        val WF_THEMES = listOf("X6100", "AetherSDR", "deskHPSDR", "Ember", "Ice", "Grey")
        val STYLES = listOf("X6100 look 3", "AetherSDR", "Lit terrain", "Neon wire")
    }
}
