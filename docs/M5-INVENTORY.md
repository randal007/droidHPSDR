# M5 inventory: receive DSP

Step 1 of M5 (ROADMAP "How each section is done"): every control in
FEATURES.md's M5 part, plus squelch, CW peak and binaural, and what each
really does on an HL2. Read from upstream (`src/`, as compiled) on
2026-11-24. Proposed "not doing" lines are **proposals until Randal agrees**.

Everything here is receive-side DSP inside WDSP, so it works the same on an
HL2 as on any radio. `receiver.c` (already compiled) applies it:
`rx_set_noise()`, `rx_set_anf()`, `rx_set_notch()`, `rx_set_squelch()`,
`rx_set_equalizer()`, `rx_set_af_binaural()`. The props file already saves
every field below.

**Per mode, like deskHPSDR.** NR, NB, ANF, SNB and all their fine settings
are kept per mode: `noise_menu.c`'s `update_noise_for_rx()` copies them into
`mode_settings[mode]` and calls `copy_mode_settings()`. So NR2 on SSB and
nothing on CW is remembered. noise_menu.c itself is GTK, so we copy that one
small function (with attribution) instead of compiling the dialog.

## Noise menu (`noise_menu.c`): the main buttons

| Control | What it does | Note |
|---|---|---|
| NR: NONE / NR / NR2 / NR3 / NR4 / NNR | one noise reduction at a time (`rx->nr` 0-5) | NR = LMS, NR2 = spectral (EMNR), NR3 = rnnoise, NR4 = libspecbleach, NNR = WDSP 2.11's neural one. **Off in DIGU/DIGL** (upstream forces it off, the button greys out) |
| NB: NONE / NB / NB2 | noise blanker (`rx->nb` 0-2) | impulse noise; works before the filter |
| ANF | automatic notch (LMS) for carriers | **off in CW and DIGI** (upstream) |
| SNB | spectral noise blanker | off in DIGI |
| MNF + notch width | one manual notch at a frequency, 10-15000 Hz wide | deskHPSDR: **right-click on the panadapter** puts the notch there, shift-right-click clears it. Phone: a gesture on the spectrum (pictures) |
| NR/NR2/ANF position: Pre AGC / Post AGC | where NR sits in the chain | |

## Noise menu: fine settings (shown per NR/NB type in deskHPSDR)

| Group | Controls | Note |
|---|---|---|
| NR2 | Gain method (Linear, Log, Gamma, Trained), NPE method (OSMS, MMSE, NSTAT), Artifact elimination, Post-processing on/off, Post level, rate, factor, taper, Trained threshold, Trained T2 | |
| NB / NB2 | NB2 mode (Zero, Sample&Hold, Mean Hold, Hold Sample, Interpolate), slew, lead, lag (ms), threshold | |
| NR4 | Reduction (dB), smoothing (%), whitening (%), rescale (dB), post-filter threshold (dB) | |
| NNR | Model: Standard / Premium, mask floor (dB), Defaults (Standard, -25 dB) | models are built into WDSP 2.11 (`nnr_model_*.c`): check the log for "no wdsp_nnr_3.bin" and the CPU cost on the phone. **NNR switches binaural off** (upstream) |
| Defaults | puts the fine settings back | |

## Filter menu leftovers and squelch

| Control | What it does | Note |
|---|---|---|
| CW audio peak filter (`cwAudioPeakFilter`, per VFO) | narrow peak at the sidetone pitch in CW | lives in deskHPSDR's Filter menu; shown there only in CW |
| Squelch on/off + level | `rx_set_squelch()` picks the kind from the mode: AM squelch (AM/SAM), FM squelch (FM), voice squelch (SSB, others) | a slider in deskHPSDR's slider area. Phone idea: a line on the spectrum like the AGC line |

## DSP button (`fft_menu.c`) — deskHPSDR's "DSP" opens this

