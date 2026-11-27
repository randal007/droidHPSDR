/*
 * cw.c — M7 step 3, CW: deskHPSDR's CW menu (cw_menu.c) for the HL2+'s own
 * keyer, paddle in the KEY jack (Randal's answer 7; docs/M7-INVENTORY.md
 * "Step 3: CW").
 *
 * The keyer is in the radio. old_protocol.c (compiled unchanged) sends the
 * settings every round (0x16 speed / mode / weight / spacing / reversed,
 * 0x1E sidetone level + PTT delay, 0x20 hang time + sidetone frequency) and
 * arms the keyer with one bit, 0x1E C1 bit 0, while the TX mode is CW and
 * cw_keyer_internal is set. The radio then keys itself from the paddle and
 * reports PTT; ext_mox_update() switches the app to transmit after the fact.
 *
 * droidHPSDR's safety (docs/M7-INVENTORY.md, safety findings 1 and 2):
 *   - the radio's keyer doesn't ask the app, so the app arms it only while
 *     transmitting is allowed: Transmit enabled on, no demo band, no TX
 *     inhibit, in band, and not stopped by the timeout. Otherwise
 *     cw_keyer_internal stays 0 and the paddle does nothing (iambic.c, the
 *     host keyer, isn't compiled: keyer_event() is a no-op stub);
 *   - the TX timeout can't end a key-down by dropping MOX: in CW it disarms
 *     the keyer until the user arms it again (TXC_REARM), so a stuck paddle
 *     can't key the radio again by itself.
 *
 * "CW handled in Radio" is always on and not shown (with no host keyer, off
 * would mean the paddle does nothing). "CW Break-In" is not shown: on an
 * HL2 it only steers the host keyer; the radio's hang time is the break-in
 * delay. The ramp width is Protocol 2 / host keyer only.
 *
 * Copied from deskHPSDR cw_menu.c (the value callbacks and cw_changed()),
 * radio.c (the props entries): (C) 2015 John Melton G0ORX/N6LYT,
 * 2024-2026 Heiko Amft DL1BZ (Project deskHPSDR), GPL-3.0-or-later.
 *
 * GPL v3 or later, like the rest of droidHPSDR.
 */
#include <stdatomic.h>
#include <gtk/gtk.h>

#include "band.h"
#include "ext.h"
#include "mode.h"
#include "new_protocol.h"
#include "old_protocol.h"
#include "property.h"
#include "radio.h"
#include "receiver.h"
#include "transmitter.h"
#include "vfo.h"

#include "droid_app.h"

static atomic_int cw_timed_out;   /* the timeout disarmed the keyer: off until TXC_REARM */
static atomic_int cw_lock;        /* why the keyer isn't armed in a CW mode (TXR_*), for the UI */
static int zb_pending;            /* loop thread: a zero beat is running */
static long long zb_from;         /* the frequency it started on */
static atomic_int zb_done;        /* zero beats finished (the UI shows the result when it grows) */
static atomic_int zb_hz;          /* how far the last one moved the VFO */

static int clampi(double v, int lo, int hi) { int i = (int)(v + (v < 0 ? -0.5 : 0.5)); return i < lo ? lo : i > hi ? hi : i; }

static int is_cw(int m) { return m == modeCWU || m == modeCWL; }

/* cw_menu.c cw_changed(), for an HL2 (P1): keyer_update() belongs to the
 * host keyer (not compiled) and the transmit-specific packet to P2; the
 * settings go out in the next round of the command loop by themselves. */
static void cw_changed(void) {
  schedule_transmit_specific();
  g_idle_add(ext_vfo_update, NULL);
}

void droid_cw_create(void) {
  /* radio.c's entries (and defaults, in radio_state.c) */
  GetPropI0("cw_keys_reversed", cw_keys_reversed);
  GetPropI0("cw_keyer_speed", cw_keyer_speed);
  GetPropI0("cw_keyer_mode", cw_keyer_mode);
  GetPropI0("cw_keyer_weight", cw_keyer_weight);
  GetPropI0("cw_keyer_spacing", cw_keyer_spacing);
  GetPropI0("cw_keyer_sidetone_volume", cw_keyer_sidetone_volume);
  GetPropI0("cw_keyer_ptt_delay", cw_keyer_ptt_delay);
  GetPropI0("cw_keyer_hang_time", cw_keyer_hang_time);
  GetPropI0("cw_keyer_sidetone_frequency", cw_keyer_sidetone_frequency);
  /* "cw_keyer_internal" and "cw_breakin" are not read: always on / not used (above) */
  cw_keyer_internal = 0;   /* armed by droid_cw_poll() when transmitting is allowed */
}

void droid_cw_save(void) {
  SetPropI0("cw_keys_reversed", cw_keys_reversed);
  SetPropI0("cw_keyer_speed", cw_keyer_speed);
  SetPropI0("cw_keyer_mode", cw_keyer_mode);
  SetPropI0("cw_keyer_weight", cw_keyer_weight);
  SetPropI0("cw_keyer_spacing", cw_keyer_spacing);
  SetPropI0("cw_keyer_sidetone_volume", cw_keyer_sidetone_volume);
  SetPropI0("cw_keyer_ptt_delay", cw_keyer_ptt_delay);
  SetPropI0("cw_keyer_hang_time", cw_keyer_hang_time);
  SetPropI0("cw_keyer_sidetone_frequency", cw_keyer_sidetone_frequency);
  SetPropI0("cw_keyer_internal", 1);   /* deskHPSDR reading this file gets its radio keyer */
}

