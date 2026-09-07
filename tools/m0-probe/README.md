# m0-probe

Milestone 0 transport check. One C file, no WDSP, no GLib, no UI — it exists to
answer one question before any porting work starts:

> Can the phone exchange OpenHPSDR Protocol 1 discovery packets with the radio,
> over WiFi and over USB Ethernet?

## Build

```
./build.sh          # arm64 for the phone (needs the NDK)
./build.sh -host    # this desktop, for comparing against a known-good path
```

`build.sh` looks for the NDK at `$ANDROID_NDK_HOME`, falling back to
`~/Android/Sdk/ndk/27.3.13750724`.

## Run

```
adb push m0-probe /data/local/tmp/ && adb shell chmod 755 /data/local/tmp/m0-probe
adb shell /data/local/tmp/m0-probe -l           # list interfaces
adb shell /data/local/tmp/m0-probe -i wlan0     # WiFi path
adb shell /data/local/tmp/m0-probe -i eth0 -w   # USB Ethernet path
```

Exit status is 0 if at least one radio answered, 1 otherwise, so it can be used
in a script.

| Flag | Meaning |
|---|---|
| `-i iface` | Bind to this interface. Default: first usable one |
| `-u ip` | Unicast to `ip` instead of broadcasting |
| `-w` | Wired path — unicast to `169.254.19.221` |
| `-t ms` | Listen this long for replies (default 2000) |
| `-n` | Do not send METIS STOP first |
| `-l` | List interfaces and exit |
| `-v` | Show packets that were ignored |

## Why it binds to an interface

The probe binds its socket to the chosen interface's address rather than
letting the kernel pick. With cellular up, the default route is not the one we
want, and a discovery broadcast that leaves over the mobile interface simply
disappears. Explicit binding is the whole point of the exercise.

`SO_BINDTODEVICE` would be the more direct way to say this, but it needs
`CAP_NET_RAW`. The phone is not rooted, so binding to the address is the
portable equivalent.

## The wired path needs a manual step

Android does not self-assign an IPv4 link-local address the way Windows and
desktop Linux do. With no DHCP server on a direct USB Ethernet link, the HL2
times out and falls back to APIPA at `169.254.19.221 / 255.255.0.0`, but the
phone's `eth0` stays addressless — so there is no route, and every send fails
with `ENETUNREACH`.

Set `eth0` by hand in One UI to `169.254.1.1 / 255.255.0.0`. Android persists
this per adapter, so it is a one-time step. Allow time after powering the radio
on: its DHCP attempt has to time out before APIPA kicks in.

**Scanning the link-local range is not a workaround for a missing local
address** — without one the packets cannot leave the phone at all.

## Packet format

Protocol 1 discovery, as implemented in deskHPSDR's `src/old_discovery.c`
(John Melton G0ORX/N6LYT; Heiko Amft DL1BZ). No deskHPSDR source is included
here — only the wire format is reused.

- **Send** 63 bytes to UDP port 1024: `EF FE 02`, then zeros.
- Optionally preceded by METIS STOP, 64 bytes: `EF FE 04 00`, then zeros.
  A radio left streaming by a client that never stopped it will otherwise flood
  the receive loop with sample packets.
- **Reply** is at least 60 bytes:

| Offset | Meaning |
|---|---|
| 0–1 | `EF FE` |
| 2 | Status: `02` idle, `03` in use |
| 3–8 | MAC address |
| 9 | Gateware/firmware major version |
| 10 | Board ID (6 = Hermes Lite) |
| 11 | HL2 EEPROM flags: `0x80` fixed IP, `0x20` DHCP overrides it, `0x40` MAC modified |
| 13–16 | EEPROM fixed IP, when set |
| 17–18 | Modified MAC, low two bytes |
| 21 | HL2 gateware minor version |

A Hermes Lite reports board ID 6 for both generations; `10*major + minor >= 400`
distinguishes an HL2 from an HL1.

## Verified

Against a Hermes Lite 2, gateware 75.0, MAC `00:1c:c0:a2:13:dd`, over a direct
USB Ethernet link from the Linux desktop — both unicast (`-w`) and broadcast.
