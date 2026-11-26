/*
 * m1_main.c — droidHPSDR milestone 1: headless receive test.
 *
 * Finds the radio, starts a Protocol 1 stream with deskHPSDR's own
 * old_protocol.c and receiver.c (compiled unchanged), runs WDSP on the
 * phone, and prints the S-meter. RECEIVE ONLY: see stubs.c for why this
 * build cannot put the radio into transmit.
 *
 *   m1-rx [-H nethandle] [-a ip] [-f hz] [-m usb|lsb|cwu|cwl|am] [-g gain_db]
 *         [-t seconds] [-C]
 *
 *   -H  Android network handle to bind to (the Ethernet network). Without
 *       it, unicast to a 169.254.x.x radio goes out over WiFi and is lost.
 *       run-m1.sh looks it up in `dumpsys connectivity`.
 *   -a  the radio's IP: targeted discovery instead of a broadcast (the wired
 *       link: -a 169.254.19.221)
 *   -f  frequency in Hz (default 14074000)
 *   -g  HL2 LNA gain in dB, -12..48 (default 20)
 *   -t  how long to run (default 30 s; Ctrl-C, or adb going away, stops
 *       early and still sends METIS STOP)
 *   -C  HL2+ codec OFF (default ON: demodulated audio to the HL2+ speaker)
 *
 * GPL v3 or later, like the rest of droidHPSDR.
 */
#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <gtk/gtk.h>

#ifdef __ANDROID__
#include <android/log.h>
#include <android/multinetwork.h>
#endif

#include "adc.h"
#include "band.h"
#include "channel.h"
#include "discovered.h"
#include "discovery.h"
#include "filter.h"
#include "mode.h"
#include "old_discovery.h"
#include "old_protocol.h"
#include "radio.h"
#include "radio_menu.h"
#include "receiver.h"
#include "transmitter.h"
#include "vfo.h"
#include "agc.h"

#include "droid_headless.h"



static volatile sig_atomic_t stop_requested;
static void on_signal(int sig) { (void)sig; stop_requested = 1; }

void droid_log(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vprintf(fmt, ap);
  va_end(ap);
  printf("\n");
#ifdef __ANDROID__
  va_start(ap, fmt);
  __android_log_vprint(ANDROID_LOG_INFO, "droidHPSDR", fmt, ap);
  va_end(ap);
#endif
}

static int parse_mode(const char *s) {
  if (!strcmp(s, "lsb")) return modeLSB;
  if (!strcmp(s, "cwu")) return modeCWU;
  if (!strcmp(s, "cwl")) return modeCWL;
  if (!strcmp(s, "am")) return modeAM;
  if (!strcmp(s, "usb")) return modeUSB;
  return -1;
}

/* vfo.c's modesettingsRestoreState() defaults for the fields that matter
 * without a props file; everything else stays 0 (NB, NR, EQ off). */
static void mode_settings_defaults(void) {
  for (int i = 0; i < MODES; i++) {
    switch (i) {
    case modeLSB: case modeUSB: case modeDSB:
      mode_settings[i].agc = AGC_MEDIUM; mode_settings[i].filter = filterF5;
      mode_settings[i].step = 100; mode_settings[i].rit_step = 100;
      break;
    case modeDIGL: case modeDIGU:
      mode_settings[i].agc = AGC_FAST; mode_settings[i].filter = filterF6;
      mode_settings[i].step = 50; mode_settings[i].rit_step = 100;
      break;
    case modeCWL: case modeCWU:
      mode_settings[i].agc = AGC_FAST; mode_settings[i].filter = filterF4;
      mode_settings[i].step = 25; mode_settings[i].rit_step = 10;
      break;
    default:
      mode_settings[i].agc = AGC_MEDIUM; mode_settings[i].filter = filterF3;
      mode_settings[i].step = 100; mode_settings[i].rit_step = 100;
      break;
    }
    mode_settings[i].nb_tau = 0.00001;
    mode_settings[i].nb_advtime = 0.00001;
    mode_settings[i].nb_hang = 0.00001;
    mode_settings[i].nb_thresh = 4.95;
    mode_settings[i].nr2_ae = 1;
  }
}

