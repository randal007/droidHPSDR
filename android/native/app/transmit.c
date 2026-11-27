/*
 * transmit.c — M7, transmit: what deskHPSDR's radio.c (and a few lines of
 * ext.c, sliders.c, actions.c, toolset.c) does around upstream's
 * transmitter.c, which droidHPSDR compiles unchanged.
 *
 * radio.c itself is not compiled (it is the GTK main window), so its
 * transmit functions are copied here, unchanged except that the lines
 * that move GTK panels are left out: rxtx(), radio_set_mox(),
 * radio_set_tune(), radio_set_vox(), the drive level for the HL2, the TX
 * inhibit sources. Where droidHPSDR adds something it says so.
 *
 * droidHPSDR's own safety (docs/M7-INVENTORY.md, Randal's answers):
 *   - "Transmit enabled" (the TX page's safety switch, off until switched
 *     on) and the demo band work as one more TX inhibit source, so every
 *     upstream way into transmit (MOX, TUNE, VOX, the HL2+ mic PTT) is
 *     refused by upstream's own radio_get_tx_inhibit() check;
 *   - the screen PTT is held: the UI repeats it while the finger is down,
 *     and the radio goes back to receive when the repeats stop (a lost
 *     "release" can't leave it transmitting);
 *   - a TX timeout (default 2 min) ends any transmission;
 *   - VOX is off at every start;
 *   - CW (step 3, cw.c): the radio's keyer is armed only while transmitting
 *     is allowed, and the timeout disarms it;
 *   - the radio leaves transmit before the stream stops (radio_core.c).
 *   - VOX (step 2) goes through upstream's vox.c and the same checks, and
 *     is refused out of band like the screen PTT.
 *
 * Step 2, the TX audio chain: the lines of tx_menu.c, equalizer_menu.c and
 * vox_menu.c that store a setting (per mode where deskHPSDR keeps it per
 * mode: mode_settings[] + copy_mode_settings()) and hand it to WDSP
 * (tx_set_compressor / tx_set_dexp / tx_set_equalizer), without the dialogs.
 *
 * Copied from deskHPSDR: radio.c, ext.c, sliders.c set_drive(), actions.c
 * (TUNE_T), toolset.c sort_cfc() / sort_tx_eq(), tx_menu.c spinbtn_cb() /
 * chkbtn_cb(), equalizer_menu.c enable_cb() / freq_changed_cb() /
 * gain_changed_cb(), vox_menu.c: (C) 2015 John Melton
 * G0ORX/N6LYT, 2024-2026 Heiko Amft DL1BZ (Project deskHPSDR),
 * GPL-3.0-or-later.
 *
 * GPL v3 or later, like the rest of droidHPSDR.
 */
#include <ctype.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include <wdsp.h>

#include "audio.h"
#include "band.h"
#include "channel.h"
#include "ext.h"
#include "main.h"
#include "message.h"
#include "mode.h"
#include "new_protocol.h"
#include "old_protocol.h"
#include "property.h"
#include "rtty_engine.h"
#include "radio.h"
#include "receiver.h"
#include "sliders.h"
#include "tci.h"
#include "toolset.h"
#include "transmitter.h"
#include "tx_off.h"
#include "vfo.h"
#include "vox.h"

#include "droid_app.h"

/* ---- radio.c globals the transmit side uses (radio_state.c has the rest) ---- */
_Atomic int mox = 0;
_Atomic int vox = 0;
int pre_mox = 0;
int memory_tune = 0;
int full_tune = 0;
int OCfull_tune_time = 2800; // ms
int high_swr_seen = 0;
int pa_power = PA_5W;        /* droidHPSDR: upstream PA_1W; the HL2's own meter is 5 W (M7 picture M7) */
const int pa_power_list[] = {1, 5, 10, 15, 20, 25, 30, 50, 75, 100, 125, 200, 500, 1000};
double pa_trim[11];
double drive_max = 100.0;
double drive_digi_max = 100.0; // maximum drive in DIGU/DIGL
int tx_filter_low = 100;
int tx_filter_high = 2900;
int use_tx_audiochain = 1;
double vox_threshold = 0.001;
double vox_hang = 250.0;
int cw_ramp_width = 9;                 // default value (in ms)
int cw_breakin = 1;                    // 0=disabled 1=enabled
const int tx_dialog_height = 400;
int have_radioberry1 = 0;
int have_radioberry2 = 0;
int have_radioberry3 = 0;
static int pre_tune_mode;
static int pre_tune_cw_internal;
/* main.c: the GTK window (never created here) */
int full_screen;
int use_wayland;
GtkWidget *top_window = NULL;
GtkWidget *topgrid;
/* actions.c, voice_keyer.c: audio capture and the voice keyer are not built yet */
int is_cap = 0;
int is_vk = 0;

/* ---- droidHPSDR's transmit state ---- */
static atomic_int tx_enabled;          /* the safety switch (TX page) */
static atomic_int tx_timeout_s = 120;  /* 0: off */
static atomic_llong ptt_seen_us;       /* last screen PTT repeat */
static atomic_int ptt_screen;          /* the screen PTT holds MOX */
static atomic_int timeouts;            /* times the timeout ended a transmission */
static atomic_int refused;             /* last refusal reason, for the UI (TXR_*) */
static long long tx_since_us;          /* when this transmission started (loop thread) */
static atomic_int tx_since_s;          /* seconds on the air, for the UI */
static atomic_int timed_out;           /* the timeout ended a screen PTT: no TX until it is released */
static atomic_int vox_refusals;        /* VOX refused out of band, for the UI */
static atomic_int pa_file_ok;          /* last PA calibration Save (1, -1 failed) / Load (2, -2 failed) */

static long long now_us(void) { return g_get_monotonic_time(); }

/* The demo band and the safety switch hold the radio out of transmit, as
 * one more inhibit source next to upstream's hardware and serial ones. */
static int droid_blocked(void) {
  return !atomic_load(&tx_enabled) || atomic_load(&droid_playback_on) || !can_transmit;
}

/* ---- radio.c: TX inhibit (hardware input, serial input) ---- */

#define TX_INHIBIT_SOURCE_HARDWARE 0x01
#define TX_INHIBIT_SOURCE_SERIAL   0x02

static gint tx_inhibit_sources = 0;

int radio_get_tx_inhibit(void) {
  return g_atomic_int_get(&tx_inhibit_sources) != 0 || droid_blocked();   /* droidHPSDR: || droid_blocked() */
}

static void radio_set_tx_inhibit_source(gint source, int state) {
  gint previous;
  if (state) {
    previous = __atomic_fetch_or(&tx_inhibit_sources, source, __ATOMIC_SEQ_CST);
    if (previous == 0) {
      g_idle_add(ext_mox_update_immediate, GINT_TO_POINTER(0));
    }
  } else {
    __atomic_fetch_and(&tx_inhibit_sources, ~source, __ATOMIC_SEQ_CST);
  }
}

void radio_set_hardware_tx_inhibit(int state) {
  radio_set_tx_inhibit_source(TX_INHIBIT_SOURCE_HARDWARE, state);
}

void radio_set_serial_tx_inhibit(int state) {
  radio_set_tx_inhibit_source(TX_INHIBIT_SOURCE_SERIAL, state);
}

/* ---- radio.c: RX/TX switching. The GTK panel moves are left out:
 *      droidHPSDR's screen follows radio_is_transmitting() itself. ---- */

