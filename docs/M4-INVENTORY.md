# M4 inventory: receive operating

Step 1 of M4 (ROADMAP "How each section is done"): every control in
FEATURES.md's M4 part, plus the toolbar actions that belong to it, and what
each really does on an HL2. Read from upstream `9f49c21` on 2026-11-24.
Proposed "not doing" lines are **proposals until Randal agrees**.

Already in the app from M3: band picker, mode picker, filter presets and
edge dragging (with per-mode limits and ESSB), step, AF, LNA gain, sample
rate, phone audio. M4 finishes these and adds the rest below.

## VFO (`vfo_menu.c`, VFO bar, actions)

| Control | On an HL2 | Note |
|---|---|---|
| VFO A / VFO B | two frequencies, one receiver | B is a stored frequency; with RX2 "later" it is not a second receiver |
| A>B, B>A, A<>B | copy / swap | upstream `vfo_a_to_b()` etc. in vfo.c (not compiled yet) |
| Split, Quick Split | TX on B | display only until M7 (no transmit) |
| Step, VFO step | tuning step | already a chip; upstream steps list |
| RIT, RIT +/-, RIT step (1/10/100 Hz), RIT clear | receive offset | XIT is TX: M7 |
| CTUN | click-tune inside a fixed window instead of moving the DDC | changes what dragging does: the window stays, the VFO moves inside it |
| Lock VFOs | no tuning | touch tuning must respect it |
| Duplex | TX display while receiving | M7 |
| VFO Fix | hold the frequency against tuning commands | check upstream |

## Band, band stack, memories

| Control | On an HL2 | Note |
|---|---|---|
| Bands 160-6, GEN, WWV | yes | HL2 covers to 54 MHz; IO board switches filters per band (seen in M1) |
| Bands 4, 2 m, 70 cm and up, AIR | only with transverters | M6 (XVTR) |
| Band +/- | yes | swipe gesture candidate |
| Band stack (MHz entries) | yes, 5 entries per band in Region 2 (`US_BANDSTACK_ENTRIES`) | remembers freq, mode, filter per entry |
| Store / recall M0-M9 | yes (`NUM_OF_MEMORYS` 10) | freq, mode, filter |
| Tune Drive Reset | TX | M7 |

## Mode and filter

| Control | On an HL2 | Note |
|---|---|---|
| Modes LSB USB DSB CWL CWU FM AM DIGU DIGL SAM SPEC DRM | all receive | SPEC and DRM: propose **not doing** (SPEC is a display mode; DRM needs a decoder upstream does not have) |
| Filter presets per mode, Var1/Var2 | yes | presets done in M3; Var2 to add |
| Filter width, shift, cut low/high, default | yes | edge dragging done; shift = drag the passband body |
| FM 11k/16k (deviation 2.5k/5.0k) | yes | FM filter = deviation, not edges |
| CW audio peak filter, double pole | yes | M5 with the DSP? it is in the Filter menu upstream |
| Set TX = RX filter edges | TX | M7 |
| "Observe legal TX bandwidth limits when using ESSB!" | heading/warning | show it with ESSB presets |

## AGC and gain

| Control | On an HL2 | Note |
|---|---|---|
| AGC Off / Long / Slow / Medium / Fast | yes | |
| AGC gain (slider), hang threshold | yes | deskHPSDR draws the AGC line on the panadapter: drag it on the 2D view |
| AGC Automatic, auto offset | yes (`agc_auto`) | |
| RF gain / LNA | HL2 LNA -12..+48 dB | done in M3 as LNA |
| HL2 ADC Auto Gain RxPGA, time-regulated | **HL2-specific**, yes | automatic LNA |
| Preamp, Alex attenuator 0-30 dB, bypass ADC filters | not on an HL2 | propose **not doing: no such hardware on the HL2** |
| Random bit, Dither bit | HL2's AD9866 has neither; the dither bit carries the AK4951 flag and is forced on | propose **not doing** |
| Select ADC, ADC/DDC assignment | one ADC | propose **not doing** |

## Meter

| Control | On an HL2 | Note |
|---|---|---|
| S-meter peak / average | yes (`smetermode`) | |
| Meter type digital / analog | yes | the M3 meter card is digital; an analog needle is a mock-up option |
| TX ALC reading | TX | M7 |

## RX menu, the rest

| Control | On an HL2 | Note |
|---|---|---|
| Sample rate 48/96/192/384k | yes | done in M3 (96-384) |
| 768k / 1536k | Protocol 2 only | propose **not doing on the HL2** |
| Mute audio to radio | zeroes the samples **before** both the radio and local audio, so it mutes the phone too | not the "phone only" switch; a true phone-only needs new code |
| Mute when not active | RX2 | later (RX2) |
| DIGU / DIGL offset, RTTY offset | yes | digital-mode dial offsets |
| Use computer audio output | = PHONE chip | done in M3 (untested) |
| Stereo / left / right | yes | for phone and HL2+ audio |
| Test audio | yes | plays a test tone on the phone path |
| RX audio network reserve | yes | larger audio buffer: useful on WiFi |
| P2 jitter buffer | Protocol 2 | propose **not doing** |
| CoreAudio latency correction | macOS | propose **not doing** |
| Mouse gestures / wheel behaviour | desktop | propose **not doing: touch replaces it** |

## Settings persistence

deskHPSDR's props file (`loadProperties`/`saveProperties`, stubbed since
M1): reading and writing it lets a desktop profile be copied over. The app's
own settings today are Android preferences; M4 moves the radio settings to
upstream's props so `rx_save_state`/`rx_restore_state` work unchanged.

## Where things could live (for the mock-ups)

The open design question, for the pictures: on layout 1, where do VFO B,
split, RIT, CTUN, lock, band stack, memories, AGC and the meter type go,
without covering the waterfall or adding a knob.
