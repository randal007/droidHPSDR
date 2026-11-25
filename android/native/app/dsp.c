/*
 * dsp.c — M5, receive DSP: deskHPSDR's Noise menu, the manual notch,
 * squelch, the RX equalizer and the DSP (fft_menu) settings, applied the
 * way upstream's menus apply them.
 *
 * The menus themselves (noise_menu.c, equalizer_menu.c, fft_menu.c,
 * filter_menu.c) are GTK dialogs and are not compiled. The few lines that
 * store a change and apply it are copied here from them, so the result is
 * the same as in deskHPSDR: NR, NB, ANF, SNB and their settings, the EQ and
 * the CW peak filter are kept per mode (mode_settings[] + copy_mode_settings),
 * then receiver.c's rx_set_*() hands them to WDSP.
 *
 * Copied from deskHPSDR: (C) 2016 John Melton G0ORX/N6LYT, 2024-2026 Heiko
 * Amft DL1BZ (Project deskHPSDR), GPL-3.0-or-later: noise_menu.c update_noise_for_rx() /
 * update_anf_for_rx() / update_notch_for_rx(), equalizer_menu.c enable_cb() /
 * freq_changed_cb() / gain_changed_cb(), filter_menu.c cw_peak_cb(),
 * fft_menu.c filter_type_cb() / filter_size_cb() / binaural_cb().
 *
 * GPL v3 or later, like the rest of droidHPSDR.
 */
#include <math.h>
#include <gtk/gtk.h>

#include <stdatomic.h>
#include "adc.h"
#include "audio.h"
#include "band.h"
#include "mode.h"
#include "radio.h"
#include "receiver.h"
#include "vfo.h"

#include "ext.h"
#include "droid_app.h"

/* noise_menu.c's noise_nr_allowed(): no noise reduction in the digital modes */
static int nr_allowed(const RECEIVER *rx) {
  int mode = vfo[rx->id].mode;
  return (mode != modeDIGL && mode != modeDIGU);
}

/* noise_menu.c update_noise_for_rx(), without the dialog and slider updates */
static void update_noise(RECEIVER *rx) {
  int mode = vfo[rx->id].mode;
  if (!nr_allowed(rx)) {
    rx->nr = 0;
    rx->snb = 0;
  }
  if (rx->id == 0) {
    mode_settings[mode].nr = rx->nr;
    mode_settings[mode].nb = rx->nb;
    mode_settings[mode].anf = rx->anf;
    mode_settings[mode].snb = rx->snb;
    mode_settings[mode].nr2_ae = rx->nr2_ae;
    mode_settings[mode].nr2_post = rx->nr2_post;
    mode_settings[mode].nr2_post_taper = rx->nr2_post_taper;
    mode_settings[mode].nr2_post_nlevel = rx->nr2_post_nlevel;
    mode_settings[mode].nr2_post_factor = rx->nr2_post_factor;
    mode_settings[mode].nr2_post_rate = rx->nr2_post_rate;
    mode_settings[mode].nr_agc = rx->nr_agc;
    mode_settings[mode].nb2_mode = rx->nb2_mode;
    mode_settings[mode].nr2_gain_method = rx->nr2_gain_method;
    mode_settings[mode].nr2_npe_method = rx->nr2_npe_method;
    mode_settings[mode].nr2_trained_threshold = rx->nr2_trained_threshold;
    mode_settings[mode].nr2_trained_t2 = rx->nr2_trained_t2;
    mode_settings[mode].nb_tau = rx->nb_tau;
    mode_settings[mode].nb_advtime = rx->nb_advtime;
    mode_settings[mode].nb_hang = rx->nb_hang;
    mode_settings[mode].nb_thresh = rx->nb_thresh;
    mode_settings[mode].nr4_reduction_amount = rx->nr4_reduction_amount;
    mode_settings[mode].nr4_smoothing_factor = rx->nr4_smoothing_factor;
    mode_settings[mode].nr4_whitening_factor = rx->nr4_whitening_factor;
    mode_settings[mode].nr4_noise_rescale = rx->nr4_noise_rescale;
    mode_settings[mode].nr4_post_filter_threshold = rx->nr4_post_filter_threshold;
    mode_settings[mode].nnr_model = rx->nnr_model;
    mode_settings[mode].nnr_mask_floor = rx->nnr_mask_floor;
    copy_mode_settings(mode);
  }
  rx_set_noise(rx);
  g_idle_add(ext_vfo_update, NULL);
}

