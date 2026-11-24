/*
 * droid_app.h — what the app's native parts share: the radio core
 * (radio_core.c), the display hook (display_bridge.c), the GPU renderer
 * (gl_render.c) and the JNI bridge (jni_bridge.c).
 *
 * No Compose, GL or JNI types here: radio_core.c and display_bridge.c only
 * talk to upstream deskHPSDR code; gl_render.c and jni_bridge.c only read
 * what is declared below.
 *
 * GPL v3 or later, like the rest of droidHPSDR.
 */
#ifndef DROID_APP_H
#define DROID_APP_H
#include <stdint.h>

/* ---- spectrum rows: display_bridge.c fills, the renderer drains ---- */

#define DROID_ROW_MAX 4096   /* most pixels per row */
#define DROID_ROW_QUEUE 64   /* rows kept for a renderer that fell behind */

typedef struct {
  float db[DROID_ROW_MAX]; /* dBm per pixel, calibrated like upstream's panadapter */
  int width;               /* pixels in this row */
  double fmin;             /* Hz at the left edge of pixel 0 */
  double hzpp;             /* Hz per pixel */
  uint64_t seq;            /* 1, 2, 3, ... */
} droid_row;

/* Calls fn for every row newer than *seq, oldest first, and advances *seq.
 * Rows older than the queue are skipped. Returns how many rows it passed. */
int droid_rows_pull(uint64_t *seq, void (*fn)(const droid_row *row, void *ctx), void *ctx);

/* S-meter in dBm, corrected like upstream's (rx_update_display). */
double droid_meter_dbm(void);

/* Noise floor of the newest row (20th percentile), dBm: the display's auto levels. */
double droid_noise_floor(void);

/* ---- radio core (radio_core.c), RECEIVE ONLY ---- */

typedef struct {
  long long freq;      /* Hz */
  int mode;            /* upstream mode number (modeUSB, ...) */
  double lna_db;       /* HL2 LNA gain, -12..48 */
  int codec;           /* 1: demodulated audio to the HL2+ speaker */
  int sample_rate;     /* 48000, 96000, 192000 or 384000 */
  int pixels;          /* spectrum pixels per row */
  int fps;             /* spectrum rows per second */
  char radio_ip[64];   /* empty: broadcast discovery */
} droid_start_opts;

/* Discovery, receiver set-up as radio.c does for an HL2, stream start.
 * Returns 0 when streaming, else a negative error (see radio_core.c). */
int droid_radio_start(const droid_start_opts *o, char *msg, int msglen);
void droid_radio_stop(void);
int droid_radio_running(void);

/* Requests from the UI. They queue and run on the radio's own loop thread,
 * the one that plays GTK's main loop for upstream code. */
void droid_set_frequency(long long hz);
void droid_set_mode(int mode);
void droid_set_filter(int low, int high);
void droid_set_volume_db(double db);   /* -40..0 */
void droid_set_lna_db(double db);      /* -12..48 */

typedef struct {
  long long freq;
  int mode;
  int filter_low, filter_high;   /* Hz relative to freq */
  int sample_rate;
  double meter_dbm;
  unsigned lost_packets;
  int adc_overloads;
  double lna_db, volume_db;
  double noise_floor;
  int running;
} droid_status;
void droid_get_status(droid_status *s);

/* WDSP's FFT wisdom: slow the first time (about 11 s on the S26 Ultra). */
int droid_wisdom(const char *dir);
const char *droid_wisdom_status(void);

/* ---- GPU renderer (gl_render.c), called on the GL thread ---- */

typedef struct {
  double view_lo, view_hi;   /* Hz at the left and right screen edges */
  double split;              /* 0..1: where the 3D view ends and the waterfall starts */
  int style;                 /* 0 X6100 look 3, 1 AetherSDR curtains, 2 lit terrain, 3 wire */
  float floor_db, range_db;  /* 3D: dBm at the floor and the dB to a full ridge */
  float wf_lo, wf_hi;        /* waterfall colour range, dBm */
} droid_view;

void droid_gl_init(void);
void droid_gl_resize(int w, int h);
void droid_gl_draw(const droid_view *v);

void droid_log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#endif