static void rxtx(int state) {
  int i;
  if (!can_transmit) {
    t_print("WARNING: rxtx called but no transmitter!");
    return;
  }
  //
  // Abort any running Capture, Transmit, Replay
  //
  switch (capture_state) {
  case CAP_RECORDING:
    capture_state = CAP_RECORD_DONE;
    schedule_action(CAPTURE, PRESSED, 0);
    break;
  case CAP_XMIT:
    capture_state = CAP_XMIT_DONE;
    schedule_action(capture_trigger_action, PRESSED, 0);
    break;
  case CAP_REPLAY:
    capture_state = CAP_REPLAY_DONE;
    schedule_action(REPLAY, PRESSED, 0);
    break;
  }
  pre_mox = state && !duplex;
  if (state) {
    // switch to tx
    RECEIVER *rx_feedback = receiver[PS_RX_FEEDBACK];
    RECEIVER *tx_feedback = receiver[PS_TX_FEEDBACK];
    if (rx_feedback) { rx_feedback->samples = 0; }
    if (tx_feedback) { tx_feedback->samples = 0; }
    if (!duplex) {
      // Start all WDSP receiver slew-downs first so they can run in parallel.
      // Do not proceed until every receiver has completed its flush.
      for (i = 0; i < receivers; i++) {
        rx_begin_off(receiver[i]);
      }
      for (i = 0; i < receivers; i++) {
        rx_wait_off(receiver[i]);
      }
      for (i = 0; i < receivers; i++) {
        receiver[i]->displaying = 0;
        rx_set_displaying(receiver[i]);
      }
    }
    if (transmitter->puresignal) {
      tx_ps_mox(transmitter, 1);
    }
    tx_on(transmitter);
    transmitter->displaying = 1;
    tx_set_displaying(transmitter);
  } else {
    // switch to rx
    if (transmitter->puresignal) {
      tx_ps_mox(transmitter, 0);
    }
    tx_off(transmitter);
    transmitter->displaying = 0;
    tx_set_displaying(transmitter);
    if (!duplex) {
      //
      // Set parameters for the "silence first RXIQ samples after TX/RX transition" feature
      // the default is "no silence", that is, fastest turnaround.
      // Seeing "tails" of the own TX signal (from crosstalk at the T/R relay) has been observed
      // for RedPitayas (the identify themself as STEMlab or HERMES) and HermesLite2 devices,
      // we also include the original HermesLite in this list (which can be enlarged if necessary).
      //
      int do_silence = 0;
      if (device == DEVICE_HERMES_LITE2 || device == DEVICE_HERMES_LITE ||
          device == DEVICE_HERMES || device == DEVICE_STEMLAB || device == DEVICE_STEMLAB_Z20) {
        //
        // These systems get a significant "tail" of the RX feedback signal into the RX after TX/RX,
        // leading to AGC pumping. The problem is most severe if there is a carrier until the end of
        // the TX phase (TUNE, AM, FM), the problem is virtually non-existent for CW, and of medium
        // importance in SSB. On the other hand, one wants a very fast turnaround in CW.
        // So there is no "muting" for CW, 31 msec "muting" for TUNE/AM/FM, and 16 msec for other modes.
        //
        // Note that for doing "TwoTone" the silence is built into tx_set_twotone().
        //
        switch (vfo_get_tx_mode()) {
        case modeCWU:
        case modeCWL:
          do_silence = 0; // no "silence"
          break;
        case modeAM:
        case modeFMN:
          do_silence = 5; // leads to 31 ms "silence"
          break;
        default:
          do_silence = 6; // leads to 16 ms "silence"
          break;
        }
        if (tune) { do_silence = 5; } // 31 ms "silence" for TUNEing in any mode
      }
      for (i = 0; i < receivers; i++) {
#ifdef AUDIO_RINGBUFFER
        audio_reprime_output(receiver[i]);
#endif
        rx_on(receiver[i]);
        receiver[i]->displaying = 1;
        rx_set_displaying(receiver[i]);
        //
        // There might be some left-over samples in the RX buffer that were filled in
        // *before* going TX, delete them
        //
        receiver[i]->samples = 0;
        if (do_silence) {
          receiver[i]->txrxmax = receiver[i]->sample_rate >> do_silence;
        } else {
          receiver[i]->txrxmax = 0;
        }
        receiver[i]->txrxcount = 0;
      }
    }
  }
}

void radio_mox_update(int state) {
  if (!can_transmit) { return; }
  if (state && !TransmitAllowed()) {
    state = 0;
    tx_set_out_of_band(transmitter);
  }
  radio_set_mox(state);
  g_idle_add(ext_vfo_update, NULL);
}

void radio_mox_update_immediate(int state) {
  if (!can_transmit) { return; }
  if (state && !TransmitAllowed()) {
    state = 0;
    tx_set_out_of_band(transmitter);
  }
  radio_set_mox_immediate(state);
  g_idle_add(ext_vfo_update, NULL);
}

void radio_tune_update(int state) {
  if (!can_transmit) { return; }
  // Switching to TUNE is a mode transition, not a speech-tail operation.
  radio_set_mox_immediate(0);  // This will also cancel VOX and TUNE
  if (state && !TransmitAllowed()) {
    state = 0;
    tx_set_out_of_band(transmitter);
  }
  radio_set_tune(state);
  g_idle_add(ext_vfo_update, NULL);
}

static int radio_mox_uses_speech_audio(void) {
  int txmode = vfo_get_tx_mode();
  return txmode != modeCWL && txmode != modeCWU && !tune &&
         !transmitter->twotone && !transmitter->noise;
}

static void radio_set_mox_now(int state) {
  int was_tune = tune;
  if (!can_transmit) { return; }
  if (state && radio_get_tx_inhibit()) { return; }
  //
  // - setting MOX (no matter in which direction) stops TUNEing
  // - setting MOX (no matter in which direction) ends a pending VOX
  // - activating MOX while VOX is pending continues transmission
  // - deactivating MOX while VOX is pending makes a TX/RX transition
  //
  if (tune) {
    radio_set_tune(0);
  }
  vox_cancel();  // remove VOX hang/drain
  //
  // If MOX is activated while VOX is already pending,
  // then switch from VOX to MOX mode but no RX/TX
  // transition is necessary.
  //
  if (state != radio_is_transmitting()) {
    rxtx(state);
  }
  mox  = state;
  tune = 0;
  vox  = 0;
  update_slider_mic_gain_btn();
  if (!tci_is_applying() && (!was_tune || state)) {
    tci_mox_changed(state);
  }
  switch (protocol) {
  case NEW_PROTOCOL:
    schedule_high_priority();
    schedule_receive_specific();
    break;
  default:
    break;
  }
}

static void radio_graceful_mox_off_complete(void) {
  /*
   * Hardware PTT is sampled in the protocol thread before its main-loop
   * update runs. If the footswitch was pressed again at the very end of the
   * guard interval, keep TX active instead of producing a short RX pulse.
   * (droidHPSDR: no serial CTS footswitch.)
   */
  radio_set_mox_now(radio_ptt ? 1 : 0);
  g_idle_add(ext_vfo_update, NULL);
}

void radio_set_mox(int state) {
  if (!can_transmit) { return; }
  if (state && radio_get_tx_inhibit()) { return; }
  if (!state && atomic_load_explicit(&CAT_rtty_is_active, memory_order_acquire)) {
    /* Manual/foreign MOX OFF aborts native RTTY immediately. */
    rtty_engine_abort();
    return;
  }
  if (state) {
    // PTT/MOX ON while an OFF operation is pending aborts that operation.
    // Since mox is still physically ON, radio_set_mox_now() performs no
    // RX/TX transition and transmission continues without an RX pulse.
    tx_off_cancel_target(TX_OFF_TARGET_MOX);
    radio_set_mox_now(1);
    return;
  }
  if (mox && radio_mox_uses_speech_audio()) {
    vox_cancel();
    if (tx_off_request(TX_OFF_TARGET_MOX,
                       radio_graceful_mox_off_complete)) {
      // radio_get_mox() now reports the requested logical OFF state, while
      // radio_is_transmitting() remains true until the fence/guard completes.
      update_slider_mic_gain_btn();
      return;
    }
  }
  // CW, TUNE, inactive MOX and unsupported/conflicting states stop directly.
  radio_set_mox_now(0);
}

void radio_set_mox_immediate(int state) {
  tx_off_cancel();
  radio_set_mox_now(state);
}

int radio_get_mox(void) {
  return mox && !tx_off_pending_target(TX_OFF_TARGET_MOX);
}

void radio_set_vox(int state) {
  if (!can_transmit) { return; }
  if (mox || tune) { return; }
  if (state && radio_get_tx_inhibit()) { return; }
  if (state) {
    tx_off_cancel_target(TX_OFF_TARGET_VOX);
  }
  if (vox != state) {
    rxtx(state);
  }
  vox = state;
  schedule_high_priority();
  schedule_receive_specific();
}