int main(int argc, char **argv) {
  unsigned long long net_handle = 0;
  long long freq = 14074000;
  int mode = modeUSB;
  double gain = 20.0;
  int seconds = 30;
  int codec = 1;
  const char *ip = "";
  int opt;

  setvbuf(stdout, NULL, _IOLBF, 0);
  while ((opt = getopt(argc, argv, "H:a:f:m:g:t:C")) != -1) {
    switch (opt) {
    case 'H': net_handle = strtoull(optarg, NULL, 10); break;
    case 'a': ip = optarg; break;
    case 'f': freq = atoll(optarg); break;
    case 'm': mode = parse_mode(optarg); break;
    case 'g': gain = atof(optarg); break;
    case 't': seconds = atoi(optarg); break;
    case 'C': codec = 0; break;
    default:
      mode = -1;
      break;
    }
    if (mode < 0) {
      fprintf(stderr, "usage: %s [-H nethandle] [-a ip] [-f hz] [-m usb|lsb|cwu|cwl|am] [-g dB] [-t s] [-C]\n",
              argv[0]);
      return 2;
    }
  }
  signal(SIGINT, on_signal);
  signal(SIGTERM, on_signal);
  /* `adb shell` without a pty: Ctrl-C on the PC, or adb over WiFi dropping,
   * reaches us as SIGHUP. Stop cleanly then too, so the radio gets METIS STOP. */
  signal(SIGHUP, on_signal);
  signal(SIGPIPE, SIG_IGN);

  droid_log("droidHPSDR M1 headless RX — RECEIVE ONLY, transmit is compiled out");

#ifdef __ANDROID__
  if (net_handle) {
    if (android_setprocnetwork((net_handle_t)net_handle) != 0) {
      droid_log("android_setprocnetwork(%llu) failed: %s", net_handle, strerror(errno));
      return 1;
    }
    droid_log("sockets bound to network handle %llu", net_handle);
  } else {
    droid_log("no -H: using the default network (unicast to 169.254.x.x will fail)");
  }
#endif

  /* ---- discovery: deskHPSDR's own, broadcast on every interface ---- */
  protocol = ORIGINAL_PROTOCOL;
  snprintf(ipaddr_radio, 64, "%s", ip);   /* empty: broadcast */
  old_discovery();
  int pick = -1;
  for (int i = 0; i < devices; i++) {
    const char *at = inet_ntoa(discovered[i].info.network.address.sin_addr);
    droid_log("found: %s, device %d, gateware %d.%d, %s, via %s", discovered[i].name,
              discovered[i].device, discovered[i].software_version / 10, discovered[i].software_version % 10,
              at, discovered[i].info.network.interface_name);
    if (ip[0] && strcmp(ip, at) != 0) continue;
    if (pick < 0 && discovered[i].device == DEVICE_HERMES_LITE2) pick = i;
  }
  if (pick < 0) {
    droid_log("no Hermes Lite 2 found");
    return 1;
  }

  /* ---- what radio.c's start_radio() sets for an HL2 ---- */
  radio = &discovered[pick];
  device = radio->device;
  protocol = radio->protocol;
  n_adc = 1;
  filter_board = N2ADR;
  n2adr_oc_settings();   /* radio.c does this for every HL2: the filter board follows the band */
  hl2_audio_codec = codec ? HL2_CODEC_AK4951 : HL2_CODEC_OFF;
  receivers = 1;
  PS_TX_FEEDBACK = 2;
  PS_RX_FEEDBACK = 3;

  adc[0].antenna = ANTENNA_1;
  adc[0].filters = AUTOMATIC;
  adc[0].hpf = HPF_13;
  adc[0].lpf = LPF_30_20;
  adc[0].gain = gain;
  adc[0].min_gain = -12.0;   /* radio.c's HL2 values */
  adc[0].max_gain = 48.0;

  for (int i = 0; i < MAX_VFOS; i++) {
    vfo[i].band = get_band_from_frequency(freq);
    vfo[i].frequency = freq;
    vfo[i].mode = mode;
    vfo[i].filter = mode_settings[mode].filter;
    vfo[i].deviation = 2500;
    vfo[i].step = 100;
    vfo[i].rit_step = 10;
  }
  mode_settings_defaults();
  for (int i = 0; i < MAX_VFOS; i++) vfo[i].filter = mode_settings[mode].filter;

  /* ---- receiver (deskHPSDR's, unchanged) and an inert transmitter ---- */
  /* Like radio.c: create both receivers (old_protocol.c reads receiver[1]
   * unconditionally), show/use only the first (receivers = 1). */
  for (int i = 0; i < 2; i++) {
    receiver[i] = rx_create_receiver(CHANNEL_RX0 + i, 1024, 1024, 200);
    receiver[i]->displaying = 0;
  }
  active_receiver = receiver[0];
  for (int i = 0; i < 2; i++) rx_frequency_changed(receiver[i]);
  /* All zeros: drive 0, no PureSignal, no local mic, no WDSP TX channel.
   * old_protocol.c reads it; nothing can make it transmit (stubs.c). */
  transmitter = calloc(1, sizeof(TRANSMITTER));
  can_transmit = 0;

  droid_log("HL2 at %s: %.3f MHz %s, LNA %+.0f dB, HL2+ codec %s, %d Hz",
            inet_ntoa(radio->info.network.address.sin_addr), freq / 1e6, mode_string[mode], gain,
            codec ? "ON" : "off", receiver[0]->sample_rate);

  /* ---- stream ---- */
  old_protocol_init(receiver[0]->sample_rate);

  struct timespec t0, now;
  clock_gettime(CLOCK_MONOTONIC, &t0);
  int ticks = 0;
  while (!stop_requested && !atomic_load(&droid_fatal_seen)) {
    usleep(100000);
    droid_compat_run_pending();   /* the g_idle_add queue (UI notifications) */
    clock_gettime(CLOCK_MONOTONIC, &now);
    double elapsed = (now.tv_sec - t0.tv_sec) + (now.tv_nsec - t0.tv_nsec) * 1e-9;
    if (++ticks % 5 == 0) {
      /* calibrated like receiver.c's rx_update_display(): WDSP's raw meter
       * reads high by the LNA gain (N2ADR board: no Alex/Charly25 terms) */
      const BAND *band = band_get_band(vfo[0].band);
      double dbm = rx_get_smeter(receiver[0]) + (double)(rx_gain_calibration - band->gain)
                   + (double)adc[0].attenuation - adc[0].gain;
      droid_log("t=%5.1fs  S-meter %7.1f dBm  lost packets %u  ADC overloads %d",
                elapsed, dbm, atomic_load(&droid_sequence_errors), hl2_adc0_ovf_events);
    }
    if (elapsed >= seconds) break;
  }

  droid_log("stopping stream");
  old_protocol_stop();
  usleep(200000);
  if (atomic_load(&droid_fatal_seen)) {
    droid_log("failed (FATAL above)");
    return 1;
  }
  droid_log("done");
  return 0;
}
