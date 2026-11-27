/*
 * puresignal.c — M7 step 4, PureSignal: what deskHPSDR's ps_menu.c (and a
 * few lines of radio.c) does around upstream's transmitter.c, which
 * droidHPSDR compiles unchanged (tx_ps_onoff, tx_ps_reset/resume,
 * tx_set_twotone / tx_set_noise, the feedback samples in old_protocol.c).
 *
 * ps_menu.c is a GTK dialog, so it is not compiled. Copied from it:
 * ps_calibration_timer() (Auto Attenuate, unchanged but for the GTK
 * lines), and the lines of its callbacks that change a setting:
 * enable_cb, reset_cb, resume_cb, att_spin_cb, oneshot_cb,
 * tolerance_mode_cb, ps_tx_delay_cb, setpk_cb, feedback_cb, twotone_cb,
 * noise_cb, noise_level_cb, auto_cb; ampview's tx_ps_getdisp() read.
 * From radio.c: the two feedback receivers made with the transmitter,
 * restoring PS at start, ps_zero_att_warning.
 *
 * droidHPSDR's own (Randal's safety rules, docs/M7-INVENTORY.md):
 *   - Two Tone and Noise key the radio: refused like the screen PTT
 *     (Transmit enabled, inhibit, out of band, the timeout);
 *   - any way back to receive (PTT release, the TX timeout, TUNE, stop)
 *     also switches the generator off, so it never keys the radio again
 *     on the next MOX (deskHPSDR switches them off when the PS menu closes).
 *
 * Copied code: (C) 2017 John Melton G0ORX/N6LYT, 2015 John Melton,
 * 2024-2026 Heiko Amft DL1BZ (Project deskHPSDR), GPL-3.0-or-later.
 *
 * GPL v3 or later, like the rest of droidHPSDR.
 */
#include <math.h>
#include <pthread.h>
#include <stdatomic.h>
#include <string.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include <wdsp.h>

#include "band.h"
#include "new_protocol.h"
#include "old_protocol.h"
#include "property.h"
#include "radio.h"
#include "receiver.h"
#include "transmitter.h"
#include "vfo.h"
#include "droid_app.h"

#define INFO_SIZE 16

/* radio.c: warn when PS is switched on with TX ATT 0 (deskHPSDR's dialog; here the UI shows it) */
gboolean ps_zero_att_warning = TRUE;
static atomic_int warn_count;          /* times the warning is due: the UI shows it when this grows */
static atomic_int gen_refused;         /* times Two Tone / Noise was refused (the reason is in TXS_REFUSED) */
static int ps_wanted;                  /* the props file's PS state, applied once the stream runs */
static long long gen_since_us;         /* when Two Tone / Noise was switched on (loop thread) */
static int gen_keyed;                  /* it has keyed the radio since */

static void ps_warn_if_zero_att(void) {
  if (ps_zero_att_warning && transmitter->attenuation == 0) atomic_fetch_add(&warn_count, 1);
}

/* ps_menu.c */
static void store_tx_att_for_current_band(void) {
  int tx_vfo = vfo_get_tx_vfo();
  BANDSETTINGS *bs = band_get_settings(vfo[tx_vfo].band);
  if (bs != NULL) {
    bs->ps_tx_att = transmitter->attenuation;
  }
}