void radio_set_tune(int state) {
  t_print("%s: mox=%d vox=%d tune=%d NewState=%d\n", __func__, mox, vox, tune, state);
  if (!can_transmit) { return; }
  if (state && radio_get_tx_inhibit()) { return; }
  int tune_changed = (tune != state);
  // if state==tune, this function is a no-op
  if (tune != state) {
    tx_off_cancel();
    vox_cancel();
    if (vox || mox) {
      rxtx(0);
      vox = 0;
      mox = 0;
    }
    if (state) {
      //
      // A clean solution is to disable compressor/CFC temporarily
      // while TUNEing (DL1YCF; see radio.c).
      //
      int _save_lev = transmitter->lev_enable;
      int _save_phrot = transmitter->phrot_enable;
      int _save_cfc  = transmitter->cfc;
      int _save_cfc_eq = transmitter->cfc_eq;
      int _save_cmpr = transmitter->compressor;
      transmitter->lev_enable = 0;
      transmitter->phrot_enable = 0;
      transmitter->cfc = 0;
      transmitter->cfc_eq = 0;
      transmitter->compressor = 0;
      // set off if TUNEing, but restore later
      tx_set_compressor(transmitter);
      //
      // Keep previous state in transmitter data, so we just need
      // call tx_set_compressor when TUNEing ends.
      //
      transmitter->lev_enable = _save_lev;
      transmitter->phrot_enable = _save_phrot;
      transmitter->cfc = _save_cfc;
      transmitter->cfc_eq = _save_cfc_eq;
      transmitter->compressor = _save_cmpr;
      if (transmitter->puresignal && ! transmitter->ps_oneshot) {
        //
        // So before start tuning: Reset PS engine (DL1YCF; see radio.c)
        //
        tx_ps_reset(transmitter);
        usleep(50000);
      }
      if (full_tune) {
        if (OCfull_tune_time != 0) {
          struct timeval te;
          gettimeofday(&te, NULL);
          tune_timeout = (te.tv_sec * 1000LL + te.tv_usec / 1000) + (long long) OCfull_tune_time;
        }
      }
      if (memory_tune) {
        if (OCmemory_tune_time != 0) {
          struct timeval te;
          gettimeofday(&te, NULL);
          tune_timeout = (te.tv_sec * 1000LL + te.tv_usec / 1000) + (long long) OCmemory_tune_time;
        }
      }
    }
    schedule_high_priority();
    if (state) {
      if (!duplex) {
        // Start all WDSP receiver slew-downs first, then wait for all of them.
        for (int i = 0; i < receivers; i++) {
          rx_begin_off(receiver[i]);
        }
        for (int i = 0; i < receivers; i++) {
          rx_wait_off(receiver[i]);
        }
        for (int i = 0; i < receivers; i++) {
          receiver[i]->displaying = 0;
          rx_set_displaying(receiver[i]);
          schedule_high_priority();
        }
      }
      int txmode = vfo_get_tx_mode();
      pre_tune_mode = txmode;
      pre_tune_cw_internal = cw_keyer_internal;
      double freq = 0.0;
      tx_set_singletone(transmitter, 1, freq);
      switch (txmode) {
      case modeCWL:
        cw_keyer_internal = 0;
        tx_set_mode(transmitter, modeLSB);
        break;
      case modeCWU:
        cw_keyer_internal = 0;
        tx_set_mode(transmitter, modeUSB);
        break;
      }
      tune = state;
      radio_calc_drive_level();
      rxtx(state);
    } else {
      tx_set_singletone(transmitter, 0, 0.0);
      rxtx(state);
      switch (pre_tune_mode) {
      case modeCWL:
      case modeCWU:
        tx_set_mode(transmitter, pre_tune_mode);
        cw_keyer_internal = pre_tune_cw_internal;
        break;
      }
      if (transmitter->puresignal && !transmitter->ps_oneshot) {
        tx_ps_resume(transmitter);
      }
      // restore settings we switched off earlier
      tx_set_compressor(transmitter);
      tune = state;
      radio_calc_drive_level();
      transmitter->is_tuned = 1;
    }
  }
  if (tune_changed && !tci_is_applying()) {
    tci_tune_changed(state);
    tci_mox_changed(state);
  }
  schedule_high_priority();
  schedule_transmit_specific();
  schedule_receive_specific();
}

int radio_get_tune(void) {
  return tune;
}

int radio_is_transmitting(void) {
  return mox | vox | tune;
}

double radio_get_drive(void) {
  if (can_transmit) {
    return transmitter->drive;
  } else {
    return 0.0;
  }
}

int radio_get_drive_as_int(void) {
  if (can_transmit) {
    return transmitter->drive;
  } else {
    return 0;
  }
}

static int calcLevel(double d) {
  int level = 0;
  int v = vfo_get_tx_vfo();
  const BAND *band = band_get_band(vfo[v].band);
  double target_dbm = 10.0 * log10(d * 1000.0);
  double gbb = band->pa_calibration;
  target_dbm -= gbb;
  double target_volts = sqrt(pow(10, target_dbm * 0.1) * 0.05);
  double volts = fmin((target_volts / 0.8), 1.0);   /* radio.c's min() macro */
  double actual_volts = volts * (1.0 / 0.98);
  if (actual_volts < 0.0) {
    actual_volts = 0.0;
  } else if (actual_volts > 1.0) {
    actual_volts = 1.0;
  }
  level = (int)(actual_volts * 255.0);
  return level;
}

void radio_calc_drive_level(void) {
  int level;
  if (!can_transmit) { return; }
  if (tune && !transmitter->tune_use_drive) {
    level = calcLevel(transmitter->tune_drive);
  } else {
    level = calcLevel(transmitter->drive);
  }
  //
  // For most of the radios, just copy the "level" and switch off scaling
  //
  transmitter->do_scale = 0;
  transmitter->drive_level = level;
  //
  // The HermesLite-II needs a combination of hardware attenuation and TX IQ scaling.
  // The inverse of the scale factor is needed to reverse the scaling for the TX DAC feedback
  // samples used in the PureSignal case.
  // (droidHPSDR: only the HL2 branch of radio.c's function; Penelope/Atlas left out.)
  //
  if (device == DEVICE_HERMES_LITE2) {
    //
    // Calculate a combination of TX attenuation (values from -7.5 to 0 dB are encoded as 0, 16, 32, ..., 240)
    // and a TX IQ scaling. If level is above 107, the scale factor will be between 0.94 and 1.00, but if
    // level is smaller than 107 it may adopt any value between 0.0 and 1.0
    //
    double d = level;
    if (level > 240) {
      transmitter->drive_level = 240;                     //  0.0 dB hardware ATT
      transmitter->drive_scale = d * 0.0039215;
    } else if (level > 227) {
      transmitter->drive_level = 224;                     // -0.5 dB hardware ATT
      transmitter->drive_scale = d * 0.0041539;
    } else if (level > 214) {
      transmitter->drive_level = 208;                     // -1.0 dB hardware ATT
      transmitter->drive_scale = d * 0.0044000;
    } else if (level > 202) {
      transmitter->drive_level = 192;
      transmitter->drive_scale = d * 0.0046607;
    } else if (level > 191) {
      transmitter->drive_level = 176;
      transmitter->drive_scale = d * 0.0049369;
    } else if (level > 180) {
      transmitter->drive_level = 160;
      transmitter->drive_scale = d * 0.0052295;
    } else if (level > 170) {
      transmitter->drive_level = 144;
      transmitter->drive_scale = d * 0.0055393;
    } else if (level > 160) {
      transmitter->drive_level = 128;
      transmitter->drive_scale = d * 0.0058675;
    } else if (level > 151) {
      transmitter->drive_level = 112;
      transmitter->drive_scale = d * 0.0062152;
    } else if (level > 143) {
      transmitter->drive_level = 96;
      transmitter->drive_scale = d * 0.0065835;
    } else if (level > 135) {
      transmitter->drive_level = 80;
      transmitter->drive_scale = d * 0.0069736;
    } else if (level > 127) {
      transmitter->drive_level = 64;
      transmitter->drive_scale = d * 0.0073868;
    } else if (level > 120) {
      transmitter->drive_level = 48;
      transmitter->drive_scale = d * 0.0078245;
    } else if (level > 113) {
      transmitter->drive_level = 32;
      transmitter->drive_scale = d * 0.0082881;
    } else if (level > 107) {
      transmitter->drive_level = 16;
      transmitter->drive_scale = d * 0.0087793;
    } else {
      transmitter->drive_level = 0;
      transmitter->drive_scale = d * 0.0092995;    // can be between 0.0 and 0.995
    }
    transmitter->drive_iscal = 0.9999 / transmitter->drive_scale;
    transmitter->do_scale = 1;
  }
  schedule_high_priority();
}

