# deskHPSDR feature checklist

Every labelled control in deskHPSDR's menus and every action, **generated
from the source** (upstream `9f49c21`, `src/*_menu.c`, `src/actions.c`) by
`tools/features/gen_features.py`, so nothing is forgotten while the phone UI
is designed section by section.

- Tick an item when droidHPSDR has it **and** it was checked on the radio.
- "not doing: <why>" for anything Randal decides to drop.
- Some labels are headings or dialog buttons, not features: mark them
  "heading" and tick them the first time the section is worked on.
- After an upstream update, regenerate into a new file and diff (see the
  script's docstring); never overwrite this file, it holds the ticks.

Which milestone each section belongs to: docs/ROADMAP.md.


## Receive operating (M4)


### Vfo menu (`src/vfo_menu.c`)

- [ ] RIT step
- [ ] 1 Hz  _(choice)_
- [ ] 10 Hz  _(choice)_
- [ ] 100 Hz  _(choice)_
- [ ] VFO step
- [ ] Lock VFOs
- [ ] Duplex
- [ ] CTUN
- [ ] Split

### Band menu (`src/band_menu.c`)

- [ ] Tune Drive Reset

### Bandstack menu (`src/bandstack_menu.c`)

- [ ] MHz

### Mode menu (`src/mode_menu.c`)


### Filter menu (`src/filter_menu.c`)

- [ ] Set TX = RX filter edges
- [ ] 11k
- [ ] (Deviation=2.5k, use for 12.5 kHz raster)
- [ ] 16k
- [ ] (Deviation=5.0k, use for 25.0 kHz raster)
- [ ] /RADEV1
- [ ] Observe legal TX bandwidth limits when using ESSB !
- [ ] Filter Width
- [ ] Filter Shift
- [ ] Filter Cut Low
- [ ] Filter Cut High
- [ ] Default
- [ ] Enable CW Audio peak filter
- [ ] 'double pole' CW Audio peak filter

### Agc menu (`src/agc_menu.c`)

- [ ] AGC
- [ ] Off  _(choice)_
- [ ] Long  _(choice)_
- [ ] Slow  _(choice)_
- [ ] Medium  _(choice)_
- [ ] Fast  _(choice)_
- [ ] AGC Automatic
- [ ] On
- [ ] AGC Auto Offset
- [ ] Hang Threshold

### Meter menu (`src/meter_menu.c`)

- [ ] Meter Type
- [ ] Digital  _(choice)_
- [ ] Analog  _(choice)_
- [ ] S-Meter Reading
- [ ] Peak  _(choice)_
- [ ] Average  _(choice)_
- [ ] TX ALC Reading
- [ ] Gain  _(choice)_

### Store menu (`src/store_menu.c`)

- [ ] Store M

### Rx menu (`src/rx_menu.c`)

- [x] Sample Rate (checked on the radio 2026-11-24)
- [ ] 48000  _(choice)_
- [x] 96000  _(choice)_ (checked on the radio 2026-11-24)
- [x] 192000  _(choice)_ (checked on the radio 2026-11-24)
- [x] 384000  _(choice)_ (checked on the radio 2026-11-24)
- [ ] 768000  _(choice)_
- [ ] 1536000  _(choice)_
- [ ] Select ADC
- [ ] ADC-
- [ ] RX ADC assignment in ADC/DDC Menu
- [ ] Random Bit
- [ ] Dither Bit
- [ ] HL2 ADC Auto Gain RxPGA
- [ ] HL2 Auto Gain time-regulated
- [ ] Preamp
- [ ] Mute Audio to Radio
- [ ] Mute when not active
- [ ] DIGU Offset
- [ ] DIGL Offset
- [ ] Hz
- [ ] Offset RTTY
- [ ] Offset OFF
- [x] Use Computer Audio Output — PHONE button, AAudio (checked on the radio 2026-11-24)
- [ ] Test Audio
- [ ] Stereo / Mono Downmix (L+R)  _(choice)_
- [ ] Left Channel only  _(choice)_
- [ ] Right Channel only  _(choice)_
- [ ] Alex Attenuator
- [ ] 0 dB  _(choice)_
- [ ] 10 dB  _(choice)_
- [ ] 20 dB  _(choice)_
- [ ] 30 dB  _(choice)_
- [ ] Bypass ADC0 RX filters
- [ ] Bypass ADC1 RX filters
- [ ] No global hardware controls available
- [ ] P2 Network Jitter Buffer
- [ ] Buffer depth
- [ ] ms
- [ ] RX Audio Network Reserve
- [ ] Audio reserve
- [ ] CoreAudio RX Latency Correction
- [ ] RX Latency Correction
- [ ] Change Mouse Gestures/Wheel behaviour
- [ ] Receiver not available

## Receive DSP and display (M3/M5)


### Noise menu (`src/noise_menu.c`)

- [ ] SNB
- [ ] ANF
- [ ] Noise Reduction
- [ ] NONE  _(choice)_
- [ ] NR  _(choice)_
- [ ] NR2  _(choice)_
- [ ] NR3  _(choice)_
- [ ] NR4  _(choice)_
- [ ] NNR  _(choice)_
- [ ] NR/NR2/ANF Position
- [ ] Pre AGC  _(choice)_
- [ ] Post AGC  _(choice)_
- [ ] Noise Blanker
- [ ] NB  _(choice)_
- [ ] NB2  _(choice)_
- [ ] MNF
- [ ] NR Settings
- [ ] NR2 Gain Method
- [ ] Linear  _(choice)_
- [ ] Log  _(choice)_
- [ ] Gamma  _(choice)_
- [ ] Trained  _(choice)_
- [ ] NR2 NPE Method
- [ ] OSMS  _(choice)_
- [ ] MMSE  _(choice)_
- [ ] NSTAT  _(choice)_
- [ ] NR2 Artifact Elimination
- [ ] NR2 Post-Processing
- [ ] NR2 Post Level
- [ ] NR2 Post Rate
- [ ] NR2 Post Factor
- [ ] NR2 Post Taper
- [ ] NR2 Trained Thresh
- [ ] NR2 Trained T2
- [ ] NB2 mode
- [ ] Zero  _(choice)_
- [ ] Sample&Hold  _(choice)_
- [ ] Mean Hold  _(choice)_
- [ ] Hold Sample  _(choice)_
- [ ] Interpolate  _(choice)_
- [ ] NB Slew time (ms)
- [ ] NB Lead time (ms)
- [ ] NB Lag time (ms)
- [ ] NB Threshold
- [ ] NR4 Reduction (dB)
- [ ] NR4 Smoothing (%)
- [ ] NR4 Whitening (%)
- [ ] NR4 rescale (dB)
- [ ] NR4 post filter threshold (dB)
- [ ] NNR Model
- [ ] Standard  _(choice)_
- [ ] Premium  _(choice)_
- [ ] NNR Mask Floor (dB)
- [ ] Defaults

### Equalizer menu (`src/equalizer_menu.c`)

- [ ] RX1 EQ Settings
- [ ] Enable
- [ ] Added Frequency-Independent Gain
- [ ] RX Equalizer — Continuous-Gain EQ Model
- [ ] TX Equalizer — Continuous-Gain EQ Model
- [ ] Frequency
- [ ] Gain

### Display menu (`src/display_menu.c`)

- [ ] General Settings
- [ ] Frames Per Second
- [ ] Relation PanWaterfall: (in Percent)
- [ ] Panadapter High
- [ ] Panadapter Low
- [ ] Panadapter Step
- [ ] Waterfall High
- [ ] Waterfall Low
- [x] Waterfall Automatic — auto levels (checked on the radio 2026-11-24)
- [x] Panadapter Automatic: (Related to Noisefloor) — auto levels (checked on the radio 2026-11-24)
- [ ] Noisefloor Margin
- [ ] Show Worldmap
- [x] 3D Waterfall History — 3D view (four looks) (checked on the radio 2026-11-24)
- [ ] Detector
- [ ] Peak  _(choice)_
- [ ] Rosenfell  _(choice)_
- [ ] Average  _(choice)_
- [ ] Sample  _(choice)_
- [ ] Averaging
- [ ] None  _(choice)_
- [ ] Recursive  _(choice)_
- [ ] Time Window  _(choice)_
- [ ] Log Recursive  _(choice)_
- [ ] Av. Time (ms)
- [x] Display Panadapter — 2D view (checked on the radio 2026-11-24)
- [x] Fill Panadapter (checked on the radio 2026-11-24)
- [x] Gradient Panadapter (checked on the radio 2026-11-24)
- [x] Display Waterfall (checked on the radio 2026-11-24)
- [ ] Display Info Bar
- [ ] Show Solardata in Info Bar
- [ ] Show ADC OVF Alarm
- [ ] Show clock & UDP broadcast
- [ ] Show AH4 state
- [x] Enable Peak Labels on Panadapter (checked on the radio 2026-11-24)
- [x] Peak Labels as S-Meter values — default on (checked on the radio 2026-11-24)
- [ ] Peak Labels in Passband Only
- [ ] Hide Peaks Below Noise Floor
- [x] Number of Peaks to label (checked on the radio 2026-11-24)
- [ ] Panadapter Ignore Adjacent Peaks
- [ ] Panadapter Noise Floor Percentile
- [x] Enable PEAKS & HOLD (checked on the radio 2026-11-24)
- [ ] Type of Peaks & Hold function
- [ ] Decay hold time (s)
- [ ] Drop (dbm/s)
- [ ] Enable PEAKS & HOLD for TX
- [ ] PEAKS & HOLD line color
- [ ] TX pan line/fill color

### Fft menu (`src/fft_menu.c`)

- [ ] WDSP FIR Filter Type
- [ ] WDSP FIR Filter NC
- [ ] Binaural
- [ ] RX Image Measure
- [ ] Image Offset Hz
- [ ] RX IQ Gain
- [ ] RX IQ Phase
- [ ] RX IQ Reset
- [ ] RX IQ Auto
- [ ] RX IQ Status
- [ ] RX1
- [ ] RX2 — later: one receiver for now, RX2 maybe later (Randal, 2026-11-24)
- [ ] TX
- [ ] Linear Phase  _(choice)_
- [ ] Low Latency  _(choice)_
- [ ] Reset
- [ ] Auto RX IQ

### Screen menu (`src/screen_menu.c`)

- [ ] Save CSS
- [ ] Remove CSS
- [ ] Reload CSS
- [ ] Touch UI (for Touchscreen)
- [ ] Default Theme  _(choice)_
- [ ] Dark Theme  _(choice)_
- [ ] Main Window Width
- [ ] Main Window Height
- [ ] Main Window size fixed here, free Main Window scaling NOT supported
- [ ] deskHPSDR is made for Desktops, therefore no VFO bar layouts available !
- [ ] Minimum Screensize must be 1280x600 or higher !
- [x] Full Screen Mode — always full screen (checked on the radio 2026-11-24)
- [ ] Slider Background Color
- [ ] Menu Background Color
- [x] Display Zoom/Pan — pinch zoom, upstream zoom 1-16 (checked on the radio 2026-11-24)
- [ ] Display Sliders
- [ ] Display Toolbar
- [ ] Display Extras
- [ ] Display Warnings
- [ ] Display PA current
- [ ] Save Zoom Level
- [ ] Preserve Narrow Peaks
- [ ] Rectangular  _(choice)_
- [ ] 4-term Blackman-Harris  _(choice)_
- [ ] Hann  _(choice)_
- [ ] Flat-top  _(choice)_
- [ ] Hamming  _(choice)_
- [ ] Kaiser (default)  _(choice)_
- [ ] 7-term Blackman-Harris  _(choice)_
- [ ] Auto (default)  _(choice)_
- [ ] TX Audio Levels Display Settings [Wayland compatibility mode]
- [ ] TX Audio Levels Display Settings
- [ ] Display TX AF Levels
- [ ] Show PEAK levels
- [ ] Use Popover
- [ ] Show Popover inside

### Diversity menu (`src/diversity_menu.c`)

- [ ] Diversity Enable — later: needs RX2 (one receiver for now, Randal 2026-11-24; the HL2 has one ADC)
- [ ] Brick3 [Previous/Older Firmware] — later: needs RX2 (one receiver for now, Randal 2026-11-24; the HL2 has one ADC)
- [ ] Gain (dB, coarse) — later: needs RX2 (one receiver for now, Randal 2026-11-24; the HL2 has one ADC)
- [ ] Gain (dB, fine) — later: needs RX2 (one receiver for now, Randal 2026-11-24; the HL2 has one ADC)
- [ ] Phase (coarse) — later: needs RX2 (one receiver for now, Randal 2026-11-24; the HL2 has one ADC)
- [ ] Phase (fine) — later: needs RX2 (one receiver for now, Randal 2026-11-24; the HL2 has one ADC)

### Ddc menu (`src/ddc_menu.c`)

- [ ] OpenHPSDR P2 ADC/DDC Assignment
- [ ] Advanced Protocol 2 setting: Select which ADC feeds each DDC.
- [ ] Advanced Protocol 2 setting: If HERMES no selection, has only one ADC.
- [ ] DDC
- [ ] ADC0
- [ ] ADC1
- [ ] RX1
- [ ] RX2 — later: one receiver for now, RX2 maybe later (Randal, 2026-11-24)
- [ ] Reset HERMES
- [ ] Reset ANAN-100D 1 Ant
- [ ] Reset ANAN-100D 2 Ant

### Toolbar menu (`src/toolbar_menu.c`)

- [ ] FNC()
- [ ] X()
- [ ] Toolbar

### Switch menu (`src/switch_menu.c`)

- [ ] Defaults

## Hardware: HL2, IO board, HL2+, PA, filters (M6)


### Radio menu (`src/radio_menu.c`)

- [ ] Receivers — later: one receiver for now (Randal, 2026-11-24)
- [ ] RIT/XIT step (Hz)
- [ ] 10  _(choice)_
- [ ] 100  _(choice)_
- [ ] Audio Capture Time (sec)
- [ ] SAT mode
- [ ] SAT Off  _(choice)_
- [ ] SAT  _(choice)_
- [ ] RSAT  _(choice)_
- [x] Channel Marker for 60m
- [ ] IARU Region
- [ ] NONE (VFO)  _(choice)_
- [ ] UK Channels  _(choice)_
- [ ] US Channels  _(choice)_
- [ ] WRC15  _(choice)_
- [ ] CA Channels  _(choice)_
- [ ] Other RX Panadapter Marker
- [ ] IARU protected beacon areas
- [ ] IARU Contest-Free Areas
- [ ] Filter Board
- [ ] NONE  _(choice)_
- [ ] ALEX  _(choice)_
- [ ] APOLLO  _(choice)_
- [ ] CHARLY25  _(choice)_
- [ ] N2ADR (LPF RX + TX + HPF)  _(choice)_
- [ ] N2ADR (LPF TX only)  _(choice)_
- [ ] +RX: N2ADR HPF 3MHz
- [ ] VFO Encoder Divisor
- [ ] ATLAS bus settings
- [ ] 10 MHz source
- [ ] Atlas  _(choice)_
- [ ] Penelope  _(choice)_
- [ ] Mercury  _(choice)_
- [ ] 122.88 MHz source
- [ ] Mic source
- [ ] Janus  _(choice)_
- [ ] TX config
- [ ] No TX  _(choice)_
- [ ] Pennylane  _(choice)_
- [ ] Janus Only
- [ ] ORION/SATURN Mic jack
- [ ] PTT On Ring, Mic and Bias on Tip
- [ ] Mic Input
- [ ] 3.5mm  _(choice)_
- [ ] XLR  _(choice)_
- [ ] PTT Enabled
- [ ] BIAS Enabled
- [ ] Split operation
- [ ] Mute RX when TX
- [ ] HL2 5W PA enable
- [ ] PA enable
- [ ] Duplex operation
- [ ] HL2 Force IO Board
- [ ] Mute Spkr Amp
- [ ] No audio codec  _(choice)_
- [ ] HL2+ audio codec (AK4951)  _(choice)_
- [ ] SQUARE SDR 2 audio codec  _(choice)_
- [ ] HL2 CL1 10Mhz Ref Clock
- [ ] HL2 ATU TUNE support
- [ ] Generic HERMES  _(choice)_
- [ ] Anan-10E/100B  _(choice)_
- [ ] Brick/Brick2 SDR  _(choice)_
- [ ] Enable external TxInhibit Input
- [ ] Enable external AutoTune Input
- [ ] Disable Split on Band Change
- [ ] Freq. Calibration (ppm factor)
- [ ] ADC Gain Calibration (dB)
- [ ] Your Callsign
- [ ] Set
- [ ] Your Locator
- [ ] SDR-Device Samplerate Settings in RX Menu

### Ant menu (`src/ant_menu.c`)

- [ ] Band
- [ ] RX Ant
- [ ] TX Ant
- [ ] Ant1  _(choice)_
- [ ] Ant2  _(choice)_
- [ ] Ant3  _(choice)_
- [ ] Ext1  _(choice)_
- [ ] Ext2  _(choice)_
- [ ] Xvtr  _(choice)_
- [ ] HF
- [ ] Brick SDR: has fixed ANT input, PS input is dedicated feedback.
- [ ] ANAN 100/200 new PA board

### Oc menu (`src/oc_menu.c`)

- [ ] Band
- [ ] Rx
- [ ] Tx
- [ ] Tune (ORed with TX)
- [ ] Full Tune(ms)
- [ ] Memory Tune(ms)

### Pa menu (`src/pa_menu.c`)

- [ ] Reset
- [ ] MAX Power
- [ ] 1W  _(choice)_
- [ ] 5W  _(choice)_
- [ ] 10W  _(choice)_
- [ ] 15W  _(choice)_
- [ ] 20W  _(choice)_
- [ ] 25W  _(choice)_
- [ ] 30W  _(choice)_
- [ ] 50W  _(choice)_
- [ ] 75W  _(choice)_
- [ ] 100W  _(choice)_
- [ ] 125W  _(choice)_
- [ ] 200W  _(choice)_
- [ ] 500W  _(choice)_
- [ ] 1KW  _(choice)_
- [ ] Transmit out of band
- [ ] Save
- [ ] Import
- [ ] Calibrate
- [ ] Watt Meter Calibrate

### Xvtr menu (`src/xvtr_menu.c`)

- [ ] Update
- [ ] Title
- [ ] Min Frq(MHz)
- [ ] Max Frq(MHz)
- [ ] LO Frq(MHz)
- [ ] LO Err(Hz)
- [ ] Gain (dB)
- [ ] Disable PA
- [ ] Reset

## Transmit (M7, only when Randal asks)


### Tx menu (`src/tx_menu.c`)

- [ ] Yes
- [ ] No
- [ ] TX Basic Settings
- [ ] Activate
- [ ] Save
- [ ] Import file
- [ ] Export file
- [ ] Use computer audio input
- [ ] SDR Radio Input
- [ ] Mic In  _(choice)_
- [ ] Mic Boost  _(choice)_
- [ ] Line In  _(choice)_
- [ ] SDR LineIn (dB)
- [ ] TX Filter Low
- [ ] TX uses RX Filter
- [ ] Local Mic PreAmp Gain
- [ ] TX Filter High
- [ ] Tune Drive ≙ TX drive
- [ ] Tune Drive level
- [ ] Use Drive levels per Band
- [ ] Tune Drive Stepping
- [ ] Panadapter High
- [ ] SWR Protection
- [ ] SWR alarm at
- [ ] Panadapter Low
- [ ] CTCSS Enable
- [ ] CTCSS Frequency
- [ ] Panadapter Step
- [ ] FM PreEmp/ALC
- [ ] AM Carrier Level
- [ ] Fill TX Panadapter
- [ ] Max Digi Drv
- [ ] Phase Rotator
- [ ] Leveler
- [ ] Speech Proc
- [ ] Auto CESSB
- [ ] WDSP TX EQ Ctfmode
- [ ] Use DEXP
- [ ] Use Side Channel Filter
- [ ] Expansion Factor (dB)
- [ ] Filter Low-Cut (Hz)
- [ ] Hysteresis Ratio
- [ ] Filter High-Cut (Hz)
- [ ] Trigger Level (dB)
- [ ] Trigger Attack tau (ms)
- [ ] Trigger Attack Time (ms)
- [ ] Trigger Release Time (ms)
- [ ] Trigger Hold Time (ms)
- [ ] Use Pre-CFC
- [ ] Use Post-CFC
- [ ] Pre Comp
- [ ] Post Gain
- [ ] CFC: Multiband Compressor, not an EQ.
- [ ] Freq.
- [ ] Pre
- [ ] Post
- [ ] Show Peak Numbers on Panadapter
- [ ] Show Peaks in Passband Only
- [ ] Hide Peaks Below Noise Floor
- [ ] Number of Peaks to label
- [ ] Panadapter Ignore Adjacent Peaks
- [ ] Panadapter Noise Floor Percentile

### Cw menu (`src/cw_menu.c`)

- [ ] CW handled in Radio
- [ ] CW Speed (WPM)
- [ ] CW Break-In, Delay (ms)
- [ ] Sidetone Level
- [ ] Sidetone Freq
- [ ] CW Zero Beat Freq. Corr
- [ ] Weight
- [ ] Paddle Mode
- [ ] Straight Key  _(choice)_
- [ ] Iambic Mode A  _(choice)_
- [ ] Iambic Mode B  _(choice)_
- [ ] Keys reversed
- [ ] Enforce letter spacing

### Vox menu (`src/vox_menu.c`)

- [ ] VOX Enable
- [ ] Mic Level
- [ ] VOX Threshold
- [ ] VOX Hang (ms)
- [ ] Use Side Channel Filter
- [ ] Filter Low-Cut (Hz)
- [ ] Filter High-Cut (Hz)

### Ps menu (`src/ps_menu.c`)

- [ ] Don't show this warning again
- [ ] Feedback Lvl
- [ ] Correcting
- [ ] Noise
- [ ] AmpView
- [ ] Enable PS
- [ ] Two Tone
- [ ] Auto Attenuate
- [ ] OFF
- [ ] Restart
- [ ] MON
- [ ] PS FeedBk ANT
- [ ] Internal  _(choice)_
- [ ] Ext1  _(choice)_
- [ ] ByPass  _(choice)_
- [ ] OneShot
- [ ] PS Stability
- [ ] Strict  _(choice)_
- [ ] Medium  _(choice)_
- [ ] Relaxed  _(choice)_
- [ ] PS TX Delay (ns)
- [ ] [Optimal feedback level between 140..165]
- [ ] GetPk
- [ ] SetPk
- [ ] TX ATT

## Station integration (M8)


### Rigctl menu (`src/rigctl_menu.c`)

- [ ] Enable CAT Debug
- [ ] Enable TCI Debug
- [ ] Block [RX] CAT command during TUNE
- [ ] TCP
- [ ] + start rigctld at port 4533
- [ ] Enable
- [ ] Andromeda
- [ ] AutoRprt
- [ ] 4800 Bd  _(choice)_
- [ ] 9600 Bd  _(choice)_
- [ ] 19200 Bd  _(choice)_
- [ ] 38400 Bd  _(choice)_
- [ ] RTS/CTS
- [ ] RTS: TUNE output DTR: TX output
- [ ] CTS TX Inhibit (active low)
- [ ] RTS & CTS as PTT Input
- [ ] TCI
- [ ] RumLogNG compatibility
- [ ] TCI RX Audio ATT
- [ ] 0 dB  _(choice)_
- [ ] -3 dB  _(choice)_
- [ ] -6 dB  _(choice)_
- [ ] -12 dB  _(choice)_
- [ ] TCI TX Audio ATT
- [ ] Report TX Frequency Only
- [ ] TCI I/Q swap
- [ ] TCI I/Q conjugate
- [ ] TCI Audio Monitor

### Midi menu (`src/midi_menu.c`)

- [ ] MIDI device(s)
- [ ] No MIDI devices found!
- [ ] Event
- [ ] Channel
- [ ] Note
- [ ] Type
- [ ] Value
- [ ] Min
- [ ] Max
- [ ] Action
- [ ] Invert
- [ ] Toggle
- [ ] Delete All
- [ ] Update
- [ ] Delete
- [ ] Ignore Controller Pairs
- [ ] Configure WHEEL parameters
- [ ] Left <<<
- [ ] Left <<
- [ ] Left <
- [ ] Right >
- [ ] Right >>
- [ ] Right >>>
- [ ] KEY  _(choice)_
- [ ] WHEEL  _(choice)_
- [ ] KNOB/SLIDER  _(choice)_

### Extras menu (`src/extras_menu.c`)

- [ ] Telnet DX Cluster Window Setup
- [ ] DXC Login
- [ ] Set
- [ ] DXC Address
- [ ] DXC Port
- [ ] DX Spots on RX Panadapter Setup
- [ ] DX Spots Lifetime
- [ ] DX Spots max. rows
- [ ] Show spots only on active RX
- [ ] Reverse Beacon Network Setup
- [ ] Enable RBN spots
- [ ] CW
- [ ] RTTY
- [ ] CQ spots only
- [ ] RBN address
- [ ] RBN Port
- [ ] User defined Window with Webaccess
- [ ] Titlebar
- [ ] URL
- [ ] Window Width
- [ ] Window Height
- [ ] Action Button

## Probably not on Android (decide per item)


### Saturn menu (`src/saturn_menu.c`)

- [ ] Saturn Server Enable
- [ ] Client Transmit Enable
- [ ] *Client is RX Only*

### Exit menu (`src/exit_menu.c`)

- [ ] Exit
- [ ] Reboot
- [ ] Shutdown

### About menu (`src/about_menu.c`)


### New menu (`src/new_menu.c`)

- [ ] Restart Protocol P1
- [ ] Restart Protocol P2
- [ ] Restart Protocol
- [ ] Iconify
- [ ] SDR Device
- [ ] Screen
- [ ] Display
- [ ] Meter
- [ ] XVTR
- [ ] Discovery
- [ ] Saturn
- [ ] VFO
- [ ] Band
- [ ] BandStack
- [ ] Mode
- [ ] Memory
- [ ] RX
- [ ] RX Filter
- [ ] Noise
- [ ] AGC
- [ ] Diversity — later: needs RX2 (one receiver for now, Randal 2026-11-24; the HL2 has one ADC)
- [ ] TX
- [ ] PA
- [ ] VOX
- [ ] PS
- [ ] CW
- [ ] DSP
- [ ] WDSP EQ
- [ ] Ant
- [ ] P2 ADC/DDC
- [ ] OC Output
- [ ] Extras
- [ ] Toolbar
- [ ] CAT/TCI
- [ ] MIDI
- [ ] About

## Actions (toolbar buttons, MIDI, shortcuts; `src/actions.c`)

Each needs a home in the phone UI (button, gesture, slider, menu entry) or a "not doing".

- [ ] A<>B
- [ ] A<B
- [ ] A>B
- [ ] AF Gain
- [ ] AF Gain RX1
- [ ] AF Gain RX2 — later: one receiver for now, RX2 maybe later (Randal, 2026-11-24)
- [ ] AGC
- [ ] AGC Gain
- [ ] AGC Gain RX1
- [ ] AGC Gain RX2 — later: one receiver for now, RX2 maybe later (Randal, 2026-11-24)
- [ ] AGC Menu
- [ ] ANF
- [ ] Atten
- [ ] Band 10
- [ ] Band 12
- [ ] Band 1240
- [ ] Band 136
- [ ] Band 144
- [ ] Band 15
- [ ] Band 160
- [ ] Band 17
- [ ] Band 20
- [ ] Band 220
- [ ] Band 2300
- [ ] Band 30
- [ ] Band 8
- [ ] Band 40
- [ ] Band 430
- [ ] Band 6
- [ ] Band 60
- [ ] Band 70
- [ ] Band 80
- [ ] Band 902
- [ ] Band AIR
- [ ] Band GEN
- [ ] Band -
- [ ] Band +
- [ ] Band WWV
- [ ] BndStack -
- [ ] BndStack +
- [ ] Binaural
- [ ] Band Menu
- [ ] BndStack Menu
- [ ] Capture
- [ ] Cmpr On/Off
- [ ] Cmpr Level
- [ ] CTUN
- [ ] CW Audio Peak Fltr
- [ ] CW Frequency
- [ ] CW Left
- [ ] CW Right
- [ ] CW Speed
- [ ] CW Key (Keyer)
- [ ] PTT (CW Keyer)
- [ ] Speed (Keyer)
- [ ] CW Straight Key
- [ ] CW Zero Beat
- [ ] DIV On/Off
- [ ] DIV Gain
- [ ] DIV Gain Coarse
- [ ] DIV Gain Fine
- [ ] DIV Phase
- [ ] DIV Phase Coarse
- [ ] DIV Phase Fine
- [ ] DIV Menu
- [ ] Duplex
- [ ] DIGU Offset
- [ ] DIGL Offset
- [ ] DXC Window
- [ ] DXC Test
- [ ] Filter -
- [ ] Filter +
- [ ] Filter Cut Low
- [ ] Filter Cut High
- [ ] Filter Cut Default
- [ ] Full Screen
- [ ] RX Filter Menu
- [ ] Function
- [ ] FuncRev
- [ ] Greyline Window
- [ ] IF Shift
- [ ] IF Shift RX1
- [ ] IF Shift RX2 — later: one receiver for now, RX2 maybe later (Randal, 2026-11-24)
- [ ] IF Width
- [ ] IF Width RX1
- [ ] IF Width RX2 — later: one receiver for now, RX2 maybe later (Randal, 2026-11-24)
- [ ] Linein Gain
- [ ] Lock
- [ ] Main Menu
- [ ] Memory Menu
- [ ] Mic Gain
- [ ] MNF On/Off
- [ ] MNF Center
- [ ] MNF BW
- [ ] Mode -
- [ ] Mode +
- [ ] Mode Menu
- [ ] MOX
- [ ] Multi
- [ ] Multi Action Select
- [ ] Multi Toggle
- [ ] Mute Audio
- [ ] Mute RX
- [ ] Mute RX1
- [ ] Mute RX2 — later: one receiver for now, RX2 maybe later (Randal, 2026-11-24)
- [ ] NB
- [ ] NR
- [ ] Noise Menu
- [ ] NumPad 0
- [ ] NumPad 1
- [ ] NumPad 2
- [ ] NumPad 3
- [ ] NumPad 4
- [ ] NumPad 5
- [ ] NumPad 6
- [ ] NumPad 7
- [ ] NumPad 8
- [ ] NumPad 9
- [ ] NumPad BS
- [ ] NumPad CL
- [ ] NumPad Dec
- [ ] NumPad kHz
- [ ] NumPad MHz
- [ ] NumPad Enter
- [ ] PanZoom
- [ ] Pan-
- [ ] Pan+
- [ ] Panadapter High
- [ ] Panadapter Low
- [ ] Panadapter Step
- [ ] Preamp On/Off
- [ ] PS On/Off
- [ ] PS Menu
- [ ] PTT
- [ ] Quick Split
- [ ] Rcl 0
- [ ] Rcl 1
- [ ] Rcl 2
- [ ] Rcl 3
- [ ] Rcl 4
- [ ] Rcl 5
- [ ] Rcl 6
- [ ] Rcl 7
- [ ] Rcl 8
- [ ] Rcl 9
- [ ] Replay
- [ ] Voice Keyer
- [ ] VK Replay
- [ ] VK Playback
- [ ] VK Playback T1
- [ ] VK Playback T2
- [ ] VK Playback T3
- [ ] VK Playback T4
- [ ] VK Playback T5
- [ ] VK Playback T6
- [ ] VK Stop XMIT
- [ ] RF Gain
- [ ] RF Gain RX1
- [ ] RF Gain RX2 — later: one receiver for now, RX2 maybe later (Randal, 2026-11-24)
- [x] RIT
- [x] RIT Clear
- [x] RIT On/Off
- [ ] RIT -
- [ ] RIT +
- [ ] RIT RX1
- [ ] RIT RX2 — later: one receiver for now, RX2 maybe later (Randal, 2026-11-24)
- [ ] RIT Step
- [ ] RIT/XIT
- [ ] RIT/XIT Cycle
- [ ] RIT/XIT Clear
- [ ] RSAT
- [ ] RX Menu
- [ ] RX1
- [ ] RX2 — later: one receiver for now, RX2 maybe later (Randal, 2026-11-24)
- [ ] SAT
- [ ] EXIT App
- [ ] SNB
- [ ] Split
- [x] Squelch
- [ ] Squelch RX1
- [ ] Squelch RX2 — later: one receiver for now, RX2 maybe later (Randal, 2026-11-24)
- [ ] Swap RX
- [ ] ToolBar1
- [ ] ToolBar2
- [ ] ToolBar3
- [ ] ToolBar4
- [ ] ToolBar5
- [ ] ToolBar6
- [ ] ToolBar7
- [ ] ToolBar8
- [ ] Tune
- [ ] Tune (toggle)
- [ ] Tune IOB
- [ ] Tune Drv
- [ ] Tune Full
- [ ] Tune Mem
- [ ] AH-4 Run
- [ ] AH-4 Bypass
- [ ] TX Drive
- [ ] Two-Tone
- [ ] TX Menu
- [ ] VFO
- [ ] VFO Fix
- [ ] VFO Menu
- [ ] VFO Step -
- [ ] VFO Step +
- [ ] VFO A
- [ ] VFO B
- [ ] VOX On/Off
- [ ] VOX Level
- [ ] Wfall High
- [ ] Wfall Low
- [ ] XIT
- [ ] XIT Clear
- [ ] XIT On/Off
- [ ] XIT -
- [ ] XIT +
- [ ] Zoom
- [ ] Zoom -
- [ ] Zoom +
- [ ] XVTR 1
- [ ] XVTR 2
- [ ] XVTR 3
- [ ] XVTR 4
- [ ] XVTR 5
- [ ] XVTR 6
- [ ] XVTR 7
- [ ] XVTR 8
- [ ] XVTR 9
- [ ] XVTR 10
- [ ] TX Monitor On/Off
- [ ] TX Monitor Volume
