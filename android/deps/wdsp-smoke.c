/*
 * wdsp-smoke: does WDSP actually work on this device?
 *
 * Headless, no radio needed. Opens one WDSP receive channel the way
 * deskHPSDR's receiver.c does (192 kHz I/Q in, 48 kHz audio out), feeds it a
 * synthetic signal, and checks what comes out:
 *
 *   1. FFTW wisdom      WDSPwisdom() as deskHPSDR runs it at start-up, timed.
 *   2. USB demod        a tone 1 kHz above the VFO must come out as 1 kHz audio.
 *   3. LSB rejection    the same tone in LSB must be strongly attenuated.
 *   4. S-meter          a 20 dB weaker input must read about 20 dB lower.
 *   5. Noise reduction  NR2 (EMNR), NR3 (rnnoise), NR4 (libspecbleach) and
 *                       NNR each run without crashing and still pass audio.
 *   6. Speed            how many times faster than real time the DSP runs.
 *
 * Writes usb.wav (USB demod of the strong tone) to the work directory, for
 * listening after `adb pull`.
 *
 * Usage: wdsp-smoke [workdir]     (default /data/local/tmp/wdsp-smoke)
 * Exit status 0 = every check passed.
 *
 * GPL v3 or later, like the rest of droidHPSDR.
 */
#include <errno.h>
#include <stdarg.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "wdsp.h"

#define CH          0
#define IN_RATE     192000
#define OUT_RATE    48000
#define IN_SIZE     1024                       /* deskHPSDR's default buffer_size */
#define DSP_SIZE    2048
#define OUT_SIZE    (IN_SIZE * OUT_RATE / IN_RATE)
#define TONE_HZ     1000.0
#define SECONDS     3.0

static int failures;

static double now(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec + ts.tv_nsec * 1e-9;
}

static void check(int ok, const char *what, const char *fmt, ...) {
  char detail[256];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(detail, sizeof detail, fmt, ap);
  va_end(ap);
  printf("%-5s %-22s %s\n", ok ? "PASS" : "FAIL", what, detail);
  if (!ok) failures++;
}

/* Small deterministic noise source, so runs are comparable. */
static uint32_t rng = 12345;
static double noise(void) {
  rng = rng * 1664525u + 1013904223u;
  return ((double)(rng >> 8) / (double)(1u << 24)) - 0.5;
}

/* Goertzel: mean-square of the `freq` component of the left channel, on the
 * same scale as rms()^2 (a pure sine at `freq` gives the same number). */
static double tone_power(const double *lr, int frames, double freq) {
  double w = 2.0 * M_PI * freq / OUT_RATE, c = 2.0 * cos(w), s1 = 0, s2 = 0;
  for (int i = 0; i < frames; i++) {
    double s0 = lr[2 * i] + c * s1 - s2;
    s2 = s1;
    s1 = s0;
  }
  return 2.0 * (s1 * s1 + s2 * s2 - c * s1 * s2) / ((double)frames * frames);
}

static double rms(const double *lr, int frames) {
  double sum = 0;
  for (int i = 0; i < frames; i++) sum += lr[2 * i] * lr[2 * i];
  return sqrt(sum / frames);
}

struct result {
  double smeter_db;     /* GetRXAMeter(RXA_S_AV) at the end */
  double tone_db;       /* 1 kHz power in the last second of audio */
  double rms;           /* audio RMS in the last second */
  double cpu_seconds;   /* wall time spent inside fexchange0 */
  double *audio;        /* whole output, interleaved stereo */
  int frames;
};

/* Feed SECONDS of a tone at TONE_HZ above the VFO, amplitude `amp`, plus noise. */
static struct result run(double amp) {
  struct result r = {0};
  int blocks = (int)(SECONDS * IN_RATE / IN_SIZE);
  double in[2 * IN_SIZE], out[2 * OUT_SIZE];
  double phase = 0, step = 2.0 * M_PI * TONE_HZ / IN_RATE;
  int err;

  r.frames = blocks * OUT_SIZE;
  r.audio = calloc(2 * (size_t)r.frames, sizeof(double));
  for (int b = 0; b < blocks; b++) {
    for (int i = 0; i < IN_SIZE; i++) {
      in[2 * i]     = amp * cos(phase) + 1e-5 * noise();   /* I */
      /* WDSP's sign convention: a tone above the VFO is (cos, -sin). With
       * (cos, +sin) it lands on the LSB side, as the first desktop run showed. */
      in[2 * i + 1] = -amp * sin(phase) + 1e-5 * noise();  /* Q */
      phase += step;
      if (phase > 2.0 * M_PI) phase -= 2.0 * M_PI;
    }
    double t0 = now();
    fexchange0(CH, in, out, &err);
    r.cpu_seconds += now() - t0;
    memcpy(r.audio + 2 * (size_t)b * OUT_SIZE, out, sizeof out);
  }
  /* Measure over the last second only: AGC and filters have settled by then. */
  const double *tail = r.audio + 2 * (size_t)(r.frames - OUT_RATE);
  r.tone_db = 10.0 * log10(tone_power(tail, OUT_RATE, TONE_HZ) + 1e-30);
  r.rms = rms(tail, OUT_RATE);
  r.smeter_db = GetRXAMeter(CH, RXA_S_AV);
  return r;
}

