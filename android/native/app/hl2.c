/*
 * hl2.c — HL2 setup from the phone (M6, picture F2): reboot and the EEPROM
 * network settings (DHCP preference, alternate MAC), over the HL2's
 * configuration port (UDP 1025).
 *
 * hl2_command(), hl2_eeprom(), hl2_write_checked() and the reboot packet are
 * copied from deskHPSDR's p2_programmer.c (its P1 Setup dialog, GTK, not
 * compiled here), with the dialog parts taken out: the target is the radio
 * found by discovery, errors come back as text.
 *
 * p2_programmer.c: Copyright (C) 2024-2026 - Heiko Amft, DL1BZ (Project
 * deskHPSDR); GPL-3.0-or-later.
 *
 * The static IP write is deliberately not here (CLAUDE.md: it switches DHCP
 * off, so the HL2 can no longer be found on WiFi). Gateware flashing is not
 * here either yet: it is the one action that can leave the HL2 needing a
 * JTAG recovery, so it gets its own step.
 *
 * GPL v3 or later, like the rest of droidHPSDR.
 */
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <gtk/gtk.h>

#include "discovered.h"
#include "old_protocol.h"
#include "radio.h"

#include "droid_app.h"

/* the radio discovery found (radio.c's `radio`), as the target */
static int target(struct sockaddr_in *dst, unsigned char mac[6], char *error, size_t error_len) {
  if (radio == NULL || radio->device != DEVICE_HERMES_LITE2) {
    snprintf(error, error_len, "No HL2 found yet: start the radio once (over Ethernet), then try again.");
    return -1;
  }
  *dst = radio->info.network.address;
  dst->sin_port = htons(1025);
  memcpy(mac, radio->info.network.mac_address, 6);
  return 0;
}

/* HL2 configuration commands use UDP 1025 (not the FPGA bootloader port). */
static int hl2_command(unsigned char addr, const unsigned char data[4], unsigned char reply[60], char *error,
                       size_t error_len) {
  struct sockaddr_in dst;
  unsigned char mac[6];
  if (target(&dst, mac, error, error_len) != 0) return -1;
  int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sock < 0) {
    snprintf(error, error_len, "socket: %s", strerror(errno));
    return -1;
  }
  struct timeval tv = { 2, 0 };
  setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
  unsigned char packet[60] = {0xef, 0xfe, 0x05, 0x7f};
  packet[4] = (unsigned char)(addr << 1);
  memcpy(packet + 5, data, 4);
  int result = -1;
  for (int attempt = 0; attempt < 3; ++attempt) {
    if (sendto(sock, packet, sizeof(packet), 0, (struct sockaddr *)&dst, sizeof(dst)) != sizeof(packet)) {
      snprintf(error, error_len, "HL2 command send failed: %s", strerror(errno));
      break;
    }
    for (;;) {
      struct sockaddr_in from;
      socklen_t fromlen = sizeof(from);
      ssize_t n = recvfrom(sock, reply, 60, 0, (struct sockaddr *)&from, &fromlen);
      if (n < 0) {
        if (errno == EINTR) { continue; }
        break;
      }
      if (n == 60 && from.sin_addr.s_addr == dst.sin_addr.s_addr &&
          from.sin_port == dst.sin_port && reply[0] == 0xef && reply[1] == 0xfe &&
          memcmp(reply + 3, mac, 6) == 0) {
        result = 0;
        break;
      }
    }
    if (result == 0) { break; }
  }
  close(sock);
  if (result != 0 && !error[0]) { snprintf(error, error_len, "No HL2 response on UDP 1025."); }
  return result;
}

