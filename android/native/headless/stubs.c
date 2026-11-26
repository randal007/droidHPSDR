/*
 * stubs.c — stand-ins for upstream functions whose files droidHPSDR does not
 * compile in the headless (M1) build: the GTK UI, local PC audio, TCI, CAT,
 * menus, the display, and the whole transmit side.
 *
 * RECEIVE ONLY. Everything that could put the radio into transmit is a
 * no-op or reports "not transmitting":
 *   - radio_is_transmitting() is always 0, so old_protocol.c never sets MOX;
 *   - radio_state.c leaves cw_keyer_internal = 0 and pa_enabled = 0;
 *   - the host program's TRANSMITTER is all zeros (drive level 0) and has no
 *     WDSP TX channel; tx_add_mic_sample() discards mic samples.
 *
 * Prototypes come from the upstream headers, so a signature change upstream
 * fails the build here instead of misbehaving.
 *
 * GPL v3 or later, like the rest of droidHPSDR.
 */
#include <stdatomic.h>
#include <math.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <gtk/gtk.h>

#include "actions.h"
#include "audio.h"
#include "discovery.h"
#include "ext.h"
#include "iambic.h"
#include "main.h"
#include "meter.h"
#include "new_menu.h"
#include "new_protocol.h"
#include "property.h"
#include "radio.h"
#include "receiver.h"
#include "rigctl.h"
#include "rx_panadapter.h"
#include "sliders.h"
#include "tci.h"
#include "tci_audio.h"
#include "toolset.h"
#include "transmitter.h"
#include "tx_off.h"
#include "vfo.h"
#include "vfo_menu.h"
#include "waterfall.h"
#include "zoompan.h"

#include "droid_headless.h"

/* ---- radio.c ---- */

/* radio.c ((C) 2015 John Melton G0ORX/N6LYT, 2024-2026 Heiko Amft DL1BZ
 * (Project deskHPSDR); GPL-3.0-or-later), copied unchanged: the HL2's
 * frequency calibration (SDR Device sheet, "Freq. Calibration (ppm factor)"). */
double ppm_factor = 0.0;
long long apply_ppm_ll(long long f_hz) {
  const long long cal_0p1ppm = llround(ppm_factor * 10.0);
  const long long scale  = 10000000LL;
  const long long factor = scale + cal_0p1ppm;
#if defined(__SIZEOF_INT128__)
  return (long long)(((__int128) f_hz * factor) / scale);
#else
  // Overflow-sicher ohne __int128 (32-bit ARM)
  const long long q = f_hz / scale;
  const long long r = f_hz % scale;
  return (q * factor) + ((r * factor) / scale);
#endif
}
int radio_get_drive_as_int(void) { return 0; }
int radio_is_transmitting(void) { return 0; }             /* RX ONLY */
void radio_set_alex_antennas(void) {}
void radio_set_hardware_tx_inhibit(int state) { (void)state; }
void radio_start_auto_tune(void) {}
void radio_tx_vfo_changed(void) {}

/* Upstream keeps a timed loss window for the GUI; headless just counts. */
atomic_uint droid_sequence_errors;
uint32_t sequence_error_add(uint32_t expected, uint32_t received) {
  uint32_t missing = received - expected;
  if (missing != 0 && missing < UINT32_C(0x80000000)) {
    atomic_fetch_add(&droid_sequence_errors, missing);
    return missing;
  }
  return 0;
}

/* ---- discovery.c (the GTK dialog): only the unicast/TCP paths use these ---- */

int discovery_last_device_matches(int protocol, const struct sockaddr_in *addr, const unsigned char *mac) {
  (void)protocol; (void)addr; (void)mac;
  return 0;
}
/* discovery_resolve_target(): headless/net_target.c */

/* ---- main.c / ext.c / menus / sliders: UI notifications ---- */

atomic_int droid_fatal_seen;
int fatal_error(void *data) {
  droid_log("FATAL: %s", data ? (const char *)data : "(no message)");
  atomic_store(&droid_fatal_seen, 1);
  return G_SOURCE_REMOVE;
}
int ext_vfo_update(void *data) { (void)data; return G_SOURCE_REMOVE; }
int ext_mox_update(void *data) { (void)data; return G_SOURCE_REMOVE; }
int ext_start_rx(void *data) { (void)data; return G_SOURCE_REMOVE; }
int ext_start_noise(void *data) { (void)data; return G_SOURCE_REMOVE; }
int ext_set_duplex(void *data) { (void)data; return G_SOURCE_REMOVE; }   /* TX display (M7) */
int menu_active_receiver_changed(void *data) { (void)data; return G_SOURCE_REMOVE; }
int sliders_active_receiver_changed(void *data) { (void)data; return G_SOURCE_REMOVE; }
int zoompan_active_receiver_changed(void *data) { (void)data; return G_SOURCE_REMOVE; }
void update_slider_binaural_btn(void) {}
void update_slider_local_mic_button(void) {}
void show_filter_high(int rx, int value) { (void)rx; (void)value; }
void show_filter_low(int rx, int value) { (void)rx; (void)value; }
void show_filter_shift(int rx, int value) { (void)rx; (void)value; }
void show_filter_width(int rx, int value) { (void)rx; (void)value; }
void set_pan(int rx, double value) { (void)rx; (void)value; }
void schedule_action(enum ACTION action, enum ACTION_MODE mode, int val) { (void)action; (void)mode; (void)val; }
void vfo_num_pad(int val, int v) { (void)val; (void)v; }
gboolean rigctl_debug = FALSE;

/* ---- display: headless/display_null.c (m1-rx) or app/display_bridge.c (the app) ---- */

