# droidHPSDR roadmap

**This is the status list.** One file, top to bottom: what's done, what's
next, what's waiting on whom. Session history lives in git and in
`docs/M1-PLAN.md` (milestone 1, finished).

## Now (in short)

- **M3 under way (branch `m3-app`, 2026-11-24).** The app runs on the S26
  Ultra against the HL2: layout 1 ("glass cockpit", Randal's pick from the
  round-1 pictures), GPU 3D view + waterfall, X6100 band bar and segment
  tag, touch tuning, HL2+ speaker audio. Details in the M3 section.
- **Waiting on the radio (Randal):** a test run of the 2026-11-24 evening
  builds - phone audio, 2D view, peak hold/labels, smooth motion, sharp
  zoom, hold-to-tune, display settings, screen-off audio.
- No transmit until Randal asks for it (M7).
- **Waiting on Randal:** whether to delete the empty leftover repo
  `randal007/DroidHPSDR-scaffold` (README only, from 2026-09-07).
- **Phone:** adb over **wireless debugging** (paired 2026-11-24; the port
  changes, find it with `adb mdns services`); Ethernet adapter static
  169.254.1.1/16. The app asks for *Local network* access (Android 17), which
  it needs to reach the HL2.

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

### M3 — The app and the waterfall *(in progress)*

The first thing that looks like droidHPSDR.

**Randal's picks and decisions (2026-11-23/24):**
- Layout 1 from round 1 (`~/Pictures/droidhpsdr-mockups/round1`), plus the
  X6100 band bar and segment alert (round 2, picture E for out of band).
- Keep **all four 3D looks as themes** (X6100 look 3, AetherSDR curtains,
  lit terrain, neon wire); more may come later.
- Filter edges limited per mode by deskHPSDR's widest presets; ESSB kept.
- **Phone audio as well as the HL2+** (CLAUDE.md decision 1 changed).
- 3D looks are **settings** (Display sheet, hold the 3D button); the button
  itself only switches 2D/3D.
- Peak labels: **S-units by default**; otherwise deskHPSDR's own settings
  and defaults. In general **every deskHPSDR setting is included** (the
  FEATURES.md rule), placed the phone's way.
- 2D was "a little fast": averaged over deskHPSDR's Av. Time (250 ms).

**Done and checked on the radio:**
- Android app (Gradle, Kotlin, Compose; native core as one JNI library),
  foreground service, Ethernet binding, targeted discovery to
  169.254.19.221, WDSP wisdom on first start.
- GPU 3D view and waterfall from upstream's own display timer
  (`rx_panadapter_update` hook), per-row frequency so tuning doesn't smear
  (WDSP display averaging off; the 3D view averages rows itself).
- Band bar + segment tag (X6100 `band_plan.c` tables), drag/fling/pinch/tap
  tuning, drag the filter edges, per-mode filter limits and memory.
- FT8 audio on the HL2+ speaker; IO board detected.

**Built, not yet run on the radio:** phone audio (upstream
`buffered_audio.c` + AAudio backend, PHONE chip), 2D view with peak hold,
six waterfall themes, peak labels, smooth motion between rows, sharp zoom
(upstream zoom 1-16), hold-and-drag the frequency digits, display settings
(split, fps, FFT window, detector, 3D depth), radio address fields (WiFi
needs the IP: an app cannot broadcast), battery-optimisation request,
demo band (`--ez demo true`) for display work with the radio off.

**Still to do:**
- Test all of the above on the radio, plus audio with the screen off and
  Bluetooth headphones; tune the row delay (menu slider, 90 ms now).
- FEATURES.md: tick the checked display items; propose "not doing" lines
  for desktop-only ones (CSS, window size) for Randal to agree.

**Done when:** on the radio, the waterfall looks the way Randal picked from
the mock-ups, tunes by touch, and audio keeps playing with the screen off.

### M4 — Receive operating

Everything needed to *use* the receiver day to day.

**Inventory:** `docs/M4-INVENTORY.md` (proposed "not doing" lines wait for
Randal). **Pictures:** `~/Pictures/droidhpsdr-mockups/m4-round1` to `round7`.

**Randal's picks so far (2026-11-24):**
- VFO: the A+B mix. The top pill shows VFO A big and **VFO B small under
  it** (no "hold to swap" text; holding B swaps). Tapping the pill opens the
  full VFO panel (A/B, A>B, B>A, A<>B, split, RIT, RIT 0, CTUN, lock, step;
  round 2 picture 3).
- Indicators (RIT, split, lock, ...): a column **inside the pill, on the
  right**, making it wider, never taller; **only the ones that are on**,
  and none at all (narrow pill) when nothing is on. Nothing over the 3D
  view. Pill glass more transparent (round 7; how much: open).
- Band sheet with the band's stack and memories M0-M9 (round 1, E).
- Keep F (drag the AGC line on the spectrum), G (analog meter option with
  peak/avg) and H (RIT by dragging the passband; TX-B marker for split).
- Open: glass transparency for pill, meter card and buttons (round 8:
  58 % now, 40 %, 28 %).

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
- Second receiver (RX2): **later** (Randal, 2026-11-24). One receiver for
  now; deskHPSDR can run RX2, so it may come back after the main work.
  `receiver[1]` already exists in code because `old_protocol.c` reads it.
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
4. PureSignal (PS menu). Randal wants it (2026-11-24). Upstream runs PS at
   any Protocol 1 rate (`tx_ps_set_sample_rate`, 8:1 at 384 kHz). To test:
   does the HL2's link carry two RX plus two feedback DDCs at 384 kHz? If
   not, PS drops to 192 kHz (the menu's sample-rate setting).
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