void radio_set_drive(double value) {
  t_print("%s: drive=%d\n", __func__, (int) value);
  if (!can_transmit) { return; }
  transmitter->drive = (int) value;
  switch (protocol) {
  case ORIGINAL_PROTOCOL:
  case NEW_PROTOCOL:
    radio_calc_drive_level();
    break;
  }
}

void radio_tx_vfo_changed(void) {
  //
  // When changing the active receiver or changing the split status,
  // the VFO that controls the transmitter my flip between VFOA/VFOB.
  // In these cases, we have to update the TX mode,
  // and re-calculate the drive level from the band-specific PA calibration
  // values.
  //
  if (can_transmit) {
    tx_set_mode(transmitter, vfo_get_tx_mode());
    tx_set_analyzer(transmitter);
    radio_calc_drive_level();
  }
  schedule_high_priority();         // possibly update RX/TX antennas
  schedule_transmit_specific();     // possibly un-set "CW mode"
  schedule_general();               // possibly update PA disable
}

void reassign_pa_trim(void) {
  for (int j = 0; j < 11; j++) {
    pa_trim[j] = j * pa_power_list[pa_power] * 0.1;
  }
}

/* The IO board's AutoTune input (enable_auto_tune): stays off in droidHPSDR
 * until there is a reason; old_protocol.c only calls this when it is on. */
void radio_start_auto_tune(void) {}

/* ---- sliders.c set_drive(), without the slider widget ---- */
void set_drive(double value) {
  int txmode = vfo_get_tx_mode();
  if (txmode == modeDIGU || txmode == modeDIGL) {
    if (value > drive_digi_max) { value = drive_digi_max; }
  }
  radio_set_drive(value);
}

/* ---- ext.c: the protocol thread's requests, run on the loop thread ---- */

/* cw.c: the same "no" as a PTT, for the radio's keyer */
int droid_tx_blocked(void) { return droid_blocked(); }

/* droid_tx_set (any thread, also while stopped): the switch counts from now; the queued TX_ENABLED
 * still does the rest (back to receive) on the loop thread. */
void droid_tx_enabled_now(int on) { atomic_store(&tx_enabled, on); }

/* droidHPSDR: a screen PTT ends; the radio may still hold PTT (the CW keyer sending, the HL2+ mic PTT),
 * and old_protocol.c only reports a change, so transmit then stays with the radio's PTT (code review) */
static void screen_ptt_off(void) {
  radio_mox_update(radio_ptt && !radio_get_tx_inhibit() ? 1 : 0);
}

int ext_mox_update(void *data) {
  /* the HL2+ mic PTT and the radio's CW keyer (old_protocol.c, radio_ptt) */
  int state = GPOINTER_TO_INT(data);
  if (state && radio_get_tx_inhibit()) {   /* droidHPSDR: say why */
    atomic_store(&refused, droid_blocked() ? TXR_DISABLED : TXR_INHIBIT);
    return G_SOURCE_REMOVE;
  }
  /* droidHPSDR: the screen PTT held in CW (Randal: as deskHPSDR, the paddle sends inside it) keeps
   * transmit when the radio's PTT drops after the hang time; the finger's release ends it */
  if (!state && atomic_load(&ptt_screen) && mox) return G_SOURCE_REMOVE;
  radio_mox_update(state);
  return G_SOURCE_REMOVE;
}

int ext_mox_update_immediate(void *data) {
  int state = GPOINTER_TO_INT(data);
  radio_mox_update_immediate(state);
  return G_SOURCE_REMOVE;
}

/* droidHPSDR: why a press that keys the radio (PureSignal's Two Tone / Noise) can't, as for the
 * screen PTT; 0 = it may. The reason is kept for the UI (TXS_REFUSED). */
int droid_tx_refuse_reason(void) {
  int why = TXR_NONE;
  if (atomic_load(&timed_out)) why = TXR_TIMEOUT;   /* the screen PTT the timeout stopped is still held */
  else if (radio_get_tx_inhibit()) why = droid_blocked() ? TXR_DISABLED : TXR_INHIBIT;
  else if (!TransmitAllowed() && !tx_out_of_band_allowed) why = TXR_BAND;
  atomic_store(&refused, why);
  return why;
}

int ext_set_vox(void *data) {
  /* droidHPSDR: out of band, VOX is refused like the screen PTT */
  if (GPOINTER_TO_INT(data) && !TransmitAllowed() && !tx_out_of_band_allowed) {
    atomic_store(&refused, TXR_BAND);
    atomic_fetch_add(&vox_refusals, 1);
    return G_SOURCE_REMOVE;
  }
  radio_set_vox(GPOINTER_TO_INT(data));
  return G_SOURCE_REMOVE;
}

int ext_set_duplex(void *data) { (void)data; return G_SOURCE_REMOVE; }   /* duplex: later (Randal) */

/* ---- toolset.c ---- */
void sort_cfc(TRANSMITTER *tx) {
  sort_cfc_profile(tx->cfc_freq, tx->cfc_lvl, tx->cfc_post,
                   tx->cfc_comp_weight, tx->cfc_post_weight);
}

void sort_tx_eq(TRANSMITTER *tx) {
  sort_eq_profile(tx->eq_freq, tx->eq_gain, tx->eq_weight);
}

void get_screen_size(int *width, int *height) { *width = 1280; *height = 600; }

/* ---- not built yet: Protocol 2, the RTTY engine, TCI audio, the voice keyer
 *      (step 3), slider widgets (PureSignal's timer: puresignal.c) ---- */
void new_protocol_iq_samples(int isample, int qsample) { (void)isample; (void)qsample; }
void new_protocol_cw_audio_samples(short l, short r) { (void)l; (void)r; }
uint64_t new_protocol_tx_fence_begin(void) { return 0; }
int new_protocol_tx_fence_complete(uint64_t fence) { (void)fence; return 1; }
void rtty_engine_render_iq(double *iq, int frames, int sample_rate, int txmode) {
  (void)iq; (void)frames; (void)sample_rate; (void)txmode;
}
void rtty_engine_abort(void) {}
/* tci_audio.c replaces the mic sample only while TCI TX audio plays; no TCI here: leave it alone
 * (the first version zeroed it: every mic was silent, Randal 2026-11-25) */
void tci_get_next_mic_sample(double *micsample) { (void)micsample; }
void tci_mox_changed(int state) { (void)state; }
void tci_tune_changed(int state) { (void)state; }
void voice_keyer_stop_for_ptt_takeover(void) {}
void update_slider_mic_gain_btn(void) {}
void update_slider_tune_drive_btn(void) {}

/* ---- the TX spectrum: transmitter.c's display timer hands each new
 *      analyzer row to the TX panadapter; droidHPSDR keeps the newest one
 *      for the GPU view (gl_render.c) ---- */
static pthread_mutex_t txrow_lock = PTHREAD_MUTEX_INITIALIZER;
static float txrow[DROID_ROW_MAX];
static int txrow_w;
static double txrow_fmin, txrow_hzpp;
static uint64_t txrow_seq;

void tx_panadapter_init(TRANSMITTER *tx, int width, int height) { (void)tx; (void)width; (void)height; }

