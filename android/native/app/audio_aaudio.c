/*
 * audio_aaudio.c — upstream's audio backend interface (src/audio_backend.h)
 * on Android's AAudio, so deskHPSDR's own buffered audio engine
 * (src/buffered_audio.c, compiled unchanged) can play the receiver on the
 * phone: its "local audio", as on a PC.
 *
 * The HL2+ codec stays the main audio path (CLAUDE.md decision 1, as
 * changed 2026-11-24): phone audio plays as well, when the user turns it on.
 *
 * Input (M7, Randal 2026-11-25): the phone's microphone, built-in or
 * Bluetooth, as deskHPSDR's "local microphone": the stream's callback hands
 * the samples to buffered_audio.c's mic ring (audio_process_local_mic_input),
 * which old_protocol.c reads instead of the HL2+ mic. The app picks the
 * device (droid_mic_route); for Bluetooth it has already made the headset
 * Android's communication device.
 *
 * The stream's callback pulls samples from upstream's ring with
 * audio_render_local_output(), as the CoreAudio and miniaudio backends do.
 * When Android drops the stream (headphones plugged in, Bluetooth comes or
 * goes), a helper thread opens a new one on the new output, so the user's
 * choice survives a device change.
 *
 * GPL v3 or later, like the rest of droidHPSDR.
 */
#include <aaudio/AAudio.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <gtk/gtk.h>

#include "audio.h"
#include "audio_backend.h"
#include "message.h"
#include "receiver.h"

#include "droid_app.h"

typedef struct {
  RECEIVER *rx;
  AAudioStream *stream;
  pthread_mutex_t lock;   /* guards stream against the reopen thread */
  atomic_int closing;
  atomic_int reopening;
} out_t;

