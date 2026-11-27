# M7 inventory: transmit

Step 1 of M7 (ROADMAP "How each section is done"): every control in
FEATURES.md's transmit part (TX, CW, VOX, PS menus, 107 items), the
transmit lines left over from M6 (Radio, ANT, OC, PA menus) and the
transmit actions, with what each really does on an HL2. Read from upstream
(`src/`, = upstream 9f49c21) on 2026-11-25.

M7 goes in ROADMAP's five steps, each checked on the air (low drive first, Randal watching the XPA125B) before the
next: 1 SSB voice + TUNE, 2 TX audio chain, 3 CW, 4 PureSignal, 5 AM / FM /
digital / test signals. The tables below are sorted into those steps.

Marked **check**: things I could not settle from the code alone. They get
checked on the radio, not assumed.

## How the app is kept receive-only today

Five separate locks, all in `android/native/headless/`:

| Lock | Where | Effect |
|---|---|---|
| `can_transmit = 0` | radio_state.c | upstream's MOX / TUNE / VOX / drive functions return at once |
| `radio_is_transmitting()` always 0 | stubs.c | old_protocol.c never sets the MOX bit |
| `pa_enabled = 0` | radio_state.c | the HL2's PA-enable bit (0x12 C2 bit 3) never goes out |
| `tx_add_mic_sample()` throws samples away | stubs.c | no WDSP TX channel exists |
| `cw_keyer_internal = 0` | radio_state.c | the HL2's own keyer never arms, so the KEY jack can't key it |

M7 removes them one step at a time, never all at once. Until step 1 has
been checked on the radio, transmit stays behind a switch in
settings, off by default.

## Safety findings from the reading

1. **SWR protection and the power meter only run while the TX display
   runs.** Upstream computes forward/reverse power, SWR and the "drive to 0
   on high SWR" protection inside `tx_update_display()` (transmitter.c
   ~640-890), the TX display timer. In deskHPSDR that timer always runs. On
   the phone the screen can be off while transmitting (decision 4: digital
   modes in the background). **Proposed:** run that timer from the radio
   core's own thread, like the RX display timer is today, never from the GL
   surface. Check: with the screen off during TUNE into a mismatched load,
   protection still drops the drive.
2. **The demo band must never transmit.** The IQ recording plays through
   the real receiver (`playback.c`). **Proposed:** MOX, TUNE, VOX and the
   mic PTT are refused while the demo runs, and the demo refuses to start
   while transmitting.
3. **Answered 2026-11-25 (Randal, on the air): the HL2 unkeys when the app is
   force-stopped and when the Ethernet adapter is pulled.** Was: what the HL2 does when the app stops mid-transmission is unknown.
   It could happen if the app is killed, the Ethernet adapter is pulled or
   the phone reboots. old_protocol.c sends 20 ms PTT hang and 40 ms TX
   latency (12 ms in CW) to the HL2 (register 0x2E). I don't know from the
   code whether the gateware drops TX when the host goes silent.
   **Check at low drive, Randal watching the amp:** TUNE, then force-stop the app;
   TUNE, then pull the adapter. Watch the watt meter.
   **Proposed either way:** the app goes to receive when the radio service
   stops, when the network is lost and when the watchdog restarts the
   stream.
4. **Nothing transmits on open** (your rule from the X6100). MOX, TUNE and
   VOX always start off. VOX is a saved setting in deskHPSDR's props file,
   so **proposed:** VOX starts off at every app start, whatever was saved
   (question 4).
5. **Out of band:** `TransmitAllowed()` (band.c) refuses MOX, and
   old_protocol.c sends drive 0 outside the band's limits even while
   receiving, so a KEY-jack press out of band produces no power. Both are
   already compiled. "Transmit out of band" (PA menu) turns both off.
