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
- **Samsung Galaxy S26 Ultra, not rooted** (replaced the S24 Ultra the
  September notes were written for). Ample CPU and GPU headroom for WDSP and a
  fancy display. No root means no `ifconfig` and no `CAP_NET_ADMIN` — network
  interfaces can only be configured through One UI's settings, never
  programmatically. Wired link: a USB-C Ethernet adapter straight to the radio.
- **HL2 IO board (N2ADR), installed. A must-have**, not an extra (Randal,
  2026-11-23). deskHPSDR already drives it over I2C address 0x1D in
  `old_protocol.c` (detection, band/antenna, TUNE_IOB, AH-4); reuse that.
- Both boards must work together: HL2+ codec and IO board at once.

## Restart 2026-11-23 — what changed since September

Randal picked the project back up. Nothing under `src/` was touched in
September; only the docs and `tools/m0-probe` (built for arm64, verified from
the desktop, **never run on a phone**) exist. New requirements:

- S26 Ultra replaces S24 Ultra (above).
- IO board and HL2+ are both required (above).
- **Look:** the spectrum and 3D waterfall should look like the X6100 JS8
  project's 3D spectrum (OpenX6100 PR #344, `~/Work/x6100/wt-spectrum-3d/src/spectrum.c`,
  mock-ups `~/Pictures/x6100-spectrum-3d-mockups`, picked look = "3" coloured by
  strength), but GPU-drawn and fancier, since the phone can afford it. Also
  deskHPSDR's own 3D waterfall (`src/waterfall.c`, our colour-theme patches on
  `randal007/deskhpsdr` branch display-features).
- **UI stack decided (Randal, 2026-11-23): Kotlin + Jetpack Compose** for
  controls, menus and settings, plus a native GPU surface (GLES 3 or Vulkan,
  drawn from C++) for the spectrum and 3D waterfall. Replaces the September
  ImGui choice: it looks like a desktop tool. Port the X6100 3D spectrum's
  *look* (colours, perspective, drift), not its code (that one is CPU-drawn).
- Heiko Amft DL1BZ (deskHPSDR author, has a private iOS/macOS port) advised:
  build WDSP for the target first; everything else is network code + UI around
  it; Warren Pratt's WDSP API doc is the reference. He swapped FFTW for Apple
  vDSP; on Android FFTW cross-compiles, so no swap is needed.
- upstream deskHPSDR has moved on (2026-11-16, IO board I2C hardening, Pico
  detection, codec fixes). The fork's `src/` is still identical to upstream as
  of 2026-09-07, so rebasing onto current upstream before Box 2 is free.

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

No GTK, Compose, GL or JNI types anywhere under `core/`. This is the whole point
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
natural that looks from a GL render callback.

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

**Verified on the S26 Ultra, 2026-11-23:**

- Static setup: Settings > Connections > More connection settings > Ethernet >
  Configure Ethernet device > Static IP. **Save stays greyed out until the
  Ethernet switch on that page is turned off**; set the address, save, switch
  Ethernet back on. eth0 then shows 169.254.1.1/16.
- m0-probe broadcast over eth0 finds the HL2 (169.254.19.221, MAC
  00:1c:c0:a2:13:dd, gateware 75.0). `ping -I eth0` 169.254.19.221: ~1.5 ms.
- **Unicast fails unless the socket is bound to the Ethernet *network*.**
  Android routes per network: an unbound socket uses the default network
  (WiFi), so `m0-probe -w` (bound only to the source address) and a plain
  `ping` get nothing back, while broadcasts happen to get through. The app
  must bind its sockets to the Ethernet `Network` (`Network.bindSocket()` in
  Kotlin, or `android_setsocknetwork()` from the NDK with the handle passed
  down over JNI). Binding to the local address is not enough on Android.
