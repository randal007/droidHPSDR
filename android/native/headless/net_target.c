/*
 * net_target.c — discovery_resolve_target() for targeted discovery (a radio
 * at a known address, e.g. the HL2 on the wired link at 169.254.19.221).
 *
 * Copied from upstream src/discovery.c (deskHPSDR, GPL-3.0-or-later, by
 * DL1YCF/DL1BZ and contributors), which droidHPSDR does not compile because
 * the rest of that file is the GTK discovery dialog. One change: inside an
 * Android app getifaddrs() may not report the interface (the app sandbox
 * hides interface details), so when the routed local address is not found
 * there, a link-local /16 or a /24 netmask is assumed instead of failing.
 *
 * discovery.c: Copyright (C) 2015 - John Melton, G0ORX/N6LYT,
 * 2024-2026 - Heiko Amft, DL1BZ (Project deskHPSDR); GPL-3.0-or-later.
 *
 * GPL v3 or later, like the rest of droidHPSDR.
 */
#include <errno.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <gtk/gtk.h>

#include "discovery.h"
#include "message.h"

int discovery_resolve_target(const char *host, struct sockaddr_in *target, struct sockaddr_in *local_addr,
                             struct sockaddr_in *netmask, char *ifname, size_t ifname_len, int *is_direct) {
  struct addrinfo hints = {0};
  struct addrinfo *result = NULL;
  struct ifaddrs *addrs = NULL;
  struct sockaddr_in route_target, route_local;
  socklen_t route_local_len = sizeof(route_local);
  int route_socket = -1;
  int rc = -1;
  if (host == NULL || host[0] == '\0' || target == NULL || local_addr == NULL ||
      netmask == NULL || ifname == NULL || ifname_len == 0 || is_direct == NULL) {
    errno = EINVAL;
    return -1;
  }
  memset(target, 0, sizeof(*target));
  memset(local_addr, 0, sizeof(*local_addr));
  memset(netmask, 0, sizeof(*netmask));
  memset(&route_target, 0, sizeof(route_target));
  memset(&route_local, 0, sizeof(route_local));
  ifname[0] = '\0';
  *is_direct = 0;
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;
  if (getaddrinfo(host, NULL, &hints, &result) != 0 || result == NULL) {
    t_print("%s: cannot resolve target %s\n", __func__, host);
    goto cleanup;
  }
  memcpy(target, result->ai_addr, sizeof(*target));
  target->sin_family = AF_INET;
  route_target = *target;
  route_target.sin_port = htons(radio_port);
  route_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (route_socket < 0) {
    t_perror("discovery_resolve_target: create route socket failed");
    goto cleanup;
  }
  if (connect(route_socket, (struct sockaddr *) &route_target, sizeof(route_target)) < 0) {
    t_perror("discovery_resolve_target: connect route socket failed");
    goto cleanup;
  }
  if (getsockname(route_socket, (struct sockaddr *) &route_local, &route_local_len) < 0) {
    t_perror("discovery_resolve_target: getsockname failed");
    goto cleanup;
  }
  if (getifaddrs(&addrs) == 0) {
    for (struct ifaddrs *ifa = addrs; ifa != NULL; ifa = ifa->ifa_next) {
      struct sockaddr_in addr, mask;
      if (ifa->ifa_addr == NULL || ifa->ifa_netmask == NULL || ifa->ifa_addr->sa_family != AF_INET ||
          ifa->ifa_netmask->sa_family != AF_INET) {
        continue;
      }
      memcpy(&addr, ifa->ifa_addr, sizeof(addr));
      memcpy(&mask, ifa->ifa_netmask, sizeof(mask));
      if (addr.sin_addr.s_addr != route_local.sin_addr.s_addr) continue;
      *local_addr = addr;
      local_addr->sin_port = htons(0);
      *netmask = mask;
      g_strlcpy(ifname, ifa->ifa_name, ifname_len);
      *is_direct = ((target->sin_addr.s_addr & mask.sin_addr.s_addr) == (addr.sin_addr.s_addr & mask.sin_addr.s_addr));
      rc = 0;
      break;
    }
  }
  if (rc != 0) {
    /* droidHPSDR: the sandbox hid the interface; use the routed address. */
    int link_local = (ntohl(route_local.sin_addr.s_addr) >> 16) == 0xA9FE;   /* 169.254/16 */
    *local_addr = route_local;
    local_addr->sin_port = htons(0);
    netmask->sin_family = AF_INET;
    netmask->sin_addr.s_addr = htonl(link_local ? 0xFFFF0000u : 0xFFFFFF00u);
    g_strlcpy(ifname, link_local ? "eth" : "net", ifname_len);
    *is_direct = ((target->sin_addr.s_addr & netmask->sin_addr.s_addr) ==
                  (route_local.sin_addr.s_addr & netmask->sin_addr.s_addr));
    t_print("%s: interface for %s not listed; assuming %s\n", __func__, inet_ntoa(route_local.sin_addr),
            link_local ? "link-local /16" : "/24");
    rc = 0;
  }
cleanup:
  if (addrs != NULL) freeifaddrs(addrs);
  if (route_socket >= 0) close(route_socket);
  if (result != NULL) freeaddrinfo(result);
  return rc;
}
