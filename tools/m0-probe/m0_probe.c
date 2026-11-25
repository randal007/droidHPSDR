/* m0-probe — droidHPSDR Milestone 0 transport check.
 *
 * Copyright (C) 2026 droidHPSDR contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Packet layout follows OpenHPSDR Protocol 1 as implemented in deskHPSDR's
 * src/old_discovery.c (John Melton G0ORX/N6LYT; Heiko Amft DL1BZ), GPL-3.0-or-later.
 * No deskHPSDR source is included here — this is a standalone diagnostic with
 * no WDSP, no GLib and no UI, so it can be built and run before any of the
 * porting work begins.
 *
 * Answers exactly one question: can this phone exchange Protocol 1 discovery
 * packets with the radio, over WiFi and over USB Ethernet?
 *
 * Build:  see build.sh
 * Usage:  m0-probe [-i iface] [-u ip | -w] [-t ms] [-n] [-l] [-v]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <ifaddrs.h>

#define DISCOVERY_PORT      1024
#define P1_DISCOVERY_LEN      63
#define P1_STOP_LEN           64
#define REPLY_MIN             60
#define DEFAULT_WIRED_IP  "169.254.19.221"
#define MAX_SEEN              16

/* Protocol 1 board IDs, from deskHPSDR src/discovered.h */
static const char *board_name(int id, int gateware_x10) {
    switch (id) {
    case 0:   return "Metis";
    case 1:   return "Hermes";
    case 2:   return "Griffin";
    case 4:   return "Angelia";
    case 5:   return "Orion";
    case 6:   return gateware_x10 >= 400 ? "Hermes Lite 2" : "Hermes Lite V1";
    case 7:   return "Ozy";
    case 10:  return "Orion2";
    case 100: return "STEMlab";
    case 101: return "STEMlab Z20";
    default:  return "unknown board";
    }
}

struct seen { uint8_t mac[6]; };
static struct seen seen[MAX_SEEN];
static int n_seen;

static int already_seen(const uint8_t *mac) {
    for (int i = 0; i < n_seen; i++)
        if (!memcmp(seen[i].mac, mac, 6)) return 1;
    if (n_seen < MAX_SEEN) memcpy(seen[n_seen++].mac, mac, 6);
    return 0;
}

static void list_interfaces(void) {
    struct ifaddrs *ifa, *p;
    if (getifaddrs(&ifa) < 0) { perror("getifaddrs"); return; }
    printf("Interfaces with an IPv4 address:\n");
    for (p = ifa; p; p = p->ifa_next) {
        if (!p->ifa_addr || p->ifa_addr->sa_family != AF_INET) continue;
        if (p->ifa_flags & IFF_LOOPBACK) continue;
        char a[INET_ADDRSTRLEN] = "?", m[INET_ADDRSTRLEN] = "?";
        inet_ntop(AF_INET, &((struct sockaddr_in *)p->ifa_addr)->sin_addr, a, sizeof a);
        if (p->ifa_netmask)
            inet_ntop(AF_INET, &((struct sockaddr_in *)p->ifa_netmask)->sin_addr, m, sizeof m);
        printf("  %-10s %-15s netmask %-15s %s\n", p->ifa_name, a, m,
               (p->ifa_flags & IFF_UP) ? "up" : "DOWN");
    }
    freeifaddrs(ifa);
}

/* Find the IPv4 address of `want`, or of the first usable interface if NULL.
 * Binding to this address is what forces the probe out of the interface we
 * mean: with cellular up, the default route is not the one we want. */
static int pick_interface(const char *want, struct in_addr *addr,
                          struct in_addr *mask, char *name, size_t namelen) {
    struct ifaddrs *ifa, *p;
    int found = 0;
    if (getifaddrs(&ifa) < 0) { perror("getifaddrs"); return 0; }
    for (p = ifa; p && !found; p = p->ifa_next) {
        if (!p->ifa_addr || p->ifa_addr->sa_family != AF_INET) continue;
        if (p->ifa_flags & IFF_LOOPBACK) continue;
        if (!(p->ifa_flags & IFF_UP)) continue;
        if (want && strcmp(want, p->ifa_name)) continue;
        *addr = ((struct sockaddr_in *)p->ifa_addr)->sin_addr;
        mask->s_addr = p->ifa_netmask
            ? ((struct sockaddr_in *)p->ifa_netmask)->sin_addr.s_addr : 0;
        snprintf(name, namelen, "%s", p->ifa_name);
        found = 1;
    }
    freeifaddrs(ifa);
    return found;
}

