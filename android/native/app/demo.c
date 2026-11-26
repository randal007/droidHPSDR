/*
 * demo.c — a made-up 20 m band for working on the display without a radio.
 *
 * The same signal model as the UI mock-ups (tools/mockups/render.js): CW,
 * FT8 in real 15 s slots, JS8, PSK31, RTTY, the 14.100 beacon and SSB, over
 * a noise floor near -118 dBm. Rows go through droid_rows_push() like the
 * radio's, so the renderer, the levels and the tuning are all the real ones.
 * The UI shows DEMO while it runs. It never runs while the radio does.
 *
 * GPL v3 or later, like the rest of droidHPSDR.
 */
#include <math.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "droid_app.h"

#define W 2048
#define RATE 192000.0
static atomic_int fps = 30;   /* rows per second: the Display menu's speed, like the radio's */

typedef enum { CW, FT8, PSK, RTTY, CAR, SSB } kind_t;
typedef struct { kind_t k; double f, l, wpm; int par; double p; } demo_sig;

static demo_sig sigs[64];
static int nsig;
static atomic_int running;
static _Atomic long long center = 14074000;
static pthread_t thread;

static uint32_t rng_state = 20261123;
static double rnd(void) {   /* mulberry32 */
  uint32_t t = (rng_state += 0x6D2B79F5u);
  t = (t ^ (t >> 15)) * (1u | t);
  t ^= t + (t ^ (t >> 7)) * (61u | t);
  return ((t ^ (t >> 14)) >> 0) / 4294967296.0;
}

static double h2(int a, long long b) {
  uint32_t h = (uint32_t)(a * 374761393u + (uint32_t)b * 668265263u) * 1274126177u;
  h ^= h >> 13;
  h *= 1274126177u;
  return (h ^ (h >> 16)) / 4294967296.0;
}

static void add(kind_t k, double f, double l) {
  if (nsig < 64) sigs[nsig++] = (demo_sig){ .k = k, .f = f, .l = l, .wpm = 4 + rnd() * 6 };
}

static void make_band(void) {
  static const double cw[][2] = {
    { 14.0105, -78 }, { 14.0127, -97 }, { 14.0182, -91 }, { 14.0231, -71 }, { 14.0254, -99 }, { 14.0307, -84 },
    { 14.0335, -101 }, { 14.0364, -90 }, { 14.0402, -87 }, { 14.0448, -95 }, { 14.0521, -80 }, { 14.0573, -103 },
    { 14.0606, -89 }, { 14.0652, -96 } };
  for (unsigned i = 0; i < sizeof cw / sizeof cw[0]; i++) add(CW, cw[i][0] * 1e6, cw[i][1]);
  for (int i = 0; i < 24; i++) {
    add(FT8, 14.074e6 + 180 + rnd() * 2750, -114 + rnd() * 38);
    sigs[nsig - 1].par = i % 2;
    sigs[nsig - 1].p = 0.55 + rnd() * 0.45;
  }
  for (int i = 0; i < 6; i++) {
    add(FT8, 14.078e6 + 400 + rnd() * 2100, -110 + rnd() * 22);
    sigs[nsig - 1].par = (i + 1) % 2;
    sigs[nsig - 1].p = 0.5;
  }
  for (int i = 0; i < 4; i++) add(PSK, 14.0705e6 + rnd() * 1800, -106 + rnd() * 18);
  add(RTTY, 14.0835e6, -82);
  add(RTTY, 14.0868e6, -96);
  add(CAR, 14.1000e6, -92);
  static const double ssb[][2] = { { 14.1505, -76 }, { 14.1625, -90 }, { 14.1700, -70 }, { 14.1785, -94 },
                                   { 14.1855, -81 }, { 14.1950, -99 } };
  for (unsigned i = 0; i < sizeof ssb / sizeof ssb[0]; i++) add(SSB, ssb[i][0] * 1e6, ssb[i][1]);
}

