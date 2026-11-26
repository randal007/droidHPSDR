/*
 * jni_bridge.c — the one bridge between the Kotlin app and the native core
 * (CLAUDE.md decision 3). Kotlin side: io.github.randal007.droidhpsdr.Native.
 *
 * GPL v3 or later, like the rest of droidHPSDR.
 */
#include <jni.h>
#include <pthread.h>
#include <stdint.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <android/log.h>

#include "droid_app.h"

void droid_log(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  __android_log_vprint(ANDROID_LOG_INFO, "droidHPSDR", fmt, ap);
  va_end(ap);
}

/* Upstream prints its messages (t_print) to stdout/stderr, which an app
 * throws away; pipe both into logcat under the tag "deskHPSDR". */
static void *log_pump(void *arg) {
  int fd = (int)(intptr_t)arg;
  char buf[1024];
  size_t used = 0;
  for (;;) {
    ssize_t n = read(fd, buf + used, sizeof buf - 1 - used);
    if (n <= 0) break;
    used += (size_t)n;
    char *start = buf, *nl;
    while ((nl = memchr(start, '\n', used - (size_t)(start - buf))) != NULL) {
      *nl = 0;
      __android_log_write(ANDROID_LOG_INFO, "deskHPSDR", start);
      start = nl + 1;
    }
    used -= (size_t)(start - buf);
    memmove(buf, start, used);
    if (used == sizeof buf - 1) {
      buf[used] = 0;
      __android_log_write(ANDROID_LOG_INFO, "deskHPSDR", buf);
      used = 0;
    }
  }
  return NULL;
}

JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved) {
  (void)vm; (void)reserved;
  int p[2];
  if (pipe(p) == 0) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    dup2(p[1], 1);
    dup2(p[1], 2);
    pthread_t t;
    pthread_create(&t, NULL, log_pump, (void *)(intptr_t)p[0]);
    pthread_detach(t);
  }
  return JNI_VERSION_1_6;
}

#define FN(name) Java_io_github_randal007_droidhpsdr_Native_##name

JNIEXPORT jint JNICALL FN(wisdom)(JNIEnv *env, jobject self, jstring dir) {
  (void)self;
  const char *d = (*env)->GetStringUTFChars(env, dir, NULL);
  char path[512];
  snprintf(path, sizeof path, "%s/", d);   /* WDSP appends the file name directly */
  (*env)->ReleaseStringUTFChars(env, dir, d);
  return droid_wisdom(path);
}

JNIEXPORT jstring JNICALL FN(wisdomStatus)(JNIEnv *env, jobject self) {
  (void)self;
  const char *s = droid_wisdom_status();
  return (*env)->NewStringUTF(env, s ? s : "");
}

/* Returns "" when streaming, else why not. */
JNIEXPORT jstring JNICALL FN(start)(JNIEnv *env, jobject self, jlong freq, jint mode, jdouble lna, jboolean codec,
                                    jint sample_rate, jint pixels, jint fps, jstring ip, jstring props_dir) {
  (void)self;
  droid_start_opts o = { .freq = freq, .mode = mode, .lna_db = lna, .codec = codec, .sample_rate = sample_rate,
                         .pixels = pixels, .fps = fps };
  if (props_dir) {
    const char *s = (*env)->GetStringUTFChars(env, props_dir, NULL);
    snprintf(o.props_dir, sizeof o.props_dir, "%s", s);
    (*env)->ReleaseStringUTFChars(env, props_dir, s);
  }
  if (ip) {
    const char *s = (*env)->GetStringUTFChars(env, ip, NULL);
    snprintf(o.radio_ip, sizeof o.radio_ip, "%s", s);
    (*env)->ReleaseStringUTFChars(env, ip, s);
  }
  char msg[256] = "";
  int rc = droid_radio_start(&o, msg, sizeof msg);
  droid_log("start: %d %s", rc, msg);
  return (*env)->NewStringUTF(env, rc == 0 ? "" : msg);
}

/* the demo band: an IQ recording in place of the radio (playback.c); freq = the recording's centre */
JNIEXPORT jstring JNICALL FN(startPlayback)(JNIEnv *env, jobject self, jstring path, jlong freq, jint mode,
                                            jint pixels, jint fps, jstring props_dir) {
  (void)self;
  droid_start_opts o = { .freq = freq, .mode = mode, .lna_db = 0, .codec = 0, .sample_rate = 192000,
                         .pixels = pixels, .fps = fps };
  const char *s = (*env)->GetStringUTFChars(env, path, NULL);
  snprintf(o.playback, sizeof o.playback, "%s", s);
  (*env)->ReleaseStringUTFChars(env, path, s);
  s = (*env)->GetStringUTFChars(env, props_dir, NULL);
  snprintf(o.props_dir, sizeof o.props_dir, "%s", s);
  (*env)->ReleaseStringUTFChars(env, props_dir, s);
  char msg[256] = "";
  int rc = droid_radio_start(&o, msg, sizeof msg);
  droid_log("playback: %d %s", rc, msg);
  return (*env)->NewStringUTF(env, rc == 0 ? "" : msg);
}

