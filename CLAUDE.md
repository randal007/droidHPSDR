# droidHPSDR — working context

Read this before touching code. It records decisions already made and the
reasoning behind them, so they don't get re-litigated or accidentally undone.

## What this is

An Android SDR transceiver front end for OpenHPSDR Protocol 1/2, forked from
deskHPSDR (DL1BZ), which forked piHPSDR (DL1YCF). GPLv3.

Target hardware is a Hermes Lite 2 with the HL2+ companion board. Everything
else is secondary.

## Hardware actually in hand

Assume these are available for testing; don't write speculative abstractions for
hardware that isn't here.

- **Hermes Lite 2** with **HL2+ companion board installed** (AK4951 codec, mic
  jack with PTT, phones, local speaker, CW sidetone). Requires the custom
  gateware, not stock HL2 gateware.
- **Samsung Galaxy S24 Ultra, not rooted.** Snapdragon 8 Gen 3, ample CPU
  headroom for WDSP. No root means no `ifconfig` and no `CAP_NET_ADMIN` — network
  interfaces can only be configured through One UI's settings, never
  programmatically.
- HL2 IO board: planned, not yet in scope.

SDR++ Brown already connects to this radio from this phone, so the transport
path is known-good. It does **not** use the HL2+ codec, which is the gap this
project exists to fill.

## Non-negotiable design decisions

### 1. Audio goes through the radio, never through Android

Mic and speaker audio ride inside the Protocol 1 frames — mic samples up in the
EP6 stream, demodulated audio down in the EP2 stream, to and from the AK4951 on
the companion board.

**Do not add Oboe, AAudio, or any Android audio dependency.** If a task seems to
need one, the design is being misread. A local Android audio path is a distant
secondary feature for plain-HL2 users and is not in scope now.

piHPSDR/deskHPSDR already have a LOCAL vs RADIO audio routing concept. Find and
adapt it rather than writing new.

### 2. WDSP stays

PureSignal, the noise reduction suite, the TX audio chain and the filter set all
live in WDSP. Replacing the DSP means losing them. WDSP + FFTW3 cross-compile
for arm64 with the NDK.

PureSignal is a primary reason for choosing this base over alternatives. Do not
propose swapping the DSP layer.

### 3. `core/` contains no UI types

No GTK, ImGui, GL or JNI types anywhere under `core/`. This is the whole point
of the port — deskHPSDR fails this today, with `GtkWidget*` members sitting
inside receiver and transmitter structs and cross-thread updates going through
`g_idle_add` on a GMainLoop.

Worth attempting first: **GLib without GTK does cross-compile for Android.**
Keeping GLib and removing only the widget layer may avoid rewriting the
threading model entirely. Try that before anything more invasive.

### 4. Radio threads are independent of rendering

Protocol, DSP and audio threads must keep running when the GL surface stops.
Backgrounding the app must not stall the radio, because running alongside
digital-mode apps is a goal. Do not drive DSP from the render loop, however
natural that looks with ImGui.

Needs a foreground service, a WiFi lock, and a battery-optimisation exemption.

### 5. Transport is abstracted

USB Ethernet and WiFi are both first-class behind one interface. Buffer depth is
a user setting, defaulting deeper on WiFi. Always allow entering the radio's IP
directly.

## Network setup

Two paths, deliberately different:

**WiFi (home LAN).** HL2 gets a DHCP lease from the router. Normal P1 discovery
broadcast to 255.255.255.255:1024. Bind to the WiFi interface explicitly —
with cellular up, the default route is not the one you want.

**USB Ethernet (direct).** No DHCP server on the link, so the HL2 times out and
falls back to APIPA at **169.254.19.221, netmask 255.255.0.0**. It is
consistently that address. Allow time after power-on — the DHCP attempt must
time out first.

The phone does **not** self-assign an IPv4 link-local address the way Windows
does, so eth0 must be given a static address by hand in One UI:
`169.254.1.1 / 255.255.0.0`. Android persists this per adapter, so it is a
one-time step.

Consequences for code:

- With no address on eth0 there is no route for 169.254.0.0/16 and every send
  fails with `ENETUNREACH`. **Scanning the link-local range is not a workaround
  for a missing local address** — the scan cannot leave the phone. Do not write
  one.
- On the wired path, skip discovery and target 169.254.19.221 directly.
- Do **not** program a fixed IP into the HL2 EEPROM. It disables DHCP, which
  breaks the WiFi path, and the EEPROM stores only an address — no netmask, no
  gateway.

## Fork and git strategy

This is a **fork of deskHPSDR's git history**, not a fresh repository. That
keeps blame intact, makes GPL attribution unambiguous, and allows cherry-picking
DL1BZ's ongoing HL2 fixes.

- `upstream` remote points at `dl1bz/deskhpsdr`, with a pristine tracking branch
  that is never modified locally.