/* noise_menu.c update_anf_for_rx() */
static void update_anf(RECEIVER *rx) {
  if (!rx_anf_allowed(rx)) {
    rx->anf = 0;
  }
  if (rx->id == 0 && rx_anf_allowed(rx)) {
    int mode = vfo[rx->id].mode;
    mode_settings[mode].anf = rx->anf;
    copy_mode_settings(mode);
  }
  rx_set_anf(rx);
  g_idle_add(ext_vfo_update, NULL);
}

/* equalizer_menu.c: store the RX EQ per mode, then update_eq() */
static void update_eq(RECEIVER *rx) {
  if (rx->id == 0) {
    int mode = vfo[0].mode;
    mode_settings[mode].en_rxeq = rx->eq_enable;
    for (int i = 0; i < 13; i++) {
      mode_settings[mode].rx_eq_freq[i] = rx->eq_freq[i];
      mode_settings[mode].rx_eq_gain[i] = rx->eq_gain[i];
    }
    copy_mode_settings(mode);
  }
  rx_set_equalizer(rx);
  g_idle_add(ext_vfo_update, NULL);
}

static double clampd(double v, double lo, double hi) { return v < lo ? lo : v > hi ? hi : v; }
static int clampi(double v, int lo, int hi) { int i = (int)lround(v); return i < lo ? lo : i > hi ? hi : i; }

/* Field numbers: keep in step with Dsp in Radio.kt. Units are the ones
 * deskHPSDR's menus show (NB times in ms, NB threshold as the menu's number). */
enum {
  F_NR, F_NB, F_ANF, F_SNB, F_MNF, F_MNF_FREQ, F_MNF_WIDTH, F_NR_POST_AGC,
  F_NR2_GAIN, F_NR2_NPE, F_NR2_AE, F_NR2_POST, F_NR2_POST_LEVEL, F_NR2_POST_RATE, F_NR2_POST_FACTOR,
  F_NR2_POST_TAPER, F_NR2_TRAINED_THRESH, F_NR2_TRAINED_T2,
  F_NB2_MODE, F_NB_SLEW, F_NB_LEAD, F_NB_LAG, F_NB_THRESH,
  F_NR4_REDUCTION, F_NR4_SMOOTHING, F_NR4_WHITENING, F_NR4_RESCALE, F_NR4_THRESHOLD,
  F_NNR_MODEL, F_NNR_FLOOR,
  F_SQL_ON, F_SQL, F_EQ_ON, F_BINAURAL, F_CW_PEAK, F_LOW_LATENCY, F_FIR_SIZE,
  F_EQ_GAIN0,                 /* 13 gains: [0] is the added frequency-independent gain */
  F_EQ_FREQ0 = F_EQ_GAIN0 + 13,
  /* the AGC lines on the spectrum (M4 pick F): gain and hang threshold to set,
   * the two lines' levels in dBm to read (rx_panadapter.c draws them there) */
  F_AGC_GAIN = F_EQ_FREQ0 + 13, F_AGC_HANG_TH, F_AGC_LINE, F_AGC_HANG_LINE,
  /* deskHPSDR's RX menu, the parts that matter on an HL2 */
  F_AUTOGAIN, F_AUTOGAIN_TIME, F_DIGU_OFFSET, F_DIGL_OFFSET, F_AUDIO_CHANNEL, F_TEST_AUDIO,
  F_RESERVE_ON, F_RESERVE_MS, F_MUTE_RADIO, F_RX_IQ_GAIN, F_RX_IQ_PHASE, F_LNA,
  /* agc_menu.c's AGC Automatic and meter_menu.c's S-meter reading */
  F_AGC_AUTO, F_AGC_AUTO_OFFSET, F_SMETER_PEAK,
  F_COUNT,
};

int droid_dsp_count(void) { return F_COUNT; }