JNIEXPORT void JNICALL FN(stop)(JNIEnv *env, jobject self) { (void)env; (void)self; droid_radio_stop(); }

JNIEXPORT void JNICALL FN(setFrequency)(JNIEnv *env, jobject self, jlong hz) {
  (void)env; (void)self;
  droid_set_frequency(hz);
}
JNIEXPORT void JNICALL FN(setMode)(JNIEnv *env, jobject self, jint mode) {
  (void)env; (void)self;
  droid_set_mode(mode);
}
JNIEXPORT void JNICALL FN(setFilter)(JNIEnv *env, jobject self, jint low, jint high) {
  (void)env; (void)self;
  droid_set_filter(low, high);
}
JNIEXPORT void JNICALL FN(setVolumeDb)(JNIEnv *env, jobject self, jdouble db) {
  (void)env; (void)self;
  droid_set_volume_db(db);
}
JNIEXPORT void JNICALL FN(setRowDelayMs)(JNIEnv *env, jobject self, jint ms) {
  (void)env; (void)self;
  droid_row_delay_us = ms * 1000;
}
JNIEXPORT void JNICALL FN(setZoom)(JNIEnv *env, jobject self, jint zoom) {
  (void)env; (void)self;
  droid_set_zoom(zoom);
}
JNIEXPORT void JNICALL FN(setDisplay)(JNIEnv *env, jobject self, jint fps, jint window, jint detector,
                                      jint fft_size) {
  (void)env; (void)self;
  droid_set_fps(fps);
  droid_set_window(window);
  droid_set_detector(detector);
  droid_set_pan_fft(fft_size);
}
JNIEXPORT void JNICALL FN(setPhoneAudio)(JNIEnv *env, jobject self, jboolean on) {
  (void)env; (void)self;
  droid_set_phone_audio(on);
}
JNIEXPORT void JNICALL FN(setLnaDb)(JNIEnv *env, jobject self, jdouble db) {
  (void)env; (void)self;
  droid_set_lna_db(db);
}

/* [running, freq, mode, filterLow, filterHigh, sampleRate, meterDbm, lostPackets, adcOverloads, lna, volume,
 *  noiseFloor, phoneAudio, freqB, modeB, ritOn, rit, ctun, locked, split, step, band, bandstack, filter, agc,
 *  rows] */
#define STATUS_N 27
JNIEXPORT void JNICALL FN(status)(JNIEnv *env, jobject self, jdoubleArray out) {
  (void)self;
  droid_status s;
  droid_get_status(&s);
  jdouble v[STATUS_N] = { s.running, (double)s.freq, s.mode, s.filter_low, s.filter_high, s.sample_rate,
                          s.meter_dbm, s.lost_packets, s.adc_overloads, s.lna_db, s.volume_db, s.noise_floor,
                          s.phone_audio, (double)s.freq_b, s.mode_b, s.rit_on, (double)s.rit, s.ctun, s.locked,
                          s.split, s.step, s.band, s.bandstack, s.filter, s.agc, (double)s.rows, s.rit_step };
  (*env)->SetDoubleArrayRegion(env, out, 0, STATUS_N, v);
}

/* ---- M4: deskHPSDR's VFO, band stack and memory calls ---- */
#define CALL1(name, fn, T) \
  JNIEXPORT void JNICALL FN(name)(JNIEnv *env, jobject self, T a) { (void)env; (void)self; fn(a); }
CALL1(filterPreset, droid_filter_preset, jint)
CALL1(band, droid_band, jint)
CALL1(bandstack, droid_bandstack, jint)
CALL1(vfoOp, droid_vfo_op, jint)
CALL1(ctun, droid_ctun, jboolean)
CALL1(lock, droid_lock, jboolean)
CALL1(step, droid_step, jint)
CALL1(agc, droid_agc, jint)
JNIEXPORT void JNICALL FN(rit)(JNIEnv *env, jobject self, jboolean on, jlong hz) {
  (void)env; (void)self;
  droid_rit(on, hz);
}
JNIEXPORT void JNICALL FN(memory)(JNIEnv *env, jobject self, jboolean store, jint slot) {
  (void)env; (void)self;
  droid_memory(store, slot);
}
JNIEXPORT void JNICALL FN(save)(JNIEnv *env, jobject self) { (void)env; (void)self; droid_save(); }