static aaudio_data_callback_result_t on_data(AAudioStream *s, void *user, void *data, int32_t frames) {
  (void)s;
  out_t *o = user;
  audio_render_local_output(o->rx, (float *)data, (unsigned)frames, 2);
  return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

static AAudioStream *open_stream(out_t *o);

static void *reopen(void *arg) {
  out_t *o = arg;
  usleep(200000);   /* let Android settle on the new output */
  pthread_mutex_lock(&o->lock);
  if (!atomic_load(&o->closing)) {
    if (o->stream) {
      AAudioStream_close(o->stream);
      o->stream = NULL;
    }
    o->stream = open_stream(o);
    droid_log("phone audio: reopened on the new output (%s)", o->stream ? "ok" : "failed");
  }
  pthread_mutex_unlock(&o->lock);
  atomic_store(&o->reopening, 0);
  return NULL;
}

static void on_error(AAudioStream *s, void *user, aaudio_result_t err) {
  (void)s;
  out_t *o = user;
  droid_log("phone audio: stream error %s", AAudio_convertResultToText(err));
  /* Never close a stream from its own callback: do it on another thread.
   * Claim `reopening` first, then look at `closing` (close does the two the
   * other way round), so a close at this moment either waits for us or we
   * see it and back off. */
  if (err != AAUDIO_ERROR_DISCONNECTED || atomic_exchange(&o->reopening, 1)) return;
  if (atomic_load(&o->closing)) {
    atomic_store(&o->reopening, 0);
    return;
  }
  pthread_t t;
  pthread_create(&t, NULL, reopen, o);
  pthread_detach(t);
}

static AAudioStream *open_stream(out_t *o) {
  AAudioStreamBuilder *b;
  AAudioStream *s = NULL;
  if (AAudio_createStreamBuilder(&b) != AAUDIO_OK) return NULL;
  AAudioStreamBuilder_setDirection(b, AAUDIO_DIRECTION_OUTPUT);
  AAudioStreamBuilder_setSampleRate(b, 48000);   /* the receiver's audio rate; AAudio resamples if needed */
  AAudioStreamBuilder_setChannelCount(b, 2);
  AAudioStreamBuilder_setFormat(b, AAUDIO_FORMAT_PCM_FLOAT);
  AAudioStreamBuilder_setPerformanceMode(b, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
  AAudioStreamBuilder_setSharingMode(b, AAUDIO_SHARING_MODE_SHARED);
  AAudioStreamBuilder_setUsage(b, AAUDIO_USAGE_MEDIA);   /* the phone's media volume keys set the level */
  AAudioStreamBuilder_setContentType(b, AAUDIO_CONTENT_TYPE_SPEECH);
  AAudioStreamBuilder_setDataCallback(b, on_data, o);
  AAudioStreamBuilder_setErrorCallback(b, on_error, o);
  aaudio_result_t r = AAudioStreamBuilder_openStream(b, &s);
  AAudioStreamBuilder_delete(b);
  if (r != AAUDIO_OK) {
    droid_log("phone audio: open failed: %s", AAudio_convertResultToText(r));
    return NULL;
  }
  r = AAudioStream_requestStart(s);
  if (r != AAUDIO_OK) {
    droid_log("phone audio: start failed: %s", AAudio_convertResultToText(r));
    AAudioStream_close(s);
    return NULL;
  }
  droid_log("phone audio: %d Hz, %d frames per burst, device %d", AAudioStream_getSampleRate(s),
            AAudioStream_getFramesPerBurst(s), AAudioStream_getDeviceId(s));
  return s;
}

void *audio_backend_output_open(RECEIVER *rx, const char *device_name, int *channels) {
  (void)device_name;
  out_t *o = calloc(1, sizeof *o);
  if (!o) return NULL;
  o->rx = rx;
  pthread_mutex_init(&o->lock, NULL);
  o->stream = open_stream(o);
  if (!o->stream) {
    free(o);
    return NULL;
  }
  *channels = 2;
  return o;
}

void audio_backend_output_close(void *handle) {
  out_t *o = handle;
  if (!o) return;
  atomic_store(&o->closing, 1);
  while (atomic_load(&o->reopening)) usleep(10000);
  pthread_mutex_lock(&o->lock);
  if (o->stream) {
    AAudioStream_requestStop(o->stream);
    AAudioStream_close(o->stream);   /* returns once the callback has left */
    o->stream = NULL;
  }
  pthread_mutex_unlock(&o->lock);
  /* Not freed: an error callback that started just before closing may still
   * be about to read o's flags (AAudio doesn't promise close waits for it).
   * A few dozen bytes each time PHONE is turned off. */
}

/* A dropped stream is reopened above, so the output only counts as lost when
 * that failed; upstream's watchdog then turns local audio off. */
int audio_backend_output_is_alive(void *handle) {
  out_t *o = handle;
  if (!o) return 0;
  if (atomic_load(&o->reopening)) return 1;
  pthread_mutex_lock(&o->lock);
  int alive = o->stream != NULL;
  pthread_mutex_unlock(&o->lock);
  return alive;
}

/* ---- the phone's microphone ---- */

static atomic_int mic_device;      /* AAudio device id, 0 = Android's default input */
static atomic_int mic_bluetooth;   /* 1: a Bluetooth headset (communication device) */

void droid_mic_route(int device_id, int bluetooth) {
  atomic_store(&mic_device, device_id);
  atomic_store(&mic_bluetooth, bluetooth);
}

static aaudio_data_callback_result_t on_mic(AAudioStream *s, void *user, void *data, int32_t frames) {
  (void)s; (void)user;
  audio_process_local_mic_input((const float *)data, (unsigned)frames);
  return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

void *audio_backend_input_open(const char *device_name) {
  (void)device_name;
  AAudioStreamBuilder *b;
  AAudioStream *s = NULL;
  if (AAudio_createStreamBuilder(&b) != AAUDIO_OK) return NULL;
  AAudioStreamBuilder_setDirection(b, AAUDIO_DIRECTION_INPUT);
  AAudioStreamBuilder_setSampleRate(b, 48000);   /* the TX mic rate; AAudio resamples (Bluetooth is 16 kHz) */
  AAudioStreamBuilder_setChannelCount(b, 1);
  AAudioStreamBuilder_setFormat(b, AAUDIO_FORMAT_PCM_FLOAT);
  AAudioStreamBuilder_setPerformanceMode(b, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
  AAudioStreamBuilder_setSharingMode(b, AAUDIO_SHARING_MODE_SHARED);
  if (atomic_load(&mic_device) > 0) AAudioStreamBuilder_setDeviceId(b, atomic_load(&mic_device));
  /* built-in: no phone-call processing (WDSP does the TX chain); Bluetooth: the call path the headset uses */
  AAudioStreamBuilder_setInputPreset(b, atomic_load(&mic_bluetooth) ? AAUDIO_INPUT_PRESET_VOICE_COMMUNICATION
                                                                      : AAUDIO_INPUT_PRESET_VOICE_RECOGNITION);
  AAudioStreamBuilder_setDataCallback(b, on_mic, NULL);
  aaudio_result_t r = AAudioStreamBuilder_openStream(b, &s);
  AAudioStreamBuilder_delete(b);
  if (r != AAUDIO_OK) {
    droid_log("phone mic: open failed: %s", AAudio_convertResultToText(r));
    return NULL;
  }
  r = AAudioStream_requestStart(s);
  if (r != AAUDIO_OK) {
    droid_log("phone mic: start failed: %s", AAudio_convertResultToText(r));
    AAudioStream_close(s);
    return NULL;
  }
  droid_log("phone mic: %d Hz, device %d%s", AAudioStream_getSampleRate(s), AAudioStream_getDeviceId(s),
            atomic_load(&mic_bluetooth) ? " (Bluetooth)" : "");
  return s;
}

void audio_backend_input_close(void *handle) {
  AAudioStream *s = handle;
  if (!s) return;
  AAudioStream_requestStop(s);
  AAudioStream_close(s);
}

int audio_backend_input_is_alive(void *handle) {
  AAudioStream *s = handle;
  if (!s) return 0;
  aaudio_stream_state_t st = AAudioStream_getState(s);
  return st != AAUDIO_STREAM_STATE_DISCONNECTED && st != AAUDIO_STREAM_STATE_CLOSED;
}

/* No TCI monitor. */
void *audio_backend_tci_monitor_open(const char *device_name, int *channels) {
  (void)device_name; (void)channels;
  return NULL;
}
void audio_backend_tci_monitor_close(void *handle) { (void)handle; }
int audio_backend_tci_monitor_is_alive(void *handle) { (void)handle; return 0; }
int audio_backend_get_cards(void) { return 0; }

#if defined(MINIAUDIO) && defined(__linux__)
char miniaudio_backend[16] = "aaudio";
#endif
