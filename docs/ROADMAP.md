# droidHPSDR roadmap

**This is the status list.** One file, top to bottom: what's done, what's
next, what's waiting on whom. Session history lives in git and in
`docs/M1-PLAN.md` (milestone 1, finished).

## Now (in short)

- **2026-11-24 evening (demo mode, Randal away from the radio):** the ☰
  button opens deskHPSDR's main menu (same columns and order); M5 built from
  Randal's picks (D1 Noise drawer, D5 notch, D6 squelch line, D7 EQ graph,
  D8 settings pages, D9 editable bottom row); M4 leftovers built: the AGC
  lines on the spectrum (F) and the RX page. **Nothing of this is tested on
  the radio yet**; the notch/squelch/AGC fixes and the RX page are not even
  on the phone yet (adb dropped).
- **2026-11-25 (Randal away from home, phone not reachable):** code review
  of M4+M5 (`m3-done..HEAD`, high): 10 findings, all checked by hand and
  fixed in f5defd6 (CTUN snapped back, start/stop races and wake-lock leak
  on restarts, per-mode step not followed, auto gain stalled the loop on
  toggle, band-stack index unchecked, toolbar drag one place only, CW
  squelch scale, an attribution). **Not on the phone yet.** Extra things to
  try with the radio: CTUN on + drag (stays put, spectrum not shifted);
  unplug/replug the adapter, then Stop right after replugging; step per
  mode (USB 100 → CWU 25 → back).
- **Code review per milestone (Randal 2026-11-25):** tags `m1-done`
  (master tip) and `m3-done` (e580fa6) mark the ends; review = `/code-review
  <level> <tag>..<tag>`. M4+M5 done; M3 done 2026-11-25 (10 findings,
  all checked and fixed: CW filter edges and spectrum now shifted by the
  pitch like deskHPSDR, command queue order, Stop while connecting, phone
  audio close race, PHONE switch follows the radio, zoom after a
  sample-rate change, LNA set while connecting, locks released when a
  start fails, a clear message when the radio's address changes - that one
  needs a fresh app process, old_protocol.c keeps the address). Radio
  checks: CW tap-to-tune lands in the passband; drag a CW filter edge.
  **`13d5bda..ff3c548` (everything after the M4+M5 review: the review fixes,
  M5 display, E2, M6, the IQ demo, notch handles) done 2026-11-25** (high,
  10 findings, all checked and fixed): the demo no longer leaks into the
  radio (never saves props while playing, VFO A and `radio` put back,
  props path per real radio; a recording first uses the newest real
  props), Start radio / service stop / restart while the demo plays,
  sample-rate change during the demo, poll/span not saved from a
  recording, playback can't spin on an empty or unreadable WAV, HL2 setup
  targets the real HL2, full queue keeps the newest command, lock holds in
  the demo, WAVE_FORMAT_EXTENSIBLE read properly, iq_convert.py walks the
  chunks. **`ff3c548..45849bd` done 2026-11-25** (high, 9 findings, all
  checked and fixed): a recording now snapshots and restores deskHPSDR's
  VFOs, filter tables and per-mode settings (nothing from the demo reaches
  the radio or its props); a recording first with no radio props seeds the
  radio's VFOs from the app at its first start; PHONE during the demo is
  shown but not saved; stopping or starting the radio while a recording
  opens; mode/filter display put back; a recording that stops producing
  rows ends the demo with a message (the service watchdog leaves it
  alone); no Restart Protocol during the demo; a full command queue keeps
  the order; iq_convert.py reads only the data chunk. Next review: from
  this commit on. **`45849bd..59f982b` done 2026-11-25** (high, 7 findings,
  all fixed): a demo stopped before its native start began is stopped
  after it; 60 m table copied on the loop thread and read under a lock
  (the poll refreshes it); markers only with the radio running; RIT step
  only with the radio, guarded against the poll; ANT sheet follows the
  radio starting/stopping; iq_convert.py clamps a too-long data length.
  M1 done 2026-11-25 (`9f49c21..m1-done`, high; 10 findings, 8 fixed:
  m1-rx's S-meter now calibrated like receiver.c (read high by the LNA
  gain), stops with an error on an upstream FATAL, catches SIGHUP so a
  dropped adb still sends METIS STOP, rejects an unknown -m, new `-a ip`
  for targeted discovery; GLib shim runs a destroy notify exactly once;
  WDSP build prints the compiler error; m0-probe -t 0 and -u bogus. Not
  needed: thread structs (threads start once per process), vfo.c copy
  (compiled since M4). Radio check: `run-m1.sh -a 169.254.19.221 -t 20`
  shows the same dBm as the app. Every milestone so far reviewed. Tag
  `m5-done` when M5 is accepted.
