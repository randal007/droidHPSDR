/*
 * radio_core.c — starts and runs deskHPSDR's receive path for the app.
 *
 * The set-up follows headless/m1_main.c (milestone 1, tested on the radio),
 * which follows radio.c's start_radio() for an HL2. On top of M1 it turns on
 * upstream's display timer (receiver.c), runs a loop thread that plays GTK's
 * main loop for upstream code, and takes the UI's requests on that thread.
 *
 * RECEIVE ONLY, like M1: the transmitter is all zeros and the stubs in
 * headless/stubs.c keep the radio out of transmit (radio_is_transmitting()
 * is always 0, no PA, no keyer).
 *
 * Network: the Kotlin service binds the whole process to the Ethernet (or
 * WiFi) network before calling droid_radio_start, so the sockets that
 * old_discovery.c and old_protocol.c open use that network.
 *
 * GPL v3 or later, like the rest of droidHPSDR.
 */
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <gtk/gtk.h>

#include "adc.h"
#include "agc.h"
#include "audio.h"
#include "band.h"
#include "channel.h"
#include "discovered.h"
#include "discovery.h"
#include "filter.h"
#include "mode.h"
#include "old_discovery.h"
#include "old_protocol.h"
#include "radio.h"
#include "receiver.h"
#include "transmitter.h"
#include "vfo.h"
#include <wdsp.h>

#include "droid_app.h"
#include "droid_headless.h"

/* ---- requests from the UI, run on the loop thread ---- */

enum { CMD_FREQ, CMD_MODE, CMD_FILTER, CMD_VOLUME, CMD_LNA, CMD_PHONE_AUDIO };
typedef struct { int type; long long i; double d; int a, b; } cmd;
#define CMD_MAX 256
static cmd cmds[CMD_MAX];
static int cmd_head, cmd_tail;
static pthread_mutex_t cmd_lock = PTHREAD_MUTEX_INITIALIZER;

static void post(cmd c) {
  pthread_mutex_lock(&cmd_lock);
  /* A newer frequency replaces one still waiting, so a fast drag never lags. */
  if (c.type == CMD_FREQ || c.type == CMD_VOLUME || c.type == CMD_LNA || c.type == CMD_FILTER) {
    for (int k = cmd_tail; k != cmd_head; k = (k + 1) % CMD_MAX) {
      if (cmds[k].type == c.type) {
        cmds[k] = c;
        pthread_mutex_unlock(&cmd_lock);
        return;
      }
    }
  }
  int next = (cmd_head + 1) % CMD_MAX;
  if (next != cmd_tail) {
    cmds[cmd_head] = c;
    cmd_head = next;
  }
  pthread_mutex_unlock(&cmd_lock);
}

void droid_set_frequency(long long hz) { post((cmd){ .type = CMD_FREQ, .i = hz }); }
void droid_set_mode(int mode) { post((cmd){ .type = CMD_MODE, .a = mode }); }
void droid_set_filter(int low, int high) { post((cmd){ .type = CMD_FILTER, .a = low, .b = high }); }
void droid_set_volume_db(double db) { post((cmd){ .type = CMD_VOLUME, .d = db }); }
void droid_set_lna_db(double db) { post((cmd){ .type = CMD_LNA, .d = db }); }
void droid_set_phone_audio(int on) { post((cmd){ .type = CMD_PHONE_AUDIO, .a = on }); }

/* ---- when the radio was on which frequency ----
 * A spectrum row describes samples from some time before it is drawn (the
 * radio's round trip plus the analyzer's FFT window), so display_bridge.c
 * labels each row with the frequency of that moment, not of now. Without
 * this, rows drawn while tuning land in the wrong place and smear. */
#define FHIST 64
static struct { int64_t t_us; long long f; } fhist[FHIST];
static int fhist_n;
static pthread_mutex_t fhist_lock = PTHREAD_MUTEX_INITIALIZER;

static int64_t now_us(void) { return g_get_monotonic_time(); }

static void fhist_add(long long f) {
  pthread_mutex_lock(&fhist_lock);
  fhist[fhist_n % FHIST].t_us = now_us();
  fhist[fhist_n % FHIST].f = f;
  fhist_n++;
  pthread_mutex_unlock(&fhist_lock);
}

long long droid_freq_ago(int64_t age_us) {
  int64_t t = now_us() - age_us;
  long long f = vfo[0].frequency;
  pthread_mutex_lock(&fhist_lock);
  /* newest entry not after t */
  for (int k = fhist_n - 1; k >= 0 && k >= fhist_n - FHIST; k--) {
    if (fhist[k % FHIST].t_us <= t) {
      f = fhist[k % FHIST].f;
      break;
    }
    if (k == fhist_n - FHIST || k == 0) f = fhist[k % FHIST].f;   /* older than we remember */
  }
  pthread_mutex_unlock(&fhist_lock);
  return f;
}