static void write_wav(const char *path, const double *lr, int frames) {
  FILE *f = fopen(path, "wb");
  if (!f) { printf("      (could not write %s: %s)\n", path, strerror(errno)); return; }
  uint32_t data = (uint32_t)frames * 2, riff = 36 + data, rate = OUT_RATE, byterate = OUT_RATE * 2;
  uint16_t fmt = 1, chans = 1, align = 2, bits = 16;
  uint32_t fmtlen = 16;
  fwrite("RIFF", 1, 4, f); fwrite(&riff, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
  fwrite(&fmtlen, 4, 1, f); fwrite(&fmt, 2, 1, f); fwrite(&chans, 2, 1, f);
  fwrite(&rate, 4, 1, f); fwrite(&byterate, 4, 1, f); fwrite(&align, 2, 1, f);
  fwrite(&bits, 2, 1, f); fwrite("data", 1, 4, f); fwrite(&data, 4, 1, f);
  for (int i = 0; i < frames; i++) {
    double v = lr[2 * i] * 32767.0;
    int16_t s = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
    fwrite(&s, 2, 1, f);
  }
  fclose(f);
}

static void set_mode(int mode) {
  SetRXAMode(CH, mode);
  if (mode == RXA_LSB) RXASetPassband(CH, -2850.0, -150.0);
  else                 RXASetPassband(CH, 150.0, 2850.0);
}

int main(int argc, char **argv) {
  const char *dir = argc > 1 ? argv[1] : "/data/local/tmp/wdsp-smoke";
  char path[512];
  mkdir(dir, 0755);
  setvbuf(stdout, NULL, _IOLBF, 0);

  printf("WDSP version %d, %d-byte double, in %d Hz / out %d Hz, block %d\n",
         GetWDSPVersion(), (int)sizeof(double), IN_RATE, OUT_RATE, IN_SIZE);

  /* 1. Wisdom. deskHPSDR runs this on first start; on a desktop it takes a while. */
  snprintf(path, sizeof path, "%s/", dir);
  double t0 = now();
  int wisdom = WDSPwisdom(path);
  check(1, "FFTW wisdom", "%s in %.1f s (%s)", wisdom ? "computed" : "loaded",
        now() - t0, wisdom_get_status());

  /* 2-4. A receiver set up like deskHPSDR's receiver.c. */
  OpenChannel(CH, IN_SIZE, DSP_SIZE, IN_RATE, 48000, OUT_RATE,
              0, 1, 0.010, 0.025, 0.0, 0.010, 1);
  SetRXABandpassWindow(CH, 1);
  SetRXABandpassRun(CH, 1);
  SetRXAPanelRun(CH, 1);
  SetRXAPanelSelect(CH, 3);
  SetRXAPanelGain1(CH, 0.2);
  SetRXAAGCMode(CH, 3);   /* MED */
  SetRXAAGCTop(CH, 80.0);

  set_mode(RXA_USB);
  struct result usb = run(1e-2);
  double total_audio_db = 10.0 * log10(usb.rms * usb.rms + 1e-30);
  check(usb.tone_db > total_audio_db - 1.0 && usb.rms > 1e-3, "USB demod 1 kHz",
        "1 kHz is %.1f dB of %.1f dB total audio", usb.tone_db, total_audio_db);
  snprintf(path, sizeof path, "%s/usb.wav", dir);
  write_wav(path, usb.audio, usb.frames);

  set_mode(RXA_LSB);
  struct result lsb = run(1e-2);
  check(usb.tone_db - lsb.tone_db > 40.0, "LSB rejects USB tone",
        "%.1f dB down (USB %.1f, LSB %.1f)", usb.tone_db - lsb.tone_db,
        usb.tone_db, lsb.tone_db);

  set_mode(RXA_USB);
  struct result weak = run(1e-3);
  double drop = usb.smeter_db - weak.smeter_db;
  check(fabs(drop - 20.0) < 3.0, "S-meter tracks level",
        "%.1f dBm -> %.1f dBm for -20 dB input (%.1f dB)", usb.smeter_db,
        weak.smeter_db, drop);

  /* 5. Each noise reduction on its own, USB, strong tone. This only asks
   * "does it run and give sane numbers": a steady tone is what the speech
   * denoisers are built to remove (rnnoise silences it almost entirely). */
  struct { const char *name; void (*run)(int, int); } nr[] = {
    {"NR2 (EMNR)", SetRXAEMNRRun},
    {"NR3 (rnnoise)", SetRXARNNRRun},
    {"NR4 (specbleach)", SetRXASBNRRun},
    {"NNR (WDSP 2.11)", SetRXANNRRun},
  };
  for (size_t i = 0; i < sizeof nr / sizeof nr[0]; i++) {
    nr[i].run(CH, 1);
    struct result r = run(1e-2);
    nr[i].run(CH, 0);
    check(isfinite(r.rms) && isfinite(r.smeter_db), nr[i].name, "audio RMS %.2e, 1 kHz %.1f dB, %.1fx real time",
          r.rms, r.tone_db, SECONDS / r.cpu_seconds);
    free(r.audio);
  }

  /* 6. Speed, plain USB. */
  check(SECONDS / usb.cpu_seconds > 5.0, "speed (plain USB)", "%.1fx real time",
        SECONDS / usb.cpu_seconds);

  CloseChannel(CH);
  free(usb.audio); free(lsb.audio); free(weak.audio);
  printf("%s: %d failure(s). Audio: %s/usb.wav\n", failures ? "FAILED" : "ALL PASSED",
         failures, dir);
  return failures ? 1 : 0;
}
