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
                                    jint sample_rate, jint pixels, jint fps, jstring ip) {
  (void)self;
  droid_start_opts o = { .freq = freq, .mode = mode, .lna_db = lna, .codec = codec, .sample_rate = sample_rate,
                         .pixels = pixels, .fps = fps };
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
JNIEXPORT void JNICALL FN(setLnaDb)(JNIEnv *env, jobject self, jdouble db) {
  (void)env; (void)self;
  droid_set_lna_db(db);
}

/* [running, freq, mode, filterLow, filterHigh, sampleRate, meterDbm, lostPackets, adcOverloads, lna, volume,
 *  noiseFloor] */
JNIEXPORT void JNICALL FN(status)(JNIEnv *env, jobject self, jdoubleArray out) {
  (void)self;
  droid_status s;
  droid_get_status(&s);
  jdouble v[12] = { s.running, (double)s.freq, s.mode, s.filter_low, s.filter_high, s.sample_rate,
                    s.meter_dbm, s.lost_packets, s.adc_overloads, s.lna_db, s.volume_db, s.noise_floor };
  (*env)->SetDoubleArrayRegion(env, out, 0, 12, v);
}

JNIEXPORT void JNICALL FN(glInit)(JNIEnv *env, jobject self) { (void)env; (void)self; droid_gl_init(); }
JNIEXPORT void JNICALL FN(glResize)(JNIEnv *env, jobject self, jint w, jint h) {
  (void)env; (void)self;
  droid_gl_resize(w, h);
}
JNIEXPORT void JNICALL FN(glDraw)(JNIEnv *env, jobject self, jdouble lo, jdouble hi, jdouble split, jint style,
                                  jfloat floor_db, jfloat range_db, jfloat wf_lo, jfloat wf_hi) {
  (void)env; (void)self;
  droid_view v = { .view_lo = lo, .view_hi = hi, .split = split, .style = style, .floor_db = floor_db,
                   .range_db = range_db, .wf_lo = wf_lo, .wf_hi = wf_hi };
  droid_gl_draw(&v);
}
