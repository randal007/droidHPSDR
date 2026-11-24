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
            if (s.phase == Phase.RUNNING) {
                floor = if (floor.isNaN()) s.noiseFloor else floor + (s.noiseFloor - floor) * 0.02
            }
            val nf = if (floor.isNaN()) -125.0 else floor
            val style = Radio.style.value
            val f = Radio.freq.value.toDouble()
            val half = Radio.span.value / 2
            Native.glDraw(
                f - half, f + half, SPLIT.toDouble(), style,
                (nf + if (style == 0) 1.5 else -2.5).toFloat(), 52f,
                (nf - 2.5).toFloat(), (nf + 48.0).toFloat(),
            )
        }
    }

    companion object {
        /** Fraction of the height for the 3D view; the waterfall takes the rest. */
        const val SPLIT = 0.5f
    }
}
