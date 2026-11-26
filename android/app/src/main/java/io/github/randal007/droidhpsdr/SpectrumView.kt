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
        setRenderer(Renderer(ctx.applicationContext))
        renderMode = RENDERMODE_CONTINUOUSLY
    }

    private class Renderer(private val ctx: Context) : GLSurfaceView.Renderer {
        private var floor = Double.NaN
        private var bgLoaded = -1   // which picture the texture holds (-1: none uploaded in this context)
        private var bgTex = 0
        // decoded on a background thread; the GL thread only uploads it
        @Volatile private var pending: Pair<Int, android.graphics.Bitmap?>? = null
        @Volatile private var decoding = -1

        override fun onSurfaceCreated(gl: GL10?, config: EGLConfig?) {
            Native.glInit()
            bgLoaded = -1; bgTex = 0   // a new GL context: the old texture is gone
        }

        /**
         * The picture chosen in Display → Look, as a GL texture: only while the 2D view shows (3D
         * never draws it, so no texture then). Decoded on a background thread, uploaded here.
         */
        private fun syncBackground() {
            val want = if (Radio.flat.value) Disp.bgImage.value else 0
            if (want == bgLoaded) return
            val file = backgroundFiles(ctx).getOrNull(want - 1)
            var bmp: android.graphics.Bitmap? = null
            if (file != null) {
                val ready = pending
                if (ready == null || ready.first != want) {
                    if (decoding != want) {
                        decoding = want
                        Thread {
                            val b = runCatching {
                                ctx.assets.open("backgrounds/$file").use { android.graphics.BitmapFactory.decodeStream(it) }
                            }.getOrNull()
                            pending?.second?.recycle()   // an older one nobody took
                            pending = want to b
                        }.start()
                    }
                    return   // the old picture stays until the new one is decoded
                }
                bmp = ready.second
                pending = null
                decoding = -1
            }
            if (bgTex != 0) { android.opengl.GLES30.glDeleteTextures(1, intArrayOf(bgTex), 0); bgTex = 0 }
            var w = 0; var h = 0
            if (bmp != null) {
                val t = IntArray(1)
                android.opengl.GLES30.glGenTextures(1, t, 0)
                bgTex = t[0]
                android.opengl.GLES30.glBindTexture(android.opengl.GLES30.GL_TEXTURE_2D, bgTex)
                for ((p, v) in listOf(
                    android.opengl.GLES30.GL_TEXTURE_MIN_FILTER to android.opengl.GLES30.GL_LINEAR,
                    android.opengl.GLES30.GL_TEXTURE_MAG_FILTER to android.opengl.GLES30.GL_LINEAR,
                    android.opengl.GLES30.GL_TEXTURE_WRAP_S to android.opengl.GLES30.GL_CLAMP_TO_EDGE,
                    android.opengl.GLES30.GL_TEXTURE_WRAP_T to android.opengl.GLES30.GL_CLAMP_TO_EDGE,
                )) android.opengl.GLES30.glTexParameteri(android.opengl.GLES30.GL_TEXTURE_2D, p, v)
                android.opengl.GLUtils.texImage2D(android.opengl.GLES30.GL_TEXTURE_2D, 0, bmp, 0)
                w = bmp.width; h = bmp.height
                bmp.recycle()
            }
            Native.glSetBackground(bgTex, w, h)
            bgLoaded = want
        }

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
            syncBackground()
            val bgAlpha = if (Radio.flat.value && Disp.bgImage.value > 0) Disp.bgStrength.value / 100f else 0f
            Native.glDraw(
                f - half, f + half, Radio.split.value.toDouble(), style,
                floor3.toFloat(), range3.toFloat(), wfLo.toFloat(), wfHi.toFloat(),
                Radio.flat.value, panLo.toFloat(), panHi.toFloat(),
                Radio.wfTheme.value, Radio.depthRows.value,
                pk != Disp.PEAK_OFF, if (pk == Disp.PEAK_HOLD) 1e9f else Disp.holdS.value.toFloat(),
                if (pk == Disp.PEAK_HOLD) 0f else Disp.dropDbS.value.toFloat(), Radio.avgRows(),
                Disp.avgMode.value, Disp.panStep.value.toFloat(), Disp.preserve.value, Disp.peakArgb(), bgAlpha,
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
