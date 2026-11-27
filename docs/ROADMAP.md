# droidHPSDR roadmap

**This is the status list.** One file, top to bottom: what's done, what's
next, what's waiting on whom. Session history lives in git and in
`docs/M1-PLAN.md` (milestone 1, finished).

## Now (in short)

*Offload 2026-11-27.* Branch `m3-app` (pushed) holds M3-M7 so far; `master` is still at M1.

- **M7 (transmit) in progress.** Step 1 (SSB voice, PTT, drive, TUNE) is
  on the air and works (2026-11-25). **Built, reviewed, not yet checked on
  the radio:** step 2 TX audio chain (820b539 + 71d3881), step 4
  PureSignal (44e150f + 1b87ff0; done before step 3 at Randal's request),
  step 3 CW (7de3d13 + 0249ff5). Each has its "radio check" list in the M7
  section below. Step 1 leftovers still to check: HL2+ jack mic and mic
  PTT, timeout, split/XIT, PA Save/Load, inhibit input.
- **Next session:** install (the phone has only the step 2 build,
  71d3881; adb wasn't connected at offload: wireless debugging off), then
  Randal's radio checks (steps 2, 4, 3), then **step 3b voice keyer**
  (Randal said yes: inventory + pictures first), then step 5 (AM / FM /
  digital / test signals).
- **Feature count (2026-11-27,** page
  https://claude.ai/artifact/RjT8MD478BJFRKRedNaqcF, sorted by reading the
  code, not saved in FEATURES.md): 846 items, 33 ticked, 453 built and
  waiting for a radio check, 168 left to build (85 of them M8), 140 on the
  later list, 39 waiting for Randal's "not doing / keep", 13 headings.
- **Code review:** every commit up to `0249ff5` reviewed (`/code-review
  high`), findings checked by hand and fixed. **Next review starts after
  `0249ff5`.** Workflow (Randal): build → `/code-review` the new commits →
  fix → install (ask first if the app is in front or streaming).
- **Pictures:** `~/Pictures/droidhpsdr-mockups/m7-round1..4` (round 4 = CW
  C1-C6, picked C1 + C4).
- **Waiting on Randal:** the radio checks above; his own background
  pictures; gateware flashing from the phone (later); whether to delete the
  empty repo `randal007/DroidHPSDR-scaffold`; the 39 "decide" items.
- **Phone:** debug build of 71d3881 (step 2); the test IQ recording
  `demo_14225000Hz.wav` in `Android/data/io.github.randal007.droidhpsdr/files/`
  (no licence: test only). adb over wireless debugging (it switches itself
  off; the phone's WiFi address changes: `adb mdns services`).
- One receiver; RX2 later. Nothing is "not doing" without Randal: non-HL2
  items go on a **later** list (2026-11-24).

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

### M5 — Receive DSP *(done 2026-11-25, tag m5-done)*

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

**Picks and state (2026-11-24/25):** round 1 D1 Noise drawer, D5 notch
(double-tap; width handles added after Randal couldn't drag a narrow
notch's edge), D6 squelch line, D7 EQ graph, D8 settings pages, D9
editable bottom row. Round 2: E1 Levels tab, E3 averaging + peaks & hold,
E6 Screen page; info bar E8 (in the gaps beside the VFO pill; nothing moves
or gets smaller). E2 (dragging the dB scale) was built, then replaced by
the LEVELS chip's side panel (pick A, picture G1) as too hard to use. AGC
lines fade 3 s after a change. 2D background pictures (Thetis-style): 10
NASA photos, Display → Look. All built, none tried on the radio yet.

### M6 — Hardware: HL2, IO board, HL2+, PA *(done 2026-11-25, tag m6-done; gateware flashing later)*

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

### M7 — Transmit *(Randal asked 2026-11-25: next, after the radio-test fixes)*

**Started 2026-11-25:** step 1 done, `docs/M7-INVENTORY.md` (what each
control does on an HL2, the build plan, 6 safety findings, 9 questions for
Randal). **His answers (same day):** hold-to-transmit PTT/MOX chip; mic
source setting HL2+ / phone / Bluetooth; VOX off at every start; TX
timeout, default 2 min; CW paddle with all deskHPSDR settings (keyboard
CW and digital-mode "apps" later); voice keyer yes; duplex no (PureSignal
instead). Setup: HL2 → XPA125B → antenna, no dummy load (Randal watches the
amp; low drive first). HL2+ mic researched (inventory). **Pictures round 1** (step 1):
M1-M8 in `~/Pictures/droidhpsdr-mockups/m7-round1` (`tools/mockups/layouts11.js`):
PTT chip, three TX views (A meter card grows, B big meter mid-screen,
C red frame + tall bars), drive/mic/TUNE panel, TX page, PA page,
warnings. **Randal picked A** (meter card becomes the TX meter); asked
how tune % is changed → M5b adds a TUNE DRIVE slider to the panel.
One button labelled just TUNE (Randal: no XPA125B on it); with the Pico
it also starts the XPA125B's tuner, as in deskHPSDR. Tune IOB not shown.
One **TX** chip (shows the drive %) opens the transmit panel, instead of
separate MIC / DRIVE / TUNE chips (Randal).
**Randal happy with round 1 (2026-11-25):** A, M1, M5b, M6, M7, M8 as
drawn. **Step 1 built the same day** (29424fb; code review of `059d3d1..29424fb`: 10 findings,
all fixed in eb55688; then 2063a1d (watchdog cut TUNE/PTT after 3 s: fixed; TX log;
PA default), reviewed `8499dca..2063a1d`, fixes eadad0b; then dd68587 (the TCI stub zeroed every mic sample: fixed; reviewed `5f6101c..dd68587`, clean);
then the leftovers 56b3109, reviewed `d92521c..56b3109`, fixes 4597c58;
**next review starts after 4597c58**;
installed on the phone 2026-11-25). Randal's PA was off (props saved 0): he turns it on himself: upstream transmitter.c /
tx_off.c / vox.c compiled unchanged, radio.c's TX functions in
`android/native/app/transmit.c`, PTT chip (hold, repeats every 250 ms,
back to RX after 1.5 s without one), TX chip + panel, TX and PA pages, TX
meter card (A), orange TX spectrum over the dimmed waterfall, phone /
Bluetooth mic (AAudio input), TX timeout, warnings. Not built yet: watt
meter calibration, PA Save/Import, XIT, TX antenna, external TX inhibit
input. **Not run on the radio yet.**

**On the air 2026-11-25 (Randal, HL2 → XPA125B → antenna, 40 m LSB, drive 20 %):**
TUNE works and stays on (after the watchdog fix), 3.2 W at tune drive 18 %.
Phone mic: after raising MIC GAIN, ALC peaks -0.7..-5 dB, HL2 fwd 0.1-0.17 W
(averaged meter, SSB). Bluetooth mic: ALC 0.0 dB, WDSP mic +1..+1.6 dB (a
little hot: one mic gain for all sources, as deskHPSDR). **Force-stopping the
app or pulling the Ethernet adapter while transmitting: the HL2 unkeys** (Randal).
Not done yet: the HL2+ jack mic (no mic at hand: skipped for now), the
timeout, the HL2+ mic PTT.
**Step 4 (PureSignal) before step 3, Randal 2026-11-26:** inventory in
docs/M7-INVENTORY.md "Step 4" (84927b2): the HL2 has no sampler; Randal's
sampler after the XPA125B feeds the IO board's PS input, PS already works
for him in Thetis and deskHPSDR. Pictures S1-S6 in
`~/Pictures/droidhpsdr-mockups/m7-round3`. **Randal picked S1 (drawer) and
S4 (AmpView as deskHPSDR)**; no sample-rate switch (Thetis now runs PS at
384 kHz on an HL2; deskHPSDR uses the receivers' rate). Built 2026-11-26:
`android/native/app/puresignal.c` (ps_menu.c's calibration timer and
callbacks, radio.c's feedback receivers and PS restore at start, the TX ATT
0 warning), `PureSignal.kt` (drawer, AmpView, settings page, warning), PS
chip (added once before TX), ☰ → PureSignal. droidHPSDR's own: Two Tone /
Noise are refused like the screen PTT and end with any way back to receive
(the TX timeout included). Code review `71d3881..44e150f`: 6 findings
(TUNE during Two Tone stopped TUNE and left the Pico's tuner start on;
Two Tone during TUNE left it on too; a stop during Two Tone queued a MOX
off for the next start; a late refusal was silent and the timeout hold
was skipped; the SetPk slider snapped back; the AmpView line didn't
update), all fixed in 1b87ff0. Not run on the radio yet.
**Step 3 (CW), 2026-11-27:** inventory in docs/M7-INVENTORY.md "Step 3"
(e3d614b): the keyer is the HL2+'s own (paddle in the KEY jack), armed by
one bit while in CWU/CWL; the radio keys itself and the app follows its PTT.
Pictures C1-C6 (`tools/mockups/layouts14.js`). **Randal picked C1 (CW
drawer) and C4 (sending view)**; screen PTT in CW as deskHPSDR (holds
transmit, the paddle sends); defaults 30 ms PTT delay / 500 ms hang; **no
amplifier named on screen** (others use other amps); voice keyer next as
step 3b. Built: `android/native/app/cw.c` (cw_menu.c's settings, radio.c's
props, zero beat with a result) and droidHPSDR's safety: the radio's keyer
is armed only while transmitting is allowed (Transmit enabled, demo, TX
inhibit, band), and the TX timeout disarms it until "Arm the keyer again".
`Cw.kt` (drawer, settings page, the TX meter card's CW row), CW chip (only
in CW modes, added once), ☰ → CW. Code review `1b87ff0..7de3d13`: 9
findings, checked by hand; 8 fixed in the next commit (a screen PTT let go
while the paddle sends left the radio keyed with the app in receive and no
timeout; the keyer stayed armed through a stop and, at the next start,
before Transmit enabled was known; the PADDLE label never showed; a TUNE
timeout re-armed the keyer; a zero beat survived a stop; a refused zero
beat said "no tone"; the timeout note could miss the CW part; the CW chip
migration could land after TX), the 9th (`active_receiver` NULL) guarded
anyway. Also fixed: the screen PTT held in CW now survives the radio's PTT
dropping after the hang time. All in 0249ff5; **next review starts after
`0249ff5`**. Pictures: `~/Pictures/droidhpsdr-mockups/m7-round4`.
Not run on the radio yet.

**Step 3 radio check (low drive first, Randal watching the amplifier):**
1. CWU, Transmit enabled **off**: the paddle must not key (watch the amp's
   PTT and the watt meter); CW chip shows 🔒.
2. Transmit enabled on: the paddle keys, sidetone on the HL2+ speaker /
   phones, the app switches to the TX meter with the CW row, PADDLE on the
   PTT chip, back to receive after the hang time.
3. Speed, Iambic A / B / Straight, weight, keys reversed, letter spacing,
   sidetone level and pitch: does the HL2+ follow each one?
4. Hang time and PTT delay change what the amp does (no hot switching).
5. Out of band: the paddle does nothing. TX timeout set to 1 min, hold a
   dash: the keyer goes off, message; "Arm the keyer again" brings it back.
6. Zero beat on a CW signal: moves onto it, says how far.
7. Screen PTT in CW: transmit without carrier, the paddle sends in it.

**Step 4 radio check (low drive first, Randal watching the XPA125B):**
1. PureSignal on (drawer): receive keeps running after the short restart;
   info bar: no lost packets at 384 kHz.
2. Two Tone, Auto att. on: TX ATT settles, feedback 140-165, Correcting;
   Two Tone off: back to receive.
3. Voice with PS on: Correcting stays on; MON shows the feedback on the
   TX spectrum, cleaner shoulders than with PS off.
4. AmpView: the curves fill in during Two Tone.
5. Transmit enabled off: Two Tone refused with the reason; TX timeout
   ends Two Tone.
6. Restart the app: PS comes back on (and the TX ATT 0 warning if 0 dB).
**Step 1 leftovers (Randal 2026-11-25):** built: the external TX inhibit
input switch (TX page, as deskHPSDR's radio menu), XIT (VFO panel, with
split now enabled), PA calibration Save / Load saved (band.c's own file in
the app folder). Later (no effect on an HL2, for other Protocol 1 radios):
watt meter calibration, TX antenna. Not run on the radio yet.

**Step 2 (TX audio chain), started 2026-11-26:** pictures round 1, N1-N6 in
`~/Pictures/droidhpsdr-mockups/m7-round2` (`tools/mockups/layouts12.js`):
N1 A drawer (like the Noise drawer), N2 B chain strip with live stage
levels while transmitting, N3 C tabs on the TX page; N4 CFC curves, N5 TX
EQ graph, N6 VOX + noise gate + monitor. Settings are per mode
(mode_settings, copy_mode_settings), as deskHPSDR's tx_menu.c. **Randal
picked A (the drawer) and N4-N6 as drawn (2026-11-26).** Built the same
day (820b539): `TxAudio.kt` (drawer, CFC / TX EQ / VOX-gate-monitor pages,
chips PROC, VOX, MON added once before TX; ☰ TX audio, VOX),
`transmit.c` tx_audio_apply() (tx_menu.c / equalizer_menu.c / vox_menu.c's
lines). VOX: off at every start, threshold and hang in the props file,
refused out of band (with a note), the timeout switches it off; switched
off while keyed it unkeys at once. Also fixed: vfo.c's update_noise() /
update_eq() after a mode change were empty stubs (the new mode's NR/NB/EQ
waited for a control to be touched); dsp.c has the real ones. Code review
`4597c58..820b539`: 8 findings (graph drag crash on unloaded points, silent
out-of-band VOX refusal, VOX chip flicker, wrong mode label with split,
graphs ate page scrolls, PROC/MON chips stale after stop, NULL guard,
this paragraph), all fixed in the next commit; next review: see step 4. Not run on the radio yet.

**Step 2 radio check (low drive, Randal watching the XPA125B):**
1. Drawer: processor on, LEVEL 5-10 dB: ALC/meter on voice peaks, does it
   sound right on the monitor (MON, Post)?
2. TX EQ: drag a point, hear it on the monitor; switch LSB → USB → LSB:
   the same curve (SSB modes share it); DIGU has its own.
3. CFC on with Post EQ: hear it on the monitor.
4. VOX page: the mic bar moves while talking (HL2+ jack and phone mic);
   drag the VOX marker above the background, VOX on: talk = TX, silence =
   back to RX after the hang; Transmit enabled off = no TX; TX timeout ends
   it and switches VOX off.
5. Noise gate: on, trigger above the room noise: gaps go quiet.
6. Restart the app: VOX is off, the rest kept.

**First on-air check (low drive, Randal watching the XPA125B):**
1. TX page: Transmit enabled off → PTT shows the lock and refuses.
2. On, drive ~10 %, 20 m SSB: hold PTT, speak into the HL2+ mic: meter
   card shows power/ALC/MIC, orange spectrum; release → receive.
3. HL2+ mic PTT keys and unkeys.
4. TUNE at 10 %: carrier, and does the XPA125B's tuner start (Pico
   register 7)?
5. Timeout: set 1 min, hold PTT past it.
6. Force-stop the app while transmitting at low drive; pull the Ethernet
   adapter while transmitting: what does the HL2 do? (unknown, M7-INVENTORY
   safety 3)
7. Phone mic and Bluetooth mic.

Strict order, each step tested before the next (on the antenna through
the XPA125B, low drive first, Randal watching: no dummy load, 2026-11-25):

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
