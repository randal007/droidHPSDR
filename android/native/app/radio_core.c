/*
 * radio_core.c — starts and runs deskHPSDR's receive path for the app.
 *
 * The set-up follows headless/m1_main.c (milestone 1, tested on the radio),
 * which follows radio.c's start_radio() for an HL2. On top of M1 it turns on
 * upstream's display timer (receiver.c), runs a loop thread that plays GTK's
 * main loop for upstream code, and takes the UI's requests on that thread.
 *
 * Transmit (M7): upstream's transmitter.c with app/transmit.c, made at the
 * first start with a real radio; an IQ recording never transmits
 * (can_transmit = 0 while one plays). Before any stop the radio goes back
 * to receive (droid_tx_off_now).
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
#include <errno.h>
#include <unistd.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
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
#include "main.h"
#include "mode.h"
#include "old_discovery.h"
#include "old_protocol.h"
#include "radio.h"
#include "radio_menu.h"
#include "receiver.h"
#include "transmitter.h"
#include "vfo.h"
#include "store.h"
#include "store_menu.h"
#include "property.h"
#include <wdsp.h>

#include "droid_app.h"
#include "droid_headless.h"

/* ---- requests from the UI, run on the loop thread ---- */

enum { CMD_FREQ, CMD_MODE, CMD_FILTER, CMD_VOLUME, CMD_LNA, CMD_PHONE_AUDIO, CMD_ZOOM, CMD_FPS, CMD_WINDOW, CMD_DETECTOR, CMD_PAN_FFT, CMD_HL2, CMD_ANT, CMD_RIT_STEP,
       CMD_FILTER_PRESET, CMD_BAND, CMD_BANDSTACK, CMD_VFO_OP, CMD_RIT, CMD_CTUN, CMD_LOCK, CMD_STEP, CMD_MEMORY,
       CMD_AGC, CMD_SAVE, CMD_DSP, CMD_TX, CMD_NONE };
typedef struct { int type; long long i; double d; int a, b; } cmd;
#define CMD_MAX 256
static cmd cmds[CMD_MAX];
static int cmd_head, cmd_tail;
static pthread_mutex_t cmd_lock = PTHREAD_MUTEX_INITIALIZER;