//
// This is called periodically while a PS calibration source
// (Two Tone or Noise) is active. If running PURESIGNAL
// with Auto Attenuate, this thread will adjust the TX-ATT
// value. This thread also
// updates the PS status. If PS is not enabled,
// this is essentially a no-op.
//
// (ps_menu.c, unchanged but for clear_fields(), which only clears the dialog)
int ps_calibration_timer(gpointer arg) {
  guint *timer = (guint *) arg;
  static int state = -1;
  static gint last_hl2_ovf_events = 0;
  static gint64 last_ovf_adjust_us = 0;
  if (!transmitter->twotone && !transmitter->noise) {
    state = -1;
    last_hl2_ovf_events = g_atomic_int_get(&hl2_adc0_ovf_events);
    last_ovf_adjust_us = 0;
    *timer = 0;
    return G_SOURCE_REMOVE;
  }
  if (state < 0) {
    //
    // Initialized PS calibration
    //
    state = 1;          // start with PS reset
    last_hl2_ovf_events = g_atomic_int_get(&hl2_adc0_ovf_events);
    last_ovf_adjust_us = 0;
  }
  if (transmitter->puresignal) {
    int tx_att_min;
    int tx_att_max;
    static int old4 = -1;
    static int count = 0;
    int info[INFO_SIZE];
    if (device == DEVICE_HERMES_LITE2) {
      tx_att_min = -29;
      tx_att_max = 31;
    } else {
      tx_att_min = 0;
      tx_att_max = 31;
    }
    tx_ps_getinfo(transmitter, info);
    //
    // newcal is set if the feedback level changed, or if it stayed
    // constant for more than 10 timer cycles. A new calibration alone
    // is not sufficient: info[4] may still describe the feedback level
    // from before the most recent TX attenuation change.
    //
    int newcal = 0;
    if (info[4] != old4 || count > 10) {
      old4 = info[4];
      newcal = 1;
      count = 0;
    } else {
      count++;
    }
    if (!transmitter->auto_on) {
      // Force an immediate feedback evaluation when Auto Attenuate is
      // enabled again, without coupling the PS reset/resume state machine
      // to Auto Attenuate.
      old4 = -1;
      count = 0;
    }
    // P1 reports hardware ADC0 clipping independently of the WDSP feedback
    // level.  Use an atomic event counter: the panadapter clears adc0_p_ovl
    // and must not be able to consume an overflow before PS sees it.
    // Do not use the RX-only RxPGA auto-gain path here.
    if (device == DEVICE_HERMES_LITE2) {
      gint events = g_atomic_int_get(&hl2_adc0_ovf_events);
      if (state == 0 && transmitter->auto_on && radio_is_transmitting()
          && events != last_hl2_ovf_events) {
        gint64 now_us = g_get_monotonic_time();
        if (last_ovf_adjust_us == 0 || now_us - last_ovf_adjust_us >= 500000) {
          int new_att = transmitter->attenuation + 6;
          if (new_att > tx_att_max) { new_att = tx_att_max; }
          last_hl2_ovf_events = events;
          last_ovf_adjust_us = now_us;
          if (new_att != transmitter->attenuation) {
            transmitter->attenuation = new_att;
            store_tx_att_for_current_band();
            schedule_high_priority();
            schedule_transmit_specific();
            // Discard the pre-adjustment WDSP feedback level and recalibrate.
            old4 = -1;
            count = 0;
            state = 1;
            return G_SOURCE_CONTINUE;
          }
        }
      } else if (!transmitter->auto_on || !radio_is_transmitting()) {
        last_hl2_ovf_events = events;
        last_ovf_adjust_us = 0;
      }
    }
    switch (state) {
    case 0:
      //
      // PureSignal calibration must run independently of Auto Attenuate.
      // Auto Attenuate only changes the hardware attenuation after a new
      // calibration result; it must not gate the PS reset/resume sequence.
      //
      if (transmitter->auto_on && newcal
          && ((info[4] > 155 && transmitter->attenuation < tx_att_max)
              || (info[4] < 150 && transmitter->attenuation > tx_att_min))) {
        int delta_att;
        int new_att;
        if (info[4] > 275) {
          // If signal is very strong, increase attenuation by 15 dB
          // Note the value is limited to about 300-350 due to ADC clipping/IQ overflow,
          // so the feedback level might be much stronger than indicated here
          delta_att = 15;
          if (transmitter->attenuation < -15) { delta_att += 15; }
        } else if (info[4] < 25) {
          // If signal is very weak, decrease attenuation by 15 dB
          delta_att = -15;
        } else {
          // calculate new delta, this mostly succeeds in one step
          delta_att = (int) lround(20.0 * log10((double) info[4] / 152.293));
        }
        new_att = transmitter->attenuation + delta_att;
        // keep new value of attenuation in allowed range
        if (new_att < tx_att_min) { new_att = tx_att_min; }
        if (new_att > tx_att_max) { new_att = tx_att_max; }
        // A PS reset is only necessary if the attenuation has actually changed.
        // First update the hardware attenuation, then reset and resume PS in the
        // following timer steps.
        if (transmitter->attenuation != new_att) {
          transmitter->attenuation = new_att;
          store_tx_att_for_current_band();
          schedule_high_priority();
          schedule_transmit_specific();
          state = 1;
        }
      }
      break;
    case 1:
      // Perform a PS reset and proceed to a PS restart.
      state = 2;
      tx_ps_reset(transmitter);
      break;
    case 2:
      // Perform a PS restart and proceed to the calibration loop.
      state = 0;
      tx_ps_resume(transmitter);
      break;
    }
  }
  return G_SOURCE_CONTINUE;
}

