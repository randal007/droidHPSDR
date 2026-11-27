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
  TXS_VOX_PEAK,                    /* vox.c's detector level (DEXP), 0..1: the mic bar of the VOX page */
  TXS_VOX_REFUSED,                 /* times VOX was refused out of band (the UI says so when it grows) */
  TXS_TX_MODE,                     /* the TX VFO's mode: the per-mode settings are that mode's */
  TXS_SCREEN_PTT,                  /* the screen PTT holds MOX (not the radio's PTT: CW's PADDLE label) */
  TXS_LIVE_END = 72,               /* the live fields are TXS_ON_AIR .. TXS_LIVE_END-1 */
  /* M7 step 2, the TX audio chain (tx_menu.c, equalizer_menu.c, vox_menu.c, the TX monitor).
   * deskHPSDR keeps the ones marked "per mode" in mode_settings[] (copy_mode_settings). */
  TXA_COMP = 72,                   /* speech processor, per mode */
  TXA_COMP_LEVEL,                  /* 0..20 dB, per mode */
  TXA_LEV, TXA_LEV_GAIN,           /* leveler on, 0..15 dB, per mode */
  TXA_LEV_DECAY,                   /* 0..500 ms */
  TXA_CESSB,
  TXA_PHROT,                       /* phase rotator, per mode */
  TXA_PHROT_STAGES, TXA_PHROT_FREQ,  /* 1..15, 1..500 Hz */
  TXA_CFC, TXA_CFC_EQ,             /* CFC ("Use Pre-CFC") and its post EQ ("Use Post-CFC"), per mode */
  TXA_EQ,                          /* TX equalizer, per mode */
  TXA_CTFMODE,
  TXA_DEXP,                        /* the noise gate (DEXP), per mode, like all its settings: */
  TXA_DEXP_TRIGGER,                /* -40..-10 dB */
  TXA_DEXP_EXP,                    /* 0..30 dB */
  TXA_DEXP_ATTACK, TXA_DEXP_RELEASE, TXA_DEXP_HOLD, TXA_DEXP_TAU,   /* ms */
  TXA_DEXP_HYST,                   /* 0.05..0.95 */
  TXA_DEXP_FILTER, TXA_DEXP_FILTER_LOW, TXA_DEXP_FILTER_HIGH,       /* side channel filter, Hz */
  TXA_VOX,                         /* vox_enabled: off at every start (Randal) */
  TXA_VOX_THRESH,                  /* 0..1 (vox_menu.c's 0..1000 / 1000) */
  TXA_VOX_HANG,                    /* ms */
  TXA_MON, TXA_MON_GAIN, TXA_MON_POST,   /* the TX monitor: on, dB, 1 = after WDSP */
  TXA_ADDGAIN, TXA_ADDGAIN_GAIN,   /* "Local Mic PreAmp Gain", 1..20 dB */
  TXA_EQ_GAIN0 = 104,              /* 13: [0] the added gain, [1..12] the points; per mode */
  TXA_EQ_FREQ0 = TXA_EQ_GAIN0 + 13,
  TXA_CFC_FREQ0 = TXA_EQ_FREQ0 + 13,   /* 13: [0] unused */
  TXA_CFC_LVL0 = TXA_CFC_FREQ0 + 13,   /* [0] Pre Comp, 0..20 dB */
  TXA_CFC_POST0 = TXA_CFC_LVL0 + 13,   /* [0] Post Gain, -20..20 dB */
  /* M7 step 4, PureSignal (puresignal.c, deskHPSDR's ps_menu.c). Events: ENABLE, RESET, RESUME, TWOTONE, NOISE. */
  TXP_FIRST = TXA_CFC_POST0 + 13,
  TXP_ENABLE = TXP_FIRST,          /* tx_ps_onoff(): restarts the stream with the feedback receivers */
  TXP_RESET,                       /* "OFF": PS reset */
  TXP_RESUME,                      /* "Restart" */
  TXP_TWOTONE, TXP_NOISE,          /* the calibration sources: they key the radio until switched off */
  TXP_NOISE_LEVEL,                 /* -12..+3 dB */
  TXP_AUTO,                        /* Auto Attenuate */
  TXP_ATT,                         /* TX ATT, -29..+31 dB on an HL2, kept per band */
  TXP_MON,                         /* the TX spectrum shows the feedback (transmitter->feedback) */
  TXP_ONESHOT,
  TXP_TOLERANCE,                   /* 0 Strict, 1 Medium, 2 Relaxed */
  TXP_DELAY,                       /* TX delay, ns */
  TXP_SETPK,
  TXP_ZERO_WARN,                   /* radio.c ps_zero_att_warning */
  TXP_END,
  /* live, read-only */
  TXPS_FEEDBACK = TXP_END,         /* GetPSInfo: info[4], the feedback level */
  TXPS_COR_COUNT,                  /* info[5] */
  TXPS_SLN_CHECK,                  /* info[6] */
  TXPS_DG_COUNT,                   /* info[13] */
  TXPS_CORRECTING,                 /* info[14] */
  TXPS_STATE,                      /* info[15]: 0 RESET .. 9 TURNON */
  TXPS_GETPK,                      /* GetPSMaxTX */
  TXPS_WARN,                       /* times the "TX ATT is 0" warning is due (grows: the UI shows it) */
  TXPS_GEN_REFUSED,                /* times Two Tone / Noise was refused (reason: TXS_REFUSED) */
  TXPS_BAND,                       /* the TX VFO's band: TX ATT is kept per band */
  TXPS_END,
  /* M7 step 3, CW (cw.c, deskHPSDR's cw_menu.c) for the HL2+'s keyer. Events: ZB_START, REARM. */
  TXC_FIRST = TXPS_END,
  TXC_SPEED = TXC_FIRST,           /* 1..60 WPM */
  TXC_MODE,                        /* 0 straight, 1 iambic A, 2 iambic B */
  TXC_WEIGHT,                      /* 0..100 */
  TXC_SPACING,                     /* enforce letter spacing */
  TXC_REVERSED,                    /* keys reversed */
  TXC_HANG,                        /* the radio's hang time (break-in delay), 0..1000 ms */
  TXC_PTT_DELAY,                   /* RF after the amplifier's PTT, 0..255 ms (old_protocol.c caps it at 900 / speed) */
  TXC_SIDE_LEVEL,                  /* sidetone level 0..127 */
  TXC_PITCH,                       /* sidetone frequency = the CW pitch, 100..1000 Hz */
  TXC_ZB_CORR,                     /* zero beat correction, -50..50 Hz (active receiver) */
  TXC_ZB_START,                    /* zero beat now */
  TXC_REARM,                       /* arm the keyer again after the TX timeout disarmed it */
  TXC_END,
  /* live, read-only */
  TXCS_ARMED = TXC_END,            /* the radio's keyer is armed (CW mode, transmitting allowed) */
  TXCS_LOCK,                       /* why it isn't (TXR_*) */
  TXCS_ZB_ACTIVE,                  /* a zero beat is listening */
  TXCS_ZB_DONE,                    /* zero beats finished (grows) */
  TXCS_ZB_HZ,                      /* how far the last one moved the VFO, Hz (0: no clear tone / already on it) */
  TXCS_CW,                         /* the TX mode is CWU / CWL */
  TXCS_END,
  TX_COUNT = TXCS_END
};
enum { CW_ZB_REFUSED = 99999 };   /* TXCS_ZB_HZ: the zero beat couldn't start (not CW on RX, or transmitting) */
/* PureSignal's AmpView data (droid_ps_disp): float offsets */
enum { PSD_SAMPLES = 4096, PSD_POINTS = 512, PSD_X = 3, PSD_YM = PSD_X + PSD_SAMPLES, PSD_YC = PSD_YM + PSD_SAMPLES,
       PSD_YS = PSD_YC + PSD_SAMPLES, PSD_XM_COR = PSD_YS + PSD_SAMPLES, PSD_YM_COR = PSD_XM_COR + PSD_POINTS,
       PSD_XA_COR = PSD_YM_COR + PSD_POINTS, PSD_YA_COR = PSD_XA_COR + PSD_POINTS, PSD_COUNT = PSD_YA_COR + PSD_POINTS };