/* Current values, for the UI; read without a lock, like droid_get_status(). */
void droid_dsp_get(double *out) {
  const RECEIVER *rx = receiver[0];
  for (int i = 0; i < F_COUNT; i++) out[i] = 0;
  if (rx == NULL) return;
  out[F_NR] = rx->nr; out[F_NB] = rx->nb; out[F_ANF] = rx->anf; out[F_SNB] = rx->snb;
  out[F_MNF] = rx->mnf; out[F_MNF_FREQ] = rx->mnf_cfreq; out[F_MNF_WIDTH] = rx->mnf_fbw;
  out[F_NR_POST_AGC] = rx->nr_agc;
  out[F_NR2_GAIN] = rx->nr2_gain_method; out[F_NR2_NPE] = rx->nr2_npe_method; out[F_NR2_AE] = rx->nr2_ae;
  out[F_NR2_POST] = rx->nr2_post; out[F_NR2_POST_LEVEL] = rx->nr2_post_nlevel;
  out[F_NR2_POST_RATE] = rx->nr2_post_rate; out[F_NR2_POST_FACTOR] = rx->nr2_post_factor;
  out[F_NR2_POST_TAPER] = rx->nr2_post_taper;
  out[F_NR2_TRAINED_THRESH] = rx->nr2_trained_threshold; out[F_NR2_TRAINED_T2] = rx->nr2_trained_t2;
  out[F_NB2_MODE] = rx->nb2_mode;
  out[F_NB_SLEW] = rx->nb_tau * 1000.0; out[F_NB_LEAD] = rx->nb_advtime * 1000.0;
  out[F_NB_LAG] = rx->nb_hang * 1000.0; out[F_NB_THRESH] = rx->nb_thresh / 0.165;
  out[F_NR4_REDUCTION] = rx->nr4_reduction_amount; out[F_NR4_SMOOTHING] = rx->nr4_smoothing_factor;
  out[F_NR4_WHITENING] = rx->nr4_whitening_factor; out[F_NR4_RESCALE] = rx->nr4_noise_rescale;
  out[F_NR4_THRESHOLD] = rx->nr4_post_filter_threshold;
  out[F_NNR_MODEL] = rx->nnr_model; out[F_NNR_FLOOR] = rx->nnr_mask_floor;
  out[F_SQL_ON] = rx->squelch_enable; out[F_SQL] = rx->squelch;
  out[F_EQ_ON] = rx->eq_enable; out[F_BINAURAL] = rx->binaural;
  out[F_CW_PEAK] = vfo[0].cwAudioPeakFilter;
  out[F_LOW_LATENCY] = rx->low_latency; out[F_FIR_SIZE] = rx->fft_size;
  for (int i = 0; i < 13; i++) {
    out[F_EQ_GAIN0 + i] = rx->eq_gain[i];
    out[F_EQ_FREQ0 + i] = rx->eq_freq[i];
  }
  /* rx_panadapter.c: the lines sit at agc_thresh / agc_hang + soffset */
  const BAND *band = band_get_band(vfo[0].band);
  double soffset = (double)(rx_gain_calibration - band->gain) + (double)adc[rx->adc].attenuation - adc[rx->adc].gain;
  out[F_AGC_GAIN] = rx->agc_gain;
  out[F_AGC_HANG_TH] = rx->agc_hang_threshold;
  out[F_AGC_LINE] = rx->agc_thresh + soffset;
  out[F_AGC_HANG_LINE] = rx->agc_hang + soffset;
  out[F_AUTOGAIN] = autogain_enabled; out[F_AUTOGAIN_TIME] = autogain_time_enabled;
  out[F_DIGU_OFFSET] = rx->digi_offset_u; out[F_DIGL_OFFSET] = rx->digi_offset_l;
  out[F_AUDIO_CHANNEL] = rx->audio_channel;
  out[F_RESERVE_ON] = g_atomic_int_get(&rx_audio_network_reserve_enabled);
  out[F_RESERVE_MS] = g_atomic_int_get(&rx_audio_network_reserve_ms);
  out[F_MUTE_RADIO] = rx->mute_radio;
  out[F_RX_IQ_GAIN] = atomic_load_explicit(&((RECEIVER *)rx)->rx_iq_gain, memory_order_relaxed);
  out[F_RX_IQ_PHASE] = atomic_load_explicit(&((RECEIVER *)rx)->rx_iq_phase, memory_order_relaxed);
  out[F_LNA] = adc[rx->adc].gain;
  out[F_AGC_AUTO] = rx->agc_auto; out[F_AGC_AUTO_OFFSET] = rx->agc_auto_offset;
  out[F_SMETER_PEAK] = rx->smetermode == SMETER_PEAK;
}

