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
3. **What the HL2 does when the app stops mid-transmission is unknown.**
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

## Step 3: CW

The HL2's FPGA keyer arms only in a CW mode, paddle in the KEY jack
(Zeus HL2+ work, docs/HARDWARE.md). With "CW handled in Radio" the FPGA
does the keying and old_protocol.c doesn't set MOX.

| Control | Proposed |
|---|---|
| CW handled in Radio (`cw_keyer_internal`) | **build**, on |
| CW speed, weight, letter spacing, paddle mode (straight / iambic A / B), keys reversed | **build** (sent to the HL2 in 0x16 / 0x1E) |
| Break-in + delay (hang time), PTT delay | **build** |
| Sidetone level and frequency | **build**; **check** whether the sidetone comes out of the HL2+ speaker/phones |
| CW zero beat freq. corr., CW Zero Beat action | **build** |
| Actions: CW Key, PTT (CW keyer), CW Left/Right, Straight key, Speed, Frequency | **build** what fits a touch screen (question 7) |
| Keyboard CW (`cw_engine.c`, text → Morse) | **build** if wanted (question 7) |
| Voice keyer (record from the HL2+ mic, play slots) | **build** if wanted (question 8) |

## Step 4: PureSignal

| Control | Proposed |
|---|---|
| Enable PS, OFF, Restart, MON, OneShot, Two Tone | **build** |
| Auto Attenuate, TX ATT (−29..+31 dB on an HL2), GetPk / SetPk, feedback level ("optimal 140..165"), Correcting / Noise status | **build** |
| PS feedback antenna: Internal / Ext1 / ByPass | **check** which ones mean anything on the HL2 (my guess: Internal only, not confirmed) |
| PS stability Strict / Medium / Relaxed, PS TX delay (ns) | **build** (settings page) |
| AmpView (the correction curves) | **build** as a GPU graph (pictures) |
| "Don't show this warning again" | heading, ticked when the page is built |
| Sample rate: two RX + two feedback DDCs at 384 kHz over the link? | **check** (ROADMAP M7 step 4); falls back to 192 kHz |

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