static int hl2_eeprom(unsigned char reg, int value, unsigned char *read_value, char *error, size_t error_len) {
  unsigned char cmd[4] = {value < 0 ? 0x07 : 0x06, 0xac,
                          (unsigned char)((reg << 4) | (value < 0 ? 0x0c : 0)),
                          (unsigned char)(value < 0 ? 0 : value)
                         };
  unsigned char reply[60];
  if (hl2_command(0x3d, cmd, reply, error, error_len) != 0) { return -1; }
  if (read_value) { *read_value = reply[0x19]; } /* response_data bits 15:8 */
  return 0;
}

static int hl2_write_checked(unsigned char reg, unsigned char value, char *error, size_t error_len) {
  unsigned char actual;
  if (hl2_eeprom(reg, value, NULL, error, error_len) != 0) { return -1; }
  usleep(10000);
  if (hl2_eeprom(reg, -1, &actual, error, error_len) != 0) { return -1; }
  if (actual != value) {
    snprintf(error, error_len, "HL2 EEPROM register %u: readback %02X, expected %02X.", reg, actual, value);
    return -1;
  }
  return 0;
}

int droid_hl2_read_flags(int *flags, char *error, int error_len) {
  unsigned char f;
  error[0] = 0;
  if (hl2_eeprom(6, -1, &f, error, error_len) != 0) return -1;
  *flags = f;
  return 0;
}

/* p2_programmer.c's hl2_dhcp_clicked(): prefer DHCP (bit 0x20), static IP off (bit 0x80) */
int droid_hl2_set_dhcp(char *error, int error_len) {
  unsigned char flags;
  error[0] = 0;
  if (hl2_eeprom(6, -1, &flags, error, error_len) != 0) return -1;
  return hl2_write_checked(6, (unsigned char)((flags & ~0x80) | 0x20), error, error_len);
}

/* p2_programmer.c's hl2_mac_clicked(): the last two MAC octets, then the "use alternate MAC" bit */
int droid_hl2_set_mac(int a, int b, char *error, int error_len) {
  unsigned char flags;
  error[0] = 0;
  if (a < 0 || a > 255 || b < 0 || b > 255 || (a == 0xff && b == 0xff)) {
    snprintf(error, error_len, "Invalid alternate MAC address.");
    return -1;
  }
  if (hl2_eeprom(6, -1, &flags, error, error_len) == 0 &&
      hl2_write_checked(12, (unsigned char)a, error, error_len) == 0 &&
      hl2_write_checked(13, (unsigned char)b, error, error_len) == 0) {
    hl2_write_checked(6, flags | 0x40, error, error_len);
  }
  return error[0] ? -1 : 0;
}

/* p2_programmer.c's hl2_reboot_clicked(): the HL2 restarts; the app finds it again by starting the radio */
int droid_hl2_reboot(char *error, int error_len) {
  struct sockaddr_in dst;
  unsigned char mac[6];
  error[0] = 0;
  if (target(&dst, mac, error, error_len) != 0) return -1;
  unsigned char cmd[4] = {0, 0, 0, 1};
  unsigned char packet[60] = {0xef, 0xfe, 0x05, 0x7f, 0x74};
  memcpy(packet + 5, cmd, 4);
  int sock = socket(AF_INET, SOCK_DGRAM, 0);
  int ok = sock >= 0 && sendto(sock, packet, sizeof(packet), 0, (struct sockaddr *)&dst, sizeof(dst)) == sizeof(packet);
  if (!ok) snprintf(error, error_len, "Failed to send HL2 reboot command.");
  if (sock >= 0) close(sock);
  return ok ? 0 : -1;
}

/* What the IO board detection found (old_protocol.c), for the SDR Device sheet. */
void droid_hl2_info(int *iob, int *pico, char *lpf, int lpf_len, int *gateware) {
  *iob = hl2_iob_is_present();
  *pico = hl2_pico_is_present();
  const char *s = *pico && hl2_iob_get_lpf_status() != 0 ? hl2_iob_get_lpf_status_str() : "";
  snprintf(lpf, lpf_len, "%s", s ? s : "");
  *gateware = radio ? radio->software_version : 0;
}