- adb over WiFi while the only USB-C port holds the adapter: with the phone
  on USB once, `adb tcpip 5555`, then `adb connect <phone-wifi-ip>:5555`
  (approve the prompt again). Lasts until the phone reboots.

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
src/              upstream deskHPSDR, never edited (cherry-pick fixes in)
wdsp-2.11/        upstream WDSP, never edited
android/
  deps/           build-deps.sh (FFTW, rnnoise, libspecbleach, WDSP), wdsp-smoke.c
  native/         CMakeLists.txt; compat/ (stand-in glib/gtk/gdk/cairo headers);
                  headless/ (radio_state.c, stubs.c, m1_main.c); run-m1.sh
  (app/ arrives in M3: Gradle, Kotlin, Compose, JNI)
tools/m0-probe/   P1 discovery check
tools/features/   gen_features.py -> docs/FEATURES.md
docs/             ROADMAP.md (status), FEATURES.md, PORTING-NOTES.md,
                  HARDWARE.md, M1-PLAN.md (history)
build-android/    build output, ignored
```

The September plan to `git mv` upstream files into `core/` was dropped
(2026-11-23): upstream files compile unchanged from `src/`, and keeping
them there keeps DL1BZ cherry-picks conflict-free.

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

## Status, plan and features

- **`docs/ROADMAP.md` is the status list and the plan** (sections M3-M9,
  the steps every section follows, the phone UI principles). Read its
  "Now" first.
- **`docs/FEATURES.md`** is every deskHPSDR control and action (846 items),
  generated by `tools/features/gen_features.py`. Nothing gets dropped
  without a "not doing" line that Randal agreed to.
- `docs/M1-PLAN.md` is the finished milestone 1, kept as history.
- Randal works one section at a time and wants each nailed down before the
  next: mock-ups and his pick before UI code, a radio test before a tick.
- **No transmit until Randal asks.** The headless build is receive-only by
  construction (`android/native/headless/stubs.c`); keep it that way.

## Building and running (today)

```
android/deps/build-deps.sh            # FFTW, rnnoise, libspecbleach, WDSP -> build-android/arm64-v8a
cmake -S android/native -B build-android/cmake -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=$HOME/Android/Sdk/ndk/27.3.13750724/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=31 -DCMAKE_BUILD_TYPE=Release
cmake --build build-android/cmake
ADB_SERIAL=<phone-wifi-ip>:5555 android/native/run-m1.sh -f 7074000 -t 30
```

How the native build works and why: `docs/PORTING-NOTES.md`. Hardware
(HL2+ codec, IO board): `docs/HARDWARE.md`.

## Closing a session ("offload")

Randal's word for it is **offload** (or "closing down the session"). Do
every step, then report. Nothing may live only in this conversation.

1. **Running work:** background jobs and test runs started this session
   finished or stopped; nothing left streaming from the phone
   (`adb shell pidof m1-rx` empty). What can't finish now goes into
   ROADMAP.md as a "Next" with the exact command.
2. **docs/ROADMAP.md:** rewrite "Now (in short)"; update every section
   touched (state, Randal's decisions and picks, what waits on whom).
   "Decided: not doing" for anything he dropped.
3. **docs/FEATURES.md:** tick what was finished *and* checked on the
   radio; "not doing: why" lines for agreed drops.
4. **Other docs:** README.md (status, building), CLAUDE.md (anything a
   new session must know: traps, commands, decisions), PORTING-NOTES.md /
   HARDWARE.md when code or hardware findings changed.
5. **GitHub:** everything committed and pushed to randal007/droidHPSDR,
   `git status` clean. Say which branch is current.
6. **The phone and radio:** what's installed on the phone, how it was
   reached (adb over WiFi lasts until the phone restarts), anything
   changed in phone settings.
7. **Claude's memory** (on this PC): new lessons or preferences of
   Randal's; keep the droidhpsdr-project memory's index line current. No
   copy of the status (that's ROADMAP.md).
8. **Report to Randal** in plain words: what was done, what's waiting
   and on whom, and anything he has to do himself.

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
- Whether One UI on the S26 Ultra exposes static Ethernet config as expected.
- Thermal behaviour under sustained WDSP load. WDSP is double-heavy.
