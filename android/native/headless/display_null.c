/*
 * display_null.c — the display hooks for the headless m1-rx build: no
 * screen, so upstream's panadapter, waterfall and meter updates do nothing.
 * The app links app/display_bridge.c instead.
 *
 * GPL v3 or later, like the rest of droidHPSDR.
 */
#include <netinet/in.h>
#include <gtk/gtk.h>

#include "receiver.h"
#include "meter.h"
#include "rx_panadapter.h"
#include "waterfall.h"

void rx_panadapter_init(RECEIVER *rx, int width, int height) { (void)rx; (void)width; (void)height; }
void rx_panadapter_update(RECEIVER *rx) { (void)rx; }
void waterfall_init(RECEIVER *rx, int width, int height) { (void)rx; (void)width; (void)height; }
void waterfall_update(RECEIVER *rx) { (void)rx; }
void meter_update(RECEIVER *rx, int meter_type, double value, double alc, double swr) {
  (void)rx; (void)meter_type; (void)value; (void)alc; (void)swr;
}