static void run(const cmd *c) {
  RECEIVER *rx = receiver[0];
  switch (c->type) {
  case CMD_FREQ:
    if (c->i < 100000 || c->i > 61440000) break;   /* HL2 range */
    vfo[0].band = get_band_from_frequency(c->i);
    rx_set_frequency(rx, c->i);
    fhist_add(c->i);
    break;
  case CMD_MODE:
    if (c->a < 0 || c->a >= MODES) break;
    vfo[0].mode = c->a;
    vfo[0].filter = mode_settings[c->a].filter;
    rx_mode_changed(rx);
    break;
  case CMD_FILTER: {
    /* a variable filter, as deskHPSDR's filter menu sets one */
    FILTER *f = &filters[vfo[0].mode][filterVar1];
    f->low = c->a;
    f->high = c->b;
    vfo[0].filter = filterVar1;
    rx_filter_changed(rx);
    break;
  }
  case CMD_VOLUME:
    rx->volume = c->d;
    rx_set_af_gain(rx);
    break;
  case CMD_LNA:
    adc[0].gain = c->d < -12 ? -12 : c->d > 48 ? 48 : c->d;   /* old_protocol.c sends it */
    break;
  case CMD_PHONE_AUDIO:
    /* deskHPSDR's "local audio": receiver.c then also hands every audio
     * sample to buffered_audio.c, which plays it through audio_aaudio.c.
     * The HL2+ speaker keeps playing. */
    if (c->a && !rx->local_audio) {
      snprintf(rx->audio_name, sizeof rx->audio_name, "phone");
      if (audio_open_output(rx) == 0) rx->local_audio = 1;
      droid_log("phone audio %s", rx->local_audio ? "on" : "could not start");
    } else if (!c->a && rx->local_audio) {
      rx->local_audio = 0;
      audio_close_output(rx);
      droid_log("phone audio off");
    }
    break;
  }
}

/* ---- the loop thread: GTK's main loop for upstream code ---- */

static atomic_int running;
static pthread_t loop_thread;

static void *loop(void *arg) {
  (void)arg;
  while (atomic_load(&running)) {
    for (;;) {
      cmd c;
      pthread_mutex_lock(&cmd_lock);
      int have = cmd_tail != cmd_head;
      if (have) {
        c = cmds[cmd_tail];
        cmd_tail = (cmd_tail + 1) % CMD_MAX;
      }
      pthread_mutex_unlock(&cmd_lock);
      if (!have) break;
      run(&c);
    }
    droid_compat_run_pending();   /* g_idle_add / g_timeout_add, incl. the display timer */
    usleep(4000);
  }
  return NULL;
}

/* vfo.c's modesettingsRestoreState() defaults, as m1_main.c sets them. */
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