/* On the radio's loop thread (radio_core.c CMD_DSP). */
void droid_dsp_apply(int field, double v) {
  RECEIVER *rx = receiver[0];
  if (rx == NULL || field < 0 || field >= F_COUNT) return;
  switch (field) {
  case F_NR: rx->nr = clampi(v, 0, 5); update_noise(rx); break;
  case F_NB: rx->nb = clampi(v, 0, 2); update_noise(rx); break;
  case F_SNB: rx->snb = v != 0; update_noise(rx); break;
  case F_NR_POST_AGC: rx->nr_agc = v != 0; update_noise(rx); rx_set_anf(rx); break;
  case F_NR2_GAIN: rx->nr2_gain_method = clampi(v, 0, 3); update_noise(rx); break;
  case F_NR2_NPE: rx->nr2_npe_method = clampi(v, 0, 2); update_noise(rx); break;
  case F_NR2_AE: rx->nr2_ae = v != 0; update_noise(rx); break;
  case F_NR2_POST: rx->nr2_post = v != 0; update_noise(rx); break;
  case F_NR2_POST_LEVEL: rx->nr2_post_nlevel = clampi(v, 0, 100); update_noise(rx); break;
  case F_NR2_POST_RATE: rx->nr2_post_rate = clampi(v, 0, 100); update_noise(rx); break;
  case F_NR2_POST_FACTOR: rx->nr2_post_factor = clampi(v, 0, 100); update_noise(rx); break;
  case F_NR2_POST_TAPER: rx->nr2_post_taper = clampi(v, 0, 100); update_noise(rx); break;
  case F_NR2_TRAINED_THRESH: rx->nr2_trained_threshold = clampd(v, -5.0, 5.0); update_noise(rx); break;
  case F_NR2_TRAINED_T2: rx->nr2_trained_t2 = clampd(v, 0.02, 0.3); update_noise(rx); break;
  case F_NB2_MODE: rx->nb2_mode = clampi(v, 0, 4); update_noise(rx); break;
  case F_NB_SLEW: rx->nb_tau = 0.001 * clampd(v, 0.0, 0.1); update_noise(rx); break;
  case F_NB_LEAD: rx->nb_advtime = 0.001 * clampd(v, 0.0, 0.1); update_noise(rx); break;
  case F_NB_LAG: rx->nb_hang = 0.001 * clampd(v, 0.0, 0.1); update_noise(rx); break;
  case F_NB_THRESH: rx->nb_thresh = 0.165 * clampd(v, 15.0, 500.0); update_noise(rx); break;
  case F_NR4_REDUCTION: rx->nr4_reduction_amount = clampd(v, 0.0, 20.0); update_noise(rx); break;
  case F_NR4_SMOOTHING: rx->nr4_smoothing_factor = clampd(v, 0.0, 100.0); update_noise(rx); break;
  case F_NR4_WHITENING: rx->nr4_whitening_factor = clampd(v, 0.0, 100.0); update_noise(rx); break;
  case F_NR4_RESCALE: rx->nr4_noise_rescale = clampd(v, 0.0, 12.0); update_noise(rx); break;
  case F_NR4_THRESHOLD: rx->nr4_post_filter_threshold = clampd(v, -10.0, 10.0); update_noise(rx); break;
  case F_NNR_MODEL: rx->nnr_model = clampi(v, 0, 1); update_noise(rx); break;
  case F_NNR_FLOOR: rx->nnr_mask_floor = clampd(v, -50.0, -10.0); update_noise(rx); break;
  case F_ANF: rx->anf = v != 0; update_anf(rx); break;
  /* the manual notch: noise_menu.c mnf_cb / mnf_fbw_cb, rx_panadapter.c's right-click */
  case F_MNF: rx->mnf = v != 0; rx_set_notch(rx); g_idle_add(ext_vfo_update, NULL); break;
  case F_MNF_FREQ: rx->mnf_cfreq = v > 1.0 ? v : 0.0; rx_set_notch(rx); break;
  case F_MNF_WIDTH: rx->mnf_fbw = clampd(v, 10.0, 15000.0); rx_set_notch(rx); break;
  /* squelch: sliders.c's squelch slider and enable button, 0..100 */
  case F_SQL_ON: rx->squelch_enable = v != 0; rx_set_squelch(rx); break;
  case F_SQL: rx->squelch = clampd(v, 0.0, 100.0); rx_set_squelch(rx); break;
  case F_EQ_ON: rx->eq_enable = v != 0; update_eq(rx); break;
  case F_BINAURAL:
    rx->binaural = (v != 0 && rx_binaural_allowed(rx)) ? 1 : 0;
    rx_set_af_binaural(rx);
    break;
  case F_CW_PEAK:   /* filter_menu.c cw_peak_cb() */
    vfo[0].cwAudioPeakFilter = v != 0;
    if (vfo[0].cwAudioPeakFilter) {
      for (int i = 0; i < 2; i++) {   /* receiver[0..1], as old_protocol.c has them */
        if (receiver[i] != NULL) receiver[i]->use_cw_dp_filter = 1;
      }
    }
    mode_settings[vfo[0].mode].cwPeak = vfo[0].cwAudioPeakFilter;
    copy_mode_settings(vfo[0].mode);
    rx_filter_changed(rx);
    g_idle_add(ext_vfo_update, NULL);
    break;
  /* sliders.c's AGC gain slider (-20..120) and agc_menu.c's hang threshold (0..100) */
  case F_AGC_GAIN: rx->agc_gain = clampd(v, -20.0, 120.0); rx_set_agc_gain(rx); break;
  case F_AGC_HANG_TH: rx->agc_hang_threshold = clampi(v, 0, 100); rx_set_agc(rx); break;
  /* rx_menu.c */
  case F_AUTOGAIN: autogain_enabled = v != 0; droid_autogain_run(1); g_idle_add(ext_vfo_update, NULL); break;
  case F_AUTOGAIN_TIME: autogain_time_enabled = v != 0; break;   /* the thread reads it each second */
  case F_DIGU_OFFSET: rx->digi_offset_u = clampi(v, 0, 4000); rx_frequency_changed(rx); g_idle_add(ext_vfo_update, NULL); break;
  case F_DIGL_OFFSET: rx->digi_offset_l = clampi(v, 0, 4000); rx_frequency_changed(rx); g_idle_add(ext_vfo_update, NULL); break;
  case F_AUDIO_CHANNEL: rx->audio_channel = clampi(v, STEREO, RIGHT); break;
  case F_TEST_AUDIO: audio_test_start(rx); break;
  case F_RESERVE_ON: g_atomic_int_set(&rx_audio_network_reserve_enabled, v != 0); break;
  case F_RESERVE_MS: g_atomic_int_set(&rx_audio_network_reserve_ms, clampi(v, 5, 500)); break;
  case F_MUTE_RADIO: rx->mute_radio = v != 0; break;
  /* fft_menu.c: manual RX I/Q correction, applied by receiver.c */
  case F_RX_IQ_GAIN: atomic_store_explicit(&rx->rx_iq_gain, clampd(v, -5.0, 5.0), memory_order_relaxed); break;
  case F_RX_IQ_PHASE: atomic_store_explicit(&rx->rx_iq_phase, clampd(v, -20.0, 20.0), memory_order_relaxed); break;
  case F_AGC_AUTO:
    rx->agc_auto = v != 0;
    if (rx->agc_auto) rx->panadapter_noisefloor_first_run = 1;
    break;
  case F_AGC_AUTO_OFFSET: rx->agc_auto_offset = clampd(v, -35.0, -15.0); break;
  case F_SMETER_PEAK: rx->smetermode = v != 0 ? SMETER_PEAK : SMETER_AVERAGE; break;
  case F_LOW_LATENCY: rx->low_latency = v != 0; rx_set_fft_latency(rx); break;
  case F_FIR_SIZE: {  /* a power of two, at least the DSP size, 1k..16k (fft_menu.c) */
    int s = clampi(v, 1024, 16384);
    int p = 1024;
    while (p < s) p *= 2;
    if (p < rx->dsp_size) p = rx->dsp_size;
    rx->fft_size = p;
    rx_set_fft_size(rx);
    break;
  }
  default:
    if (field >= F_EQ_GAIN0 && field < F_EQ_GAIN0 + 13) {
      rx->eq_gain[field - F_EQ_GAIN0] = clampd(v, -20.0, 20.0);
      update_eq(rx);
    } else if (field > F_EQ_FREQ0 && field < F_EQ_FREQ0 + 13) {
      /* equalizer_menu.c freq_changed_cb(): points stay in order, 10 Hz apart */
      int i = field - F_EQ_FREQ0;
      double lo = (i > 1) ? rx->eq_freq[i - 1] + 10.0 : 10.0;
      double hi = (i < 12) ? rx->eq_freq[i + 1] - 10.0 : 16000.0;
      rx->eq_freq[i] = clampd(v, lo, hi);
      update_eq(rx);
    }
    break;
  }
}