/* adds a signal component (Gaussian in frequency, peak level l dBm) */
static void comp(double *p, double fmin, double hzpp, double f, double sigma, double l) {
  double sig = fmax(sigma, hzpp * 0.6), a = pow(10.0, l / 10.0);
  int b0 = (int)floor((f - 4 * sig - fmin) / hzpp), b1 = (int)ceil((f + 4 * sig - fmin) / hzpp);
  if (b0 < 0) b0 = 0;
  if (b1 > W - 1) b1 = W - 1;
  for (int b = b0; b <= b1; b++) {
    double d = fmin + (b + 0.5) * hzpp - f;
    p[b] += a * exp(-d * d / (2 * sig * sig));
  }
}

static void *run(void *arg) {
  (void)arg;
  static double p[W];
  static float db[W];
  while (atomic_load(&running)) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    double t = (double)(ts.tv_sec % 86400) + ts.tv_nsec * 1e-9;   /* FT8 slots on the real clock */
    double c = (double)atomic_load(&center);
    double hzpp = RATE / W, fmin = c - RATE / 2;
    for (int b = 0; b < W; b++) {
      double e = 0;
      for (int q = 0; q < 4; q++) e -= log(1 - rnd() * 0.999);
      p[b] = pow(10, -11.85) * e / 4;
    }
    double meter = -127;
    for (int i = 0; i < nsig; i++) {
      demo_sig *s = &sigs[i];
      if (s->f < fmin - 4000 || s->f > fmin + RATE + 4000) continue;
      int id = i + 1;
      switch (s->k) {
      case CW:
        if (h2(id, (long long)(t * s->wpm)) > 0.42 && h2(id + 77, (long long)(t / 5)) > 0.22) comp(p, fmin, hzpp, s->f, 9, s->l);
        break;
      case FT8: {
        long long slot = (long long)(t / 15);
        double ph = t - slot * 15;
        if (slot % 2 == s->par && ph > 0.5 && ph < 13.1 && h2(id, slot) <= s->p) {
          int sym = (int)(h2(id * 7 + 1, (long long)(t / 0.16)) * 8);
          comp(p, fmin, hzpp, s->f + sym * 6.25, 11, s->l);
        }
        break;
      }
      case PSK: comp(p, fmin, hzpp, s->f, 14, s->l + (h2(id, (long long)(t * 3)) - 0.5) * 4); break;
      case RTTY:
        comp(p, fmin, hzpp, s->f, 18, s->l);
        comp(p, fmin, hzpp, s->f + 170, 18, s->l - 1);
        break;
      case CAR: comp(p, fmin, hzpp, s->f, 3, s->l); break;
      case SSB:
        if (h2(id, (long long)(t / 7)) >= 0.3) {
          double env = 0.25 + 0.75 * h2(id + 5, (long long)(t * 4));
          for (int off = 300; off <= 2700; off += 300)
            comp(p, fmin, hzpp, s->f + off, 150,
                 s->l + 10 * log10(env) - (off - 300) / 2400.0 * 14 + (h2(id + off, (long long)(t * 4)) - 0.5) * 8);
        }
        break;
      }
      if (fabs(s->f - c) < 1500 && s->l > meter) meter = s->l;
    }
    for (int b = 0; b < W; b++) db[b] = (float)(10 * log10(p[b]));
    droid_rows_push(db, 0.0f, W, fmin, hzpp);
    droid_meter_set(meter > -127 ? meter : -118 + rnd() * 2);
    usleep(1000000 / atomic_load(&fps));
  }
  return NULL;
}

void droid_demo_start(void) {
  if (atomic_exchange(&running, 1)) return;
  if (nsig == 0) make_band();
  pthread_create(&thread, NULL, run, NULL);
}

void droid_demo_stop(void) {
  if (!atomic_exchange(&running, 0)) return;
  pthread_join(thread, NULL);
}

void droid_demo_set_frequency(long long hz) { atomic_store(&center, hz); }
void droid_demo_set_fps(int f) { if (f >= 5 && f <= 60) atomic_store(&fps, f); }
