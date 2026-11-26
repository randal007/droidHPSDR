/*
 * tx_null.c — the transmit side for m1-rx, the headless receive test:
 * RECEIVE ONLY by construction. Nothing here can put the radio into
 * transmit:
 *   - radio_is_transmitting() is always 0, so old_protocol.c never sets MOX;
 *   - radio_state.c leaves cw_keyer_internal = 0 and pa_enabled = 0;
 *   - m1_main.c's TRANSMITTER is all zeros (drive level 0) with no WDSP TX
 *     channel; tx_add_mic_sample() discards the mic samples.
 *
 * The app links upstream's transmitter.c, tx_off.c, vox.c and
 * app/transmit.c instead (M7). Prototypes come from the upstream headers.
 *
 * GPL v3 or later, like the rest of droidHPSDR.
 */
#include <gtk/gtk.h>

#include "ext.h"
#include "radio.h"
#include "sliders.h"
#include "transmitter.h"
#include "tx_off.h"

int radio_get_drive_as_int(void) { return 0; }
int radio_is_transmitting(void) { return 0; }             /* RX ONLY */
void radio_set_hardware_tx_inhibit(int state) { (void)state; }
void radio_start_auto_tune(void) {}
void radio_tx_vfo_changed(void) {}
int ext_mox_update(void *data) { (void)data; return G_SOURCE_REMOVE; }
int ext_set_duplex(void *data) { (void)data; return G_SOURCE_REMOVE; }

void tx_add_mic_sample(TRANSMITTER *tx, float mic_sample) { (void)tx; (void)mic_sample; }
void tx_add_ps_iq_samples(const TRANSMITTER *tx, double i_sample_0, double q_sample_0, double i_sample_1,
                          double q_sample_1) {
  (void)tx; (void)i_sample_0; (void)q_sample_0; (void)i_sample_1; (void)q_sample_1;
}
double tx_display_span_hz(const TRANSMITTER *tx) { (void)tx; return 0.0; }
int tx_monitor_audio_active(void) { return 0; }
void tx_set_filter(TRANSMITTER *tx) { (void)tx; }
void tx_off_cancel(void) {}
void set_drive(double drive) { (void)drive; }
void tx_set_analyzer(TRANSMITTER *tx) { (void)tx; }
void tx_set_compressor(TRANSMITTER *tx) { (void)tx; }
void tx_set_ctcss(const TRANSMITTER *tx) { (void)tx; }
void tx_set_dexp(const TRANSMITTER *tx) { (void)tx; }
void tx_set_mode(TRANSMITTER *tx, int m) { (void)tx; (void)m; }

/* from src/transmitter.c (read by vfo.c's VFO bar) */
double ctcss_frequencies[CTCSS_FREQUENCIES] = {
  67.0,  71.9,  74.4,  77.0,  79.7,  82.5,  85.4,  88.5,  91.5,  94.8,
  97.4, 100.0, 103.5, 107.2, 110.9, 114.8, 118.8, 123.0, 127.3, 131.8,
  136.5, 141.3, 146.2, 151.4, 156.7, 162.2, 167.9, 173.8, 179.9, 186.2,
  192.8, 203.5, 210.7, 218.1, 225.7, 233.6, 241.8, 250.3
};
