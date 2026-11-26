/*
 * n2adr.c — the HL2 filter board's per-band OC bits, as deskHPSDR sets them.
 *
 * radio.c's start_radio() calls n2adr_oc_settings() for every Hermes Lite 2
 * (and again after loading the props file), so the HL2's N2ADR low-pass /
 * high-pass filter board switches with the band. The function lives in
 * radio_menu.c, the GTK dialog droidHPSDR does not compile, so it is copied
 * here unchanged, with n2adr_oc_settings_tx() for the "N2ADR (LPF TX only)"
 * choice and its n2adr_hpf_enable (radio.c, default 1).
 *
 * radio_menu.c: Copyright (C) 2015 - John Melton, G0ORX/N6LYT,
 * 2024-2026 - Heiko Amft, DL1BZ (Project deskHPSDR); GPL-3.0-or-later.
 *
 * GPL v3 or later, like the rest of droidHPSDR.
 */
#include <gtk/gtk.h>

#include "band.h"
#include "new_protocol.h"   /* schedule_high_priority(): a no-op on P1 (stubs.c), OC goes out in every frame */
#include "radio.h"
#include "radio_menu.h"

int n2adr_hpf_enable = 1;

void n2adr_oc_settings(void) {
  //
  // set OC outputs for each band according to the N2ADR board requirements
  // unlike load_filters(), this can be executed outside the GTK queue
  //
  BAND *band;
  band = band_get_band(band160);
  band->OCrx = band->OCtx = 1;
  band = band_get_band(band80);
  band->OCrx = band->OCtx = 66;
  band = band_get_band(band60);
  band->OCrx = band->OCtx = 68;
  band = band_get_band(band40);
  band->OCrx = band->OCtx = 68;
  band = band_get_band(band30);
  band->OCrx = band->OCtx = 72;
  band = band_get_band(band20);
  band->OCrx = band->OCtx = 72;
  band = band_get_band(band17);
  band->OCrx = band->OCtx = 80;
  band = band_get_band(band15);
  band->OCrx = band->OCtx = 80;
  band = band_get_band(band12);
  band->OCrx = band->OCtx = 96;
  band = band_get_band(band10);
  band->OCrx = band->OCtx = 96;
  schedule_high_priority();
}

void n2adr_oc_settings_tx(void) {
  //
  // set OC outputs for each band according to the N2ADR board requirements
  // unlike load_filters(), this can be executed outside the GTK queue
  //
  // Bit 6 is the 3 Mhz HPF and should be set from 80m - 10m except 160m
  // to set Bit 6 (shown as 7 in OC Menu) OCrx must be decimal 64 (01000000)
  // hpf_flag = 0 -> HPF OFF, hpf_flag = 1 -> HPF ON
  int rx_val = (n2adr_hpf_enable == 1) ? 64 : 0;
  BAND *band;
  band = band_get_band(band160);
  band->OCrx = 0; // 160m HPF already OFF
  band->OCtx = 1;
  band = band_get_band(band80);
  band->OCrx = rx_val;
  band->OCtx = 66;
  band = band_get_band(band60);
  band->OCrx = rx_val;
  band->OCtx = 68;
  band = band_get_band(band40);
  band->OCrx = rx_val;
  band->OCtx = 68;
  band = band_get_band(band30);
  band->OCrx = rx_val;
  band->OCtx = 72;
  band = band_get_band(band20);
  band->OCrx = rx_val;
  band->OCtx = 72;
  band = band_get_band(band17);
  band->OCrx = rx_val;
  band->OCtx = 80;
  band = band_get_band(band15);
  band->OCrx = rx_val;
  band->OCtx = 80;
  band = band_get_band(band12);
  band->OCrx = rx_val;
  band->OCtx = 96;
  band = band_get_band(band10);
  band->OCrx = rx_val;
  band->OCtx = 96;
  schedule_high_priority();
}