static void send_to(int s, const void *buf, size_t len, struct in_addr dst, const char *what) {
    struct sockaddr_in to = {0};
    to.sin_family = AF_INET;
    to.sin_port   = htons(DISCOVERY_PORT);
    to.sin_addr   = dst;
    char a[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &dst, a, sizeof a);
    if (sendto(s, buf, len, 0, (struct sockaddr *)&to, sizeof to) < 0) {
        printf("  %-9s -> %-15s FAILED: %s\n", what, a, strerror(errno));
        if (errno == ENETUNREACH)
            printf("      ENETUNREACH means there is no route for that address.\n"
                   "      On the wired path this is almost always a missing local\n"
                   "      address: set eth0 to 169.254.1.1 / 255.255.0.0 by hand in\n"
                   "      One UI. Scanning the link-local range will not help — the\n"
                   "      packets cannot leave the phone without a local address.\n");
    } else {
        printf("  %-9s -> %s\n", what, a);
    }
}

static void print_reply(const struct sockaddr_in *from, const uint8_t *b, int n, int verbose) {
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &from->sin_addr, ip, sizeof ip);

    if (n < REPLY_MIN || b[0] != 0xEF || b[1] != 0xFE) {
        if (verbose) printf("  ignored %d bytes from %s (not a P1 reply)\n", n, ip);
        return;
    }
    int status = b[2];
    if (status != 2 && status != 3) {
        if (verbose) printf("  ignored status 0x%02x from %s\n", status, ip);
        return;
    }
    const uint8_t *mac = b + 3;
    if (already_seen(mac)) return;

    int board = b[10];
    int major = b[9], minor = b[21];
    int gateware_x10 = 10 * major + minor;

    printf("\n  RADIO FOUND\n");
    printf("    address    : %s:%d\n", ip, ntohs(from->sin_port));
    printf("    MAC        : %02x:%02x:%02x:%02x:%02x:%02x\n",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    printf("    board      : %s (id %d)\n", board_name(board, gateware_x10), board);
    if (board == 6)
        printf("    gateware   : %d.%d\n", major, minor);
    else
        printf("    firmware   : %d\n", major);
    printf("    status     : %s\n", status == 2 ? "idle, available"
                                                : "IN USE by another client");
    if (board == 6) {
        if (b[11] & 0x20)
            printf("    NOTE       : EEPROM fixed IP %d.%d.%d.%d is set (DHCP overrides)\n",
                   b[13], b[14], b[15], b[16]);
        else if (b[11] & 0x80)
            printf("    WARNING    : EEPROM fixed IP %d.%d.%d.%d is set and DHCP is\n"
                   "                 ignored. This breaks the WiFi path — droidHPSDR\n"
                   "                 deliberately does not use the EEPROM fixed IP.\n",
                   b[13], b[14], b[15], b[16]);
        if (b[11] & 0x40)
            printf("    NOTE       : MAC address modified in EEPROM (...:%02x:%02x)\n",
                   b[17], b[18]);
    }
}