void tx_panadapter_update(TRANSMITTER *tx) {
  int w = tx->pixels < DROID_ROW_MAX ? tx->pixels : DROID_ROW_MAX;
  if (w <= 0) return;
  int v = vfo_get_tx_vfo();
  /* tx_panadapter.c: the span is centred on the TX VFO (its CTUN frequency with CTUN) */
  long long f = vfo[v].ctun ? vfo[v].ctun_frequency : vfo[v].frequency;
  double span = tx_display_span_hz(tx);
  pthread_mutex_lock(&txrow_lock);
  memcpy(txrow, tx->pixel_samples, (size_t)w * sizeof(float));
  txrow_w = w;
  txrow_hzpp = span / tx->pixels;
  txrow_fmin = (double)f - span / 2;
  txrow_seq++;
  pthread_mutex_unlock(&txrow_lock);
}

unsigned long long droid_tx_rows_count(void) {
  pthread_mutex_lock(&txrow_lock);
  unsigned long long n = txrow_seq;
  pthread_mutex_unlock(&txrow_lock);
  return n;
}

int droid_tx_row(float *db, int max, double *fmin, double *hzpp, uint64_t *seq) {
  pthread_mutex_lock(&txrow_lock);
  int w = txrow_w < max ? txrow_w : max;
  if (txrow_seq != *seq) {
    memcpy(db, txrow, (size_t)w * sizeof(float));
    *fmin = txrow_fmin;
    *hzpp = txrow_hzpp;
    *seq = txrow_seq;
  } else {
    w = -1;   /* nothing new */
  }
  pthread_mutex_unlock(&txrow_lock);
  return w;
}

/* ---- droidHPSDR: start, settings, safety ---- */

/* radio_core.c, after the props file and the receivers: the transmitter as
 * radio.c's start_radio() makes it for an HL2. */
void droid_tx_create(int pixels) {
  static int made;
  if (made) return;
  made = 1;
  pa_power = PA_5W;
  pa_enabled = 1;   /* radio.c's default (radio_state.c keeps 0 for the RX-only m1-rx); the props file decides */
  GetPropI0("pa_enabled", pa_enabled);
  GetPropI0("pa_power", pa_power);
  GetPropI0("mic_boost", mic_boost);
  GetPropI0("tx_filter_low", tx_filter_low);
  GetPropI0("tx_filter_high", tx_filter_high);
  GetPropI0("tx_out_of_band", tx_out_of_band_allowed);
  GetPropI0("mute_rx_while_transmitting", mute_rx_while_transmitting);
  GetPropI0("OCfull_tune_time", OCfull_tune_time);
  GetPropI0("OCmemory_tune_time", OCmemory_tune_time);
  GetPropI0("droid_tx_timeout_s", tx_timeout_s);
  GetPropI0("enable_tx_inhibit", enable_tx_inhibit);
  GetPropF0("vox_threshold", vox_threshold);   /* radio.c's; vox_enabled is not read: VOX is off at every start */
  GetPropF0("vox_hang", vox_hang);
  int split_saved = 0;
  GetPropI0("split", split_saved);   /* radio.c restores it; applied once the transmitter exists */
  if (pa_power < PA_1W || pa_power > PA_5W + 12) pa_power = PA_5W;
  transmitter = tx_create_transmitter(CHANNEL_TX, pixels, pixels, 200);
  can_transmit = 1;
  transmitter->x = 0;
  transmitter->y = 0;
  /* droidHPSDR: no duplex (later), VOX off at every start (Randal), the radio's own mic until
   * the UI picks another. PureSignal (step 4): the feedback receivers now, PS itself once the
   * stream runs (droid_ps_started), as radio.c does. */
  duplex = 0;
  droid_ps_create(pixels);
  droid_cw_create();
  vox_enabled = 0;
  transmitter->local_microphone = 0;
  g_strlcpy(transmitter->microphone_name, "phone", sizeof transmitter->microphone_name);
  if (device == DEVICE_HERMES_LITE2) reassign_pa_trim();
  radio_calc_drive_level();
  if (split_saved) radio_set_split(1);
  droid_log("transmitter: drive %d, tune drive %d, PA %s, mic gain %.1f dB, TX filter %d..%d, timeout %d s",
            transmitter->drive, transmitter->tune_drive, pa_enabled ? "on" : "off", transmitter->mic_gain,
            transmitter->filter_low, transmitter->filter_high, atomic_load(&tx_timeout_s));
}

/* radio_core.c's save: radio.c's entries for what the TX page sets, and the transmitter's own */
void droid_tx_save(void) {
  if (transmitter == NULL || !can_transmit) return;
  droid_ps_save();
  droid_cw_save();
  SetPropI0("pa_enabled", pa_enabled);
  SetPropI0("pa_power", pa_power);
  SetPropI0("mic_boost", mic_boost);
  SetPropI0("tx_filter_low", tx_filter_low);
  SetPropI0("tx_filter_high", tx_filter_high);
  SetPropI0("tx_out_of_band", tx_out_of_band_allowed);
  SetPropI0("mute_rx_while_transmitting", mute_rx_while_transmitting);
  SetPropI0("OCfull_tune_time", OCfull_tune_time);
  SetPropI0("OCmemory_tune_time", OCmemory_tune_time);
  SetPropI0("droid_tx_timeout_s", atomic_load(&tx_timeout_s));
  SetPropI0("enable_tx_inhibit", enable_tx_inhibit);
  SetPropI0("split", split);
  SetPropF0("vox_threshold", vox_threshold);
  SetPropF0("vox_hang", vox_hang);
  tx_save_state(transmitter);
}

/* Back to receive now, whatever is transmitting (loop thread, or with the
 * loop thread stopped). */
void droid_tx_off_now(void) {
  if (!can_transmit || transmitter == NULL) return;
  atomic_store(&ptt_screen, 0);
  droid_ps_generators_off();   /* PureSignal's Two Tone / Noise end with it */
  if (tune) {
    radio_tune_update(0);
    if (device == DEVICE_HERMES_LITE2 && hl2_pico_is_present()) hl2_iob_set_antenna_tuner(0);
  }
  if (radio_is_transmitting()) radio_set_mox_immediate(0);
  vox_enabled = 0;
  droid_cw_off();   /* after TUNE ended (it restores the keyer bit): the radio's keyer off until the poll allows it */
}

/* tx_menu.c's TX filter: the global values, then the transmitter's own (unless it follows RX) */
static void apply_tx_filter(void) {
  if (transmitter == NULL) return;
  transmitter->filter_low = tx_filter_low;
  transmitter->filter_high = tx_filter_high;
  tx_set_filter(transmitter);
}

static double clampd(double v, double lo, double hi) { return v < lo ? lo : v > hi ? hi : v; }

/* ---- step 2: the TX audio chain ---- */

_Static_assert(TXA_ADDGAIN_GAIN == 103 && TXP_ENABLE == 169 && TXP_ZERO_WARN == 182 && TXPS_FEEDBACK == 183 &&
               TXPS_BAND == 192 && TXC_SPEED == 193 && TXC_REARM == 204 && TXCS_ARMED == 205 && TXCS_CW == 210 &&
               TX_COUNT == 211, "TXA_* / TXP_* / TXC_* numbering: keep in step with Tx in Transmit.kt");

static int clampi(double v, int lo, int hi) { int i = (int)lround(v); return i < lo ? lo : i > hi ? hi : i; }

/* A frequency point of the TX EQ or the CFC: in order, 10 Hz apart
 * (equalizer_menu.c freq_changed_cb(), tx_menu.c CFCFREQ). */
static double point_freq(const double *f, int k, double v) {
  double lo = k > 1 ? f[k - 1] + 10.0 : 10.0;
  double hi = k < 12 ? f[k + 1] - 10.0 : 16000.0;
  return clampd(v, lo, hi);
}

/* tx_menu.c / equalizer_menu.c / vox_menu.c: set it on the transmitter, keep
 * it for the mode where deskHPSDR does, hand it to WDSP. Returns 0 when the
 * field is not one of the chain's. */
