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
  int width = rx->width;
  if (width > DROID_ROW_MAX) width = DROID_ROW_MAX;
  if (rx->pan + width > rx->pixels) width = rx->pixels - rx->pan;
  if (width <= 0) return;

  /* rx_panadapter.c: soffset and min_display */
  const BAND *band = band_get_band(vfo[rx->id].band);
  double soffset = (double)(rx_gain_calibration - band->gain) + (double)adc[rx->adc].attenuation - adc[rx->adc].gain;
  double hzpp = (double)rx->sample_rate / (double)rx->pixels;
  /* the frequency of the row's data, not of now: see droid_freq_ago() */
  double fmin = (double)droid_freq_ago(droid_row_delay_us) - (double)rx->sample_rate * 0.5 + (double)rx->pan * hzpp;

  droid_rows_push(rx->pixel_samples + rx->pan, (float)soffset, width, fmin, hzpp);
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

double droid_meter_dbm(void) {
  pthread_mutex_lock(&lock);
  double v = meter_dbm;
  pthread_mutex_unlock(&lock);
  return v;
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