int droid_radio_start(const droid_start_opts *o, char *msg, int msglen) {
  if (atomic_load(&running)) return 0;
  droid_demo_stop();   /* the demo never feeds the display while the radio does */

  /* ---- discovery: deskHPSDR's own ---- */
  protocol = ORIGINAL_PROTOCOL;
  devices = 0;
  /* A known address (the wired link) uses upstream's targeted discovery:
   * a broadcast cannot work from an app, which cannot list the interfaces. */
  snprintf(ipaddr_radio, 64, "%s", o->radio_ip);
  old_discovery();
  int pick = -1;
  for (int i = 0; i < devices; i++) {
    const char *ip = inet_ntoa(discovered[i].info.network.address.sin_addr);
    droid_log("found: %s, device %d, gateware %d.%d, %s", discovered[i].name, discovered[i].device,
              discovered[i].software_version / 10, discovered[i].software_version % 10, ip);
    if (discovered[i].device != DEVICE_HERMES_LITE2) continue;
    if (o->radio_ip[0] && strcmp(o->radio_ip, ip) != 0) continue;
    if (pick < 0) pick = i;
  }
  if (pick < 0) {
    snprintf(msg, msglen, devices ? "found %d radio(s), but no Hermes Lite 2" : "no radio answered", devices);
    return -1;
  }

  /* ---- what radio.c's start_radio() sets for an HL2 (see m1_main.c) ---- */
  radio = &discovered[pick];
  device = radio->device;
  protocol = radio->protocol;
  n_adc = 1;
  filter_board = N2ADR;
  hl2_audio_codec = o->codec ? HL2_CODEC_AK4951 : HL2_CODEC_OFF;
  receivers = 1;
  PS_TX_FEEDBACK = 2;
  PS_RX_FEEDBACK = 3;

  adc[0].antenna = ANTENNA_1;
  adc[0].filters = AUTOMATIC;
  adc[0].hpf = HPF_13;
  adc[0].lpf = LPF_30_20;
  adc[0].gain = o->lna_db;
  adc[0].min_gain = -12.0;
  adc[0].max_gain = 48.0;

  mode_settings_defaults();
  for (int i = 0; i < MAX_VFOS; i++) {
    vfo[i].band = get_band_from_frequency(o->freq);
    vfo[i].frequency = o->freq;
    vfo[i].mode = o->mode;
    vfo[i].filter = mode_settings[o->mode].filter;
    vfo[i].deviation = 2500;
    vfo[i].step = 10;
    vfo[i].rit_step = 10;
  }

  /* WDSP channels are opened once per process; a restart reuses them. */
  if (receiver[0] == NULL) {
    for (int i = 0; i < 2; i++) {
      receiver[i] = rx_create_receiver(CHANNEL_RX0 + i, o->pixels, o->pixels, 200);
      receiver[i]->displaying = 0;
    }
    transmitter = calloc(1, sizeof(TRANSMITTER));   /* inert: see stubs.c */
  }
  active_receiver = receiver[0];
  can_transmit = 0;
  if (o->sample_rate != receiver[0]->sample_rate) rx_change_sample_rate(receiver[0], o->sample_rate);
  rx_mode_changed(receiver[0]);
  for (int i = 0; i < 2; i++) rx_frequency_changed(receiver[i]);

  /* upstream's display timer feeds display_bridge.c */
  RECEIVER *rx = receiver[0];
  rx->display_panadapter = 1;
  rx->display_waterfall = 0;
  rx->fps = o->fps;
  rx->displaying = 1;
  /* No WDSP display averaging: an averaged row blends the old and the new
   * frequency for a few hundred ms after every retune, which smears the 3D
   * view while tuning. gl_render.c smooths across rows instead, each row at
   * its own frequency. */
  rx->display_average_mode = AVG_NONE;
  rx_set_framerate(rx);   /* also rx_set_average() */

  droid_log("HL2 at %s: %.3f MHz %s, LNA %+.0f dB, HL2+ codec %s, %d Hz, %d px at %d fps",
            inet_ntoa(radio->info.network.address.sin_addr), o->freq / 1e6, mode_string[o->mode], o->lna_db,
            o->codec ? "on" : "off", rx->sample_rate, rx->pixels, rx->fps);
  snprintf(msg, msglen, "HL2 at %s, gateware %d.%d", inet_ntoa(radio->info.network.address.sin_addr),
           radio->software_version / 10, radio->software_version % 10);

  fhist_add(vfo[0].frequency);
  old_protocol_init(rx->sample_rate);
  atomic_store(&running, 1);
  pthread_create(&loop_thread, NULL, loop, NULL);
  return 0;
}

void droid_radio_stop(void) {
  if (!atomic_load(&running)) return;
  atomic_store(&running, 0);
  pthread_join(loop_thread, NULL);
  if (receiver[0] && receiver[0]->local_audio) {
    receiver[0]->local_audio = 0;
    audio_close_output(receiver[0]);
  }
  old_protocol_stop();
  droid_log("stream stopped");
  /* The receivers stay allocated: WDSP channels cannot be reopened cleanly in
   * the same process, so the service keeps the process for the next start. */
}

int droid_radio_running(void) { return atomic_load(&running); }

void droid_get_status(droid_status *s) {
  memset(s, 0, sizeof *s);
  s->running = atomic_load(&running);
  s->meter_dbm = droid_meter_dbm();      /* also fed by the demo */
  s->noise_floor = droid_noise_floor();
  if (!s->running || receiver[0] == NULL) return;
  s->freq = vfo[0].frequency;
  s->mode = vfo[0].mode;
  s->filter_low = receiver[0]->filter_low;
  s->filter_high = receiver[0]->filter_high;
  s->sample_rate = receiver[0]->sample_rate;
  s->meter_dbm = droid_meter_dbm();
  s->lost_packets = atomic_load(&droid_sequence_errors);
  s->adc_overloads = hl2_adc0_ovf_events;
  s->lna_db = adc[0].gain;
  s->volume_db = receiver[0]->volume;
  s->noise_floor = droid_noise_floor();
  s->phone_audio = receiver[0]->local_audio;
}

int droid_wisdom(const char *dir) { return WDSPwisdom((char *)dir); }
const char *droid_wisdom_status(void) { return wisdom_get_status(); }
