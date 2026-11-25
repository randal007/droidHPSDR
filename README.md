# droidHPSDR

An Android SDR transceiver front end for OpenHPSDR Protocol 1 and 2 radios, with a focus on the Hermes Lite 2 / 2+.

Forked from [deskHPSDR](https://github.com/dl1bz/deskhpsdr) by DL1BZ, which was itself forked from [piHPSDR](https://github.com/dl1ycf/pihpsdr) by DL1YCF.

---

## Status

**Pre-alpha, receive only.** There is an app (no release APK yet): on a Galaxy S26 Ultra it runs a Hermes Lite 2 with the HL2+ and the IO board over USB Ethernet at up to 384 kHz, using deskHPSDR's own protocol, receiver, VFO and settings code with WDSP on the phone.

- A GPU 3D spectrum (four looks: X6100, AetherSDR-style curtains, lit terrain, neon wire) or a 2D spectrum, over a waterfall; a colour band-plan bar with a segment alert; peak labels in S-units.
- Tuning by touch: drag, fling, pinch to zoom, tap a signal, drag the filter edges, hold the frequency and slide, or tap it and type one.
- Audio on the HL2+ speaker, and optionally on the phone (or Bluetooth headphones) as well; it keeps playing with the screen off.
- VFO A/B, band stacks, memories, filters, RIT, CTUN, lock and AGC, saved in deskHPSDR's own props file.

Transmit is not built yet. Status and plan: [docs/ROADMAP.md](docs/ROADMAP.md).

---

## Why

deskHPSDR is a mature, WDSP-based transceiver front end aimed squarely at desktop macOS and Linux, with a 1280x600 minimum screen size and a mouse-oriented GTK3 UI. It is explicitly not targeting small screens or mobile.

SDR++ Brown already runs on Android and supports the HL2 with SSB transmit, but it uses its own DSP chain rather than WDSP, and does not implement PureSignal.

droidHPSDR aims for the intersection: the WDSP DSP core and radio feature set from deskHPSDR, running on Android, with a touch-first UI.

---

## Design decisions

### Audio runs through the radio, not through Android

With a Hermes Lite 2 Plus companion board, mic and speaker audio ride inside the Protocol 1 frames. Mic samples come up in the EP6 stream and demodulated audio goes back down in the EP2 stream, to and from the AK4951 codec on the companion board.

That means **the Android audio stack is not in the signal path at all** for the primary configuration. No Oboe, no AAudio latency negotiation, no audio policy surprises. This is the single most important architectural decision in the project.

Local Android audio via Oboe is a secondary path for plain HL2 users, planned but not prioritised.

Note the tradeoff: the HL2+ requires custom gateware rather than the standard HL2 gateware.

### WDSP is kept

PureSignal, the full noise reduction suite, the TX audio chain, and the filter set all live in WDSP. Porting the DSP to something else would mean losing them. WDSP and FFTW3 cross-compile for arm64 with the NDK.

### Rendering borrowed, UI not

The waterfall and panadapter use OpenGL ES rather than Cairo. Controls and menus are Kotlin / Jetpack Compose; the spectrum and 3D waterfall are drawn natively on the GPU. Both approaches, and much of the Android bootstrap, are informed by SDR++ / SDR++ Brown — but the UI layout is original and aims at a deskHPSDR / Thetis feel, not an SDR++ feel.

### Transport is deliberately abstracted

Both wired and wireless are first-class:

- **WiFi** — the HL2 takes a DHCP lease from the router and is found by the
  normal Protocol 1 discovery broadcast. Needs a WiFi lock, a foreground
  service, and a deeper jitter buffer to survive power-save behaviour.
- **USB Ethernet** — lowest jitter, preferred for TX. With no DHCP server on
  the link the HL2 falls back to APIPA at 169.254.19.221 / 255.255.0.0, so the
  wired path skips discovery and targets it directly.

Android does not self-assign an IPv4 link-local address, so eth0 needs a static
address set once by hand (169.254.1.1 / 255.255.0.0). Android persists this per
adapter. The HL2's EEPROM fixed-IP option is deliberately *not* used — it
disables DHCP and would break the WiFi path.

The network layer exposes the same interface either way. Buffer depth is a user
setting, defaulting deeper on WiFi.

### The radio keeps running when the app is not on screen

Protocol, DSP, and audio threads must be fully independent of the render loop. The GL surface can stop; the stream cannot. This is a constraint on M1 and M2, not a feature to add later — retrofitting it means rewriting the threading model a second time.

Requires a foreground service with a persistent notification, a WiFi lock, and a battery optimisation exemption, or Doze will drop the socket.

---

## Hardware targets

| Hardware | Intent |
|---|---|
| Hermes Lite 2 | Primary target — in hand |
| HL2 Plus companion board | Primary target — in hand. Mic, phones, speaker, CW sidetone |
| N2ADR filter board | Supported |
| HL2 IO Board | Required — in hand. I2C 0x1D over Protocol 1, as deskHPSDR does it |
| PureSignal | Planned — needs PS-A plus DUPLEX and the RF3 feedback stream |
| Protocol 2 radios (ANAN etc.) | Inherited from deskHPSDR, untested |

Build targets are arm64-v8a only, minSdk 31. Development phone is an unrooted
Galaxy S26 Ultra.

---

## Architecture

```
┌─────────────────────────────────────────┐
│  UI layer          Compose, touch-first │  new
├─────────────────────────────────────────┤
│  Render layer      OpenGL ES waterfall  │  new
├─────────────────────────────────────────┤
│  Android shell     NativeActivity, JNI  │  new
├─────────────────────────────────────────┤
│  Radio state       VFO, band, mode      │  from deskHPSDR
├─────────────────────────────────────────┤
│  DSP               WDSP + FFTW3         │  unchanged
├─────────────────────────────────────────┤
│  Protocol          P1 / P2, discovery   │  from deskHPSDR
├─────────────────────────────────────────┤
│  Transport         USB Ethernet / WiFi  │  reworked
└─────────────────────────────────────────┘
```

The hard part is not any single layer. It is that deskHPSDR does not separate them — GTK types appear inside receiver and transmitter structs, and cross-thread updates go through `g_idle_add` on a GMainLoop. Untangling that is Milestone 1.

GLib without GTK does cross-compile for Android. Keeping GLib and removing only the widget layer is worth attempting before rewriting the threading model.

---

## Roadmap

The plan and current status are in [docs/ROADMAP.md](docs/ROADMAP.md): every
deskHPSDR feature ([docs/FEATURES.md](docs/FEATURES.md)), one section at a
time. Milestone 1 (headless receive on the phone) is done.

---

## Wanted, but not solved

### Running alongside digital mode apps

The goal is to leave droidHPSDR running in the background while FT8CN, a Winlink client, or similar uses the radio. Android has no virtual audio cable, and the two directions are not equivalent:

- Capturing another app's playback is possible via AudioPlaybackCapture on Android 10+, behind a MediaProjection consent prompt, and only where the source app has not opted out.
- Feeding another app's microphone input has no public API. A virtual mic needs root or a system-level HAL.

So a software-only loopback covers at most one direction. The workable approach is to keep the digital audio path physical: HL2+ analog audio to a USB audio interface that the digital app owns directly, while droidHPSDR owns only the network connection. Nothing is shared and nothing is injected. PTT then comes from HL2+ VOX or from the rigctl TCP server inherited from piHPSDR, if the digital app supports network CAT — needs verifying per app.

Wired setups will need a powered hub carrying both the Ethernet adapter and the audio interface.

### Bluetooth headset audio

Depends on the local Oboe path, since Bluetooth necessarily goes through the Android audio stack rather than the HL2+ codec. Latency decides usability:

- A2DP: 150-250 ms. Acceptable for listening, not for TX monitoring.
- HFP/SCO: low latency but narrowband mono. Poor quality.
- LE Audio / LC3: the only genuinely good option, and needs recent hardware on both ends.

Likely lands as a listening-only feature first.

## Getting started

This is a fork of deskHPSDR's git history rather than a fresh repository, so
upstream fixes can be cherry-picked. After forking on GitHub:

```
git clone https://github.com/YOURNAME/droidhpsdr.git
cd droidhpsdr
git remote add upstream https://github.com/dl1bz/deskhpsdr.git
```

Keep a pristine tracking branch of upstream and bring changes across by
cherry-pick, never by merging. Do not reformat upstream files — a style pass
makes every future cherry-pick conflict.

## Building

Linux desktop with the Android NDK (r27), CMake and Ninja. Targets arm64-v8a
only, minSdk 31.

Also JDK 21 and the Android SDK (platform 37.2, build-tools 37.0.0, CMake 4.1.2).

```
android/deps/build-deps.sh             # once: FFTW 3.3.11, rnnoise, libspecbleach, WDSP 2.11
android/build-app.sh                   # the app: android/app/build/outputs/apk/debug/app-debug.apk
ADB_SERIAL=<ip:port> android/build-app.sh install
```

The headless receive test from milestone 1 still builds with CMake alone
(`android/native/run-m1.sh`). deskHPSDR's own files compile unchanged from
`src/`; how that works is in [docs/PORTING-NOTES.md](docs/PORTING-NOTES.md).

On first start the app asks for *Local network* access (Android 17): it
needs it to reach the radio. On a USB Ethernet adapter give the phone a
static 169.254.1.1/16 in Settings (see CLAUDE.md, "Network setup").

---

## Licence and attribution

GPL v3 or later, inherited from all upstream sources.

- **deskHPSDR** — DL1BZ. Radio state, protocol handling, HL2-specific work.
- **piHPSDR** — DL1YCF. Original codebase.
- **WDSP** — Dr. Warren Pratt, NR0V. All DSP.
- **SDR++ / SDR++ Brown** — Alexandre Rouma; Sanny Sanoff. Android and rendering approach.

Please do not send droidHPSDR bug reports to any of the upstream authors. This is an unofficial fork and their projects have different goals. Open an issue here.

---

## Non-goals

- Small screens below phone size, and Raspberry Pi builds — use piHPSDR.
- Measurement, IF-tap, or panadapter-only use — use something specialised.
- Getting merged upstream. deskHPSDR targets desktop macOS and Linux; this is a permanent fork.