6. **The XPA125B.** Your IO board's Pico runs the XPA125B controller
   firmware, so the HL2 may be driving the amplifier. The HL2's PA-enable
   bit and drive level go out exactly as deskHPSDR sends them. Before the
   first key-down I need to know what sits between the HL2 and the load
   (question 1). **Answer: HL2 → XPA125B → antenna.** So the HL2's power
   meter and SWR (and upstream's SWR protection) measure the HL2's own
   output, i.e. the XPA125B's *input*, not the antenna. The amplifier's
   own meter and protection cover the antenna side.

## How the code fits together (the build plan)

| Upstream file | GUI calls | Plan |
|---|---:|---|
| `transmitter.c` (3099 lines) | 63 | **compile unchanged** with display stubs, as receiver.c is: the WDSP TX channel, mic path, drive scaling, power/SWR, monitor, PS calls, two-tone, CW ramps |
| `tx_off.c` | 0 | compile unchanged (graceful MOX-off: lets the speech tail go out before RX) |
| `vox.c` | 0 | compile unchanged |
| `iambic.c`, `cwramp.c`, `cw_engine.c` | 0 | compile unchanged (step 3) |
| `radio.c` | 56 + WebKit | **copy** the transmit functions into headless (with attribution), minus the GTK lines: `rxtx()`, `radio_set_mox/_immediate`, `radio_mox_update`, `radio_set_tune`, `radio_tune_update`, `radio_set_vox`, `radio_set_drive`, `radio_calc_drive_level` (has the HL2's attenuator + IQ-scale table), the TX inhibit sources, `radio_start_auto_tune` |
| `ps_menu.c` | GTK dialog | **copy** its timer logic (auto-attenuate, the HL2's −29..+31 dB TX ATT range, `hl2_adc0_ovf_events`), the way noise_menu.c's per-mode copy was done in M5 (step 4) |
| `voice_keyer.c` | 102 | copy the record/play logic, our own UI (step 3) |
| `actions.c` | 4 | copy only the pieces used: TUNE_IOB, Tune Full/Mem, two-tone, CW key |
| `tx_panadapter.c` | Cairo | **not compiled**: the GPU view draws the TX spectrum from WDSP's TX analyzer (pictures) |
| `meter.c` | Cairo | not compiled: our meter (M4) gets the TX scales: power, SWR, ALC, mic level |

The stubs in stubs.c that become real copies: `radio_is_transmitting`,
`radio_set_hardware_tx_inhibit`, `radio_tx_vfo_changed`, `ext_mox_update`,
`ext_set_duplex`, `tx_add_mic_sample`, `tx_add_ps_iq_samples`,
`tx_set_filter`, `tx_set_mode`, `tx_set_compressor`, `tx_set_ctcss`,
`tx_set_dexp`, `tx_set_analyzer`, `schedule_transmit_specific`,
`tx_off_cancel`, `tx_monitor_audio_active`. Most go away when the real
transmitter.c and tx_off.c are compiled. The CMake link will name any
others.

**Already in place, comes with old_protocol.c:** the HL2+ mic samples
(EP6, decimated to 48 kHz) to `tx_add_mic_sample()`; the mic jack's PTT
(`radio_ptt`, C0 bit 0) to `ext_mox_update`; dot/dash from the KEY jack;
the fwd/rev/exciter power readings; TX FIFO under/overrun flags; the
HL2's PA-enable and AH-4 tune bits; the IO board's TX frequency code and
`hl2_iob_set_antenna_tuner()`. While transmitting, the HL2+ codec gets
the monitor/sidetone audio through `old_protocol_iq_audio_samples()`.

## Step 1: SSB voice, drive, PTT, TUNE

| Control | What it does on an HL2 | Proposed |
|---|---|---|
| MOX (screen PTT) | `radio_mox_update()` → rxtx; graceful off lets the speech tail out | **build**: a PTT/MOX chip, hold to transmit (Randal's answer 2) |
| PTT from the HL2+ mic jack | radio_ptt from the HL2 → `ext_mox_update` | **build** (no UI, just works); "PTT Enabled" (`mic_ptt_enabled`) **check**: whether the HL2+ gateware uses that bit |
| TX Drive | 0-100 → `calcLevel()` with the band's PA calibration → HL2 attenuator step (0.5 dB) + IQ scale | **build** a drive chip / slider |
| Use drive levels per band | drive saved per band | **build** |
| Tune, Tune (toggle) | single tone carrier, compressor/CFC/leveler off while tuning, CW modes tune as USB/LSB | **build** |
| Tune Drive level, Tune drive ≙ TX drive, Tune Drive Stepping | separate (lower) drive for TUNE | **build** |
| Full Tune / Memory Tune (ms), "Tune (ORed with TX)" OC bits | timed tune; OC outputs while tuning | **build** the timed tunes; OC bits stay unedited (your M6 answer 4) |
| Tune IOB | TUNE + IO board register 7 = 1 (`hl2_iob_set_antenna_tuner`), for an N2ADR IO board **without** a Pico | **not shown**: with a Pico (Randal's XPA125B controller) upstream's plain TUNE / TUNE (toggle) already writes register 7 = 1 at the start and 0 at the end (actions.c). What the Pico firmware does with it (start the XPA125B's tune): **check** on the radio; its source isn't in the repo, only the uf2 |
| HL2 ATU TUNE support | gateware tune request bit for an AH-4 on the HL2 (on the HL2+ the AH-4 support is also switched by the Line-in bit) | **later**: no AH-4 here (Randal's answer 1) |
| Enable external TxInhibit / AutoTune input | IO board inputs that block TX / start a tune | **build** |
| HL2 5W PA enable (`pa_enabled`) | the HL2's own PA on; off = only the low-level exciter output | **build**, off until the first low-drive test |
| PA calibration per band | dB, minimum 38.8 = full 5 W on the HL2 | **build** (PA page) |
| MAX Power (1 W ... 1 kW) | the watt meter's full scale only | **build**: 5 W, or 100 W with the XPA125B |
| Watt meter calibrate | user-measured pairs, interpolated (`compute_power`) | **build** |
| Transmit out of band | turns off both out-of-band locks (above) | **build**, off |
| SWR protection, SWR alarm at | warning above the alarm; with protection, drive → 0 (not while tuning) | **build**, protection on by default |
| Mute RX when TX, Duplex | duplex keeps the RX display running while transmitting | **build** Mute RX; Duplex **later** (Randal: PureSignal instead) |
| TX filter low / high, TX uses RX filter | WDSP TX bandpass | **build** (drag the TX filter edges, like RX?) (pictures) |
| Mic gain | WDSP mic gain, dB | **build** |
| Mic In / Mic Boost / Line In, SDR LineIn (dB) | on the HL2+: Mic Boost = +20.25 dB digital; the Line-in bit = **HL2+ ATU enable** (see "The HL2+ microphone") | **build** Mic Boost; Line In **not shown** |
| Mic source: HL2+ jack / phone / Bluetooth (`local_microphone`) | upstream's local mic path | **build** (Randal's answer 3) |
| TX timeout (new, not in deskHPSDR) | ends any transmission after N minutes | **build**, default 2 min (Randal's answer 5) |
| XIT (on/off, +, −, clear), Split, Quick Split | TX frequency; split itself is built (M4) | **build** XIT in the VFO panel; TX-B marker on the spectrum (M4 pick H) |
| TX antenna (ANT menu) | `alexTxAntenna` per band; on an HL2 with IO board **check** what it switches | **build** if it does something |
| TX display: Panadapter High/Low/Step, Fill, peaks (6 items), Noise floor percentile | the TX spectrum while transmitting | **build** in our GPU view (pictures) |
| TX meter | fwd power, SWR, ALC, mic level | **build** (pictures; your X6100 pick: big and readable, ALC always shown) |
| TX FIFO underrun / overrun, "High SWR" | upstream's on-screen alarms | **build** (info bar) |

## Step 2: the TX audio chain

All WDSP, all set by `tx_set_compressor()`, `tx_set_dexp()`,
`tx_set_equalizer()` in transmitter.c. Nothing HL2-specific.

| Control | Proposed |
|---|---|
| Speech Proc (compressor) + level; "Cmpr Level" action | **build** |
| CFC: Pre-CFC / Post-CFC, Pre Comp, Post Gain, per-band Freq / Pre / Post (multiband compressor, not an EQ) | **build**, as a graph like the RX EQ (pictures) |
| Leveler, Phase Rotator, Auto CESSB | **build** |
| DEXP (downward expander / noise gate): trigger level, attack tau, attack/release/hold, expansion, hysteresis, side channel filter + cuts | **build** (settings page) |
| TX EQ + WDSP TX EQ Ctfmode | **build**, the M5 EQ graph reused |
| VOX: enable, threshold, hang, mic level, side channel filter + cuts | **build**; VOX starts off (safety 4) |
| TX monitor: on/off, volume, pre/post | **build** (HL2+ phones / phone audio) |
| Local Mic PreAmp Gain (`addgain`) | **build** |
| Audio profiles: Activate, Save, Import file, Export file | presets of the whole WDSP RX + TX chain. **build** (import/export through Android's file picker) |

## Step 3: CW (researched 2026-11-27)

Randal's answer 7: paddle in the KEY jack, every deskHPSDR CW setting;
keyboard CW, decoder and on-screen keys later (as add-on "apps"). Read
from upstream `cw_menu.c`, `old_protocol.c`, `transmitter.c`,
`receiver.c` (zero beat), `radio.c` (props), the HL2 wiki (Protocol, FAQ)
and hermeslite2plus.com on 2026-11-27.

### How CW works on the HL2+

- **The keyer is in the radio.** The stock HL2 has no keyer: the KEY
  jack's tip is a straight key and the gateware makes the shaped carrier
  itself (HL2 wiki, Protocol: "a shaped CW signal is generated by the
  Hermes-Lite2 ... The MOX bit must be sent as zero when sending CW").
  The HL2+ gateware adds an iambic keyer on the same jack
  (hermeslite2plus.com: "Adds an IAMBIC keyer to the "KEY" jack, a
  straight or external keyer usage is preserved, and there is even an
  electronic bug mode"), with sidetone on PHONES and the speaker "in CW
  mode". The Zeus HL2+ work had it keying with the radio's own sidetone.
- **deskHPSDR's "CW handled in Radio"** (`cw_keyer_internal`) sends one
  bit, 0x1E C1 bit 0 (HL2 register 0x0f bit 24), and only while the TX
  mode is CWU/CWL and not TUNE / Two Tone / Noise. The Zeus finding fits:
  the keyer works only in a CW mode.
- **The settings go out every round** of old_protocol.c's command loop:
  - 0x16: speed (C3 bits 0-5), paddle mode (C3 bits 6-7: straight / A /
    B), weight (C4 bits 0-6), letter spacing (C4 bit 7), keys reversed
    (C2 bit 6);
  - 0x1E: sidetone level 0-127 (C2), PTT delay (C3, capped at 900 /
    speed ms, upstream's "quirk working around a bug in the FPGA iambic
    keyer");
  - 0x20: hang time 0-1000 ms, sidetone frequency.
  The HL2 wiki documents only the hang time and the CWX bit; the rest
  is the HL2+ gateware's, undocumented. **check** which ones it follows.
- **The app learns of a key-down after the fact.** The HL2 sends PTT
  (C0 bit 0, held through the hang time) and dot (C0 bit 2).
  old_protocol.c hands PTT to `ext_mox_update`, which switches the app
  to transmit (receiver muted, TX meter). The RF has already started
  by then; deskHPSDR's comment says the first dot's antenna switching
  happens about 4 ms late.
- **Nothing is computed on the phone for the RF.** In CW, transmitter.c
  sends I = envelope × gain, and the envelope stays 0 unless the
  host keyer, CAT CW or MIDI keys it. None of those is built: iambic.c is
  not compiled, and `keyer_event` is a no-op stub. So the only key is
  the paddle.
- **Receive side, already built in M5:** the CW pitch (= sidetone
  frequency) shifts the receive filter and the spectrum. The CW audio
  peak filter is done too. Changing the pitch must call
  `rx_filter_changed()` (cw_menu.c).
- **Zero beat** (`rx_cw_zero_beat_start`, receiver.c, compiled
  unchanged): listens for a short time, finds the strongest tone near the
  pitch, retunes by up to a set amount, and logs only when it fails. The
  "Zero Beat Freq. Corr." (−50..+50 Hz, per receiver, default 15) offsets
  its target.
- **Not used on an HL2:** "CW Break-In" (`cw_breakin`) only steers the
  host keyer (iambic.c) and P2. On P1 the radio's hang time is the
  break-in delay. Ramp width is P2 / host keyer only, and isn't even
  saved upstream. "PTT delay" isn't in deskHPSDR's CW menu at all (a
  props value, default 30 ms), but it is the one that protects the
  XPA125B's relay.

### Safety findings

1. **A paddle touch keys the HL2 and the XPA125B before the app can say
   no.** Today every "no" (Transmit enabled off, the demo band, TX
   inhibit, out of band, the timeout) is a refusal of MOX in the app,
   and the radio's keyer doesn't ask the app. **Proposed:** droidHPSDR
   sends the CW bit only while transmit is allowed: Transmit enabled on,
   no demo, no inhibit, in band, and not timed out. Otherwise the bit
   stays 0, as it is today. With iambic.c not compiled, the paddle then
   does nothing. **check** on the radio: in CW with Transmit enabled
   off, the paddle doesn't key (watch the XPA125B). The Zeus work says
   it doesn't key in USB.
2. **The TX timeout can't stop the radio's keyer by dropping MOX.**
   **Proposed:** in CW the timeout clears the CW bit. It stays clear
   until the paddle has been let go (PTT from the radio back to 0) and
   pressed again, the same rule as a held screen PTT. **check**.
3. **PTT delay for the XPA125B.** The HL2's PTT out keys the amplifier,
   and the RF follows PTT delay ms later (default 30). Too short and the
   amp's relay switches hot. **Question 3.**
4. **TUNE in CW** already tunes as USB/LSB with the radio keyer off
   (transmit.c, from radio.c), and PureSignal's Two Tone / Noise clear the
   bit (old_protocol.c). No change.
5. **The screen PTT in CW.** In deskHPSDR, MOX in CW switches the app to
   transmit (receiver muted), but sends no MOX bit and no carrier. The
   paddle then sends without waiting for the hang time. **Question 2.**

| Control | Proposed |
|---|---|
| CW handled in Radio | **build**, on and **not shown** (with no host keyer, off would mean the paddle does nothing); the app gates it (safety 1) |
| CW speed 1-60 WPM | **build**; also on a chip (pictures) |
| Paddle mode: Straight Key / Iambic A / Iambic B | **build** (HL2+: "Straight" = its electronic bug mode, **check**) |
| Weight 0-100, Enforce letter spacing, Keys reversed | **build** |
| CW Break-In, Delay (ms) | the delay **build** as "Hang time" 0-1000 ms (the radio's break-in); the checkbox **not shown** (does nothing on an HL2) |
| PTT delay 0-255 ms (props only upstream) | **build** (safety 3) |
| Sidetone level 0-127, sidetone frequency (= CW pitch) 100-1000 Hz | **build**; **check** that the HL2+ follows both |
| CW Zero Beat Freq. Corr., Zero Beat action | **build**, with a result line (moved by N Hz / no signal) |
| CW ramp width | **not shown** (P2 / host keyer only) |
| Actions: CW Left/Right, Straight key, keyboard CW, CAT/MIDI CW | **later** (answer 7) |
| Voice keyer | **next, as step 3b**: deskHPSDR plays WAV slots (6 × 30 s, PCM16 48 kHz mono); recording from the mic is upstream's audio capture (step 5's list). Own pictures. |

### Pictures (`tools/mockups/layouts14.js`, C1-C6)

- C1-C3, three places for it:
  - C1 a CW drawer like PureSignal's;
  - C2 a CW chip with a speed / pitch pop-up, and the rest on a page;
  - C3 a CW strip over the spectrum while in a CW mode.
- C4 keying: what the screen shows while the paddle sends.
- C5 the locked paddle: CW with Transmit enabled off, and after the
  timeout.
- C6 the settings page (all of the table above).

### Questions for Randal

1. Which of C1-C3, and which keying view (C4)?
2. **Screen PTT in CW:** as deskHPSDR (holds transmit, paddle sends
   without break-in) or refused in CW with "the paddle keys"?
3. **PTT delay and hang time** with the XPA125B: what do you use in
   Thetis or deskHPSDR? (default 30 ms delay, 500 ms hang)
4. **Voice keyer** next (step 3b), before step 5?

**Randal's answers (2026-11-27):** C1 and C4. Screen PTT in CW: yes, as
deskHPSDR. Timing: the defaults (30 ms / 500 ms), and don't name the
XPA125B on screen: other people use other amplifiers. Voice keyer: yes,
next (step 3b).

**Built differently from the proposal:** the timeout's CW lock ends with a
tap ("Arm the keyer again", CW drawer) rather than when the paddle is let
go: once the keyer is disarmed the app can't tell whether the paddle is
still held (old_protocol.c keeps dot/dash to itself), and a stuck paddle
is what the timeout is for. The C4 hang bar isn't built for the same
reason; the CW row shows the radio's PTT instead.

## Step 4: PureSignal (researched 2026-11-26)

Randal asked for PureSignal before CW (2026-11-26), so step 4 comes before
step 3. Read from upstream `ps_menu.c`, `transmitter.c` (tx_ps_*,
tx_set_twotone / tx_set_noise) and `old_protocol.c` (= upstream 9f49c21;
`git fetch upstream` 2026-11-26: nothing newer), plus Hermes-Lite forum
threads (sources at the end; forum posts are user reports, not specs).

### How it works on an HL2 (code reading)

- **Enable PS restarts the stream with 4 receivers.** `tx_ps_onoff()`
  stops Protocol 1, sets `transmitter->puresignal`, runs it again:
  `how_many_receivers()` returns 4 for an HL2 while PS is on (RX4 is a copy
  of the TX DAC, hard-wired in the gateware; the feedback comes in on the
  PS RX channel). Then it starts the WDSP PS engine (`SetPSControl`).
  So switching PS on or off gives a short gap (~0.1 s) in receive too.
- **While transmitting with PS**, the HL2's RX gain is set from the TX ATT
  (`rxgain = 31 - attenuation`, range **-29..+31 dB**), and every sample
  pair (DAC copy + feedback) goes to `tx_add_ps_iq_samples()` → WDSP `pscc`.
- **The feedback signal has to reach the HL2's receiver from outside.**
  The HL2 has no built-in sampler (forum: K9AQ 2019 "not able to get
  enough leakage feedback"; he added a coupler). With the XPA125B the
  sampler must sit **after the XPA125B** (otherwise PS sees only the HL2's
  own 5 W PA, which the XPA125B then distorts again). The N2ADR IO board's
  **J10 (PS input)** feeds the HL2's RF3 (N2ADR, forum 2023). K9AQ ran
  about -17 dBm from his coupler; another post says at most -5 dBm.
- **IO board RF mode** (N2ADR, forum 2023): 0 = normal, J10 still injects
  into RF3; 1 = J9 is the RX input, no PS input; 2 = J9 on receive, J10
  switched to RF3 through an attenuator on transmit. Upstream sends 2 only
  when the band's RX antenna is "RX in" (EXT1) *and* PS is on; with ANT1
  it sends 0. **check:** whether the XPA125B firmware on Randal's Pico
  (DL1BZ's, only the uf2 in the repo) still does the J9/J10 switching.
- **Calibration source:** Two Tone (WDSP post-generator, 700 + 1900 Hz,
  0.5 each; LSB: negative) or Noise (pre-generator, -12..+3 dB, through the
  TX filter). Either **keys MOX until switched off** (`ext_mox_update`) and
  starts `ps_calibration_timer` (100 ms): reset → resume, and with **Auto
  Attenuate** it moves TX ATT until the feedback level is 150..155 (one step
  of `20·log10(level/152.3)` dB, ±15 dB if far off, +6 dB on each HL2 ADC
  overflow). Full average power through the XPA125B for as long as it runs.
- **Continuous vs OneShot:** normally PS keeps calibrating on every
  transmission (automode); OneShot calibrates once and holds it.
- **TX ATT is kept per band** (`band.c` ps_tx_att, applied by vfo.c on band
  changes; both compiled already).
- **SetPk** (the HL2's peak): default 0.2400 for an HL2 ("measured 0.2386").
  GetPk shows what PS measures.
- **Status** (`GetPSInfo`): feedback level (colour: red <90, yellow, green
  128..181, blue >181; "optimal 140..165"), correction count, Correcting
  yes/no, state (RESET, WAIT, MOXDELAY, SETUP, COLLECT, MOXCHECK, CALC,
  DELAY, STAYON, TURNON).
- **MON** (`transmitter->feedback`): the TX spectrum shows the feedback
  signal instead of what is sent (display only). With PS working, that is
  where the cleaner shoulders show.
- **AmpView:** the amplifier's AM/AM and AM/PM curves and PS's correction
  curves (`tx_ps_getdisp`, 4096 points, 10 times a second).
- **Feedback antenna Internal / Ext1 / ByPass:** sets ALEX relay bits that an
  HL2 has no relays for; the IO board's RF mode reads the *receiver's* RX
  antenna, not this one (code reading, not checked on the radio).
  Proposed **not shown**.
- **Sample rate.** PS sends 4 streams: 26 bytes per sample time, about
  **85 Mbit/s at 384 kHz** with framing, 43 at 192 kHz. The HL2 is gigabit
  (KF7O). The app runs at 384 kHz. The phone's adapter speed is hidden
  (`/sys/class/net/eth0/speed` not readable). Thetis users report
  scrambled TX with PS at 384 kHz; "192 kHz has been recommended for PS
  since PowerSDR"; DL1YCF: 384 kHz can't work at 100 Mbit/s. **check** at
  384 kHz (lost packets in the info bar), else 192 kHz while PS is on.

### Controls

| Control | Proposed |
|---|---|
| Enable PS (restarts the stream), OFF (reset), Restart (resume) | **build** |
| Two Tone, Noise + level, as PS calibration sources | **build**; they key the radio: Transmit enabled, out-of-band lock and the TX timeout apply; switching either off, or any "back to RX", stops the generator |
| Auto Attenuate, TX ATT (-29..+31 dB, per band) | **build** (ps_calibration_timer copied, as noise_menu.c was) |
| OneShot, Stability Strict / Medium / Relaxed, TX delay (ns), SetPk | **build** (a settings part, not the main view) |
| Feedback level, Correcting, GetPk, cor.cnt, sln.chk, dg.cnt, state | **build** (pictures) |
| MON (feedback on the TX spectrum) | **build** |
| AmpView | **build** as a GPU or Compose graph (pictures) |
| TX ATT 0 warning, "Don't show again" | **build** |
| Feedback antenna Internal / Ext1 / ByPass | **not shown** (no ALEX relays on an HL2) |

### Questions for Randal (step 4)

1. **Sampler:** is there a coupler / RF sampler on the XPA125B's output,
   and is it wired to the IO board's J10 (PS input)? Without one PS has
   no feedback: Feedback stays red and nothing is corrected.
2. **Level:** if there is one, roughly what does it put out at 100 W (or
   its coupling in dB)? With the HL2's -29..+31 dB TX ATT, about -17 dBm
   worked for K9AQ; >-5 dBm risks the ADC.
3. **RX antenna:** ANT1 on every band (IO board mode 0), or the IO board's
   RX input (mode 2, J10 switched in only on TX)?
4. **Calibration:** Two Tone keys the XPA125B at full average power until
   switched off. Proposed: a separate, short limit for Two Tone / Noise
   (e.g. 10 s) inside the TX timeout.

**Randal's answers (2026-11-26):** PureSignal already works for him in
Thetis and deskHPSDR on this station: a sampler on the XPA125B's output
feeds the IO board's PS input. 1 yes; 2 the sampler is adjustable; 3 the
regular antenna port (ANT1, IO board mode 0); 4 calibration as deskHPSDR
does it (no extra limit; the app's TX timeout still applies, as to every
transmission). **Pictures round 1:** S1-S6 in
`~/Pictures/droidhpsdr-mockups/m7-round3` (`tools/mockups/layouts13.js`):
S1 A drawer, S2 B status in the TX meter card, S3 C calibration page,
S4 AmpView as deskHPSDR (one graph), S5 AmpView split, S6 settings + the
TX ATT 0 warning.

Sources: [IO board and Pure Signal](https://groups.google.com/g/hermes-lite/c/AwyN2R8OuwY),
[Pure Signal with HL2](https://groups.google.com/g/hermes-lite/c/03cl7KLlbqM),
[Internal pure signal & sample rate](https://groups.google.com/g/hermes-lite/c/ehyX9zKA6Fc).

## Step 5: AM, FM, digital, test signals

| Control | Proposed |
|---|---|
| AM carrier level | **build** |
| FM PreEmp/ALC, CTCSS enable + frequency | **build** |
| Max Digi Drv (drive limit in DIGU/DIGL) | **build** |
| Two-Tone (action), noise test signal | **build** |
| Audio capture / replay / transmit the capture, Audio Capture Time | **build** (radio menu item from M6) |
| TX audio from phone apps (FT8CN etc.), TCI TX audio, RTTY engine | **M8** (station integration) |

## Not shown on an HL2

From the TX and Radio menus, for other radios: ORION/SATURN mic jack, "PTT
On Ring, Mic and Bias on Tip", mic bias, Mute Spkr Amp, ANAN new PA board,
the Apollo tune bit, Atlas/Penelope scaling, TX config. As in M6, each gets
a "not doing: other radios" line once you agree.

## Before building: upstream

The fork's `src/` is upstream 9f49c21 (2026-11-16). **Proposed:** before
step 1, fetch dl1bz/deskhpsdr and cherry-pick any transmit or HL2 fixes
newer than that (git strategy: cherry-pick, never merge).

## Randal's answers (2026-11-25)

1. **Gear:** HL2 → XPA125B → antenna, no separate tuner. **No dummy
   load** (Randal: the XPA125B's tuner has the antenna memorized; he
   watches the amp during every test). First key-downs at low drive.
2. **PTT:** one chip labelled PTT/MOX, **hold to transmit**. No toggle.
3. **Microphone: a setting, HL2+ mic or phone mic (built-in or
   Bluetooth).** Changes CLAUDE.md decision 1 (the mic was HL2+ only).
   Upstream already has it: `transmitter->local_microphone` and
   buffered_audio.c's local mic ring (`audio_process_local_mic_input`,
   `audio_get_next_mic_sample`, read by old_protocol.c), so the work is an
   AAudio *input* stream in `audio_aaudio.c` (`audio_open_input`), the
   RECORD_AUDIO permission, and for Bluetooth Android's communication
   device routing. **check:** a Bluetooth headset mic usually runs at
   16 kHz (narrow, phone-call quality); upstream resamples to 48 kHz.
4. **VOX always off at app start.**
5. **TX timeout: adjustable, default 2 minutes.** deskHPSDR has none; ours
   drops MOX/VOX/TUNE after the time, with a message. (Proposed: also
   applies to the voice keyer and two-tone; a CW keyer key-down from the
   KEY jack is in the FPGA, so the timeout can only end it by leaving CW
   TX via MOX off. **check**.)
6. **His mic:** doesn't remember how the HL2+ handles it; research below.
7. **CW: paddle only for now, every deskHPSDR CW setting.** Keyboard CW,
   CW decoder/sender, JS8Call, PSK, FT8 etc. come later as add-on "apps"
   inside droidHPSDR (later list, after M7).
8. **Voice keyer: yes.**
9. **Duplex: no.** PureSignal is what he wants, and PS takes the feedback
   tap. Duplex goes on the later list.

## The HL2+ microphone (researched 2026-11-25)

Randal's HL2+ is a V4.x companion board (gateware 75.0, the "HL2+ V4.x
75p0" build). Sources: hermeslite2plus.com (pages below), JI1UDD's AK4951
Controller (`~/Work/ak4951_Controller`, its manual and block diagram), and
upstream old_protocol.c.

- **Jack wiring** (3.5 mm TRS, [connecting-microphone](https://www.hermeslite2plus.com/p/connecting-microphone.html)):
  mic on the **tip**, PTT between **ring and sleeve** (grounded = TX),
  shared ground. A mono (TS) plug shorts ring to sleeve: **continuous
  transmit**. Note: the Zeus HL2+ notes say a paddle's tip in the MIC
  jack "trips PTT"; the site says ring. **check** on the radio.
- **Mic PTT reaches the app** as the HPSDR PTT bit (C0 bit 0 in every
  frame, old_protocol.c `radio_ptt`); deskHPSDR then switches to TX. The
  site doesn't document it, but it worked in the Zeus HL2+ work.
- **Mic gain:** the AK4951's analog mic amp is fixed at **18 dB** by the
  gateware; deskHPSDR's **Mic Boost** bit adds **+20.25 dB in the codec's
  digital block** (JI1UDD's block diagram). Beyond that, WDSP mic gain.
- **"Line In" is not a line input on the HL2+.** Since gateware 74p2 the
  Mic/Line-in select bit **turns the HL2+ ATU (AH-4) support on/off**
  ([gateware page](https://www.hermeslite2plus.com/p/hl2-gateware-download.html):
  "ATU enabled/disabled by "Mic/Line-in" select"). So droidHPSDR must not
  offer "Line In" as an audio choice: it would be the AH-4 switch. Proposed:
  Line In and "SDR LineIn (dB)" **not shown** (not doing: on the HL2+ that
  bit is the ATU enable).
- **Mic bias (electret power):** not documented. The AK4951 has a mic
  power supply; whether the board wires it to the jack is in the V4.1
  schematic (not read). **check** with Randal's mic.
- **Codec extras over UDP 1025** (JI1UDD's controller, register writes to
  the AK4951 through the HL2): analog mic gain 0-30 dB, a hardware ALC
  (limiter) for RX or TX audio, a 5-band EQ, loopback. Proposed: **later**
  (WDSP already has leveler, compressor and EQ); worth a look if the mic
  needs more than 18 dB + boost.
- **CW sidetone** is on the phones and the speaker in CW mode (site:
  "with sidetone in CW mode"); the Zeus work heard it "from the radio".

## Questions for Randal

1. **Test gear:** what sits between the HL2 and the dummy load for the
   tests: the HL2 alone (5 W), or the XPA125B too? Which dummy load and
   watt meter? Is there an AH-4, or is the XPA125B's tuner the only one?
   **Randal 2026-11-25:** HL2 → XPA125B → antenna (no separate tuner),
   no dummy load; he watches the amp. Low drive first.
2. **Screen PTT:** hold to talk, tap to toggle, or both (a setting)? Also
   a physical button (a volume key, or a Bluetooth button)?
3. **Phone microphone** (built-in or Bluetooth headset) as a TX source:
   decision 1 says the mic stays the HL2+'s. Later, or never?
4. **VOX always off at app start**, even if it was on when the app
   closed? (my proposal)
5. **A TX timeout** (stops transmitting after N minutes): deskHPSDR has
   none. Want one on the phone? If yes, what default?
6. **Your microphone** on the HL2+ jack: which mic, and is its PTT the
   jack's PTT line? (decides what "Mic In / Boost / Line In / PTT Enabled"
   need, with the checks above)
7. **CW:** paddle or straight key in the KEY jack? Keyboard CW (type
   text, the phone sends Morse) wanted?
8. **Voice keyer** (record messages through the HL2+ mic, play them on
   the air) wanted on the phone?
9. **Duplex** (watch the receiver while transmitting): now or later?