/* puresignal.c */
void droid_ps_create(int pixels);              /* with the transmitter: the feedback receivers */
void droid_ps_save(void);
void droid_ps_sample_rate(int rate);           /* the receivers' rate changed (stream stopped) */
void droid_ps_started(void);                   /* loop thread, after a start: PS back on if it was */
void droid_ps_poll(void);                      /* loop thread: Two Tone / Noise end with the transmission */
void droid_ps_generators_off(void);
int droid_ps_apply(int field, double v);       /* 1 if it was a PS field */
void droid_ps_get(double *out);
int droid_ps_disp(float *out, int max);        /* AmpView, PSD_COUNT floats */
/* cw.c */
void droid_cw_create(void);                    /* with the transmitter: radio.c's props entries */
void droid_cw_save(void);
void droid_cw_poll(void);                      /* loop thread: arm the radio's keyer only while TX is allowed */
void droid_cw_timeout(void);                   /* the TX timeout fired: in CW, disarm the keyer */
void droid_cw_off(void);                       /* back to receive / stop: keyer disarmed until the next poll */
void droid_tx_enabled_now(int on);             /* "Transmit enabled", from any thread, even while stopped */
int droid_cw_apply(int field, double v);       /* 1 if it was a CW field */
void droid_cw_get(double *out);
int droid_tx_blocked(void);                    /* transmit.c: Transmit enabled off, demo band, or no transmitter */
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
