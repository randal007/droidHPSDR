/*
 * audio_aaudio.c — upstream's audio backend interface (src/audio_backend.h)
 * on Android's AAudio, so deskHPSDR's own buffered audio engine
 * (src/buffered_audio.c, compiled unchanged) can play the receiver on the
 * phone: its "local audio", as on a PC.
 *
 * Output only. The HL2+ codec stays the main audio path (CLAUDE.md
 * decision 1, as changed 2026-11-24): phone audio plays as well, when the
 * user turns it on. The microphone is the HL2+'s; input is not offered.
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
  /* Never close a stream from its own callback: do it on another thread. */
  if (err == AAUDIO_ERROR_DISCONNECTED && !atomic_load(&o->closing) && !atomic_exchange(&o->reopening, 1)) {
    pthread_t t;
    pthread_create(&t, NULL, reopen, o);
    pthread_detach(t);
  }
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
  pthread_mutex_destroy(&o->lock);
  free(o);
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

/* No phone microphone (the HL2+ has the mic) and no TCI monitor. */
void *audio_backend_input_open(const char *device_name) { (void)device_name; return NULL; }
void audio_backend_input_close(void *handle) { (void)handle; }
int audio_backend_input_is_alive(void *handle) { (void)handle; return 0; }
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
