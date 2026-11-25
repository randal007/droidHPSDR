/*
 * radio_state.c — the global radio settings upstream keeps in radio.c,
 * discovery.c, main.c and vfo.c, for builds that do not compile those files.
 *
 * GENERATED from upstream definitions (deskHPSDR 9f49c21): every line below
 * is copied from the named file with its upstream default, so the headers
 * check the types. Only the lines marked RX-ONLY differ from upstream.
 * Values that depend on the radio (device, protocol, filter board, codec)
 * are set at start-up by the host program, as radio.c's start_radio() does.
 *
 * radio.c, discovery.c, main.c, vfo.c: Copyright (C) 2015 - John Melton,
 * G0ORX/N6LYT, 2024-2026 - Heiko Amft, DL1BZ (Project deskHPSDR);
 * GPL-3.0-or-later. droidHPSDR additions: GPL v3 or later.
 */
#include <stdatomic.h>
#include <netinet/in.h>
#include <gtk/gtk.h>

#include "actions.h"
#include "adc.h"
#include "band.h"
#include "discovered.h"
#include "discovery.h"
#include "filter.h"
#include "main.h"
#include "mode.h"
#include "radio.h"
#include "receiver.h"
#include "transmitter.h"
#include "vfo.h"

static char ipaddr_buf[64] = "";   // from discovery.c, backs ipaddr_radio


/* ---- from src/radio.c ---- */
RECEIVER *active_receiver;
ADC adc[2];
unsigned int ADC0 = 0;
int adc0_filter_bypass = 0; // Bypass ADC0 filters on receive
int adc0_p_ovl = 0;
unsigned int ADC1 = 0;
int adc1_filter_bypass = 0; // Bypass ADC1 filters on receiver  (ANAN-7000/8000/G2)
int adc1_p_ovl = 0;
unsigned int alex_forward_power = 0;
unsigned int alex_reverse_power = 0;
int anan10E = 0;
int atlas_clock_source_10mhz = 0;
int atlas_clock_source_128mhz = 0;
int atlas_mic_source = 0;
int atlas_penelope = 0; // 0: no TX, 1: Penelope TX, 2: PennyLane TX
atomic_int auto_tune_end = 0;
atomic_int auto_tune_flag = 0;
int can_transmit = 0;
double *capture_data = NULL;
int capture_max = 1440000;  // 30 seconds
int capture_record_pointer;
int capture_replay_pointer;
int capture_state = CAP_INIT;
enum ACTION capture_trigger_action = CAPTURE;
atomic_int CAT_cw_is_active = 0;
atomic_int CAT_rtty_is_active = 0;
int classE = 0;
int cw_keyer_hang_time = 500;          // ms
int cw_keyer_internal = 0;             // RX-ONLY: radio keyer never armed (upstream: 1 = internal)
int cw_keyer_mode = KEYER_MODE_A;      // Modes A/B and STRAIGHT
int cw_keyer_ptt_delay = 30;           // 0-255ms
int cw_keyer_sidetone_frequency = 800; // Hz
int cw_keyer_sidetone_volume = 50;     // 0-127
int cw_keyer_spacing = 0;              // 0=on 1=off
int cw_keyer_speed = 16;               // 1-60 WPM
int cw_keyer_weight = 50;              // 0-100
int cw_key_hit = 0;
int cw_keys_reversed = 0;              // 0=disabled 1=enabled
int device;
double div_cos = 1.0;      // I factor for diversity
int diversity_enabled = 0;
double div_sin = 1.0;      // Q factor for diversity
gboolean duplex = FALSE;
int eer_pwm_max = 800;
int eer_pwm_min = 100;
int enable_auto_tune = 0;
int enable_hl2_atu_gateware = 0;
int enable_tx_inhibit = 0;
unsigned int exciter_power = 0;
int filter_board = ALEX;
int force_iob = 0;
int have_alex_att = 0;
int have_preamp = 0;
volatile gint hl2_adc0_ovf_events = 0;
int hl2_audio_codec = 0;
int hl2_cl1_input = 0;
double linein_gain = 0.0;  // -34.0 ... +12.5 in steps of 1.5 dB
int mercury_software_version[2] = {0, 0};
int mic_bias_enabled = 0;
int mic_boost = 0;
int mic_linein = 0;        // Use microphone rather than linein in radio's audio codec
int mic_ptt_enabled = 0;
int mic_ptt_tip_bias_ring = 0;
atomic_int MIDI_cw_is_active = 0;
gboolean mute_rx_while_transmitting = TRUE;
int n_adc = 1;
int new_pa_board = 0; // Indicates Rev.24 PA board for HERMES/ANGELIA/ORION
int OCmemory_tune_time = 550; // ms
unsigned char OCtune = 0;
int ozy_software_version;
int pa_enabled = 0;                    // RX-ONLY: PA never enabled (upstream: 1)
int penelope_software_version;
double percent_pan_wf = 70.0;
int protocol;
int PS_RX_FEEDBACK;
int PS_TX_FEEDBACK;   /* used by m1_main.c */
DISCOVERED *radio = NULL;
int radio_ptt = 0;
RECEIVER *receiver[8];
int receivers;
int region = REGION_VFO;
int rx_gain_calibration = 0;
int save_zoom_state = 0;
TRANSMITTER *transmitter;
_Atomic int tune = 0;
long long tune_timeout;
const int tx_dialog_width = 240;
int tx_fifo_overrun = 0;
int tx_fifo_underrun = 0;
int tx_out_of_band_allowed = 0;

int split = 0;   /* from src/radio.c (used by the TX-VFO helpers in stubs.c) */
int tci_audio_monitor = 0;   /* from src/radio.c (buffered_audio.c) */
/* from src/radio.c, used by vfo.c */
int sat_mode;
int display_sliders = 0;
int locked = 0;
int disable_split_on_band_change = 0;
int vox_enabled = 0;

/* ---- from src/main.c, src/rigctl.c, src/transmitter.c (read by vfo.c's VFO bar) ---- */
int freq_bgcolor_alter = 0;
int cat_control = 0;
double ctcss_frequencies[CTCSS_FREQUENCIES] = {
  67.0,  71.9,  74.4,  77.0,  79.7,  82.5,  85.4,  88.5,  91.5,  94.8,
  97.4, 100.0, 103.5, 107.2, 110.9, 114.8, 118.8, 123.0, 127.3, 131.8,
  136.5, 141.3, 146.2, 151.4, 156.7, 162.2, 167.9, 173.8, 179.9, 186.2,
  192.8, 203.5, 210.7, 218.1, 225.7, 233.6, 241.8, 250.3
};
/* from src/radio.c: the local-audio ring's latency settings (buffered_audio.c) */
int rx_audio_network_reserve_enabled = 0;
int rx_audio_network_reserve_ms = 100;
int audio_rx_latency_correction_enabled = 1;

/* ---- from src/discovery.c ---- */
int discover_only_stemlab = 0;
char *ipaddr_radio = &ipaddr_buf[0];
char last_device_ip[INET_ADDRSTRLEN] = "";
int radio_port = 1024;  // Default discovery port

/* ---- from src/main.c ---- */
int display_debug = 0;
int iaru_region = 2;
int log_debug = 0;
int mouse_wheel_mode = 1;
int ui_debug = 0;

/* ---- src/vfo.c: compiled unchanged since M4 (vfo[], mode_settings[]) ---- */