/* ps_menu.c: PS reset, wait 100 msec, PS resume */
static void ps_off_on(void) {
  if (transmitter->puresignal) {
    tx_ps_reset(transmitter);
    usleep(100000);
    tx_ps_resume(transmitter);
  }
}

/* ---- start (radio_core.c via transmit.c) ---- */

/* radio.c start_radio(): the feedback receivers, made with the transmitter, at the receivers' rate.
 * PS itself waits for the stream (droid_ps_started): tx_ps_onoff() restarts it. */
void droid_ps_create(int pixels) {
  tx_ps_set_sample_rate(transmitter, receiver[0]->sample_rate);
  receiver[PS_TX_FEEDBACK] = rx_create_pure_signal_receiver(PS_TX_FEEDBACK, receiver[0]->sample_rate, pixels,
                                                            transmitter->fps);
  receiver[PS_RX_FEEDBACK] = rx_create_pure_signal_receiver(PS_RX_FEEDBACK, receiver[0]->sample_rate, pixels,
                                                            transmitter->fps);
  GetPropI0("radio.ps_zero_att_warning", ps_zero_att_warning);
  ps_wanted = transmitter->puresignal;   /* transmitter.c restored it from the props file */
  transmitter->puresignal = 0;           /* old_protocol.c: only changed with the protocol stopped */
  transmitter->twotone = 0;
  transmitter->noise = 0;
}

void droid_ps_save(void) {
  SetPropI0("radio.ps_zero_att_warning", ps_zero_att_warning);
}

/* radio.c's radio_change_sample_rate(): the feedback receiver and PS follow the receivers' rate */
void droid_ps_sample_rate(int rate) {
  if (receiver[PS_RX_FEEDBACK]) rx_change_sample_rate(receiver[PS_RX_FEEDBACK], rate);
  tx_ps_set_sample_rate(transmitter, rate);
}

/* The loop thread, once the stream runs after a start: radio.c switches PS on again if it was on. */
void droid_ps_started(void) {
  if (!can_transmit || transmitter == NULL) return;
  if (ps_wanted && !transmitter->puresignal) {
    tx_ps_onoff(transmitter, 1);
    ps_warn_if_zero_att();
  }
  ps_wanted = 0;
}

/* ---- the calibration sources key the radio: droidHPSDR's checks ---- */

/* transmit.c's reasons a press can't transmit (the same as the screen PTT); 0 = may transmit */
int droid_tx_refuse_reason(void);

static void gen_set(int field, int state) {
  if (state) {
    int why = droid_tx_refuse_reason();
    if (why) {
      atomic_fetch_add(&gen_refused, 1);
      return;
    }
    if (tune) {   /* the tone replaces TUNE's carrier; with the Pico the tuner start ends too (code review) */
      radio_tune_update(0);
      if (device == DEVICE_HERMES_LITE2 && hl2_pico_is_present()) hl2_iob_set_antenna_tuner(0);
    }
    gen_since_us = g_get_monotonic_time();
    gen_keyed = 0;
  }
  if (field == TXP_TWOTONE) tx_set_twotone(transmitter, state);
  else tx_set_noise(transmitter, state);
}

/* Any way back to receive, and TUNE, also end Two Tone / Noise (transmit.c's off-now, TUNE and its
 * poll). Only the generator: tx_set_twotone(0) / tx_set_noise(0) also queue a MOX off, which would
 * cancel a TUNE that takes over, or wait in the queue for the next start after a stop (code review).
 * The callers end the transmission themselves; ps_calibration_timer stops with both flags clear. */
