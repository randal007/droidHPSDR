/*
 * audio_null.c — local audio for the headless m1-rx build: none. The HL2+
 * codec carries the audio. The app links upstream's buffered_audio.c with
 * app/audio_aaudio.c instead (phone audio).
 *
 * GPL v3 or later, like the rest of droidHPSDR.
 */
#include <netinet/in.h>
#include <gtk/gtk.h>

#include "audio.h"
#include "receiver.h"

int audio_open_output(RECEIVER *rx) { (void)rx; return -1; }
int audio_open_input(void) { return -1; }
int audio_write(RECEIVER *rx, float left_sample, float right_sample) {
  (void)rx; (void)left_sample; (void)right_sample;
  return 0;
}
float audio_get_next_mic_sample(void) { return 0.0f; }
void audio_reset_mic_buffer(void) {}