int main(int argc, char **argv) {
    const char *iface = NULL, *unicast = NULL;
    int timeout_ms = 2000, send_stop = 1, verbose = 0, c;

    while ((c = getopt(argc, argv, "i:u:wt:nlvh")) != -1) {
        switch (c) {
        case 'i': iface = optarg; break;
        case 'u': unicast = optarg; break;
        case 'w': unicast = DEFAULT_WIRED_IP; break;
        case 't': timeout_ms = atoi(optarg); break;
        case 'n': send_stop = 0; break;
        case 'l': list_interfaces(); return 0;
        case 'v': verbose = 1; break;
        default:
            printf("usage: %s [-i iface] [-u ip | -w] [-t ms] [-n] [-l] [-v]\n"
                   "  -i iface  bind to this interface (default: first usable)\n"
                   "  -u ip     unicast to ip instead of broadcasting\n"
                   "  -w        wired path: unicast to " DEFAULT_WIRED_IP "\n"
                   "            (the HL2 APIPA address when no DHCP server answers)\n"
                   "  -t ms     listen this long for replies (default 2000)\n"
                   "  -n        do not send METIS STOP first\n"
                   "  -l        list interfaces and exit\n"
                   "  -v        show packets that were ignored\n", argv[0]);
            return 2;
        }
    }
    if (timeout_ms < 1) timeout_ms = 1;   /* SO_RCVTIMEO 0 would mean "wait forever" */
    struct in_addr unicast_addr = {0};
    if (unicast && inet_pton(AF_INET, unicast, &unicast_addr) != 1) {
        fprintf(stderr, "bad address: %s\n", unicast); return 1;
    }

    struct in_addr local, mask;
    char name[IFNAMSIZ] = "?";
    if (!pick_interface(iface, &local, &mask, name, sizeof name)) {
        fprintf(stderr, "no usable IPv4 interface%s%s.\n",
                iface ? " named " : "", iface ? iface : "");
        fprintf(stderr, "Run with -l to see what is available.\n");
        if (iface && !strcmp(iface, "eth0"))
            fprintf(stderr,
                "eth0 has no address. Android does not self-assign a link-local\n"
                "address the way Windows does — set it by hand in One UI to\n"
                "169.254.1.1 / 255.255.0.0. It persists per adapter.\n");
        return 1;
    }

    char la[INET_ADDRSTRLEN], lm[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &local, la, sizeof la);
    inet_ntop(AF_INET, &mask, lm, sizeof lm);
    printf("m0-probe: binding to %s (%s netmask %s)\n", name, la, lm);

    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) { perror("socket"); return 1; }

    int on = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &on, sizeof on);
    setsockopt(s, SOL_SOCKET, SO_BROADCAST, &on, sizeof on);

    struct sockaddr_in bind_addr = {0};
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_addr   = local;   /* the whole point: pin the source interface */
    bind_addr.sin_port   = htons(0);
    if (bind(s, (struct sockaddr *)&bind_addr, sizeof bind_addr) < 0) {
        perror("bind"); close(s); return 1;
    }

    uint8_t pkt[P1_STOP_LEN];

    /* A radio left streaming by a client that never sent METIS STOP will keep
     * the receive loop busy with sample packets. Stop it first. */
    if (send_stop) {
        memset(pkt, 0, P1_STOP_LEN);
        pkt[0] = 0xEF; pkt[1] = 0xFE; pkt[2] = 0x04; pkt[3] = 0x00;
        if (unicast) {
            send_to(s, pkt, P1_STOP_LEN, unicast_addr, "STOP");
        } else {
            struct in_addr d; d.s_addr = INADDR_BROADCAST;
            send_to(s, pkt, P1_STOP_LEN, d, "STOP");
        }
        usleep(20000);
    }

    memset(pkt, 0, P1_DISCOVERY_LEN);
    pkt[0] = 0xEF; pkt[1] = 0xFE; pkt[2] = 0x02;

    if (unicast) {
        struct in_addr d;
        if (inet_pton(AF_INET, unicast, &d) != 1) {
            fprintf(stderr, "bad address: %s\n", unicast); close(s); return 1;
        }
        send_to(s, pkt, P1_DISCOVERY_LEN, d, "DISCOVER");
    } else {
        struct in_addr d;
        d.s_addr = INADDR_BROADCAST;
        send_to(s, pkt, P1_DISCOVERY_LEN, d, "DISCOVER");
        /* Some stacks drop 255.255.255.255 but pass the subnet-directed
         * broadcast, so send both and let the radio answer whichever arrives. */
        if (mask.s_addr) {
            d.s_addr = local.s_addr | ~mask.s_addr;
            if (d.s_addr != INADDR_BROADCAST)
                send_to(s, pkt, P1_DISCOVERY_LEN, d, "DISCOVER");
        }
    }

    struct timeval tv = { timeout_ms / 1000, (timeout_ms % 1000) * 1000 };
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);

    printf("  listening %d ms...\n", timeout_ms);
    for (;;) {
        uint8_t buf[2048];
        struct sockaddr_in from;
        socklen_t fl = sizeof from;
        int n = recvfrom(s, buf, sizeof buf, 0, (struct sockaddr *)&from, &fl);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            perror("recvfrom"); break;
        }
        print_reply(&from, buf, n, verbose);
    }
    close(s);

    printf("\nm0-probe: %d radio(s) found on %s.\n", n_seen, name);
    return n_seen ? 0 : 1;
}
