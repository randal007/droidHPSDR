/*
 * autogain.c — the HL2's automatic RxPGA (LNA) gain, deskHPSDR's "HL2 ADC
 * Auto Gain RxPGA" and "HL2 Auto Gain time-regulated" (RX menu).
 *
 * Upstream keeps this thread in rigctl.c (built with AUTOGAIN=ON), next to
 * the CAT server, which the app does not compile. The thread is copied here
 * nearly as it is: on ADC overloads it lowers the gain 3 dB every 0.5 s;
 * once clear it raises it 1 dB at a time until the ADC overloads, then backs
 * off 3 dB. Time-regulated: it does that again every 30 s.
 * The one change: set_rf_gain() (sliders.c, GTK) becomes a plain write of
 * adc[].gain, which is all it does apart from moving the slider; old_protocol.c
 * sends it to the radio.
 *
 * Copied from deskHPSDR rigctl.c: (C) 2016 Steve Wilson, 2024-2026 Heiko Amft
 * DL1BZ (Project deskHPSDR), time-controlled adjustment by DH0DM;
 * GPL-3.0-or-later.
 *
 * GPL v3 or later, like the rest of droidHPSDR.
 */
#include <pthread.h>
#include <time.h>
#include <gtk/gtk.h>

#include "adc.h"
#include "radio.h"
#include "receiver.h"
#include "ext.h"

#include "droid_app.h"

/* radio.c's globals (radio.c is not compiled); saved in the props file by radio_core.c */
int autogain_enabled = 0;
int autogain_time_enabled = 0;

static int autogain_is_adjusted = 1;
static pthread_t autogain_thread;
static pthread_mutex_t autogain_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t autogain_lifecycle_mutex = PTHREAD_MUTEX_INITIALIZER;
static gint autogain_thread_running = 0;

static void set_gain(double value) {
  adc[active_receiver->adc].gain = value;
}

static void *autogain_thread_function(void *arg) {
  (void)arg;
  struct timespec start_time, current_time, last_autogain_increase = {0};
  time_t elapsed_time;
  int re_adjustment_time = 30;   // in sec
  int autogain_first_run = 1;    // after a start the ADC OVL state can be wrong for a while: wait
  double gain_step = 3.0;        // gain step size for automatic
  double gain = 0.0;
  double max_gain = 0.0;
  double min_gain = 0.0;
  unsigned int adc_count_limit = 4;   // count more than 1x ADC OVL as hysteresis
  unsigned int adc0_error_count = 0;
  unsigned int adc1_error_count = 0;
  pthread_mutex_lock(&autogain_mutex);
  min_gain = adc[active_receiver->adc].min_gain;
  max_gain = adc[active_receiver->adc].max_gain;
  pthread_mutex_unlock(&autogain_mutex);
  clock_gettime(CLOCK_MONOTONIC, &start_time);
  while (g_atomic_int_get(&autogain_thread_running)) {
    clock_gettime(CLOCK_MONOTONIC, &current_time);
    elapsed_time = current_time.tv_sec - start_time.tv_sec;
    if (elapsed_time > 10 && autogain_first_run) {   // a delay of 10 s
      autogain_first_run = 0;
      droid_log("autogain: start after %ds", (int)elapsed_time);
    }
    if (!radio_is_transmitting() && !radio_ptt) {
      if (adc0_p_ovl) adc0_error_count++; else adc0_error_count = 0;
      if (adc1_p_ovl) adc1_error_count++; else adc1_error_count = 0;
      pthread_mutex_lock(&autogain_mutex);
      gain = adc[active_receiver->adc].gain;
      pthread_mutex_unlock(&autogain_mutex);
      if (gain > max_gain) gain = max_gain;
      if (adc0_error_count >= adc_count_limit && !autogain_first_run) {
        pthread_mutex_lock(&autogain_mutex);
        autogain_is_adjusted = 0;
        pthread_mutex_unlock(&autogain_mutex);
        g_idle_add(ext_vfo_update, NULL);
        while (g_atomic_int_get(&autogain_thread_running) &&
               !radio_is_transmitting() && !radio_ptt && adc0_p_ovl && gain > min_gain) {
          gain -= gain_step;
          if (gain < min_gain) gain = min_gain;
          pthread_mutex_lock(&autogain_mutex);
          set_gain(gain);
          pthread_mutex_unlock(&autogain_mutex);
          g_usleep(500000);
        }
        droid_log("autogain: RxPGA re-adjusted, now %+d dB", (int)gain);
      }
      if (adc1_error_count >= adc_count_limit && !autogain_first_run) {
        while (g_atomic_int_get(&autogain_thread_running) &&
               !radio_is_transmitting() && !radio_ptt && adc1_p_ovl && gain > min_gain) {
          gain -= gain_step;
          if (gain < min_gain) gain = min_gain;
          pthread_mutex_lock(&autogain_mutex);
          set_gain(gain);
          pthread_mutex_unlock(&autogain_mutex);
          g_usleep(500000);
        }
      }
      if (!adc0_p_ovl && !autogain_is_adjusted && !autogain_first_run) {
        while (g_atomic_int_get(&autogain_thread_running) &&
               !radio_is_transmitting() && !radio_ptt && !adc0_p_ovl && gain >= min_gain && gain < max_gain) {
          gain += 1.0;
          pthread_mutex_lock(&autogain_mutex);
          set_gain(gain);
          pthread_mutex_unlock(&autogain_mutex);
          g_usleep(500000);
        }
        pthread_mutex_lock(&autogain_mutex);
        set_gain(gain - 3.0);
        autogain_is_adjusted = 1;
        pthread_mutex_unlock(&autogain_mutex);
        last_autogain_increase = current_time;   // patch by DH0DM
        g_idle_add(ext_vfo_update, NULL);
      }
    }
    // patch by DH0DM: time-controlled adjustment
    if (autogain_time_enabled && autogain_is_adjusted) {
      if (current_time.tv_sec - last_autogain_increase.tv_sec > re_adjustment_time) {
        pthread_mutex_lock(&autogain_mutex);
        autogain_is_adjusted = 0;
        pthread_mutex_unlock(&autogain_mutex);
      }
    }
    g_usleep(1000000);
  }
  return NULL;
}

/* rigctl.c launch_autogain_hl2() / restart: (re)start the thread when on and the
 * radio runs, stop it otherwise */
void droid_autogain_run(int run) {
  pthread_mutex_lock(&autogain_lifecycle_mutex);
  if (g_atomic_int_get(&autogain_thread_running)) {
    g_atomic_int_set(&autogain_thread_running, 0);
    pthread_join(autogain_thread, NULL);
  }
  if (run && autogain_enabled && active_receiver != NULL) {
    g_atomic_int_set(&autogain_thread_running, 1);
    if (pthread_create(&autogain_thread, NULL, autogain_thread_function, NULL) != 0) {
      g_atomic_int_set(&autogain_thread_running, 0);
    }
  }
  pthread_mutex_unlock(&autogain_lifecycle_mutex);
}