- **M5 round 2 pictures sent** (E1-E6, `tools/mockups/layouts6.js`):
  Display tabs + manual levels (E1) or dragging the dB scale (E2),
  averaging + peaks & hold (E3), info bar as a top strip (E4) or in the
  meter card with an overload alarm (E5), the Screen page (E6).
  **Randal 2026-11-25: E1 (Levels tab with sliders), E3 and E6 OK as drawn;
  info bar: E4's strip, but the screen must not get smaller and the view
  must not be blocked.** Round 3 pictures for that (`layouts7.js`): E7
  see-through strip over the top edge (cards 12 dp lower), E8 items in the
  gaps between the floating cards, E9 see-through strip along the bottom
  edge. **Randal picked E8.** All four built 2026-11-25 (`Display.kt`):
  Display sheet with tabs (Look, Levels, Peaks, Averaging, Info bar,
  Speed); Levels = automatic or manual high/low/step/noise margin for
  spectrum (also sets the 3D view) and waterfall; Averaging = None /
  Recursive / Time window / Log recursive (default, deskHPSDR's) done in
  the 2D shader, detector, peaks & hold Off/Hold/Decay with hold time,
  drop rate and line colour; ☰ → Screen = FFT size (upstream
  pan_fft_size), FFT window, save zoom, preserve narrow peaks (2D, 3
  analyzer pixels like rx_panadapter.c), show/hide meter, VFO pill,
  bottom row, band plan bar, info bar, frequency axis; info bar in the
  gaps beside the VFO pill (UTC clock; ADC OK / red ADC OVERLOAD for 2 s;
  radio address), measured from where the cards are, empty when a gap is
  too narrow. Solar data waits for M8. **E2 added as an option** (Randal,
  same day; Levels tab switch "dB scale on the spectrum", on by default):
  scale down the right edge under the round buttons, drag ↕ moves the
  levels, pinch changes the range, double-tap goes back to automatic;
  amber while manual. Builds; the 2D shader validates
  (glslangValidator); **not run on the phone yet** (demo mode is enough).
- Branch `m3-app` (pushed) holds M3-M5; `master` is still at M1.
- No transmit until Randal asks for it (M7). One receiver; RX2 later.
- **Decided 2026-11-24:** no "not doing" lines: what isn't an HL2 item goes
  on a **later** list (unless it would be harder later). Worldmap later as
  pictures behind the 2D waterfall (Thetis). Themes and window size are
  covered by the 3D looks and split settings.
- **Next with the radio:** the M4 test list (M4 section), then M5: does
  NR2/NR4/NNR sound right on the HL2+ and the phone, notch, squelch, EQ,
  auto gain, AGC line levels against deskHPSDR's.
- **M4 leftovers built too (untested):** G, the analog meter with
  Peak/Avg and the Meter menu (hold the meter); the AGC menu's rest (AGC
  Automatic + offset, copied from rx_panadapter.c into display_bridge.c;
  gain and hang threshold sliders). The whole M4 menu column is now built.
- **Waiting on Randal:** whether to delete the empty leftover repo
  `randal007/DroidHPSDR-scaffold`.
