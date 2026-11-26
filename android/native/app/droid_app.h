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
#include <stdatomic.h>
#include <stdint.h>

/* ---- spectrum rows: display_bridge.c fills, the renderer drains ---- */

#define DROID_ROW_MAX 8192   /* most pixels per row: zoomed rows keep ~4 screens of data */
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

/* Queues one spectrum row (db[i] + offset) for the renderer: from
 * rx_panadapter_update(), or from the demo (demo.c). */
void droid_rows_push(const float *db, float offset, int width, double fmin, double hzpp);
void droid_meter_set(double dbm);

/* Demo band (demo.c): a made-up 20 m band fed to the renderer while there is
 * no radio, for working on the display. Never runs while the radio does. */
void droid_demo_start(void);
void droid_demo_stop(void);
void droid_demo_set_frequency(long long hz);
void droid_demo_set_fps(int fps);

/* S-meter in dBm, corrected like upstream's (rx_update_display). */
double droid_meter_dbm(void);
unsigned long long droid_rows_count(void);

/* Peak labels, upstream's way (display_bridge.c); returns how many. */
int droid_peaks(double view_lo, double view_hi, double pb_lo, double pb_hi, int num, int divider, int hide_noise,
                double percentile, double *f, float *d);

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
  char props_dir[256]; /* where deskHPSDR's <MAC>.props file lives */
  char playback[512];  /* an IQ recording (WAV) to play instead of the radio; freq = its centre */
} droid_start_opts;

/* playback.c: an IQ recording in place of the radio (the demo band) */
int droid_playback_open(const char *path, int *sample_rate, char *msg, int msglen);
void droid_playback_run(void);
void droid_playback_stop(void);
extern atomic_int droid_playback_on;
/* the HL2 discovery found this run (NULL before): radio points at the recording while one plays */
struct _DISCOVERED;
struct _DISCOVERED *droid_real_radio(void);

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
void droid_set_phone_audio(int on);    /* also play on the phone (deskHPSDR's local audio) */
void droid_set_zoom(int zoom);         /* upstream's zoom, 1..16 */
void droid_set_fps(int fps);           /* spectrum rows per second */
void droid_set_window(int type);       /* FFT window, Screen menu order */
void droid_set_detector(int det);      /* 0 Peak, 1 Average, 2 Rosenfell, 3 Sample */
void droid_set_pan_fft(int size);
/* M6: the HL2 settings of the SDR Device sheet (radio_core.c, on the radio loop) */
enum { HL2_CODEC, HL2_FILTER_BOARD, HL2_HPF, HL2_PPM, HL2_ADC_CAL, HL2_FORCE_IOB,
       HL2_SPLIT_OFF_BAND, HL2_REGION_60M, HL2_IARU_REGION };
void droid_hl2_set(int field, double v);
/* ANT menu: a band's RX antenna (0 ANT1, 1 ANT2, 2 ANT3, 3 EXT1, 4 EXT2, 5 XVTR) */
int droid_band_rx_ant(int band);
void droid_set_band_rx_ant(int band, int ant);
void droid_set_rit_step(int hz);       /* 1, 10 or 100 (vfo.c, per mode) */
int droid_channels_60m(long long *freq, long long *width, int max);   /* the region's 60 m channels */
/* hl2.c: IO board detection, and HL2 setup over UDP 1025 (errors as text) */
void droid_hl2_info(int *iob, int *pico, char *lpf, int lpf_len, int *gateware);
int droid_hl2_read_flags(int *flags, char *error, int error_len);
int droid_hl2_set_dhcp(char *error, int error_len);
int droid_hl2_set_mac(int a, int b, char *error, int error_len);
int droid_hl2_reboot(char *error, int error_len);      /* Screen menu FFT size: 0 Auto, 16384 .. 262144 */

