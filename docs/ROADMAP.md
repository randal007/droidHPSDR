# droidHPSDR roadmap

**This is the status list.** One file, top to bottom: what's done, what's
next, what's waiting on whom. Session history lives in git and in
`docs/M1-PLAN.md` (milestone 1, finished).

## Now (in short)

- **M1 done 2026-11-23:** the S26 Ultra streams from the HL2 over USB
  Ethernet with deskHPSDR's own protocol and receiver code, WDSP on the
  phone, IO board detected and switching bands, FT8 audio heard on the HL2+
  speaker. Receive only.
- **Next: M3, the app and the waterfall**, started in a fresh session.
  First step: 10-20 generated UI pictures, tweaked with Randal round by
  round (see "How the look gets decided"), before any app code.
- No transmit until Randal asks for it (M7).

## Goal

Every feature of deskHPSDR, on an Android phone, nothing left behind.
deskHPSDR is the right amount of radio: not too simple, not too complex.
droidHPSDR keeps that feature set and rethinks only *where things live*,
because the screen is a phone, not a 1280×600 desktop window.

`docs/FEATURES.md` lists every labelled control in deskHPSDR's 34 menus
plus all 233 actions (846 items), generated from the source. Each section
below owns part of it, and a section is only done when its part is ticked
or explicitly marked "not doing" by Randal.

## How each section is done (same steps every time)

1. **Inventory:** read the section's part of FEATURES.md and the upstream
   code behind it. Write down what each control really does on an HL2
   (some deskHPSDR controls only matter for ANAN/Orion boards).
2. **Mock-ups:** several genuinely different layouts as images, on the
   real phone screen size, for Randal to pick from. No code before a pick.
3. **Build:** upstream logic stays unchanged in `src/` wherever possible;
   droidHPSDR adds UI and glue in `android/`.
4. **Test on the radio**, on the phone, with Randal: the feature does what
   deskHPSDR does. Compare against deskHPSDR on the desktop where useful.
5. **Tick** FEATURES.md, update this file, commit, push.

One section per session where possible. Don't start the next section with
the previous one half-ticked.

## Phone UI principles

### Randal's requirements (2026-11-23, his words condensed)

- **Really user friendly.** He doesn't like any existing Android SDR app's
  UI; droidHPSDR should not look or feel like them.
- **Tune with your finger on the spectrum/waterfall:** scroll/drag it to
  tune, swipe (fling) to move further, **pinch to zoom**, **tap a signal to
  go to it**, hold a finger on the frequency and drag it.
- **Drag the filter bandwidth** directly on the spectrum (its edges).
- **No on-screen knob, dial or scroll wheel** for tuning.

### How the look gets decided (M3 step 1)

1. Study for ideas: deskHPSDR, AetherSDR (`randal007/AetherSDR`, Qt6
   FlexRadio app), the X6100 3D spectrum, and the Android SDR apps mainly
   as examples of what to avoid.
2. **10-20 generated pictures** of genuinely different full-screen layouts
   at the S26 Ultra's real size, landscape and portrait.
3. Randal picks favourites; tweak those pictures round by round until the
   rough look is settled. **No app UI code before that.**

### Draft ideas to test in the pictures

- **The spectrum and waterfall are the screen.** Controls live around and
  over them, not instead of them.