static void post(cmd c) {
  pthread_mutex_lock(&cmd_lock);
  /* A newer frequency replaces one still waiting, so a fast drag never lags.
   * Only in place when it is the last one queued; otherwise the old one is
   * dropped and the new one goes to the end, so it never runs before
   * commands sent after the old one (a filter drag, then a mode change). */
  int room = (cmd_head + 1) % CMD_MAX != cmd_tail;
  if (c.type == CMD_FREQ || c.type == CMD_VOLUME || c.type == CMD_LNA || c.type == CMD_FILTER || c.type == CMD_ZOOM ||
      c.type == CMD_DSP || c.type == CMD_TX) {
    int last = (cmd_head + CMD_MAX - 1) % CMD_MAX;
    for (int k = cmd_tail; k != cmd_head; k = (k + 1) % CMD_MAX) {
      if (cmds[k].type == c.type && ((c.type != CMD_DSP && c.type != CMD_TX) || cmds[k].a == c.a)) {
        if (!room) {
          /* full: take the old one out, closing the gap, so the new one still goes to the end */
          for (int j = k; j != last; j = (j + 1) % CMD_MAX) cmds[j] = cmds[(j + 1) % CMD_MAX];
          cmd_head = last;
          break;
        }
        if (k == last) {
          cmds[k] = c;
          pthread_mutex_unlock(&cmd_lock);
          return;
        }
        cmds[k].type = CMD_NONE;
        break;
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
void droid_set_zoom(int zoom) { post((cmd){ .type = CMD_ZOOM, .a = zoom }); }
void droid_set_fps(int fps) { post((cmd){ .type = CMD_FPS, .a = fps }); }
void droid_set_window(int type) { post((cmd){ .type = CMD_WINDOW, .a = type }); }
void droid_set_detector(int det) { post((cmd){ .type = CMD_DETECTOR, .a = det }); }
void droid_set_pan_fft(int size) { post((cmd){ .type = CMD_PAN_FFT, .a = size }); }
void droid_hl2_set(int field, double v) { post((cmd){ .type = CMD_HL2, .a = field, .d = v }); }
void droid_set_band_rx_ant(int band, int ant) { post((cmd){ .type = CMD_ANT, .a = band, .b = ant }); }
void droid_set_rit_step(int hz) { post((cmd){ .type = CMD_RIT_STEP, .a = hz }); }
int droid_band_rx_ant(int band) { return band >= 0 && band < BANDS ? band_get_band(band)->alexRxAntenna : 0; }
/* the region's 60 m channels, copied on the loop thread when the region changes (radio_change_region()
 * writes the count and the table pointer separately), read by the UI under the lock */
static pthread_mutex_t ch60_lock = PTHREAD_MUTEX_INITIALIZER;
static long long ch60_f[16], ch60_w[16];
static int ch60_n;
static void snapshot_60m(void) {
  pthread_mutex_lock(&ch60_lock);
  ch60_n = 0;
  if (region != REGION_VFO && band_channels_60m != NULL)   /* "NONE (VFO)": no markers (rx_panadapter.c) */
    for (int i = 0; i < channel_entries && i < 16; i++, ch60_n++) {
      ch60_f[i] = band_channels_60m[i].frequency;
      ch60_w[i] = band_channels_60m[i].width;
    }
  pthread_mutex_unlock(&ch60_lock);
}
int droid_channels_60m(long long *freq, long long *width, int max) {
  pthread_mutex_lock(&ch60_lock);
  int n = ch60_n < max ? ch60_n : max;
  for (int i = 0; i < n; i++) { freq[i] = ch60_f[i]; width[i] = ch60_w[i]; }
  pthread_mutex_unlock(&ch60_lock);
  return n;
}
void droid_filter_preset(int index) { post((cmd){ .type = CMD_FILTER_PRESET, .a = index }); }
void droid_band(int band) { post((cmd){ .type = CMD_BAND, .a = band }); }
void droid_bandstack(int entry) { post((cmd){ .type = CMD_BANDSTACK, .a = entry }); }
void droid_vfo_op(int op) { post((cmd){ .type = CMD_VFO_OP, .a = op }); }
void droid_rit(int on, long long hz) { post((cmd){ .type = CMD_RIT, .a = on, .i = hz }); }
void droid_ctun(int on) { post((cmd){ .type = CMD_CTUN, .a = on }); }
void droid_lock(int on) { post((cmd){ .type = CMD_LOCK, .a = on }); }
void droid_step(int hz) { post((cmd){ .type = CMD_STEP, .a = hz }); }
void droid_memory(int store, int slot) { post((cmd){ .type = CMD_MEMORY, .a = store, .b = slot }); }
void droid_agc(int agc) { post((cmd){ .type = CMD_AGC, .a = agc }); }
void droid_save(void) { post((cmd){ .type = CMD_SAVE }); }
void droid_dsp_set(int field, double value) { post((cmd){ .type = CMD_DSP, .a = field, .d = value }); }
void droid_tx_set(int field, double value) {
  if (field == TX_PTT && value != 0) droid_tx_ptt_seen();   /* the finger is still down, even if the queue is slow */
  post((cmd){ .type = CMD_TX, .a = field, .d = value });
}

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

/* ---- deskHPSDR's props file: radio.c's save and restore order ---- */

static char props_path[400];

static void droid_radio_save_now(void) {
  /* a recording's state (its centre, CTUN on) is not the radio's: never saved */
  if (!props_path[0] || receiver[0] == NULL || atomic_load(&droid_playback_on)) return;
  clearProperties();
  for (int i = 0; i < 2; i++) rx_save_state(receiver[i]);
  filterSaveState();
  bandSaveState();
  memSaveState();
  vfo_save_state();
  /* radio.c's own entries for what the RX menu sets globally */
  SetPropI0("rx_audio_network_reserve_enabled", rx_audio_network_reserve_enabled);
  SetPropI0("rx_audio_network_reserve_ms", rx_audio_network_reserve_ms);
  SetPropI0("autogain_enabled", autogain_enabled);
  SetPropI0("autogain_time_enabled", autogain_time_enabled);
  droid_tx_save();
  saveProperties(props_path);
}

static void run(const cmd *c) {
  /* An IQ recording can't be retuned: tune inside it like CTUN, and leave
   * out what would move its centre (band, band stack, memories, VFO swaps). */
  if (atomic_load(&droid_playback_on)) {
    switch (c->type) {
    case CMD_FREQ: {
      if (locked) return;
      long long half = receiver[0]->sample_rate / 2 - 3000, f = c->i;
      if (f < vfo[0].frequency - half) f = vfo[0].frequency - half;
      if (f > vfo[0].frequency + half) f = vfo[0].frequency + half;
      vfo[0].ctun = 1;
      vfo[0].ctun_frequency = f;
      rx_frequency_changed(receiver[0]);
      return;
    }
    case CMD_BAND: case CMD_BANDSTACK: case CMD_VFO_OP: case CMD_CTUN: case CMD_MEMORY:
      return;
    default:
      break;
    }
  }
  RECEIVER *rx = receiver[0];
  switch (c->type) {
  case CMD_FREQ:
    /* vfo.c's frequency set (the one CAT uses): moves the band with the
     * frequency, leaves the band stacks alone. Like vfo_move(), not when locked. */
    if (c->i < 100000 || c->i > 61440000 || locked) break;   /* HL2 range */
    vfo_set_frequency(VFO_A, c->i);
    /* the rows' centre: with CTUN only ctun_frequency moves (receiver.c) */
    fhist_add(vfo[0].frequency);
    break;
  case CMD_MODE:
    if (c->a < 0 || c->a >= MODES) break;
    vfo_id_mode_changed(0, c->a);   /* with the mode's own settings (filter, AGC, NR, step) */
    break;
  case CMD_FILTER: {
    /* dragging the passband edges: the Var1 filter, as deskHPSDR's filter menu sets it */
    FILTER *f = &filters[vfo[0].mode][filterVar1];
    f->low = c->a;
    f->high = c->b;
    vfo_filter_changed(filterVar1);
    break;
  }
  case CMD_FILTER_PRESET:
    if (c->a < 0 || c->a >= FILTERS) break;
    vfo_filter_changed(c->a);
    break;
  case CMD_BAND:
    if (c->a < 0 || c->a >= BANDS) break;
    vfo_band_changed(0, c->a);
    fhist_add(vfo[0].frequency);
    break;
  case CMD_BANDSTACK:
    /* the UI picked from the list of the band it showed; vfo.c checks nothing */
    if (c->a < 0 || c->a >= bandstack_get_bandstack(vfo[0].band)->entries) break;
    vfo_bandstack_changed(c->a);
    fhist_add(vfo[0].frequency);
    break;
  case CMD_VFO_OP:
    if (c->a == 0) vfo_a_to_b();
    else if (c->a == 1) vfo_b_to_a();
    else vfo_a_swap_b();
    fhist_add(vfo[0].frequency);
    break;
  case CMD_RIT:
    vfo_rit_value(0, c->i);
    vfo_rit_onoff(0, c->a);
    break;
  case CMD_CTUN:
    vfo_ctun_update(0, c->a);
    fhist_add(vfo[0].frequency);   /* CTUN off moves the centre to the listening frequency */
    break;
  case CMD_LOCK:
    locked = c->a ? 1 : 0;
    break;
  case CMD_STEP:
    /* vfo.c vfo_set_step_from_index(): the step is kept per mode */
    if (c->a <= 0) break;
    vfo[0].step = c->a;
    mode_settings[vfo[0].mode].step = c->a;
    copy_mode_settings(vfo[0].mode);
    break;
  case CMD_MEMORY:
    if (c->b < 0 || c->b >= NUM_OF_MEMORYS) break;
    if (c->a) store_memory_slot(c->b);
    else { recall_memory_slot(c->b); fhist_add(vfo[0].frequency); }
    break;
  case CMD_AGC:
    if (c->a < AGC_OFF || c->a > AGC_FAST) break;
    rx->agc = c->a;
    rx_set_agc(rx);
    break;
  case CMD_SAVE:
    droid_radio_save_now();
    break;
  case CMD_DSP:
    droid_dsp_apply(c->a, c->d);
    break;
  case CMD_TX:
    droid_tx_apply(c->a, c->d);
    break;
  case CMD_VOLUME:
    rx->volume = c->d;
    rx_set_af_gain(rx);
    break;
  case CMD_LNA:
    adc[0].gain = c->d < -12 ? -12 : c->d > 48 ? 48 : c->d;   /* old_protocol.c sends it */
    break;
  case CMD_ZOOM:
    /* zoompan.c's set_zoom(): more analyzer pixels, so pinching in gets real
     * detail instead of a stretched picture. With CTUN off upstream keeps
     * the zoomed window centred on the VFO. */
    if (c->a < 1 || c->a > 16 || c->a == rx->zoom) break;
    g_mutex_lock(&rx->display_mutex);
    rx->zoom = c->a;
    rx_update_zoom_locked(rx);
    g_mutex_unlock(&rx->display_mutex);
    break;
  /* deskHPSDR's Display and Screen menus: field, then upstream's setter */
  case CMD_FPS:
    if (c->a < 5 || c->a > 60 || c->a == rx->fps) break;
    rx->fps = c->a;
    rx_set_framerate(rx);
    break;
  case CMD_WINDOW:   /* 0 Rectangular .. 5 Kaiser (default) .. 6 7-term Blackman-Harris */
    if (c->a < 0 || c->a > 6) break;
    rx->pan_window_type = c->a;
    rx_set_analyzer(rx);
    break;
  case CMD_DETECTOR:   /* 0 Peak, 1 Average, 2 Rosenfell, 3 Sample */
    if (c->a < 0 || c->a > 3) break;
    rx->display_detector_mode = DET_PEAK + c->a;
    rx_set_detector(rx);
    break;
  case CMD_HL2:   /* the HL2's lines of deskHPSDR's Radio menu (M6, picture F1) */
    switch (c->a) {
    case HL2_CODEC: hl2_audio_codec = c->d != 0 ? HL2_CODEC_AK4951 : HL2_CODEC_OFF; break;
    case HL2_FILTER_BOARD:   /* like radio_menu.c's filter_cb: the board choice sets the OC bits */
      filter_board = c->d != 0 ? N2ADR_TX : N2ADR;
      if (filter_board == N2ADR) n2adr_oc_settings(); else n2adr_oc_settings_tx();
      break;
    case HL2_HPF:
      n2adr_hpf_enable = c->d != 0;
      if (filter_board == N2ADR_TX) n2adr_oc_settings_tx();
      break;
    case HL2_PPM: ppm_factor = c->d < -100 ? -100 : c->d > 100 ? 100 : c->d; break;
    case HL2_ADC_CAL: rx_gain_calibration = (int)lround(c->d); break;
    case HL2_FORCE_IOB: force_iob = c->d != 0; break;
    case HL2_SPLIT_OFF_BAND: disable_split_on_band_change = c->d != 0; break;   /* vfo.c reads it */
    case HL2_REGION_60M:   /* radio_menu.c's region_cb: 60 m channel markers and the 60 m band stack */
      if (c->d >= REGION_VFO && c->d <= REGION_CA && (int)c->d != region) radio_change_region((int)c->d);
      snapshot_60m();
      break;
    case HL2_IARU_REGION:   /* radio_menu.c's iaru_region_cb: the 80 m / 40 m band stacks follow it */
      if (c->d >= 1 && c->d <= 3 && (int)c->d != iaru_region) { iaru_region = (int)c->d; band_apply_iaru_region(); }
      break;
    }
    break;
  case CMD_ANT:   /* ant_menu.c's rx_ant_cb: the band's RX antenna, applied now if it is the band in use */
    if (c->a < 0 || c->a >= BANDS || c->b < 0 || c->b > 5) break;
    band_get_band(c->a)->alexRxAntenna = c->b;
    if (vfo[VFO_A].band == c->a) radio_set_alex_antennas();
    break;
  case CMD_RIT_STEP:   /* radio_menu.c's rit_cb */
    if (c->a != 1 && c->a != 10 && c->a != 100) break;
    vfo_set_rit_step(c->a);
    break;
  case CMD_PAN_FFT:   /* Screen menu "FFT size": 0 Auto (upstream picks from width x zoom), or a fixed size */
    if (c->a != 0 && c->a != 16384 && c->a != 32768 && c->a != 65536 && c->a != 131072 && c->a != 262144) break;
    if (c->a == rx->pan_fft_size) break;
    rx->pan_fft_size = c->a;
    rx_set_analyzer(rx);
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
static int p1_started;
static DISCOVERED *real_radio;   /* the HL2 discovery found (radio points elsewhere while a recording plays) */
static DISCOVERED pb_radio;      /* the recording, as a radio */
/* What a recording may change (tuning, mode, filters, per-mode NR/AGC/step) is deskHPSDR's global
 * state, shared with the radio: snapshotted when a recording starts, put back when it stops, so
 * nothing from the demo reaches the radio or its props file. */
static __typeof__(vfo[0]) pb_saved_vfos[MAX_VFOS];
static FILTER pb_saved_filters[MODES][FILTERS];
static __typeof__(mode_settings[0]) pb_saved_modes[MODES];
static int pb_seeded;   /* a recording made the receivers with no props of the radio: seed the VFOs at its first start */
static TRANSMITTER inert_tx;     /* before the real transmitter exists (vfo.c reads transmitter->...) */

DISCOVERED *droid_real_radio(void) { return real_radio; }

/* The newest <MAC>.props in dir (a real radio's, not the all-zero one), so a
 * recording played before the radio this run still uses the radio's settings. */
static int newest_props(const char *dir, char *out, size_t len) {
  DIR *d = opendir(dir);
  if (!d) return 0;
  struct dirent *e;
  time_t best = 0;
  while ((e = readdir(d)) != NULL) {
    size_t n = strlen(e->d_name);
    if (n != 23 || strcmp(e->d_name + 17, ".props") != 0 || strncmp(e->d_name, "00-00-00-00-00-00", 17) == 0) continue;
    char p[512];
    struct stat st;
    snprintf(p, sizeof p, "%s/%s", dir, e->d_name);
    if (stat(p, &st) == 0 && st.st_mtime > best) { best = st.st_mtime; snprintf(out, len, "%s", p); }
  }
  closedir(d);
  return best != 0;
}   /* old_protocol_init done: the radio is found and the protocol set up */

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
    droid_compat_run_pending();   /* g_idle_add / g_timeout_add, incl. the display timers */
    droid_tx_poll();              /* the screen PTT's repeats, the TX timeout */
    usleep(4000);
  }
  return NULL;
}

/* ---- a new network under old_protocol.c's socket ----
 * old_protocol.c opens its UDP socket once ("once opened is never closed")
 * in a static function: on a desktop the network never changes. On the phone
 * unplugging the USB Ethernet adapter makes a new Android network, and the old
 * socket stays tied to the gone one ("Network is unreachable"). Without
 * editing upstream: find that socket (UDP, bound to our interface address),
 * make a new one on the current network with the same options, and put it
 * under the same descriptor number with dup2(). The receive thread's 100 ms
 * timeout lets it pick up the new socket on its next read. */
static void relink_protocol_socket(void) {
  const struct sockaddr_in *ia = &radio->info.network.interface_address;
  for (int fd = 3; fd < 1024; fd++) {
    int type;
    socklen_t l = sizeof type;
    if (getsockopt(fd, SOL_SOCKET, SO_TYPE, &type, &l) < 0 || type != SOCK_DGRAM) continue;
    struct sockaddr_in a;
    socklen_t al = sizeof a;
    if (getsockname(fd, (struct sockaddr *)&a, &al) < 0 || a.sin_family != AF_INET ||
        a.sin_addr.s_addr != ia->sin_addr.s_addr) continue;
    int nfd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (nfd < 0) return;
    int one = 1, v;
    socklen_t vl = sizeof v;
    setsockopt(nfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    setsockopt(nfd, SOL_SOCKET, SO_REUSEPORT, &one, sizeof one);
    /* the kernel reports buffer sizes doubled: set half of what it reports */
    if (getsockopt(fd, SOL_SOCKET, SO_RCVBUF, &v, &vl) == 0) { v /= 2; setsockopt(nfd, SOL_SOCKET, SO_RCVBUF, &v, sizeof v); }
    vl = sizeof v;
    if (getsockopt(fd, SOL_SOCKET, SO_SNDBUF, &v, &vl) == 0) { v /= 2; setsockopt(nfd, SOL_SOCKET, SO_SNDBUF, &v, sizeof v); }
    vl = sizeof v;
    if (getsockopt(fd, IPPROTO_IP, IP_TOS, &v, &vl) == 0) setsockopt(nfd, IPPROTO_IP, IP_TOS, &v, sizeof v);
    struct timeval tv;
    socklen_t tl = sizeof tv;
    if (getsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, &tl) == 0) setsockopt(nfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    if (bind(nfd, (const struct sockaddr *)ia, radio->info.network.interface_length) < 0) {
      droid_log("relink: bind failed: %s", strerror(errno));
      close(nfd);
      return;
    }
    dup2(nfd, fd);
    close(nfd);
    droid_log("protocol socket %d moved to the current network", fd);
    return;
  }
}

/* Start and stop come from several Android threads (start thread, watchdog,
 * network callback, the service): one at a time. */
static pthread_mutex_t life_lock = PTHREAD_MUTEX_INITIALIZER;
static int radio_start_locked(const droid_start_opts *o, char *msg, int msglen);

int droid_radio_start(const droid_start_opts *o, char *msg, int msglen) {
  pthread_mutex_lock(&life_lock);
  int r = radio_start_locked(o, msg, msglen);
  pthread_mutex_unlock(&life_lock);
  return r;
}

static int radio_start_locked(const droid_start_opts *o, char *msg, int msglen) {
  if (atomic_load(&running)) return 0;
  droid_demo_stop();   /* the demo never feeds the display while the radio does */

  /* ---- an IQ recording instead of the radio (the demo band) ---- */
  int pb_rate = 0;
  const int pb = o->playback[0] != 0;
  if (pb) {
    if (droid_playback_open(o->playback, &pb_rate, msg, msglen) != 0) return -1;
    memset(&pb_radio, 0, sizeof pb_radio);
    pb_radio.device = DEVICE_HERMES_LITE2;
    pb_radio.protocol = ORIGINAL_PROTOCOL;
    snprintf(pb_radio.name, sizeof pb_radio.name, "IQ recording");
    radio = &pb_radio;
    goto setup;
  }
  if (real_radio) radio = real_radio;

  /* ---- discovery: deskHPSDR's own; once, like old_protocol_init ---- */
  if (p1_started) {
    /* old_protocol.c keeps the radio's address from its one-time init
     * (static data_addr): a different address can't be picked up without
     * a fresh process. Say so instead of "running" with no data. */
    const char *was = inet_ntoa(radio->info.network.address.sin_addr);
    if (o->radio_ip[0] && strcmp(o->radio_ip, was) != 0) {
      snprintf(msg, msglen, "The radio address changed (%s to %s): close droidHPSDR "
               "(swipe it away in Recent apps) and open it again", was, o->radio_ip);
      return -1;
    }
    goto setup;
  }
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
  radio = &discovered[pick];   /* kept for every later start */
  real_radio = radio;

setup:
  /* ---- what radio.c's start_radio() sets for an HL2 (see m1_main.c) ---- */
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

  /* WDSP channels are opened once per process; a restart reuses them. */
  if (receiver[0] == NULL) {
    /* radio.c's radio_restore_state(): the props file is named after the
     * radio's MAC, as deskHPSDR names it, so a desktop profile can be copied
     * over. Then the receivers restore their own settings as they are made. */
    const unsigned char *m = radio->info.network.mac_address;
    snprintf(props_path, sizeof props_path, "%s/%02X-%02X-%02X-%02X-%02X-%02X.props", o->props_dir, m[0], m[1],
             m[2], m[3], m[4], m[5]);
    /* a recording first: the radio's own settings, from its last run (never saved over: see save_now) */
    if (pb && !newest_props(o->props_dir, props_path, sizeof props_path)) props_path[0] = 0;
    int have_props = o->props_dir[0] && access(props_path, R_OK) == 0;
    if (have_props) {
      loadProperties(props_path);
      GetPropI0("rx_audio_network_reserve_enabled", rx_audio_network_reserve_enabled);
      GetPropI0("rx_audio_network_reserve_ms", rx_audio_network_reserve_ms);
      GetPropI0("autogain_enabled", autogain_enabled);
      GetPropI0("autogain_time_enabled", autogain_time_enabled);
    }
    filterRestoreState();
    bandRestoreState();
    /* like radio.c after the props file: the HL2's N2ADR filter board switches with the band */
    n2adr_oc_settings();
    memRestoreState();
    vfo_restore_state();
    if (!have_props && pb) pb_seeded = 1;
    if (!have_props) {
      /* first start: no deskHPSDR settings yet, start where the app was */
      for (int i = 0; i < MAX_VFOS; i++) {
        vfo[i].band = get_band_from_frequency(o->freq);
        vfo[i].frequency = o->freq;
        vfo[i].mode = o->mode;
        vfo[i].filter = mode_settings[o->mode].filter;
      }
    }
    droid_log("settings: %s (%s)", props_path, have_props ? "loaded" : "new");
    for (int i = 0; i < 2; i++) {
      receiver[i] = rx_create_receiver(CHANNEL_RX0 + i, o->pixels, o->pixels, 200);
      receiver[i]->displaying = 0;
    }
    transmitter = &inert_tx;   /* until a real radio starts: all zeros, never transmits */
  }
  active_receiver = receiver[0];
  /* the transmitter, as radio.c's start_radio() makes it: once, with the first real radio
   * (its WDSP channel can't be reopened); an IQ recording never transmits */
  if (!pb) droid_tx_create(o->pixels);
  can_transmit = !pb && transmitter != &inert_tx;
  if (!pb && o->props_dir[0]) {
    /* a real radio saves to its own file, also when a recording made the receivers first */
    const unsigned char *m = radio->info.network.mac_address;
    snprintf(props_path, sizeof props_path, "%s/%02X-%02X-%02X-%02X-%02X-%02X.props", o->props_dir, m[0], m[1],
             m[2], m[3], m[4], m[5]);
  }
  if (!pb && pb_seeded) {
    /* the radio's first start after a recording made the receivers: start where the app was */
    for (int i = 0; i < MAX_VFOS; i++) {
      vfo[i].band = get_band_from_frequency(o->freq);
      vfo[i].frequency = o->freq;
      vfo[i].ctun = 0;
      vfo[i].mode = o->mode;
      vfo[i].filter = mode_settings[o->mode].filter;
    }
    pb_seeded = 0;
  }
  const int sample_rate = pb ? pb_rate : o->sample_rate;
  if (sample_rate != receiver[0]->sample_rate) {
    /* radio.c's radio_change_sample_rate(), with the stream stopped */
    rx_change_sample_rate(receiver[0], sample_rate);
    if (p1_started) old_protocol_set_mic_sample_rate(sample_rate);
  }
  if (pb) {
    memcpy(pb_saved_vfos, vfo, sizeof pb_saved_vfos);
    for (int m = 0; m < MODES; m++) memcpy(pb_saved_filters[m], filters[m], sizeof pb_saved_filters[m]);
    memcpy(pb_saved_modes, mode_settings, sizeof pb_saved_modes);
    /* the recording's centre stays put: listen inside it like CTUN */
    vfo[0].frequency = o->freq;
    vfo[0].ctun = 1;
    vfo[0].ctun_frequency = o->freq;
    vfo[0].band = get_band_from_frequency(o->freq);
    vfo[0].mode = o->mode;
  }
  vfo_apply_mode_settings(receiver[0]);
  radio_set_alex_antennas();   /* the band's RX antenna (IO board RX input), as radio.c's start does */
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
            inet_ntoa(radio->info.network.address.sin_addr), vfo[0].frequency / 1e6, mode_string[vfo[0].mode], o->lna_db,
            o->codec ? "on" : "off", rx->sample_rate, rx->pixels, rx->fps);
  snprintf(msg, msglen, "HL2 at %s, gateware %d.%d", inet_ntoa(radio->info.network.address.sin_addr),
           radio->software_version / 10, radio->software_version % 10);

  fhist_add(vfo[0].frequency);
  if (pb) {
    snprintf(msg, msglen, "IQ recording at %.3f MHz, %d kHz wide", o->freq / 1e6, sample_rate / 1000);
    droid_playback_run();
    atomic_store(&running, 1);
    pthread_create(&loop_thread, NULL, loop, NULL);
    return 0;
  }
  /* old_protocol.c: "old_protocol_init is only called ONCE. old_protocol_stop
   * and old_protocol_run just send start/stop packets" - so a restart (new
   * sample rate, Stop then Start) only runs it again. */
  if (p1_started) {
    relink_protocol_socket();   /* the network may have changed since (adapter unplugged) */
    old_protocol_run();
  } else {
    old_protocol_init(rx->sample_rate);
    p1_started = 1;
  }
  atomic_store(&running, 1);
  pthread_create(&loop_thread, NULL, loop, NULL);
  droid_autogain_run(1);   /* HL2 ADC auto gain, when it is on (RX page) */
  return 0;
}

void droid_radio_stop(void) {
  pthread_mutex_lock(&life_lock);
  if (!atomic_load(&running)) { pthread_mutex_unlock(&life_lock); return; }
  droid_autogain_run(0);
  atomic_store(&running, 0);
  pthread_join(loop_thread, NULL);
  /* back to receive before the stream stops: with the loop thread stopped, nothing else runs upstream code */
  droid_tx_off_now();
  if (receiver[0] && receiver[0]->local_audio) {
    receiver[0]->local_audio = 0;
    audio_close_output(receiver[0]);
  }
  droid_radio_save_now();   /* the loop thread has stopped: safe here */
  if (atomic_load(&droid_playback_on)) {
    droid_playback_stop();
    /* the radio's VFOs, filters, per-mode settings and the radio itself, as before the recording */
    memcpy(vfo, pb_saved_vfos, sizeof pb_saved_vfos);
    for (int m = 0; m < MODES; m++) memcpy(filters[m], pb_saved_filters[m], sizeof pb_saved_filters[m]);
    memcpy(mode_settings, pb_saved_modes, sizeof pb_saved_modes);
    radio = real_radio;
  } else {
    old_protocol_stop();
  }
  droid_log("stream stopped");
  pthread_mutex_unlock(&life_lock);
  /* The receivers stay allocated: WDSP channels cannot be reopened cleanly in
   * the same process, so the service keeps the process for the next start. */
}

int droid_radio_running(void) { return atomic_load(&running); }

int droid_cw_shift(void) {
  if (vfo[0].mode == modeCWU) return cw_keyer_sidetone_frequency;
  if (vfo[0].mode == modeCWL) return -cw_keyer_sidetone_frequency;
  return 0;
}

void droid_get_status(droid_status *s) {
  memset(s, 0, sizeof *s);
  s->running = atomic_load(&running);
  s->meter_dbm = droid_meter_dbm();      /* also fed by the demo */
  s->noise_floor = droid_noise_floor();
  if (!s->running || receiver[0] == NULL) return;
  /* the listening frequency; with CTUN the centre (vfo[0].frequency) stays put */
  s->freq = vfo[0].ctun ? vfo[0].ctun_frequency : vfo[0].frequency;
  s->mode = vfo[0].mode;
  /* in CW rx_set_filter() adds the pitch: report the filter's own edges (around the VFO) */
  s->filter_low = receiver[0]->filter_low - droid_cw_shift();
  s->filter_high = receiver[0]->filter_high - droid_cw_shift();
  s->sample_rate = receiver[0]->sample_rate;
  s->meter_dbm = droid_meter_dbm();
  s->lost_packets = atomic_load(&droid_sequence_errors);
  s->adc_overloads = hl2_adc0_ovf_events;
  s->lna_db = adc[0].gain;
  s->volume_db = receiver[0]->volume;
  s->noise_floor = droid_noise_floor();
  s->phone_audio = receiver[0]->local_audio;
  s->zoom = receiver[0]->zoom;
  s->freq_b = vfo[1].frequency;
  s->mode_b = vfo[1].mode;
  s->rit_on = vfo[0].rit_enabled;
  s->rit = vfo[0].rit;
  s->ctun = vfo[0].ctun;
  s->locked = locked;
  s->split = split;
  s->step = vfo[0].step;
  s->band = vfo[0].band;
  s->bandstack = vfo[0].bandstack;
  s->filter = vfo[0].filter;
  s->agc = receiver[0]->agc;
  s->rows = droid_rows_count();
  s->rit_step = vfo[0].rit_step;
}

/* ---- deskHPSDR's tables for the UI (read-only; small races are harmless) ---- */

int droid_filters(int mode, char titles[][16], int *lo, int *hi, int max) {
  if (mode < 0 || mode >= MODES) return 0;
  int n = 0;
  for (int i = 0; i < FILTERS && n < max; i++, n++) {
    const FILTER *f = &filters[mode][i];
    snprintf(titles[n], 16, "%s", f->title);
    lo[n] = f->low;
    hi[n] = f->high;
  }
  return n;
}

int droid_bands(char titles[][16], long long *fmin, long long *fmax, int max) {
  int n = 0;
  for (int i = 0; i < BANDS && n < max; i++, n++) {
    const BAND *b = band_get_band(i);
    snprintf(titles[n], 16, "%s", b->title);
    fmin[n] = b->frequencyMin;
    fmax[n] = b->frequencyMax;
  }
  return n;
}

int droid_bandstack_entries(int band, long long *freq, int *mode, int *filter, int max, int *current) {
  if (band < 0 || band >= BANDS) return 0;
  const BANDSTACK *bs = bandstack_get_bandstack(band);
  if (!bs) return 0;
  int n = 0;
  for (int i = 0; i < bs->entries && n < max; i++, n++) {
    freq[n] = bs->entry[i].frequency;
    mode[n] = bs->entry[i].mode;
    filter[n] = bs->entry[i].filter;
  }
  *current = bs->current_entry;
  return n;
}

int droid_memories(long long *freq, int *mode, int max) {
  int n = 0;
  for (int i = 0; i < NUM_OF_MEMORYS && n < max; i++, n++) {
    freq[n] = mem[i].frequency;
    mode[n] = mem[i].mode;
  }
  return n;
}

int droid_wisdom(const char *dir) { return WDSPwisdom((char *)dir); }
const char *droid_wisdom_status(void) { return wisdom_get_status(); }