void droid_ps_generators_off(void) {
  if (transmitter == NULL || !can_transmit) return;
  if (transmitter->twotone) {
    SetTXAPostGenRun(transmitter->id, 0);
    transmitter->twotone = 0;
  }
  if (transmitter->noise) {
    SetTXAPreGenRun(transmitter->id, 0);
    transmitter->noise = 0;
  }
}

/* The loop thread's poll: a generator that never keyed (refused upstream) or whose
 * transmission ended (PTT release, MOX off, the timeout) is switched off. */
void droid_ps_poll(void) {
  if (transmitter == NULL || !can_transmit) return;
  if (!transmitter->twotone && !transmitter->noise) return;
  if (radio_is_transmitting()) {
    gen_keyed = 1;
    return;
  }
  if (gen_keyed) {
    droid_ps_generators_off();
  } else if (g_get_monotonic_time() - gen_since_us > 1500000) {
    /* never keyed: refused on the way (ext_mox_update, the band check): say so (code review) */
    droid_ps_generators_off();
    atomic_fetch_add(&gen_refused, 1);
  }
}

/* ---- settings (on the loop thread) ---- */

int droid_ps_apply(int field, double v) {
  if (field < TXP_FIRST || field >= TXP_END) return 0;
  if (transmitter == NULL || !can_transmit) return 1;
  TRANSMITTER *tx = transmitter;
  int on = v != 0;
  switch (field) {
  case TXP_ENABLE:   /* enable_cb */
    if (on != tx->puresignal) {
      if (on) ps_warn_if_zero_att();
      tx_ps_onoff(tx, on);
    }
    break;
  case TXP_RESET:    /* reset_cb, the "OFF" button */
    if (tx->puresignal) tx_ps_reset(tx);
    break;
  case TXP_RESUME:   /* resume_cb, "Restart" */
    // Set the attenuation to zero if auto-adjusting and resuming.
    // A very high attenuation value here could lead to no PS calculation
    // done in WDSP, and hence no attenuation adjustment.
    // If not auto-adjusting, do not change attenuation value.
    if (tx->puresignal) {
      if ((tx->twotone || tx->noise) && tx->auto_on) {
        tx->attenuation = 0;
        store_tx_att_for_current_band();
        schedule_high_priority();
        schedule_transmit_specific();
      }
      tx_ps_resume(tx);
    }
    break;
  case TXP_TWOTONE:  /* twotone_cb: switching one on switches the other off (transmitter.c) */
  case TXP_NOISE:
    gen_set(field, on);
    break;
  case TXP_NOISE_LEVEL:
    tx_set_noise_level(tx, (int)lround(v));
    break;
  case TXP_AUTO:     /* auto_cb */
    tx->auto_on = on;
    break;
  case TXP_ATT: {    /* att_spin_cb: the calibration loop sets it while Auto Attenuate is on */
    if (tx->auto_on) break;
    int a = (int)lround(v);
    int lo = device == DEVICE_HERMES_LITE2 ? -29 : 0;
    tx->attenuation = a < lo ? lo : a > 31 ? 31 : a;
    store_tx_att_for_current_band();
    schedule_transmit_specific();
    schedule_high_priority();
    break;
  }
  case TXP_MON:      /* feedback_cb */
    tx->feedback = on;
    break;
  case TXP_ONESHOT:  /* oneshot_cb */
    tx->ps_oneshot = on;
    tx_ps_setparams(tx);
    ps_off_on();
    break;
  case TXP_TOLERANCE: { /* tolerance_mode_cb */
    int m = (int)lround(v);
    if (m < 0 || m > 2 || m == tx->ps_tolerance_mode) break;
    tx->ps_tolerance_mode = m;
    tx_ps_setparams(tx);
    ps_off_on();
    break;
  }
  case TXP_DELAY:    /* ps_tx_delay_cb: applied without a PS reset */
    if (v < -10000 || v > 10000 || v == tx->ps_ampdelay) break;
    tx->ps_ampdelay = v;
    tx_ps_setparams(tx);
    break;
  case TXP_SETPK:    /* setpk_cb */
    if (v > 0.01 && v < 1.01 && fabs(v - tx->ps_setpk) > 0.0005) {   /* the UI steps by 0.001 (code review) */
      tx->ps_setpk = v;
      tx_ps_setparams(tx);
      ps_off_on();
    }
    break;
  case TXP_ZERO_WARN:
    ps_zero_att_warning = on;
    break;
  default:
    break;
  }
  return 1;
}