- **Landscape is the operating view** (S26 Ultra, 6.9", 3120×1440);
  portrait gets a usable layout too.
- **Three layers, like deskHPSDR's own:** an always-visible VFO/meter bar;
  a strip of the most-used controls (deskHPSDR's sliders + toolbar, user
  configurable like its toolbar); and the menus as sheets that slide over
  the waterfall, grouped the way deskHPSDR groups them so a deskHPSDR user
  finds things.
- Long-press a control for its settings. Big readable numbers, no flicker
  (Randal's X6100 preferences).
- **The look:** the X6100 3D spectrum (`~/Work/x6100/wt-spectrum-3d`,
  mock-ups in `~/Pictures/x6100-spectrum-3d-mockups`, "look 3" coloured by
  strength) and deskHPSDR's 3D waterfall with our colour themes and peak
  labels (`randal007/deskhpsdr` branch display-features), drawn on the GPU
  and smoother than either.
- **The radio keeps running when the screen is off or the app is in the
  background** (foreground service; see CLAUDE.md decision 4).

## Sections

### M3 — The app and the waterfall *(next)*

The first thing that looks like droidHPSDR.

- Android project: Gradle, Kotlin, Jetpack Compose, the native core from
  `android/native` as a JNI library.
- Foreground service that owns the radio (stream keeps running with the
  screen off), Ethernet network binding via `Network.bindSocket` /
  `android_setprocnetwork`, discovery screen plus "type the IP".
- WDSP FFT wisdom on first start (11 s on this phone) with a progress note.
- Spectrum + waterfall on the GPU from WDSP's analyzer (`Spectrum0` /
  `GetPixels`): 2D first, then 3D with a 2D/3D switch; colour themes; peak
  hold and peak labels; smooth at the phone's refresh rate.
- Tuning by touch, Randal's way: drag/scroll the spectrum, fling, pinch
  zoom (deskHPSDR's zoompan), tap a signal, hold-and-drag the frequency,
  drag the filter edges for bandwidth. No knob, dial or wheel.
- FEATURES.md: the display-related parts of Display, FFT and Screen menus.

**Done when:** on the radio, the waterfall looks the way Randal picked from
the mock-ups, tunes by touch, and audio keeps playing with the screen off.

### M4 — Receive operating

Everything needed to *use* the receiver day to day.

- VFO A/B, split display, step, RIT, CTUN, lock, the VFO bar.
- Bands, band stacks, transverter bands later (M6), memories (Store).
- Modes and filters (presets, variable filters, shift/width).
- AGC (modes, hang threshold, auto AGC), AF and RF gain, LNA gain /
  attenuation for the HL2, S-meter (peak/average, Meter menu).
- Settings persistence: deskHPSDR's props file format, so a desktop
  profile can be copied over.
- FEATURES.md: VFO, Band, Bandstack, Mode, Filter, AGC, Meter, Store, RX.

### M5 — Receive DSP

- Noise: NB, NB2, NR1–NR4, NNR (WDSP 2.11 neural, voice and CW models),
  ANF, SNB; squelch; CW peak / APF; binaural.
- RX equalizer with its graph.
- Second receiver (RX2) and how two receivers fit on a phone screen.
- Remaining Display / FFT / Screen settings.
- FEATURES.md: Noise, Equalizer, Display, FFT, Screen, Diversity, DDC,
  Toolbar, Switch (several are not HL2 features: decide per item).

### M6 — Hardware: HL2, IO board, HL2+, PA

The two must-haves get their full UI here (the protocol side already works).

- IO board: status, antenna selection, tuner (TUNE_IOB, AH-4), the Pico LPF
  controller status. Band outputs already switch (seen in M1).
- HL2+ codec: everything deskHPSDR offers for it; mic/line settings wait
  for M7.
- N2ADR filter board / OC outputs, PA settings and calibration, the HL2
  parts of the Radio and ANT menus, transverters.
- FEATURES.md: Radio, ANT, OC, PA, XVTR.

### M7 — Transmit *(only when Randal asks)*

Strict order, each step tested on a dummy load first:

1. SSB voice with the HL2+ mic (the M2 mic half: EP6 mic samples), drive,
   mic gain, TX filter, PTT from the screen and the HL2+ mic PTT, TUNE.
2. TX audio chain: compressor, CFC, leveler, TX EQ, VOX, monitor.
3. CW: HL2 internal keyer from the KEY jack (arms only in CW mode, see
   docs/HARDWARE.md), sidetone, CW engine / keyboard CW, voice keyer.
4. PureSignal (PS menu).
5. AM, FM, digital modes; two-tone and the other test signals.

- FEATURES.md: TX, CW, VOX, PS, and the TX actions.

### M8 — Station integration

- CAT (rigctl server) so FT8CN and other apps on the phone, or a PC on the
  network, can control the radio; TCI.
- Running alongside digital-mode apps (see README "Wanted, but not solved").
- MIDI controllers over USB (Android MIDI), DX cluster, RBN spots, solar
  data and greyline, band-plan/regulations overlays, text-to-speech.
- FEATURES.md: Rigctl, MIDI, Extras, and the remaining actions.

### M9 — Finish

- Portrait and landscape polish, tablets and foldables.
- Battery and heat over a long session; WiFi path to a networked HL2.
- Bluetooth headset audio (listening first), local phone audio for plain
  HL2 users without the HL2+.
- Releases: signed APK on GitHub.

### Probably not on Android

Saturn/G2 internals, OZY USB radios, bootloader and FPGA programmers,
STEMlab, the simulators. Protocol 2 radios (ANAN etc.) come along with the
code but are untested without hardware. Each still gets a "not doing" line
in FEATURES.md so the decision is visible.

## Keeping the checklist

`tools/features/gen_features.py` regenerates the list from upstream. After
pulling a DL1BZ update: run it into a new file, diff against
`docs/FEATURES.md`, and carry new items across by hand (the real file holds
the ticks).
