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

long long apply_ppm_ll(long long f_hz) { return f_hz; }   /* ppm_factor 0 */
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
int discovery_resolve_target(const char *host, struct sockaddr_in *target, struct sockaddr_in *local_addr,
                             struct sockaddr_in *netmask, char *ifname, size_t ifname_len, int *is_direct) {
  (void)host; (void)target; (void)local_addr; (void)netmask; (void)ifname; (void)ifname_len; (void)is_direct;
  return -1;   /* "could not resolve": old_discovery falls back to broadcast */
}

/* ---- main.c / ext.c / menus / sliders: UI notifications ---- */

int fatal_error(void *data) {
  droid_log("FATAL: %s", data ? (const char *)data : "(no message)");
  return G_SOURCE_REMOVE;
}
int ext_vfo_update(void *data) { (void)data; return G_SOURCE_REMOVE; }
int ext_mox_update(void *data) { (void)data; return G_SOURCE_REMOVE; }
int ext_start_rx(void *data) { (void)data; return G_SOURCE_REMOVE; }
int ext_start_noise(void *data) { (void)data; return G_SOURCE_REMOVE; }
int menu_active_receiver_changed(void *data) { (void)data; return G_SOURCE_REMOVE; }
int sliders_active_receiver_changed(void *data) { (void)data; return G_SOURCE_REMOVE; }
int zoompan_active_receiver_changed(void *data) { (void)data; return G_SOURCE_REMOVE; }
void update_slider_binaural_btn(void) {}
void show_filter_high(int rx, int value) { (void)rx; (void)value; }
void show_filter_low(int rx, int value) { (void)rx; (void)value; }
void show_filter_shift(int rx, int value) { (void)rx; (void)value; }
void show_filter_width(int rx, int value) { (void)rx; (void)value; }
void set_pan(int rx, double value) { (void)rx; (void)value; }
void schedule_action(enum ACTION action, enum ACTION_MODE mode, int val) { (void)action; (void)mode; (void)val; }
void vfo_num_pad(int val, int v) { (void)val; (void)v; }
gboolean rigctl_debug = FALSE;

/* ---- display (the Compose/GL UI replaces these later) ---- */

void rx_panadapter_init(RECEIVER *rx, int width, int height) { (void)rx; (void)width; (void)height; }
void rx_panadapter_update(RECEIVER *rx) { (void)rx; }
void waterfall_init(RECEIVER *rx, int width, int height) { (void)rx; (void)width; (void)height; }
void waterfall_update(RECEIVER *rx) { (void)rx; }
void meter_update(RECEIVER *rx, int meter_type, double value, double alc, double swr) {
  (void)rx; (void)meter_type; (void)value; (void)alc; (void)swr;
}

/* ---- properties: no props file in M1, so every setting keeps its default ---- */

void loadProperties(const char *filename) { (void)filename; }
void saveProperties(const char *filename) { (void)filename; }
void clearProperties(void) {}
char *getProperty(const char *name) { (void)name; return NULL; }
void setProperty(const char *name, const char *value) { (void)name; (void)value; }
double myatof(const char *string) { return string ? strtod(string, NULL) : 0.0; }

/* ---- local PC audio: never used, the HL2+ codec carries audio ---- */

int audio_open_output(RECEIVER *rx) { (void)rx; return -1; }
int audio_open_input(void) { return -1; }
int audio_write(RECEIVER *rx, float left_sample, float right_sample) {
  (void)rx; (void)left_sample; (void)right_sample;
  return 0;
}
float audio_get_next_mic_sample(void) { return 0.0f; }
void audio_reset_mic_buffer(void) {}

/* ---- TCI and Protocol 2: not in M1 ---- */

int tci_audio_is_active(void) { return 0; }
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

/* ---- vfo.c: the TX-VFO helpers copied from upstream, the rest no-ops ---- */

int vfo_get_tx_vfo(void) {
  int txvfo = active_receiver->id;
  if (split) { txvfo = 1 - txvfo; }
  return txvfo;
}
int vfo_get_tx_mode(void) {
  return vfo[vfo_get_tx_vfo()].mode;
}
long long vfo_get_tx_freq(void) {
  int txvfo = vfo_get_tx_vfo();
  return vfo[txvfo].ctun ? vfo[txvfo].ctun_frequency : vfo[txvfo].frequency;
}
void copy_mode_settings(int mode) { (void)mode; }
void vfo_band_changed(int id, int b) { (void)id; (void)b; }
void vfo_move(long long hz, int round) { (void)hz; (void)round; }
void vfo_move_to(long long hz) { (void)hz; }
void vfo_step(int steps) { (void)steps; }

/* ---- toolset.c ---- */

void sort_rx_eq(RECEIVER *rx) { (void)rx; }
