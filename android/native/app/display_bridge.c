/*
 * display_bridge.c — upstream's display hooks for the app.
 *
 * receiver.c's own display timer (rx_update_display, unchanged) fetches the
 * analyzer pixels with rx_get_pixels() and calls rx_panadapter_update().
 * deskHPSDR draws with cairo there; we copy the visible row, calibrated the
 * same way rx_panadapter.c does, into a small queue the GPU renderer drains.
 *
 * GPL v3 or later, like the rest of droidHPSDR.
 */
#include <pthread.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <gtk/gtk.h>

#include "adc.h"
#include "band.h"
#include "radio.h"
#include "receiver.h"
#include "meter.h"
#include "rx_panadapter.h"
#include "vfo.h"
#include "waterfall.h"

#include "droid_app.h"

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static droid_row queue[DROID_ROW_QUEUE];
static uint64_t newest;   /* seq of the newest row, 0 = none yet */

/* About half the analyzer's FFT window (16384 points at 192 kHz = 85 ms)
 * plus the trip through the HL2 and WDSP's input buffer. Set from the UI
 * while testing; the right value makes a signal stay a straight line while
 * tuning. */
int droid_row_delay_us = 90000;
static double meter_dbm = -140.0;
static double noise_floor = -125.0;   /* 20th percentile of the newest row, dBm */

void rx_panadapter_init(RECEIVER *rx, int width, int height) { (void)rx; (void)width; (void)height; }
void waterfall_init(RECEIVER *rx, int width, int height) { (void)rx; (void)width; (void)height; }
void waterfall_update(RECEIVER *rx) { (void)rx; }   /* the renderer keeps its own history */

void rx_panadapter_update(RECEIVER *rx) {
  if (rx != receiver[0] || rx->pixel_samples == NULL || rx->pixels <= 0) return;
  /* Not just the zoomed window upstream would draw: as much of the whole
   * (zoomed) analyzer row as fits, centred on the VFO, so dragging a zoomed
   * view finds real data instead of black (Randal, 2026-11-24). */
  int width = rx->pixels < DROID_ROW_MAX ? rx->pixels : DROID_ROW_MAX;
  int start = rx->pixels / 2 - width / 2;
  if (start < 0) start = 0;
  if (start + width > rx->pixels) start = rx->pixels - width;

  /* rx_panadapter.c: soffset and min_display (from our start, not upstream's pan) */
  const BAND *band = band_get_band(vfo[rx->id].band);
  double soffset = (double)(rx_gain_calibration - band->gain) + (double)adc[rx->adc].attenuation - adc[rx->adc].gain;
  double hzpp = (double)rx->sample_rate / (double)rx->pixels;
  /* the frequency of the row's data, not of now: see droid_freq_ago() */
  double fmin = (double)droid_freq_ago(droid_row_delay_us) - (double)rx->sample_rate * 0.5 + (double)start * hzpp;

  droid_rows_push(rx->pixel_samples + start, (float)soffset, width, fmin, hzpp);
}

void droid_rows_push(const float *db, float offset, int width, double fmin, double hzpp) {
  if (width > DROID_ROW_MAX) width = DROID_ROW_MAX;
  pthread_mutex_lock(&lock);
  droid_row *r = &queue[(newest + 1) % DROID_ROW_QUEUE];
  for (int i = 0; i < width; i++) r->db[i] = db[i] + offset;
  r->width = width;
  r->fmin = fmin;
  r->hzpp = hzpp;
  r->seq = ++newest;

  /* noise floor for the display's auto levels: 1 dB histogram, 20th percentile */
  int hist[200] = { 0 };
  for (int i = 0; i < width; i++) {
    int b = (int)(r->db[i] + 200.0f);
    hist[b < 0 ? 0 : b > 199 ? 199 : b]++;
  }
  int want = width / 5, acc = 0, b = 0;
  while (b < 199 && (acc += hist[b]) < want) b++;
  noise_floor = b - 200.0 + 0.5;
  pthread_mutex_unlock(&lock);
}

void droid_meter_set(double dbm) {
  pthread_mutex_lock(&lock);
  meter_dbm = dbm;
  pthread_mutex_unlock(&lock);
}

void meter_update(RECEIVER *rx, int meter_type, double value, double alc, double swr) {
  (void)alc; (void)swr;
  if (rx == receiver[0] && meter_type == SMETER) {
    pthread_mutex_lock(&lock);
    meter_dbm = value;
    pthread_mutex_unlock(&lock);
  }
}

unsigned long long droid_rows_count(void) {
  pthread_mutex_lock(&lock);
  unsigned long long n = newest;
  pthread_mutex_unlock(&lock);
  return n;
}

double droid_meter_dbm(void) {
  pthread_mutex_lock(&lock);
  double v = meter_dbm;
  pthread_mutex_unlock(&lock);
  return v;
}