- Bring upstream fixes across by cherry-pick, never by merging `master`.
- Do the `core/` and `ui/` restructure as explicit `git mv` commits so history
  follows the files. Restructure and edit in **separate** commits — a move plus
  a rewrite in one commit destroys the ability to follow either.
- **Never reformat upstream files.** A whitespace or style pass makes every
  future cherry-pick conflict. Match surrounding style even where it is not to
  taste.

## Repo layout

```
core/
  protocol/     P1/P2 framing, discovery
  radio/        receiver, transmitter, vfo, band, mode
  dsp/          WDSP glue
  transport/    usb-eth and wifi behind one interface
ui/
  panadapter/   GLES waterfall
  panels/       ImGui
android/        Gradle, NativeActivity, JNI
vendor/         wdsp, fftw, ft8_lib (submodules)
tools/
  m0-probe/     standalone discovery/reachability test
docs/
  PORTING-NOTES.md
  HARDWARE.md
```

## Build targets

Decided; don't widen these without a reason.

- **arm64-v8a only.** One target device. No armeabi-v7a, no x86 emulator builds.
- **minSdk 31.** Avoids a long tail of compatibility paths.
- targetSdk must declare `foregroundServiceType` — required for the background
  operation in decision 4.
- Build host is a desktop with the Android NDK, not the phone.
- deskHPSDR builds with a Makefile plus `make.config.deskhpsdr`. Converting to
  CMake, and cross-compiling FFTW3, is a substantial piece of M1 in its own
  right — treat it as a named task, not incidental setup.

## Milestones

- **M0 — transport check.** Mostly answered. Only open item: confirm a USB
  Ethernet adapter gets a working static config on the unrooted S24 Ultra via
  One UI. Manual test, not a coding task.
- **M1 — headless core.** Cross-compile WDSP, FFTW3 and the protocol layer for
  arm64. Discover, stream, print S-meter values to logcat. No UI.
- **M2 — HL2+ codec loopback.** Headless proof that audio written into the EP2
  slots reaches the companion board's speaker, and mic samples arrive in EP6.
  Validates the entire audio architecture. Do this before any UI work.
- **M3 — picture.** Android shell + GL waterfall.
- **M4 — usable RX.** VFO, band, mode, filter, AGC.
- **M5 — TX.** SSB first, then CW.
- **M6 — extras.** IO board I2C, PureSignal, built-in FT8/FT4.

## Acceptance criteria

M1 and M2 have no screen, so "done" needs defining up front.

- **M0:** `m0-probe` on the phone gets a discovery reply from the radio over
  both WiFi and USB Ethernet.
- **M1:** S-meter values in logcat visibly track a real signal — they move when
  an antenna is connected and disconnected, and change with band.
- **M2:** demodulated audio written to a WAV file on the phone, pulled with
  `adb pull`, sounds correct on listening; and mic audio captured from the HL2+
  jack round-trips into a second WAV.

## First tasks

1. `tools/m0-probe` — one C file, no WDSP, no UI. Opens a UDP socket, sends the
   P1 discovery broadcast, prints replies; plus a direct-unicast mode targeting
   169.254.19.221 for the wired path. Build with the NDK, push with adb, run
   from a shell.
2. Survey deskHPSDR for GTK coupling. Write findings to
   `docs/PORTING-NOTES.md` — which files are clean, which have GUI types in
   data structures, whether GLib-without-GTK is viable. **Timeboxed spike: do
   not start removing anything until this document exists.**
3. Locate the LOCAL/RADIO audio routing code and the EP2/EP6 audio slot
   handling. Document in `docs/HARDWARE.md`.

## Borrowed code

- **SDR++ / SDR++ Brown** (GPLv3) — Android bootstrap and the GL texture-scroll
  waterfall. Take the renderer, not the UI layout.
- **ft8_lib** (MIT, C) — FT8/FT4, later.
- **SDRoxide** (GPLv3) — only for keyboard modes with no clean upstream library
  (Olivia, THOR, FSQ). **Never take anything from `sdroxide-deepcw`** — it is
  AGPL-3.0-only and linked, which would put AGPL section 13 obligations on the
  whole binary.
- **Quisk** — reference for IO board I2C over the HPSDR protocol. Commands must
  be spaced so they don't disrupt normal protocol traffic.

## Guardrails

- Don't refactor beyond the task at hand.
- Don't add dependencies without asking. Every one has to cross-compile.
- Don't reformat upstream files (see git strategy).
- Don't write network interface configuration code — it needs root.
- Don't add Android audio (see decision 1).
- Preserve per-file GPL copyright headers when moving or copying code.
- Don't edit `vendor/` — patch via build scripts.

## Known unknowns

- Whether GLib-without-GTK actually builds clean for Android.
- Whether the EP2 audio path works without deskHPSDR-side changes for HL2+.
- Whether One UI on the S24 Ultra exposes static Ethernet config as expected.
- Thermal behaviour under sustained WDSP load. WDSP is double-heavy.