| Control | On an HL2 | Note |
|---|---|---|
| WDSP FIR filter type RX: Linear Phase / Low Latency | yes | |
| FIR filter NC (1k-16k, default 2048) | yes | more = steeper skirts, more CPU |
| Binaural (I and Q on left/right) | phone audio only, and only in stereo | the HL2+ speaker path is the AK4951; binaural matters with headphones on the phone. Off with NNR |
| TX filter type / NC | TX | M7 |
| RX image measure, image offset, RX IQ gain / phase / reset / auto / status | **not on an HL2**: the HL2 samples RF directly and mixes in the FPGA, so there is no analogue I/Q imbalance and no image to correct | propose **not doing: no I/Q imbalance on a direct-sampling radio** |
| RX2 column | RX2 | later |

## WDSP EQ (`equalizer_menu.c`)

| Control | What it does | Note |
|---|---|---|
| Enable | RX audio equalizer on/off | greyed out in DIGU/DIGL (upstream) |
| Added frequency-independent gain | -20..+20 dB preamp | |
| Frequency / Gain points | WDSP's continuous-gain EQ: up to 12 points (default 50 Hz-8 kHz, 0 dB), each -20..+20 dB | deskHPSDR draws the curve; phone: drag the points on the graph (pictures) |
| TX equalizer | TX | M7 |

## Display menu leftovers (`display_menu.c`)

Already ticked in M3: auto levels, 3D history, 2D view, fill, gradient,
waterfall, peak labels, S-meter labels, number of peaks, peaks & hold.

| Control | State | Note |
|---|---|---|
| Frames per second, pan/waterfall relation, detector, Av. time, labels in passband only, hide peaks below noise, ignore adjacent peaks, noise floor percentile | **built in M3** (Display sheet), not ticked yet | display-only: can be checked in **demo mode**, no radio needed |
| Averaging: None / Recursive / Time Window / Log Recursive | to build | we average rows ourselves (WDSP averaging off on purpose, CLAUDE.md trap); map the four kinds onto ours |
| Panadapter high / low / step, waterfall high / low, noise floor margin | to build | manual levels for when auto is off |
| Peaks & hold: type, decay hold time, drop rate | to build | |
| Info bar: show it, solar data, ADC overload alarm, clock & UDP broadcast, AH4 state | to build | solar data needs the network (M8 has solar data); ADC overload already counted in the status line; AH4 state is M6 |
| Show worldmap | propose **not doing** for now: a desktop background picture behind the panadapter; on the phone the 3D view is the background | |
| PEAKS & HOLD for TX, TX pan colours | TX | M7 |
| Peak & hold line colour | to build | with the themes |

## Screen menu (`screen_menu.c`)

| Control | On a phone | Note |
|---|---|---|
| Full screen, Display Zoom/Pan | done (M3) | ticked |
| FFT window (7 kinds), FFT size (Auto + sizes) | window **built in M3** (Display sheet), not ticked; size to build | |
| Display sliders / toolbar / extras / warnings / PA current | maps to which strips show on the phone | ties in with the Toolbar question below |
| Save zoom level, preserve narrow peaks | to build | |
| Save / remove / reload CSS, Default / Dark theme, Touch UI, slider and menu background colours | GTK styling | propose **not doing: GTK stylesheets; the phone has its own look and the 3D themes** |
| Main window width / height, "made for desktops" notes, minimum 1280x600 | desktop window | propose **not doing: the phone screen is fixed** |
| TX audio level display, peak levels, popover | TX | M7 |

## Toolbar and Switch (`toolbar_menu.c`, `switch_menu.c`)

| Control | On a phone | Note |
|---|---|---|
| Toolbar: assign any action to the function buttons (FNC, X) | yes, the phone way: **the bottom chip row becomes user-configurable** (ROADMAP's "three layers" idea) | pictures |
| Switch menu: Defaults | GPIO switches of a controller board (Pi/G2) | propose **not doing: no GPIO controller on a phone** |

## Questions for Randal

1. The proposed "not doing" lines above: RX IQ / image correction, worldmap,
   CSS/themes/window size, Switch menu.
2. Where NR/NB/ANF/SNB/MNF live day to day: a DSP drawer, a chip, or buttons
   in the VFO pill? (pictures)
3. MNF on the phone: which gesture places the notch on the spectrum (deskHPSDR
   uses right-click)? Long-press is already "drag the frequency" on the
   digits only, so long-press on the spectrum is free.