static int cmp(const void *a, const void *b) {
  float x = *(const float *)a, y = *(const float *)b;
  return x < y ? -1 : x > y;
}

/* Peak labels, as upstream's rx_panadapter.c finds them (Display menu:
 * "Number of Peaks to label", "Panadapter Ignore Adjacent Peaks", "Hide Peaks
 * Below Noise Floor", "Panadapter Noise Floor Percentile", "Peak Labels in
 * Passband Only"): local maxima of the visible part of the newest row
 * [view_lo, view_hi], optionally only inside [pb_lo, pb_hi], above the
 * percentile + 3 dB, one per ignore range (visible width / divider), the
 * strongest num of them, strongest first. Returns how many. */
int droid_peaks(double view_lo, double view_hi, double pb_lo, double pb_hi, int num, int divider, int hide_noise,
                double percentile, double *f, float *d) {
  static float row[DROID_ROW_MAX], sorted[DROID_ROW_MAX];
  int w;
  double fmin, hzpp;
  if (num < 1) return 0;
  if (num > 32) num = 32;
  pthread_mutex_lock(&lock);
  if (newest == 0) {
    pthread_mutex_unlock(&lock);
    return 0;
  }
  const droid_row *r = &queue[newest % DROID_ROW_QUEUE];
  w = r->width;
  fmin = r->fmin;
  hzpp = r->hzpp;
  memcpy(row, r->db, sizeof(float) * (size_t)w);
  pthread_mutex_unlock(&lock);

  int i0 = (int)ceil((view_lo - fmin) / hzpp), i1 = (int)floor((view_hi - fmin) / hzpp);
  if (i0 < 1) i0 = 1;
  if (i1 > w - 2) i1 = w - 2;
  int mywidth = i1 - i0 + 1;
  if (mywidth < 3) return 0;
  double noise_level = -1e9;
  if (hide_noise) {
    memcpy(sorted, row + i0, sizeof(float) * (size_t)mywidth);
    /* the percentile by sorting, like upstream */
    qsort(sorted, (size_t)mywidth, sizeof(float), cmp);
    int k = (int)(percentile / 100.0 * mywidth);
    if (k < 0) k = 0;
    if (k >= mywidth) k = mywidth - 1;
    noise_level = sorted[k] + 3.0;
  }
  if (divider < 1) divider = 1;
  int ignore = (mywidth + divider - 1) / divider;
  double peaks[32];
  int pos[32];
  for (int a = 0; a < num; a++) { peaks[a] = -200; pos[a] = -1; }
  for (int i = i0; i <= i1; i++) {
    double fi = fmin + (i + 0.5) * hzpp;
    if (fi < pb_lo || fi > pb_hi) continue;
    double s = row[i];
    if (s < noise_level || !(s > row[i - 1] && s > row[i + 1])) continue;
    int replace = -1;
    for (int j = 0; j < num; j++) {
      if (pos[j] >= 0 && pos[j] >= i - ignore && pos[j] <= i + ignore) {
        replace = s > peaks[j] ? j : -2;
        break;
      }
    }
    if (replace >= 0) {
      peaks[replace] = s;
      pos[replace] = i;
    } else if (replace == -1) {
      int low = 0;
      for (int j = 1; j < num; j++) if (peaks[j] < peaks[low]) low = j;
      if (s > peaks[low]) { peaks[low] = s; pos[low] = i; }
    }
  }
  int out = 0;
  for (int a = 0; a < num; a++) {   /* strongest first */
    int best = -1;
    for (int j = 0; j < num; j++) if (pos[j] >= 0 && (best < 0 || peaks[j] > peaks[best])) best = j;
    if (best < 0) break;
    f[out] = fmin + (pos[best] + 0.5) * hzpp;
    d[out] = (float)peaks[best];
    out++;
    pos[best] = -1;
  }
  return out;
}

double droid_noise_floor(void) {
  pthread_mutex_lock(&lock);
  double v = noise_floor;
  pthread_mutex_unlock(&lock);
  return v;
}

int droid_rows_pull(uint64_t *seq, void (*fn)(const droid_row *row, void *ctx), void *ctx) {
  static droid_row copy;   /* only the GL thread pulls */
  int n = 0;
  for (;;) {
    pthread_mutex_lock(&lock);
    uint64_t want = *seq + 1;
    if (newest >= DROID_ROW_QUEUE && want <= newest - DROID_ROW_QUEUE) want = newest - DROID_ROW_QUEUE + 1;
    if (want > newest) {
      pthread_mutex_unlock(&lock);
      return n;
    }
    copy = queue[want % DROID_ROW_QUEUE];
    pthread_mutex_unlock(&lock);
    *seq = want;
    fn(&copy, ctx);
    n++;
  }
}