- **Phone:** the app (debug build) installed; adb over **wireless
  debugging** (paired; it switches itself off: turn it on again, find the
  port with `adb mdns services`, the phone's WiFi address changes);
  Ethernet adapter static 169.254.1.1/16; Local network permission granted.

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

### M3 — The app and the waterfall *(done 2026-11-24)*

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

**Checked on the radio 2026-11-24 (Randal):** phone audio (sounds great),
audio keeps playing with the screen off, sharp zoom with real data and no
black, smooth motion, 2D view (averaged) and S-unit peak labels, hold the
frequency digits and drag, 192/384 kHz switching. FEATURES.md ticked.

**Built after the test, to check next time:** tap the frequency to type one
(deskHPSDR's VFO number pad; was not installed: adb dropped). Bluetooth
headphones and the WiFi path (type the radio's IP) not tried yet.

**Done when:** on the radio, the waterfall looks the way Randal picked from
the mock-ups, tunes by touch, and audio keeps playing with the screen off.

### M4 — Receive operating

Everything needed to *use* the receiver day to day.

**Inventory:** `docs/M4-INVENTORY.md` (proposed "not doing" lines wait for
Randal). **Pictures:** `~/Pictures/droidhpsdr-mockups/m4-round1` to `round7`.

**State 2026-11-24: built and running on the radio.** Upstream `vfo.c`,
`store.c`, `property.c`, `appearance.c` now compile unchanged, so tuning,
modes, filters, bands, band stacks, memories, A/B, RIT, CTUN, lock and AGC
modes all run deskHPSDR's own code, and settings live in deskHPSDR's props
file (`<MAC>.props` in the app's files). The VFO pill, VFO panel, band sheet
(stack + memories), filter presets, AGC chip and RIT-by-dragging are in.
Also: Ethernet unplug/replug recovers by itself, a watchdog restarts a
silent stream, tap the frequency to type one.

**Next session, in order:**
1. Radio test with Randal: replug the adapter; VFO panel (A>B, B>A, swap,
   RIT, RIT 0, CTUN, lock, step); RIT drag; band stack cycling (tap the
   band again); store/recall a memory; filter presets and Var1; AGC;
   keypad; close and reopen the app (props restored?). Tick FEATURES.md.
2. M4 leftovers: F, the AGC line on the 2D view (drag = AGC gain / hang
   threshold, as deskHPSDR draws it; `rx->agc_gain`, `agc_hang_threshold`,
   `rx_set_agc_gain`), AGC auto + offset; G, the analog meter option and
   peak/average (`smetermode`); the rest of the RX menu that matters on an
   HL2 (HL2 ADC auto gain, DIGU/DIGL/RTTY offsets, audio channel, test
   audio, RX audio network reserve for WiFi). Randal's answers on the
   "not doing" proposals in M4-INVENTORY.md.
3. Then M5.

**Randal's picks so far (2026-11-24):**
- VFO: the A+B mix. The top pill shows VFO A big and **VFO B small under
  it** (no "hold to swap" text; holding B swaps). Tapping the pill opens the
  full VFO panel (A/B, A>B, B>A, A<>B, split, RIT, RIT 0, CTUN, lock, step;
  round 2 picture 3).
- Indicators (RIT, split, lock, ...): a column **inside the pill, on the
  right**, making it wider, never taller; **only the ones that are on**,
  and none at all (narrow pill) when nothing is on. Nothing over the 3D
  view. Glass transparency **unchanged** (58 %, Randal: "it's perfect").
- Band sheet with the band's stack and memories M0-M9 (round 1, E).
- Keep F (drag the AGC line on the spectrum), G (analog meter option with
  peak/avg) and H (RIT by dragging the passband; TX-B marker for split).

- VFO A/B, split display, step, RIT, CTUN, lock, the VFO bar.
- Bands, band stacks, transverter bands later (M6), memories (Store).
- Modes and filters (presets, variable filters, shift/width).
- AGC (modes, hang threshold, auto AGC), AF and RF gain, LNA gain /
  attenuation for the HL2, S-meter (peak/average, Meter menu).
- Settings persistence: deskHPSDR's props file format, so a desktop
  profile can be copied over.
- FEATURES.md: VFO, Band, Bandstack, Mode, Filter, AGC, Meter, Store, RX.

### M5 — Receive DSP *(planned 2026-11-24)*

**What:** deskHPSDR's Noise menu (NB/NB2, NR/NR2/NR3/NR4, NNR with its
models, ANF, SNB, MNF notches, NR position pre/post AGC, all the NR2/NR4/NB
fine settings and Defaults), squelch, CW audio peak filter, binaural, the
RX equalizer with its graph, and the remaining Display/FFT/Screen items.

**How (same steps as every section):**
1. Inventory `docs/M5-INVENTORY.md` from FEATURES.md (Noise, Equalizer,
   Display, FFT, Screen, Toolbar, Switch) and upstream: what each does on an
   HL2. Diversity and RX2 items are already "later".
2. Pictures first. Ideas to draw: the DSP drawer from round 1 (picture L5)
   on layout 1; NR mode as one segmented row with the per-mode settings
   behind a hold; the EQ as a graph you drag points on; squelch as a line on
   the spectrum like the AGC line. Randal picks.
3. Build on upstream: the settings are fields of `RECEIVER` applied by
   `rx_set_noise()`, `rx_set_anf()`, `rx_set_squelch()`,
   `rx_set_equalizer()`, `rx_set_af_binaural()`, `rx_set_cw_peak()`
   (receiver.c, compiled). deskHPSDR keeps them **per mode**: noise_menu.c's
   `update_noise_for_rx()` copies them into `mode_settings[mode]` and calls
   `copy_mode_settings()`; noise_menu.c itself is GTK, so copy that small
   function (with attribution) instead of compiling the dialog. The props
   file already saves them (rx_save_state, modesettings).
4. NNR's models are built into WDSP 2.11 (`nnr_model_*.c`): check the
   "no wdsp_nnr_3.bin" log line (built-in model in use) and the CPU cost on
   the phone.
5. Radio test (does NR2/NR4/NNR sound right on the HL2+ and the phone?),
   tick FEATURES.md.

### M6 — Hardware: HL2, IO board, HL2+, PA

**Started 2026-11-25 (Randal):** step 1 done, `docs/M6-INVENTORY.md`
(what each control does on an HL2, proposals, 7 questions for Randal).
Found and fixed on the way: the HL2 filter board's per-band OC bits were
never set (deskHPSDR's `n2adr_oc_settings()`, now `headless/n2adr.c`);
radio check: relays click on band changes. Picks B (settings) and E2 fix
B built the same day (see M6-INVENTORY.md); gateware flashing is a
separate step. M5 still waits for its radio tests and ticks.

**E2 replaced (Randal, 2026-11-25: too hard to use):** the dB scale on
the spectrum is gone; a LEVELS chip (added once to saved bottom rows) opens
a side panel on the right (pick A, picture G1, `tools/mockups/layouts9.js`)
that doesn't dim the screen: spectrum and waterfall Automatic / HIGH / LOW,
same settings as Display → Levels. AGC lines fade to a quarter 3 s after
their last change.

**2D background pictures (Randal, 2026-11-25, Thetis-style):** Display →
Look: None or one of 10 NASA pictures (public domain; aurora x4, city
lights x2, Webb Cosmic Cliffs, SDO Sun, DSN Goldstone, Earthrise; files
and credits in `android/app/src/main/assets/backgrounds/`, credit line in
About), with a strength slider (10-80 %, default 40). Drawn by the 2D
shader under the grid, fill and trace, cropped to cover. Code-generated
ones were tried and dropped (Randal). Not run on the phone yet.

**Demo band from an IQ recording (Randal, 2026-11-25):** ☰ → Discovery →
Demo band plays the first .wav in the app's folder on the phone
(`Android/data/io.github.randal007.droidhpsdr/files`) through deskHPSDR's
real receiver (`android/native/app/playback.c`, fed through
rx_add_iq_samples, looping, tuned inside the recording like CTUN, sound on
the phone; HL2+ audio diverted with `-Wl,--wrap=old_protocol_audio_samples`).
No .wav there: the old made-up band. Test file: RFSPACE's 20 m demo
recording (2004, 14.225 MHz, busy SSB), converted to 192 kHz by
`tools/iq_convert.py`, put on the phone by `tools/push-demo-iq.sh`. **It
has no licence: test only, never in git or the APK; remove before a
release.** A "choose IQ file" button waits for a picture. Not run yet.

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