/* M4, receive operating: each runs deskHPSDR's own vfo.c / store.c call */
void droid_filter_preset(int index);   /* vfo_filter_changed(): 0..9 presets, 10/11 Var1/Var2 */
void droid_band(int band);             /* vfo_band_changed(): same band again = next stack entry */
void droid_bandstack(int entry);       /* vfo_bandstack_changed() */
void droid_vfo_op(int op);             /* 0 A>B, 1 B>A, 2 A<>B, 3 split on/off */
void droid_rit(int on, long long hz);  /* vfo_rit_onoff() / vfo_rit_value() */
void droid_ctun(int on);               /* vfo_ctun_update() */
void droid_lock(int on);               /* vfo.c's 'locked' */
void droid_step(int hz);               /* VFO A's tuning step */
void droid_memory(int store, int slot);/* store_memory_slot() / recall_memory_slot() */
void droid_agc(int agc);               /* rx->agc + rx_set_agc() */
void droid_save(void);                 /* write the props file (radio.c's save order) */

/* M5, receive DSP (dsp.c): deskHPSDR's Noise menu, notch, squelch, RX EQ,
 * DSP menu. Fields are numbered (dsp.c's enum = Dsp in Radio.kt). */
void droid_dsp_set(int field, double value);   /* queued to the loop thread */
void droid_dsp_apply(int field, double value); /* on the loop thread */
void droid_dsp_get(double *out);               /* droid_dsp_count() values */
int droid_dsp_count(void);

/* M7, transmit (transmit.c): numbered like dsp.c's fields (Tx in Radio.kt).
 * Settings are set with droid_tx_set (queued to the loop thread); everything,
 * live state included (TXS_*), comes back from droid_tx_get. */
enum {
  TX_ENABLED, TX_TIMEOUT, TX_PTT, TX_TUNE, TX_DRIVE, TX_TUNE_DRIVE, TX_TUNE_USE_DRIVE, TX_DRIVE_PER_BAND,
  TX_MIC_GAIN, TX_MIC_BOOST, TX_MIC_LOCAL, TX_FILTER_LOW, TX_FILTER_HIGH, TX_USE_RX_FILTER, TX_SWR_PROT,
  TX_SWR_ALARM, TX_OUT_OF_BAND, TX_MUTE_RX, TX_PA, TX_PA_POWER,
  TX_PA_CAL0 = 20,                 /* + band number (band.h), 24 bands */
  TX_INHIBIT_EN = 44,              /* radio_menu.c "Enable external TxInhibit Input" */
  TX_XIT_ON, TX_XIT,               /* the TX VFO's XIT (vfo.c) */
  TX_PA_FILE,                      /* 1: save the PA calibration, 2: load it (band.c) */
  TXS_ON_AIR = 48, TXS_FWD, TXS_REV, TXS_SWR, TXS_ALC, TXS_MIC, TXS_HIGH_SWR, TXS_SECONDS, TXS_TIMEOUTS,
  TXS_REFUSED, TXS_RADIO_PTT, TXS_UNDERRUN, TXS_OVERRUN, TXS_OUT_OF_BAND, TXS_MAX_W, TXS_PICO,
  TXS_INHIBITED, TXS_PA_FILE_OK,
  TX_COUNT = 72
};
/* why the last PTT / TUNE did not transmit */
enum { TXR_NONE, TXR_DISABLED, TXR_INHIBIT, TXR_CW, TXR_BAND, TXR_TIMEOUT };
void droid_tx_set(int field, double value);    /* queued to the loop thread */
void droid_tx_apply(int field, double value);  /* on the loop thread */
void droid_tx_get(double *out);                /* TX_COUNT values */
void droid_tx_create(int pixels);              /* radio_core.c start: upstream's transmitter */
void droid_tx_save(void);                      /* radio_core.c save: its props */
void droid_tx_off_now(void);                   /* back to receive, now */
void droid_tx_poll(void);                      /* loop thread: PTT repeats, the timeout */
void droid_tx_ptt_seen(void);                  /* a screen PTT repeat (any thread) */
/* 1 while transmitting: the renderer shows the TX spectrum over the stopped waterfall (M7 pick A) */
int droid_tx_on_air(void);
/* the phone mic's AAudio device (0: default) and whether it is a Bluetooth headset (audio_aaudio.c) */
void droid_mic_route(int device_id, int bluetooth);
unsigned droid_mic_frames(void);   /* frames the phone mic delivered so far */
float droid_mic_peak(void);        /* its loudest sample since the last call (0..1) */
unsigned long long droid_tx_rows_count(void);   /* TX spectrum rows so far: data flows while transmitting too */
/* the newest TX spectrum row (transmit.c); -1 when there is none newer than *seq */
int droid_tx_row(float *db, int max, double *fmin, double *hzpp, uint64_t *seq);

