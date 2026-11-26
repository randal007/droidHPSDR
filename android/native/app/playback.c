/*
 * playback.c — an IQ recording in place of the radio (the demo band).
 *
 * A WAV file of IQ samples (I = left, Q = right; 16-bit or 32-bit float, at
 * an HL2 sample rate) is fed, in real time and looping, into deskHPSDR's
 * receiver through rx_add_iq_samples(): the same call old_protocol.c makes
 * for every sample the HL2 sends. Everything after it (WDSP, the display,
 * the meter, NR/NB/notch/squelch/AGC, phone audio) is the real thing.
 *
 * The recording can't be retuned: radio_core.c tunes inside it like CTUN.
 * There is no HL2+ speaker either: __wrap_old_protocol_audio_samples drops
 * the audio that would go to the radio while a recording plays (the linker
 * sends receiver.c's call here: -Wl,--wrap in CMakeLists.txt, so upstream's
 * files stay unchanged).
 *
 * GPL v3 or later, like the rest of droidHPSDR.
 */
#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <gtk/gtk.h>

#include "radio.h"
#include "receiver.h"

#include "droid_app.h"

static FILE *wav;
static long data_off, data_len;   /* bytes */
static int bits, is_float, rate;
static pthread_t thread;
static atomic_int playing;
atomic_int droid_playback_on;

static uint32_t le32(const unsigned char *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }

int droid_playback_open(const char *path, int *sample_rate, char *msg, int msglen) {
  unsigned char h[12], c[8], fmt[16];
  FILE *f = fopen(path, "rb");
  if (!f) { snprintf(msg, msglen, "can't open %s: %s", path, strerror(errno)); return -1; }
  if (fread(h, 1, 12, f) != 12 || memcmp(h, "RIFF", 4) || memcmp(h + 8, "WAVE", 4)) {
    snprintf(msg, msglen, "not a WAV file: %s", path);
    fclose(f);
    return -1;
  }
  int have_fmt = 0;
  for (;;) {
    if (fread(c, 1, 8, f) != 8) { snprintf(msg, msglen, "no data in %s", path); fclose(f); return -1; }
    long n = le32(c + 4);
    if (!memcmp(c, "fmt ", 4)) {
      if (n < 16 || fread(fmt, 1, 16, f) != 16) break;
      int tag = fmt[0] | fmt[1] << 8, ch = fmt[2] | fmt[3] << 8;
      rate = (int)le32(fmt + 4);
      bits = fmt[14] | fmt[15] << 8;
      is_float = tag == 3 || (tag == 0xFFFE && bits == 32);
      if (ch != 2 || (!((tag == 1 || tag == 0xFFFE) && bits == 16) && !(is_float && bits == 32))) {
        snprintf(msg, msglen, "the WAV must be 2 channels (I/Q), 16-bit or 32-bit float");
        fclose(f);
        return -1;
      }
      have_fmt = 1;
      fseek(f, n - 16 + (n & 1), SEEK_CUR);
    } else if (!memcmp(c, "data", 4)) {
      data_off = ftell(f);
      data_len = n;
      break;
    } else {
      fseek(f, n + (n & 1), SEEK_CUR);
    }
  }
  if (!have_fmt) { snprintf(msg, msglen, "no format in %s", path); fclose(f); return -1; }
  if (rate != 48000 && rate != 96000 && rate != 192000 && rate != 384000) {
    snprintf(msg, msglen, "the recording is at %d Hz: the HL2 rates are 48, 96, 192 or 384 kHz "
             "(tools/iq_convert.py converts it)", rate);
    fclose(f);
    return -1;
  }
  wav = f;
  *sample_rate = rate;
  return 0;
}

static void *run(void *arg) {
  (void)arg;
  const int frame = is_float ? 8 : 4;
  const int chunk = rate / 100;   /* 10 ms */
  unsigned char *buf = malloc((size_t)chunk * frame);
  long left = data_len;
  struct timespec next;
  clock_gettime(CLOCK_MONOTONIC, &next);
  fseek(wav, data_off, SEEK_SET);
  while (atomic_load(&playing)) {
    long want = (long)chunk * frame;
    if (want > left) want = left - left % frame;
    size_t got = want > 0 ? fread(buf, 1, (size_t)want, wav) : 0;
    if (got < (size_t)frame) {   /* the end: loop */
      fseek(wav, data_off, SEEK_SET);
      left = data_len;
      continue;
    }
    left -= (long)got;
    int n = (int)(got / frame);
    for (int k = 0; k < n; k++) {
      double i, q;
      if (is_float) {
        float fi, fq;
        memcpy(&fi, buf + k * 8, 4);
        memcpy(&fq, buf + k * 8 + 4, 4);
        i = fi; q = fq;
      } else {
        int16_t si, sq;
        memcpy(&si, buf + k * 4, 2);
        memcpy(&sq, buf + k * 4 + 2, 2);
        i = si / 32768.0; q = sq / 32768.0;
      }
      rx_add_iq_samples(receiver[0], i, q);
    }
    /* real time: the next 10 ms when they are due */
    next.tv_nsec += (long)n * 1000000000L / rate;
    while (next.tv_nsec >= 1000000000L) { next.tv_nsec -= 1000000000L; next.tv_sec++; }
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    if (now.tv_sec - next.tv_sec > 1) next = now;   /* fell far behind (paused): don't race to catch up */
    clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);
  }
  free(buf);
  return NULL;
}

void droid_playback_run(void) {
  atomic_store(&droid_playback_on, 1);
  atomic_store(&playing, 1);
  pthread_create(&thread, NULL, run, NULL);
}

void droid_playback_stop(void) {
  if (!atomic_load(&playing)) return;
  atomic_store(&playing, 0);
  pthread_join(thread, NULL);
  if (wav) { fclose(wav); wav = NULL; }
  atomic_store(&droid_playback_on, 0);
}

/* receiver.c's audio for the radio (the HL2+ speaker): none while a recording plays */
void __real_old_protocol_audio_samples(short left, short right);
void __wrap_old_protocol_audio_samples(short left, short right) {
  if (atomic_load(&droid_playback_on)) return;
  __real_old_protocol_audio_samples(left, right);
}