static int tx_audio_apply(TRANSMITTER *tx, int field, double v) {
  int mode = vfo_get_tx_mode();
  struct _mode_settings *ms = &mode_settings[mode];
  int on = v != 0;
  switch (field) {
  case TXA_COMP: tx->compressor = on; ms->compressor = on; copy_mode_settings(mode); tx_set_compressor(tx); return 1;
  case TXA_COMP_LEVEL:
    tx->compressor_level = clampi(v, 0, 20); ms->compressor_level = tx->compressor_level;
    copy_mode_settings(mode); tx_set_compressor(tx); return 1;
  case TXA_LEV: tx->lev_enable = on; ms->lev_enable = on; copy_mode_settings(mode); tx_set_compressor(tx); return 1;
  case TXA_LEV_GAIN:
    tx->lev_gain = clampd(v, 0, 15); ms->lev_gain = tx->lev_gain; copy_mode_settings(mode); tx_set_compressor(tx); return 1;
  case TXA_LEV_DECAY: tx->lev_decay = clampi(v, 0, 500); tx_set_compressor(tx); return 1;
  case TXA_CESSB:
    tx->cessb_enable = on;
    if (tx->low_latency) { tx->low_latency = 0; tx_set_latency(tx); }
    tx_set_compressor(tx);
    return 1;
  case TXA_PHROT: tx->phrot_enable = on; ms->phrot_enable = on; copy_mode_settings(mode); tx_set_compressor(tx); return 1;
  case TXA_PHROT_STAGES: tx->phrot_stage = clampi(v, 1, 15); tx_set_compressor(tx); return 1;
  case TXA_PHROT_FREQ: tx->phrot_freq = clampd(v, 1, 500); tx_set_compressor(tx); return 1;
  case TXA_CFC: tx->cfc = on; ms->cfc = on; copy_mode_settings(mode); tx_set_compressor(tx); return 1;
  case TXA_CFC_EQ: tx->cfc_eq = on; ms->cfc_eq = on; copy_mode_settings(mode); tx_set_compressor(tx); return 1;
  case TXA_EQ: tx->eq_enable = on; ms->en_txeq = on; copy_mode_settings(mode); tx_set_equalizer(tx); return 1;
  case TXA_CTFMODE: tx->eq_ctfmode = on; tx_set_eq_ctfmode(tx); return 1;
  case TXA_DEXP: tx->dexp = on; ms->dexp = on; break;
  case TXA_DEXP_TRIGGER: tx->dexp_trigger = clampi(v, -40, -10); ms->dexp_trigger = tx->dexp_trigger; break;
  case TXA_DEXP_EXP: tx->dexp_exp = clampi(v, 0, 30); ms->dexp_exp = tx->dexp_exp; break;
  case TXA_DEXP_ATTACK: tx->dexp_attack = 0.001 * clampd(v, 1, 250); ms->dexp_attack = tx->dexp_attack; break;
  case TXA_DEXP_RELEASE: tx->dexp_release = 0.001 * clampd(v, 1, 500); ms->dexp_release = tx->dexp_release; break;
  case TXA_DEXP_HOLD: tx->dexp_hold = 0.001 * clampd(v, 10, 1500); ms->dexp_hold = tx->dexp_hold; break;
  case TXA_DEXP_TAU: tx->dexp_tau = 0.001 * clampd(v, 1, 250); ms->dexp_tau = tx->dexp_tau; break;
  case TXA_DEXP_HYST: tx->dexp_hyst = clampd(v, 0.05, 0.95); ms->dexp_hyst = tx->dexp_hyst; break;
  case TXA_DEXP_FILTER: tx->dexp_filter = on; ms->dexp_filter = on; break;
  case TXA_DEXP_FILTER_LOW: tx->dexp_filter_low = clampi(v, 0, 1200); ms->dexp_filter_low = tx->dexp_filter_low; break;
  case TXA_DEXP_FILTER_HIGH: tx->dexp_filter_high = clampi(v, 500, 10000); ms->dexp_filter_high = tx->dexp_filter_high; break;
  case TXA_VOX:
    vox_enabled = on;
    /* droidHPSDR: switched off while it holds the radio in transmit, back to receive now (not after the hang) */
    if (!on && vox && !mox && !tune) { vox_cancel(); radio_set_vox(0); }
    return 1;
  case TXA_VOX_THRESH: vox_threshold = clampd(v, 0, 1); return 1;
  case TXA_VOX_HANG: vox_hang = clampd(v, 0, 1000); return 1;
  case TXA_MON: tx_set_monitor(on); return 1;
  case TXA_MON_GAIN: tx_set_monitor_gain_db(clampd(v, -40, 0)); return 1;
  case TXA_MON_POST: tx_set_monitor_post(on); return 1;
  case TXA_ADDGAIN: tx->addgain_enable = on; tx_set_mic_gain(tx); return 1;
  case TXA_ADDGAIN_GAIN: tx->addgain_gain = clampd(v, 1, 20); tx_set_mic_gain(tx); return 1;
  default:
    if (field >= TXA_EQ_GAIN0 && field < TXA_EQ_GAIN0 + 13) {
      int k = field - TXA_EQ_GAIN0;
      tx->eq_gain[k] = clampd(v, -20, 20); ms->tx_eq_gain[k] = tx->eq_gain[k];
      copy_mode_settings(mode); tx_set_equalizer(tx);
      return 1;
    }
    if (field > TXA_EQ_FREQ0 && field < TXA_EQ_FREQ0 + 13) {
      int k = field - TXA_EQ_FREQ0;
      tx->eq_freq[k] = point_freq(tx->eq_freq, k, v); ms->tx_eq_freq[k] = tx->eq_freq[k];
      copy_mode_settings(mode); tx_set_equalizer(tx);
      return 1;
    }
    if (field > TXA_CFC_FREQ0 && field < TXA_CFC_FREQ0 + 13) {
      int k = field - TXA_CFC_FREQ0;
      tx->cfc_freq[k] = point_freq(tx->cfc_freq, k, v); ms->cfc_freq[k] = tx->cfc_freq[k];
      copy_mode_settings(mode); tx_set_compressor(tx);
      return 1;
    }
    if (field >= TXA_CFC_LVL0 && field < TXA_CFC_LVL0 + 13) {
      int k = field - TXA_CFC_LVL0;
      tx->cfc_lvl[k] = clampd(v, 0, 20); ms->cfc_lvl[k] = tx->cfc_lvl[k];
      copy_mode_settings(mode); tx_set_compressor(tx);
      return 1;
    }
    if (field >= TXA_CFC_POST0 && field < TXA_CFC_POST0 + 13) {
      int k = field - TXA_CFC_POST0;
      tx->cfc_post[k] = clampd(v, -20, 20); ms->cfc_post[k] = tx->cfc_post[k];
      copy_mode_settings(mode); tx_set_compressor(tx);
      return 1;
    }
    return 0;
  }
  /* the DEXP cases */
  copy_mode_settings(mode);
  tx_set_dexp(tx);
  return 1;
}