/* HL2 ADC auto gain (autogain.c, from rigctl.c): start/stop the thread */
void droid_autogain_run(int run);

/* lists for the UI: deskHPSDR's tables, read on the loop thread's data */
int droid_filters(int mode, char titles[][16], int *lo, int *hi, int max);
int droid_bands(char titles[][16], long long *fmin, long long *fmax, int max);
int droid_bandstack_entries(int band, long long *freq, int *mode, int *filter, int max, int *current);
int droid_memories(long long *freq, int *mode, int max);

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
  int phone_audio;
  int zoom;
  long long freq_b;
  int mode_b;
  int rit_on, ctun, locked, split, step, band, bandstack, filter, agc;
  long long rit;
  unsigned long long rows;   /* spectrum rows so far: data is arriving while this grows */
  int running;
  int rit_step;
} droid_status;
void droid_get_status(droid_status *s);

/* The VFO frequency the radio was on age_us ago (radio_core.c keeps a short
 * history), for labelling spectrum rows with the frequency of their data. */
long long droid_freq_ago(int64_t age_us);
/* CW: deskHPSDR shows the spectrum and filter edges shifted by the sidetone
 * pitch, so a CW signal you hear at the pitch sits on the VFO line
 * (rx_panadapter.c, receiver.c rx_set_filter). +pitch in CWU, -pitch in CWL. */
int droid_cw_shift(void);

/* How old a spectrum row's data is when it is drawn, microseconds. */
extern int droid_row_delay_us;

/* WDSP's FFT wisdom: slow the first time (about 11 s on the S26 Ultra). */
int droid_wisdom(const char *dir);
const char *droid_wisdom_status(void);

/* ---- GPU renderer (gl_render.c), called on the GL thread ---- */

typedef struct {
  double view_lo, view_hi;   /* Hz at the left and right screen edges */
  double split;              /* 0..1: where the 3D (or 2D) view ends and the waterfall starts */
  int style;                 /* 0 X6100 look 3, 1 AetherSDR curtains, 2 lit terrain, 3 wire */
  float floor_db, range_db;  /* 3D: dBm at the floor and the dB to a full ridge */
  float wf_lo, wf_hi;        /* waterfall colour range, dBm */
  int flat;                  /* 1: 2D spectrum instead of the 3D view */
  float pan_lo, pan_hi;      /* 2D: dBm at the bottom and the top */
  int wf_theme;              /* waterfall (and 2D fill) colours, see gl_render.c */
  int depth_rows;            /* 3D history depth in rows */
  int peak_hold;             /* 2D: draw the peak-hold line */
  float peak_hold_s, peak_drop_db_s;   /* hold time, then fall rate */
  int avg_rows;              /* 2D: rows averaged (deskHPSDR's Av. Time x fps) */
  int avg_mode;              /* 2D: deskHPSDR's averaging kinds: 0 None, 1 Recursive, 2 Time window, 3 Log recursive */
  float grid_db;             /* 2D: dB between grid lines (deskHPSDR's panadapter step) */
  int preserve;              /* 2D: deskHPSDR's "Preserve Narrow Peaks" (max of 3 analyzer pixels) */
  float peak_rgb[3];         /* 2D: peak-hold line colour */
  float bg_alpha;            /* 2D: how much of the background picture shows (0: none) */
} droid_view;

void droid_gl_init(void);
void droid_gl_resize(int w, int h);
void droid_gl_draw(const droid_view *v);
void droid_gl_set_background(unsigned tex, int w, int h);

void droid_log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#endif