/* ---- state for the UI (any thread; WDSP's PS calls lock themselves) ---- */

void droid_ps_get(double *o) {
  if (transmitter == NULL || !can_transmit) return;
  TRANSMITTER *tx = transmitter;
  o[TXP_ENABLE] = tx->puresignal;
  o[TXP_TWOTONE] = tx->twotone;
  o[TXP_NOISE] = tx->noise;
  o[TXP_NOISE_LEVEL] = tx->noise_level_db;
  o[TXP_AUTO] = tx->auto_on;
  o[TXP_ATT] = tx->attenuation;
  o[TXP_MON] = tx->feedback;
  o[TXP_ONESHOT] = tx->ps_oneshot;
  o[TXP_TOLERANCE] = tx->ps_tolerance_mode;
  o[TXP_DELAY] = tx->ps_ampdelay;
  o[TXP_SETPK] = tx->ps_setpk;
  o[TXP_ZERO_WARN] = ps_zero_att_warning;
  o[TXPS_WARN] = atomic_load(&warn_count);
  o[TXPS_GEN_REFUSED] = atomic_load(&gen_refused);
  o[TXPS_BAND] = vfo[vfo_get_tx_vfo()].band;
  if (tx->puresignal) {
    int info[INFO_SIZE];
    tx_ps_getinfo(tx, info);
    o[TXPS_FEEDBACK] = info[4];
    o[TXPS_COR_COUNT] = info[5];
    o[TXPS_SLN_CHECK] = info[6];
    o[TXPS_DG_COUNT] = info[13];
    o[TXPS_CORRECTING] = info[14];
    o[TXPS_STATE] = info[15];
    o[TXPS_GETPK] = tx_ps_getmx(tx);
  }
}

/* WDSP 2.x keeps 16 buckets of 256 samples (calcc.c) and DISP_PTS = 512 correction points:
 * ps_menu.c's AMPVIEW_MAX_SAMPLES / AMPVIEW_CORRECTION_POINTS, the same sizes here. */
/* AmpView (ps_menu.c ampview_update_cb): WDSP's display data, 10 times a second.
 * out: [0] samples, [1] correction points, [2] phase reference (deg), then the arrays
 * at PSD_X .. PSD_YA_COR (droid_app.h). Returns the number of floats written. */
int droid_ps_disp(float *out, int max) {
  static double x[PSD_SAMPLES], ym[PSD_SAMPLES], yc[PSD_SAMPLES], ys[PSD_SAMPLES];
  static double xm[PSD_POINTS], ymc[PSD_POINTS], xa[PSD_POINTS], yac[PSD_POINTS];
  static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
  if (transmitter == NULL || !can_transmit || max < PSD_COUNT) return 0;
  pthread_mutex_lock(&lock);
  int ns = 0, cp = 0;
  double ref = 0;
  tx_ps_getdisp(transmitter, x, ym, yc, ys, xm, ymc, xa, yac, &ns, &cp, &ref);
  if (ns < 0 || ns > PSD_SAMPLES) ns = 0;
  if (cp < 0 || cp > PSD_POINTS) cp = 0;
  out[0] = ns;
  out[1] = cp;
  out[2] = ref;
  for (int i = 0; i < ns; i++) {
    out[PSD_X + i] = x[i];
    out[PSD_YM + i] = ym[i];
    out[PSD_YC + i] = yc[i];
    out[PSD_YS + i] = ys[i];
  }
  for (int i = 0; i < cp; i++) {
    out[PSD_XM_COR + i] = xm[i];
    out[PSD_YM_COR + i] = ymc[i];
    out[PSD_XA_COR + i] = xa[i];
    out[PSD_YA_COR + i] = yac[i];
  }
  pthread_mutex_unlock(&lock);
  return PSD_COUNT;
}