static void tx_audio_get(const TRANSMITTER *tx, double *o) {
  o[TXA_COMP] = tx->compressor;
  o[TXA_COMP_LEVEL] = tx->compressor_level;
  o[TXA_LEV] = tx->lev_enable;
  o[TXA_LEV_GAIN] = tx->lev_gain;
  o[TXA_LEV_DECAY] = tx->lev_decay;
  o[TXA_CESSB] = tx->cessb_enable;
  o[TXA_PHROT] = tx->phrot_enable;
  o[TXA_PHROT_STAGES] = tx->phrot_stage;
  o[TXA_PHROT_FREQ] = tx->phrot_freq;
  o[TXA_CFC] = tx->cfc;
  o[TXA_CFC_EQ] = tx->cfc_eq;
  o[TXA_EQ] = tx->eq_enable;
  o[TXA_CTFMODE] = tx->eq_ctfmode;
  o[TXA_DEXP] = tx->dexp;
  o[TXA_DEXP_TRIGGER] = tx->dexp_trigger;
  o[TXA_DEXP_EXP] = tx->dexp_exp;
  o[TXA_DEXP_ATTACK] = 1000.0 * tx->dexp_attack;
  o[TXA_DEXP_RELEASE] = 1000.0 * tx->dexp_release;
  o[TXA_DEXP_HOLD] = 1000.0 * tx->dexp_hold;
  o[TXA_DEXP_TAU] = 1000.0 * tx->dexp_tau;
  o[TXA_DEXP_HYST] = tx->dexp_hyst;
  o[TXA_DEXP_FILTER] = tx->dexp_filter;
  o[TXA_DEXP_FILTER_LOW] = tx->dexp_filter_low;
  o[TXA_DEXP_FILTER_HIGH] = tx->dexp_filter_high;
  o[TXA_VOX] = vox_enabled;
  o[TXA_VOX_THRESH] = vox_threshold;
  o[TXA_VOX_HANG] = vox_hang;
  o[TXA_MON] = tx_get_monitor();
  o[TXA_MON_GAIN] = tx_get_monitor_gain_db();
  o[TXA_MON_POST] = tx_get_monitor_post();
  o[TXA_ADDGAIN] = tx->addgain_enable;
  o[TXA_ADDGAIN_GAIN] = tx->addgain_gain;
  for (int k = 0; k < 13; k++) {
    o[TXA_EQ_GAIN0 + k] = tx->eq_gain[k];
    o[TXA_EQ_FREQ0 + k] = tx->eq_freq[k];
    o[TXA_CFC_FREQ0 + k] = tx->cfc_freq[k];
    o[TXA_CFC_LVL0 + k] = tx->cfc_lvl[k];
    o[TXA_CFC_POST0 + k] = tx->cfc_post[k];
  }
  o[TXS_VOX_PEAK] = vox_get_peak();
  o[TXS_VOX_REFUSED] = atomic_load(&vox_refusals);
  o[TXS_TX_MODE] = vfo_get_tx_mode();
  o[TXS_SCREEN_PTT] = atomic_load(&ptt_screen);
}

/* On the loop thread (radio_core.c CMD_TX). */
void droid_tx_apply(int field, double v) {
  if (field == TX_ENABLED) {
    atomic_store(&tx_enabled, v != 0);
    if (v == 0) droid_tx_off_now();
    return;
  }
  if (field == TX_TIMEOUT) { atomic_store(&tx_timeout_s, (int)clampd(v, 0, 3600)); return; }
  if (transmitter == NULL || !can_transmit) return;
  switch (field) {
  case TX_PTT:
    if (v != 0) {
      atomic_store(&ptt_seen_us, now_us());
      if (atomic_load(&ptt_screen) && mox) break;   /* a repeat while held */
      /* still the finger the timeout stopped: wait for a release (code review: the repeats re-keyed) */
      if (atomic_load(&timed_out)) { atomic_store(&refused, TXR_TIMEOUT); break; }
      if (radio_get_tx_inhibit()) { atomic_store(&refused, droid_blocked() ? TXR_DISABLED : TXR_INHIBIT); break; }
      if (!TransmitAllowed() && !tx_out_of_band_allowed) { atomic_store(&refused, TXR_BAND); break; }
      atomic_store(&refused, TXR_NONE);
      atomic_store(&ptt_screen, 1);
      radio_mox_update(1);
    } else {
      atomic_store(&timed_out, 0);   /* released: the next press may transmit */
      if (atomic_load(&ptt_screen)) {
        atomic_store(&ptt_screen, 0);
        screen_ptt_off();
      }
    }
    break;
  case TX_TUNE: {   /* actions.c TUNE_T: TUNE, and with the Pico the IO board's tuner start/stop */
    int state = radio_get_tune();
    int want = v != 0;
    if (want == state) break;
    if (want && radio_get_tx_inhibit()) { atomic_store(&refused, droid_blocked() ? TXR_DISABLED : TXR_INHIBIT); break; }
    if (want && !TransmitAllowed() && !tx_out_of_band_allowed) { atomic_store(&refused, TXR_BAND); break; }
    atomic_store(&refused, TXR_NONE);
    atomic_store(&ptt_screen, 0);
    if (want) droid_ps_generators_off();   /* TUNE's carrier replaces PureSignal's Two Tone / Noise */
    radio_tune_update(want);
    if (device == DEVICE_HERMES_LITE2 && hl2_pico_is_present()) hl2_iob_set_antenna_tuner(want ? 1 : 0);
    break;
  }
  case TX_DRIVE: {
    /* sliders.c's drive slider; with "per band" the band keeps it (band.c, vfo.c) */
    set_drive(clampd(v, 0, drive_max));
    BANDSETTINGS *bs = band_get_settings(vfo[vfo_get_tx_vfo()].band);
    if (bs != NULL && transmitter->drive_per_band) bs->tx_drive = transmitter->drive;
    break;
  }
  case TX_TUNE_DRIVE: {
    transmitter->tune_drive = (int)clampd(v, 0, 100);
    BANDSETTINGS *bs = band_get_settings(vfo[vfo_get_tx_vfo()].band);
    if (bs != NULL && transmitter->drive_per_band) bs->tune_drive = transmitter->tune_drive;
    if (tune) radio_calc_drive_level();
    break;
  }
  case TX_TUNE_USE_DRIVE: transmitter->tune_use_drive = v != 0; if (tune) radio_calc_drive_level(); break;
  case TX_DRIVE_PER_BAND: transmitter->drive_per_band = v != 0; break;
  case TX_MIC_GAIN: transmitter->mic_gain = clampd(v, -12.0, 50.0); tx_set_mic_gain(transmitter); break;
  case TX_MIC_BOOST: mic_boost = v != 0; schedule_high_priority(); break;
  case TX_MIC_LOCAL:   /* the phone's (or Bluetooth) mic: deskHPSDR's local microphone */
    if (v != 0 && !transmitter->local_microphone) {
      if (audio_open_input() == 0) transmitter->local_microphone = 1;
      droid_log("phone microphone %s", transmitter->local_microphone ? "on" : "could not start");
    } else if (v == 0 && transmitter->local_microphone) {
      transmitter->local_microphone = 0;
      audio_close_input();
      droid_log("phone microphone off: HL2+ mic jack");
    }
    break;
  case TX_FILTER_LOW: tx_filter_low = (int)clampd(v, 0, tx_filter_high - 100); apply_tx_filter(); break;
  case TX_FILTER_HIGH: tx_filter_high = (int)clampd(v, tx_filter_low + 100, 10000); apply_tx_filter(); break;
  case TX_USE_RX_FILTER: transmitter->use_rx_filter = v != 0; tx_set_filter(transmitter); break;
  case TX_SWR_PROT: transmitter->swr_protection = v != 0; break;
  case TX_SWR_ALARM: transmitter->swr_alarm = clampd(v, 1.0, 10.0); break;
  case TX_OUT_OF_BAND: tx_out_of_band_allowed = v != 0; break;
  case TX_MUTE_RX: mute_rx_while_transmitting = v != 0; break;
  case TX_PA: pa_enabled = v != 0; schedule_high_priority(); break;
  case TX_PA_POWER: pa_power = (int)clampd(v, 0, 13); reassign_pa_trim(); break;
  /* radio_menu.c toggle_cb on enable_tx_inhibit; off again: the input no longer holds TX off
   * (old_protocol.c then clears the hardware source itself) */
  case TX_INHIBIT_EN: enable_tx_inhibit = v != 0; break;
  /* vfo_menu.c / actions.c: XIT on the TX VFO */
  case TX_XIT_ON: vfo_xit_onoff(v != 0); break;
  case TX_XIT: vfo_xit_value((long long)clampd(v, -9999, 9999)); break;
  case TX_PA_FILE: {   /* pa_menu.c Save / Import, with band.c's own functions and file name */
    char name[64], file[128];
    snprintf(name, sizeof name, "%s", radio->name);
    for (size_t i = 0; name[i]; i++) if (!isalnum((unsigned char)name[i])) name[i] = '_';
    snprintf(file, sizeof file, "pa_calibration_%s.props", name);   /* band.c make_pa_calibration_filename() */
    if (v == 1) {
      unlink(file);   /* so only a fresh write counts as saved */
      PaCalibrationSave();
      atomic_store(&pa_file_ok, access(file, R_OK) == 0 ? 1 : -1);
    } else if (v == 2) {
      int ok = PaCalibrationLoad(file);
      atomic_store(&pa_file_ok, ok ? 2 : -2);
      radio_calc_drive_level();
    }
    break;
  }
  default:
    if (droid_ps_apply(field, v)) break;
    if (droid_cw_apply(field, v)) break;
    if (tx_audio_apply(transmitter, field, v)) break;
    if (field >= TX_PA_CAL0 && field < TX_PA_CAL0 + BANDS) {   /* pa_menu.c: 38.8 .. 70 dB */
      BAND *b = band_get_band(field - TX_PA_CAL0);
      b->pa_calibration = clampd(v, 38.8, 70.0);
      radio_calc_drive_level();
      atomic_store(&pa_file_ok, 0);   /* "Saved" / "Loaded" no longer describes what's set */
    }
    break;
  }
}

