# Radio test before M7

Everything built in M4, M5 and M6 (and the code-review fixes), tried on the
HL2 + IO board + HL2+ with the phone. Written 2026-11-25. When an item
passes, Claude ticks it here and in FEATURES.md; when it fails, it goes on
the fix list at the bottom with what was seen.

Before starting: adb over wireless debugging connected (so Claude can read
the log), the newest build installed, USB Ethernet adapter plugged in.

## 1. Start and connection

- [x] Start radio over Ethernet: connects, spectrum and HL2+ audio come up
- [ ] Info bar beside the VFO: UTC clock, "ADC OK", "HL2 169.254.19.221"
- [x] Unplug the adapter, plug it back: the radio comes back by itself
- [ ] Replug, then press Stop straight away: it really stops
- [ ] Close the app (swipe it away) and open it again: frequency, mode, filter, band stacks, settings all kept
- [ ] Display → Speed 15 vs 60: the waterfall scrolls slower / faster

## 2. HL2 hardware (M6)

- [ ] Change band 160 → 80 → 40 → 20 → 15 → 10: the HL2's relays click (filter board now switched)
- [ ] ☰ → SDR Device: IO board status says "Pico found", with the LPF shown
- [ ] ☰ → Ant: set 40 m to "RX in": the IO board switches to its RX input on 40 m, back on ANT1 for other bands
- [x] SDR Device → 60 m markers "CA": on 60 m, five amber channels at 5332 / 5348 / 5358.5 / 5373 / 5405 kHz
- [ ] Frequency calibration: a known carrier (WWV 10 MHz, CHU 7.85 MHz) lands right; adjust ppm if not
- [ ] ADC gain calibration moves the S-meter and spectrum dBm
- [ ] Codec "None" then back to "HL2+": speaker audio stops and returns
- [ ] HL2 setup → Reboot HL2: the radio comes back on its own after about 15 s
- [ ] (only if wanted) Prefer DHCP / alternate MAC: written, effective after a reboot

## 3. VFO and tuning (M4)

- [ ] Drag, fling, pinch, tap a signal: tuning as before
- [ ] VFO panel: A>B, B>A, A⇄B; tap VFO B to make it active
- [ ] RIT on, drag the middle of the passband, RIT 0; RIT/XIT step 1 / 10 / 100 Hz
- [ ] CTUN on, drag: the spectrum stays put, the line moves; off again: re-centred
- [ ] Lock: dragging no longer tunes
- [ ] Step per mode: USB 100 → CWU 25 → back to USB shows 100 again
- [ ] Band sheet: tap the same band again to cycle its band stack; store and recall a memory
- [ ] Type a frequency on the keypad (tap the frequency digits)

## 4. Modes, filters, AGC, meter (M4)

- [ ] Mode change brings back that mode's filter, AGC and step
- [ ] Filter presets; drag the passband edges (Var1)
- [ ] CW: tap a CW signal: it lands in the passband at the right pitch; drag a CW filter edge
- [ ] AGC modes; AGC-G and AGC-Th lines drag, then fade after 3 s
- [ ] AGC Automatic on: gain follows the noise floor; the lines don't flash every 5 s
- [ ] Analog meter (hold the meter → Meter type); Peak / Average
- [ ] RX page: HL2 ADC auto gain (on/off without a stall), sample rate change restarts the stream

## 5. Receive DSP (M5)

- [ ] NR2, NR4, NNR: sound right on the HL2+ speaker and on the phone (PHONE on)
- [ ] NB / NB2 on impulse noise; ANF on a carrier; SNB
- [ ] Notch: double-tap places it, drag moves it, the red handles change its width, push up removes it
- [x] Squelch line: drag it, it opens and closes (SSB, AM, FM)
- [ ] EQ graph: drag points, hear the change
- [ ] Binaural with headphones; CW audio peak filter in CW
- [ ] Per mode: NR2 on in SSB, off in CW, comes back when returning to SSB

## 6. Display (M5)

- [ ] LEVELS chip opens the side panel; HIGH / LOW move the spectrum and waterfall live; Automatic back
- [ ] Display tabs: Look (3D looks, colours, split), Peaks (labels), Averaging (4 kinds, peak hold, colours), Info bar, Speed
- [ ] ☰ → Screen: FFT size, FFT window, save zoom, narrow peaks, show/hide each part
- [ ] 2D background pictures: pick one, change strength, back to None
- [ ] ADC overload: a strong signal or too much LNA shows the red ADC OVERLOAD label

## 7. Demo (IQ recording)

- [ ] ☰ → Discovery → Demo band: 20 m SSB readable in **USB** (I/Q fix), on the phone
- [ ] Tune around inside the recording; NR2 and the notch work on it
- [ ] Stop demo, Start radio: the radio comes up on its own frequency and settings, PHONE as before

## 8. Headless test (optional)

- [ ] `android/native/run-m1.sh -a 169.254.19.221 -t 20`: same dBm as the app's meter

## Results 2026-11-25 (Randal, evening)

Randal: "the majority of everything works, I don't really have any
complaint". Explicitly checked: start, replug (comes back, but slowly),
60 m CA channels, squelch, RIT, the bottom row (toolbar). 40 m was dead
that evening (another radio heard nothing either), so receive checks used
what was on. Gateware flashing: not tested, later (Randal: no need now).

## Fix list (found during the test)

1. **60 m band bar wrong:** BandPlan.kt only has the WRC-15 band
   (5351.5-5366.5 kHz), so in Region 2 the Canadian/US channels (5332,
   5348, 5373, 5405) show "OUT OF BAND" and the segment label doesn't fit
   the channels. Add the Region 2 60 m channels to the band plan.
2. **RIT step does nothing:** RIT drag rounds to a fixed 10 Hz
   (Screen.kt, edge 3); it should snap to vfo rit_step (1/10/100 Hz).
   Randal asked what the step is for: it is the RIT increment.
3. **Bottom row overflow:** too many chips stack up at the right end and
   look wrong: the row should scroll (or wrap) instead.
4. **Toolbar editor:** a shadow/outline doesn't line up while choosing
   chips (the dragged/selected chip's background vs its border).
5. **Replug is slow to come back** (about 15 s unplugged, then a while):
   check where the time goes (Android's network up, the 1.5 s replug
   delay, discovery, watchdog) and shorten it if it's ours.