/* M6: ANT menu, RIT step, 60 m channels */
JNIEXPORT jint JNICALL FN(bandRxAnt)(JNIEnv *env, jobject self, jint band) { (void)env; (void)self; return droid_band_rx_ant(band); }
JNIEXPORT void JNICALL FN(setBandRxAnt)(JNIEnv *env, jobject self, jint band, jint ant) {
  (void)env; (void)self;
  droid_set_band_rx_ant(band, ant);
}
JNIEXPORT void JNICALL FN(ritStep)(JNIEnv *env, jobject self, jint hz) { (void)env; (void)self; droid_set_rit_step(hz); }
JNIEXPORT jstring JNICALL FN(channels60m)(JNIEnv *env, jobject self) {   /* "freq:width;..." */
  (void)self;
  long long f[16], w[16];
  int n = droid_channels_60m(f, w, 16);
  char out[512] = "";
  for (int i = 0; i < n; i++) {
    size_t l = strlen(out);
    snprintf(out + l, sizeof out - l, "%s%lld:%lld", i ? ";" : "", f[i], w[i]);
  }
  return (*env)->NewStringUTF(env, out);
}

/* M6: HL2 settings and setup */
JNIEXPORT void JNICALL FN(hl2Set)(JNIEnv *env, jobject self, jint field, jdouble value) {
  (void)env; (void)self;
  droid_hl2_set(field, value);
}
JNIEXPORT jstring JNICALL FN(hl2Info)(JNIEnv *env, jobject self) {   /* iob|pico|lpf|gateware */
  (void)self;
  int iob, pico, gw;
  char lpf[32], out[64];
  droid_hl2_info(&iob, &pico, lpf, sizeof lpf, &gw);
  snprintf(out, sizeof out, "%d|%d|%s|%d", iob, pico, lpf, gw);
  return (*env)->NewStringUTF(env, out);
}
/* the setup actions block up to a few seconds (UDP with retries): call them off the main thread.
 * They return "" when done, or the error text; hl2Flags returns the flags byte as text, or "!error". */
JNIEXPORT jstring JNICALL FN(hl2Reboot)(JNIEnv *env, jobject self) {
  (void)self;
  char e[256];
  droid_hl2_reboot(e, sizeof e);
  return (*env)->NewStringUTF(env, e);
}
JNIEXPORT jstring JNICALL FN(hl2Dhcp)(JNIEnv *env, jobject self) {
  (void)self;
  char e[256];
  droid_hl2_set_dhcp(e, sizeof e);
  return (*env)->NewStringUTF(env, e);
}
JNIEXPORT jstring JNICALL FN(hl2Mac)(JNIEnv *env, jobject self, jint a, jint b) {
  (void)self;
  char e[256];
  droid_hl2_set_mac(a, b, e, sizeof e);
  return (*env)->NewStringUTF(env, e);
}
JNIEXPORT jstring JNICALL FN(hl2Flags)(JNIEnv *env, jobject self) {
  (void)self;
  char e[256], out[260];
  int f;
  if (droid_hl2_read_flags(&f, e, sizeof e) == 0) snprintf(out, sizeof out, "%d", f);
  else snprintf(out, sizeof out, "!%s", e);
  return (*env)->NewStringUTF(env, out);
}

/* M5: receive DSP fields (dsp.c) */
JNIEXPORT void JNICALL FN(dspSet)(JNIEnv *env, jobject self, jint field, jdouble value) {
  (void)env; (void)self;
  droid_dsp_set(field, value);
}
JNIEXPORT jint JNICALL FN(dspGet)(JNIEnv *env, jobject self, jdoubleArray out) {
  (void)self;
  double v[128];
  int n = droid_dsp_count();
  if (n > 128) n = 128;
  droid_dsp_get(v);
  jsize len = (*env)->GetArrayLength(env, out);
  if (len < n) n = len;
  (*env)->SetDoubleArrayRegion(env, out, 0, n, v);
  return n;
}

/* Lists as text, one entry per line, fields split by '|'. */
static jstring text(JNIEnv *env, const char *s) { return (*env)->NewStringUTF(env, s); }

JNIEXPORT jstring JNICALL FN(filters)(JNIEnv *env, jobject self, jint mode) {
  (void)self;
  char t[16][16], out[1024] = "";
  int lo[16], hi[16];
  int n = droid_filters(mode, t, lo, hi, 16);
  for (int i = 0; i < n; i++) {
    char line[64];
    snprintf(line, sizeof line, "%s|%d|%d\n", t[i], lo[i], hi[i]);
    strncat(out, line, sizeof out - strlen(out) - 1);
  }
  return text(env, out);
}