/* ---- properties: upstream property.c, compiled unchanged since M4 ---- */

/* ---- local audio: headless/audio_null.c (m1-rx) or upstream buffered_audio.c
 *      with app/audio_aaudio.c (the app's phone audio) ---- */

/* ---- TCI and Protocol 2: not in M1 ---- */

int tci_audio_is_active(void) { return 0; }
void tci_audio_monitor_set_active(int active) { (void)active; }
int tci_audio_tx_enabled(void) { return 0; }
void tci_audio_rx_block(RECEIVER *rx, const float *samples, guint frames) { (void)rx; (void)samples; (void)frames; }
void tci_rx_iq_block(RECEIVER *rx, const double *iq, guint frames) { (void)rx; (void)iq; (void)frames; }
void tci_mute_changed(int receiver_id) { (void)receiver_id; }
void tci_rx_bin_enable_changed(int receiver_id) { (void)receiver_id; }
void new_protocol_audio_samples(short left_audio_sample, short right_audio_sample) {
  (void)left_audio_sample; (void)right_audio_sample;
}
void schedule_high_priority(void) {}
void schedule_receive_specific(void) {}

/* ---- transmit side: RX ONLY, nothing here may produce RF ---- */

void tx_add_mic_sample(TRANSMITTER *tx, float mic_sample) { (void)tx; (void)mic_sample; }
void tx_add_ps_iq_samples(const TRANSMITTER *tx, double i_sample_0, double q_sample_0, double i_sample_1,
                          double q_sample_1) {
  (void)tx; (void)i_sample_0; (void)q_sample_0; (void)i_sample_1; (void)q_sample_1;
}
double tx_display_span_hz(const TRANSMITTER *tx) { (void)tx; return 0.0; }
int tx_monitor_audio_active(void) { return 0; }
void tx_set_filter(TRANSMITTER *tx) { (void)tx; }
void tx_off_cancel(void) {}
void keyer_event(int left, int state) { (void)left; (void)state; }

/* ---- vfo.c: compiled unchanged since M4. What it calls elsewhere: ---- */

/* radio.c ((C) 2015 John Melton G0ORX/N6LYT, 2024-2026 Heiko Amft DL1BZ
 * (Project deskHPSDR); GPL-3.0-or-later), copied unchanged: split only takes effect when the radio can
 * transmit, so in this RECEIVE ONLY build it does nothing (until M7). */
void radio_set_split(int val) {
  if (can_transmit) {
    split = val;
    vfo_apply_ps_tx_att();
    radio_tx_vfo_changed();
    radio_set_alex_antennas();
    if (!tci_is_applying()) {
      tci_split_changed();
      tci_tx_frequency_changed();
    }
    g_idle_add(ext_vfo_update, NULL);
    update_slider_split_btn();
  }
}

/* transmit side: RX ONLY */
void set_drive(double drive) { (void)drive; }
void tx_set_analyzer(TRANSMITTER *tx) { (void)tx; }
void tx_set_compressor(TRANSMITTER *tx) { (void)tx; }
void tx_set_ctcss(const TRANSMITTER *tx) { (void)tx; }
void tx_set_dexp(const TRANSMITTER *tx) { (void)tx; }
void tx_set_mode(TRANSMITTER *tx, int m) { (void)tx; (void)m; }
void sort_cfc_profile(double *freq, double *level, double *post, double *comp_weight, double *post_weight) {
  (void)freq; (void)level; (void)post; (void)comp_weight; (void)post_weight;
}
void sort_eq_profile(double *freq, double *gain, double *weight) { (void)freq; (void)gain; (void)weight; }

/* Protocol 2 scheduling: P1 sends everything in every frame */
void schedule_general(void) {}
void schedule_transmit_specific(void) {}

/* TCI (M8) */
int tci_is_applying(void) { return 0; }
void tci_mode_changed(int id) { (void)id; }
void tci_rit_enable_changed(int receiver_id) { (void)receiver_id; }
void tci_rit_offset_changed(int receiver_id) { (void)receiver_id; }
void tci_tune_drive_changed(void) {}
void tci_tx_frequency_changed(void) {}
void tci_vfo_changed(int id) { (void)id; }
void tci_vfos_changed(void) {}
void tci_xit_enable_changed(void) {}
void tci_xit_offset_changed(void) {}
void tci_split_changed(void) {}

/* actions.c: the multifunction encoder shown in the VFO bar (no encoder here) */
int GetMultifunctionStatus(void) { return 0; }
void GetMultifunctionString(char *str, size_t len) { if (len) str[0] = 0; }

/* desktop UI refreshes: Compose reads the state itself */
gboolean band_menu_update(gpointer data) { (void)data; return G_SOURCE_REMOVE; }
void update_eq(void) {}
void update_noise(void) {}
void update_slider_agc_btn(void) {}
void update_slider_bbcompr_button(gboolean show_widget) { (void)show_widget; }
void update_slider_bbcompr_scale(gboolean show_widget) { (void)show_widget; }
void update_slider_lev_button(gboolean show_widget) { (void)show_widget; }
void update_slider_lev_scale(gboolean show_widget) { (void)show_widget; }
void update_slider_local_mic_input(int src) { (void)src; }
void update_slider_preamp_button(gboolean show_widget) { (void)show_widget; }
void update_slider_tune_drive_scale(gboolean show_widget) { (void)show_widget; }
void update_slider_split_btn(void) {}
void update_zoompan_ant_labels(void) {}
int update_zoompan_ant_labels_idle(void *data) { (void)data; return G_SOURCE_REMOVE; }

/* ---- toolset.c ---- */

void sort_rx_eq(RECEIVER *rx) { (void)rx; }