static long long zb_freq(void) {
  int id = active_receiver->id;
  return vfo[id].ctun ? vfo[id].ctun_frequency : vfo[id].frequency;
}

/* Loop thread, every round (transmit.c droid_tx_poll): arm the radio's keyer only while the app
 * would let a PTT through. TUNE handles the bit itself (radio_set_tune), so it's left alone then. */
void droid_cw_poll(void) {
  int why = TXR_NONE;
  if (atomic_load(&cw_timed_out)) why = TXR_TIMEOUT;
  else if (radio_get_tx_inhibit()) why = droid_tx_blocked() ? TXR_DISABLED : TXR_INHIBIT;
  else if (!TransmitAllowed() && !tx_out_of_band_allowed) why = TXR_BAND;
  atomic_store(&cw_lock, why);
  if (!tune) {
    int arm = why == TXR_NONE;
    if (cw_keyer_internal != arm) {
      cw_keyer_internal = arm;
      droid_log("CW: radio keyer %s%s", arm ? "armed" : "off",
                arm ? "" : why == TXR_TIMEOUT ? " (timeout)" : why == TXR_BAND ? " (out of band)" :
                why == TXR_INHIBIT ? " (TX inhibit)" : " (transmit not enabled / demo)");
    }
  }
  /* zero beat: receiver.c logs only; the result is how far the VFO moved */
  if (zb_pending && active_receiver != NULL && !active_receiver->cw_zero_beat_active) {
    zb_pending = 0;
    atomic_store(&zb_hz, (int)(zb_freq() - zb_from));
    atomic_fetch_add(&zb_done, 1);
  }
}

/* The TX timeout ended a transmission (transmit.c): in CW the radio's keyer may still be keying. */
void droid_cw_timeout(void) {
  if (!is_cw(vfo_get_tx_mode())) return;
  atomic_store(&cw_timed_out, 1);
  droid_cw_poll();   /* disarm now, not on the next round */
}

int droid_cw_apply(int field, double v) {
  if (field < TXC_FIRST || field >= TXC_END) return 0;
  switch (field) {
  case TXC_SPEED: cw_keyer_speed = clampi(v, 1, 60); break;
  case TXC_MODE: cw_keyer_mode = clampi(v, KEYER_STRAIGHT, KEYER_MODE_B); break;
  case TXC_WEIGHT: cw_keyer_weight = clampi(v, 0, 100); break;
  case TXC_SPACING: cw_keyer_spacing = v != 0; break;
  case TXC_REVERSED: cw_keys_reversed = v != 0; break;
  case TXC_HANG: cw_keyer_hang_time = clampi(v, 0, 1000); break;
  case TXC_PTT_DELAY: cw_keyer_ptt_delay = clampi(v, 0, 255); break;
  case TXC_SIDE_LEVEL: cw_keyer_sidetone_volume = clampi(v, 0, 127); break;
  case TXC_PITCH:
    cw_keyer_sidetone_frequency = clampi(v, 100, 1000);
    cw_changed();
    rx_filter_changed(active_receiver);
    // changing the side tone frequency affects BFO frequency offsets
    schedule_high_priority();
    return 1;
  case TXC_ZB_CORR:
    active_receiver->rx_cw_zero_beat_calibration_hz = clampi(v, -50, 50);
    cw_changed();
    rx_filter_changed(active_receiver);
    schedule_high_priority();
    return 1;
  case TXC_ZB_START:   /* actions.c CW_ZERO_BEAT */
    if (zb_pending || active_receiver == NULL) break;
    zb_from = zb_freq();
    rx_cw_zero_beat_start(active_receiver);
    if (active_receiver->cw_zero_beat_active) {
      zb_pending = 1;
    } else {   /* refused: not CW, or transmitting */
      atomic_store(&zb_hz, 0);
      atomic_fetch_add(&zb_done, 1);
    }
    return 1;
  case TXC_REARM:
    if (atomic_exchange(&cw_timed_out, 0)) droid_log("CW: keyer armed again by the user");
    droid_cw_poll();
    return 1;
  }
  cw_changed();
  return 1;
}

void droid_cw_get(double *o) {
  o[TXC_SPEED] = cw_keyer_speed;
  o[TXC_MODE] = cw_keyer_mode;
  o[TXC_WEIGHT] = cw_keyer_weight;
  o[TXC_SPACING] = cw_keyer_spacing;
  o[TXC_REVERSED] = cw_keys_reversed;
  o[TXC_HANG] = cw_keyer_hang_time;
  o[TXC_PTT_DELAY] = cw_keyer_ptt_delay;
  o[TXC_SIDE_LEVEL] = cw_keyer_sidetone_volume;
  o[TXC_PITCH] = cw_keyer_sidetone_frequency;
  o[TXC_ZB_CORR] = active_receiver ? active_receiver->rx_cw_zero_beat_calibration_hz : 0;
  /* live */
  int cw = is_cw(vfo_get_tx_mode());
  o[TXCS_ARMED] = cw && cw_keyer_internal && !tune;
  o[TXCS_LOCK] = atomic_load(&cw_lock);
  o[TXCS_ZB_ACTIVE] = zb_pending;
  o[TXCS_ZB_DONE] = atomic_load(&zb_done);
  o[TXCS_ZB_HZ] = atomic_load(&zb_hz);
  o[TXCS_CW] = cw;
}