JNIEXPORT jstring JNICALL FN(bands)(JNIEnv *env, jobject self) {
  (void)self;
  char t[64][16], out[4096] = "";
  long long a[64], b[64];
  int n = droid_bands(t, a, b, 64);
  for (int i = 0; i < n; i++) {
    char line[96];
    snprintf(line, sizeof line, "%s|%lld|%lld\n", t[i], a[i], b[i]);
    strncat(out, line, sizeof out - strlen(out) - 1);
  }
  return text(env, out);
}

/* first line: current entry; then freq|mode|filter */
JNIEXPORT jstring JNICALL FN(bandstackEntries)(JNIEnv *env, jobject self, jint band) {
  (void)self;
  long long f[16];
  int m[16], fl[16], cur = 0;
  int n = droid_bandstack_entries(band, f, m, fl, 16, &cur);
  char out[1024];
  snprintf(out, sizeof out, "%d\n", cur);
  for (int i = 0; i < n; i++) {
    char line[64];
    snprintf(line, sizeof line, "%lld|%d|%d\n", f[i], m[i], fl[i]);
    strncat(out, line, sizeof out - strlen(out) - 1);
  }
  return text(env, out);
}

JNIEXPORT jstring JNICALL FN(memories)(JNIEnv *env, jobject self) {
  (void)self;
  long long f[16];
  int m[16];
  int n = droid_memories(f, m, 16);
  char out[1024] = "";
  for (int i = 0; i < n; i++) {
    char line[64];
    snprintf(line, sizeof line, "%lld|%d\n", f[i], m[i]);
    strncat(out, line, sizeof out - strlen(out) - 1);
  }
  return text(env, out);
}

/* out = [f0, dB0, f1, dB1, ...]; returns how many peaks */
JNIEXPORT jint JNICALL FN(peaks)(JNIEnv *env, jobject self, jdouble view_lo, jdouble view_hi, jdouble pb_lo,
                                 jdouble pb_hi, jint num, jint divider, jboolean hide_noise, jdouble percentile,
                                 jdoubleArray out) {
  (void)self;
  double f[32];
  float d[32];
  int n = droid_peaks(view_lo, view_hi, pb_lo, pb_hi, num, divider, hide_noise, percentile, f, d);
  jdouble v[64];
  for (int i = 0; i < n; i++) {
    v[2 * i] = f[i];
    v[2 * i + 1] = d[i];
  }
  if (n > 0) (*env)->SetDoubleArrayRegion(env, out, 0, 2 * n, v);
  return n;
}

JNIEXPORT void JNICALL FN(demoStart)(JNIEnv *env, jobject self) { (void)env; (void)self; droid_demo_start(); }
JNIEXPORT void JNICALL FN(demoStop)(JNIEnv *env, jobject self) { (void)env; (void)self; droid_demo_stop(); }
JNIEXPORT void JNICALL FN(demoSetFrequency)(JNIEnv *env, jobject self, jlong hz) {
  (void)env; (void)self;
  droid_demo_set_frequency(hz);
}
JNIEXPORT void JNICALL FN(demoSetFps)(JNIEnv *env, jobject self, jint fps) {
  (void)env; (void)self;
  droid_demo_set_fps(fps);
}

JNIEXPORT void JNICALL FN(glInit)(JNIEnv *env, jobject self) { (void)env; (void)self; droid_gl_init(); }
JNIEXPORT void JNICALL FN(glResize)(JNIEnv *env, jobject self, jint w, jint h) {
  (void)env; (void)self;
  droid_gl_resize(w, h);
}
JNIEXPORT void JNICALL FN(glDraw)(JNIEnv *env, jobject self, jdouble lo, jdouble hi, jdouble split, jint style,
                                  jfloat floor_db, jfloat range_db, jfloat wf_lo, jfloat wf_hi, jboolean flat,
                                  jfloat pan_lo, jfloat pan_hi, jint wf_theme, jint depth_rows, jboolean peak_hold,
                                  jfloat hold_s, jfloat drop_db_s, jint avg_rows, jint avg_mode, jfloat grid_db,
                                  jboolean preserve, jint peak_argb) {
  (void)env; (void)self;
  droid_view v = { .view_lo = lo, .view_hi = hi, .split = split, .style = style, .floor_db = floor_db,
                   .range_db = range_db, .wf_lo = wf_lo, .wf_hi = wf_hi, .flat = flat, .pan_lo = pan_lo,
                   .pan_hi = pan_hi, .wf_theme = wf_theme, .depth_rows = depth_rows, .peak_hold = peak_hold,
                   .peak_hold_s = hold_s, .peak_drop_db_s = drop_db_s, .avg_rows = avg_rows,
                   .avg_mode = avg_mode, .grid_db = grid_db, .preserve = preserve,
                   .peak_rgb = { ((peak_argb >> 16) & 255) / 255.0f, ((peak_argb >> 8) & 255) / 255.0f,
                                 (peak_argb & 255) / 255.0f } };
  droid_gl_draw(&v);
}