/* For the UI: the settings and the live state (Radio.kt reads them). */
void droid_tx_get(double *o) {
  memset(o, 0, sizeof(double) * TX_COUNT);
  o[TX_ENABLED] = atomic_load(&tx_enabled);
  o[TX_TIMEOUT] = atomic_load(&tx_timeout_s);
  if (transmitter == NULL || !can_transmit) return;
  TRANSMITTER *tx = transmitter;
  o[TX_PTT] = mox;
  o[TX_TUNE] = tune;
  o[TX_DRIVE] = tx->drive;
  o[TX_TUNE_DRIVE] = tx->tune_drive;
  o[TX_TUNE_USE_DRIVE] = tx->tune_use_drive;
  o[TX_DRIVE_PER_BAND] = tx->drive_per_band;
  o[TX_MIC_GAIN] = tx->mic_gain;
  o[TX_MIC_BOOST] = mic_boost;
  o[TX_MIC_LOCAL] = tx->local_microphone;
  o[TX_FILTER_LOW] = tx_filter_low;
  o[TX_FILTER_HIGH] = tx_filter_high;
  o[TX_USE_RX_FILTER] = tx->use_rx_filter;
  o[TX_SWR_PROT] = tx->swr_protection;
  o[TX_SWR_ALARM] = tx->swr_alarm;
  o[TX_OUT_OF_BAND] = tx_out_of_band_allowed;
  o[TX_MUTE_RX] = mute_rx_while_transmitting;
  o[TX_PA] = pa_enabled;
  o[TX_PA_POWER] = pa_power;
  for (int b = 0; b < BANDS && TX_PA_CAL0 + b < TX_COUNT; b++) o[TX_PA_CAL0 + b] = band_get_band(b)->pa_calibration;
  /* live, read-only */
  o[TXS_ON_AIR] = radio_is_transmitting();
  o[TXS_FWD] = tx->fwd;
  o[TXS_REV] = tx->rev;
  o[TXS_SWR] = tx->swr;
  o[TXS_ALC] = radio_is_transmitting() ? tx->alc : -100.0;
  o[TXS_MIC] = radio_is_transmitting() ? GetTXAMeter(tx->id, TXA_MIC_PK) : -100.0;
  o[TXS_HIGH_SWR] = high_swr_seen;
  o[TXS_SECONDS] = atomic_load(&tx_since_s);
  o[TXS_TIMEOUTS] = atomic_load(&timeouts);
  o[TXS_REFUSED] = atomic_load(&refused);
  o[TXS_RADIO_PTT] = radio_ptt;
  o[TXS_UNDERRUN] = tx_fifo_underrun;
  o[TXS_OVERRUN] = tx_fifo_overrun;
  o[TXS_OUT_OF_BAND] = tx->out_of_band;
  o[TXS_MAX_W] = pa_power_list[pa_power < 0 || pa_power > 13 ? 1 : pa_power];
  o[TXS_PICO] = device == DEVICE_HERMES_LITE2 && hl2_pico_is_present();
  o[TX_INHIBIT_EN] = enable_tx_inhibit;
  o[TXS_INHIBITED] = (g_atomic_int_get(&tx_inhibit_sources) & TX_INHIBIT_SOURCE_HARDWARE) != 0;
  o[TXS_PA_FILE_OK] = atomic_load(&pa_file_ok);
  int txv = vfo_get_tx_vfo();
  o[TX_XIT_ON] = vfo[txv].xit_enabled;
  o[TX_XIT] = vfo[txv].xit;
  tx_audio_get(tx, o);
  droid_ps_get(o);
  droid_cw_get(o);
}

int droid_tx_on_air(void) { return can_transmit && radio_is_transmitting(); }

void droid_tx_ptt_seen(void) { atomic_store(&ptt_seen_us, now_us()); }

/* The loop thread calls this every few ms: the screen PTT's repeats, the
 * TX timeout. */
void droid_tx_poll(void) {
  if (transmitter == NULL || !can_transmit) return;
  long long t = now_us();
  /* rx_panadapter.c (not compiled) shows "High SWR" for 3 s and the TX FIFO
   * under/overrun for 2 s, then clears them: the same here, for the screen */
  static long long swr_seen_us, fifo_seen_us;
  if (high_swr_seen) {
    if (swr_seen_us == 0) swr_seen_us = t;
    else if (t - swr_seen_us > 3000000) { high_swr_seen = 0; swr_seen_us = 0; }
  }
  if (tx_fifo_underrun || tx_fifo_overrun) {
    if (fifo_seen_us == 0) fifo_seen_us = t;
    else if (t - fifo_seen_us > 2000000) { tx_fifo_underrun = 0; tx_fifo_overrun = 0; fifo_seen_us = 0; }
  }
  droid_ps_poll();   /* Two Tone / Noise end with the transmission */
  droid_cw_poll();   /* the radio's keyer: armed only while transmitting is allowed */
  int on = radio_is_transmitting();
  static long long log_us;
  static unsigned frames_then;
  if (!on) {
    log_us = 0;   /* the next transmission's log starts fresh */
    tx_since_us = 0;
    atomic_store(&tx_since_s, 0);
    return;
  }
  if (tx_since_us == 0) tx_since_us = t;
  atomic_store(&tx_since_s, (int)((t - tx_since_us) / 1000000));
  /* once a second on the air: what goes out, for checking on the radio (logcat droidHPSDR) */
  if (log_us == 0) {   /* first moment on the air: a baseline, nothing from the receive gap */
    log_us = t;
    frames_then = droid_mic_frames();
    (void)droid_mic_peak();
  } else if (t - log_us >= 1000000) {
    unsigned fr = droid_mic_frames();
    float pk = droid_mic_peak();
    double secs = (t - log_us) / 1e6;
    droid_log("TX %s: fwd %.2f W, SWR %.2f, ALC %.1f dB, WDSP mic %.1f dB, %s mic%s, drive %d (level %d), PA %s",
              tune ? "TUNE" : mox ? "MOX" : "VOX", transmitter->fwd, transmitter->swr, transmitter->alc,
              GetTXAMeter(transmitter->id, TXA_MIC_PK),
              transmitter->local_microphone ? "phone" : "HL2+", "", transmitter->drive, transmitter->drive_level,
              pa_enabled ? "on" : "OFF");
    if (transmitter->local_microphone)
      droid_log("TX phone mic: %.0f frames/s, peak %.3f", (fr - frames_then) / secs, pk);
    frames_then = fr;
    log_us = t;
  }
  /* a held screen PTT repeats every 250 ms; 1.5 s without one = the finger is gone */
  if (atomic_load(&ptt_screen) && mox && t - atomic_load(&ptt_seen_us) > 1500000) {
    droid_log("screen PTT: no repeat for 1.5 s, back to receive");
    atomic_store(&ptt_screen, 0);
    screen_ptt_off();
  }
  int limit = atomic_load(&tx_timeout_s);
  if (limit > 0 && t - tx_since_us > (long long)limit * 1000000) {
    droid_log("TX timeout (%d s): back to receive", limit);
    droid_cw_timeout();   /* in CW the radio's keyer may still be keying: disarm it (before the count: the UI's note) */
    atomic_fetch_add(&timeouts, 1);
    if (atomic_load(&ptt_screen)) atomic_store(&timed_out, 1);
    droid_tx_off_now();
    tx_since_us = 0;
  }
}
