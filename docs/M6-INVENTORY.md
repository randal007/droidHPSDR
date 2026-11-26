# M6 inventory: hardware (HL2, IO board, HL2+, PA)

Step 1 of M6 (ROADMAP "How each section is done"): every control in
FEATURES.md's M6 part (Radio, ANT, OC, PA, XVTR menus), plus the IO board,
HL2+ and HL2 setup code that has no menu line there, and what each really
does on an HL2. Read from upstream (`src/`, = upstream HEAD 9f49c21) on
2026-11-25.

**No transmit until Randal asks** still holds: everything that keys the
radio (tune, PA, ATU) is listed with its milestone, not built here.

## Found while reading: the filter board wasn't switching (fixed)

deskHPSDR's `start_radio()` calls `n2adr_oc_settings()` for every HL2: it
sets each band's OC bits, which pick the HL2's N2ADR low-pass filter (and
on 80-10 m the 3 MHz high-pass) on receive and transmit. droidHPSDR set
`filter_board = N2ADR` but never called it, so the OC bits were 0 on every
band. Fixed 2026-11-25 (`android/native/headless/n2adr.c`, copied from
radio_menu.c with attribution; called after the props file is loaded, like
radio.c). **Radio check:** the HL2's relays should click when changing
band (160 → 80 → 40 → 20 → 15 → 10), as with deskHPSDR.

## What already works (no UI needed)

| Part | State |
|---|---|
| HL2+ speaker audio | M1, by ear (the codec bit is forced, see HARDWARE.md) |
| IO board detection | old_protocol.c probes 0x1D on I2C2 and a Pico on both buses |
| IO board band outputs | the TX/RX frequency codes go out every round; Randal saw the amplifier switch bands (M1) |
| Phone audio | M3/M5 (PHONE chip) |

## IO board (N2ADR) and the Pico controller

| Control | What it does on the HL2 | Proposed |
|---|---|---|
| IO status ("IO: N2ADR Board" / "IO: Pico or SoC" / "IO: none") | deskHPSDR shows it in the VFO bar (vfo.c) | **build**: a status line (where: pictures) |
| HL2 Force IO Board | skips the detection on 0x41, always talks to the board | **build** (SDR Device / Radio page) |
| Pico LPF status ("LPF 40m", rx_panadapter.c) | only with a Pico LPF/XPA controller (repo: `hl2-xpa125b-controller/`) | **build** the label; shows only when a Pico answers |
| RX antenna per band (ANT menu, `alex_antenna`) | on an HL2 with IO board: any RX antenna other than ANT1 sets the board's "RF mode" 1 (separate RX input; 2 with PureSignal) | **build**: per-band RX antenna; TX antenna waits for M7 |
| Tuner: TUNE_IOB, AH-4 bypass, AH-4 state | keys the radio (TUNE) and drives the tuner over the board | **M7** (transmit). AH-4 support needs the `__AH4IOB__` build flag (off in our build) |
| "HL2 ATU TUNE support" (gateware ATU) | gateware-side tune | **M7** |

## HL2+ (AK4951 codec)

| Control | What it does | Proposed |
|---|---|---|
| Audio codec: none / HL2+ (AK4951) / SQUARE SDR 2 | AK4951 forces the dither bit so the gateware knows the codec is there; "none" zeroes the audio bytes | today hard-wired to AK4951. **build** the choice (none / HL2+), SQUARE SDR 2 not shown (not our hardware) |
| Speaker volume | done (AF chip) | — |
| Mic / line in, mic gain, PTT from the mic jack | the codec's input side | **M7** |

deskHPSDR has nothing else for the AK4951: the rest is in the gateware.

## Radio menu (`radio_menu.c`): the HL2's lines

| Control | On an HL2 | Proposed |
|---|---|---|
| Filter board: N2ADR (LPF RX+TX+HPF) / N2ADR (LPF TX only) / none | which OC bits the bands get; "TX only" leaves RX unfiltered, with an optional 3 MHz HPF ("+RX: N2ADR HPF 3MHz") | **build** the three choices (default N2ADR RX+TX+HPF, as now) |
| HL2 CL1 10 MHz ref clock | locks the HL2 to a 10 MHz GPSDO on CL1 (receive accuracy too) | **build** if Randal has one (question 3) |
| Freq. calibration (ppm) | corrects the HL2's clock: every frequency shifts | **build** |
| ADC gain calibration (dB) | `rx_gain_calibration`: moves the S-meter and spectrum dBm | **build** |
| RIT/XIT step | 1 / 10 / 100 Hz | **build** (VFO panel) |
| Split operation, Disable split on band change | VFO | **build** (VFO panel; split itself is M4) |
| IARU region, 60 m channel markers, beacon areas, contest-free areas | display markers on the spectrum | region done (SDR Device); **build** the markers (pictures) |
| Your callsign, your locator | used by the info bar / solar / spots | **M8** |
| HL2 5W PA enable, Duplex, Mute RX when TX, TxInhibit / AutoTune inputs | transmit | **M7** |
| Receivers 1/2, SAT / RSAT | RX2 | **later** (one receiver) |
| Audio capture time | record / play back (TX side) | **M7** |
| ATLAS bus, 10 MHz / 122.88 MHz source, mic source, TX config, Janus, ORION mic jack, PTT/BIAS, mic input, Mute Spkr Amp, Hermes mode (ANAN-10E / Brick), VFO encoder divisor | other radios / a knob | **not shown on an HL2** (deskHPSDR hides most of them for an HL2 too) |

## OC menu (`oc_menu.c`)

| Control | On an HL2 | Proposed |
|---|---|---|
| OC bits per band, RX and TX | on the HL2 these are the filter board's relays (set by the filter board choice above; changing them by hand can leave a band unfiltered) | **build** as an "expert" page, read-only unless unlocked? (question 4) |
| Tune (ORed with TX), Full / Memory Tune (ms) | outputs while tuning | **M7** |

## PA menu (`pa_menu.c`)

Per-band PA calibration (HL2: 38.8 for full 5 W), MAX power, transmit out
of band, watt meter calibration, save / import: **all M7**. Nothing here
does anything on receive.

## XVTR menu (`xvtr_menu.c`)

Transverter bands: title, min/max frequency, LO frequency and error, gain,
disable PA. Receive works on any radio (it's a frequency offset and its own
band), transmit is M7. **Build** the receive side if Randal uses a
transverter (question 5), otherwise **later**.

## HL2 setup (`p2_programmer.c`, deskHPSDR's P1 Setup)

| Action | What it does | Proposed |
|---|---|---|
| Gateware update (RBF file) | erases and re-flashes the HL2's FPGA over the network | **later** or never on a phone (question 6): a dropped WiFi packet mid-flash means a JTAG recovery |
| Static IP in EEPROM | **CLAUDE.md: never** (disables DHCP, breaks the WiFi path) | **not doing** (needs Randal's OK as a "not doing" line) |
| DHCP preference, alternate MAC, reboot | EEPROM writes, then a reboot | reboot + rediscover: **build**; the EEPROM writes: question 6 |

## Randal's answers (2026-11-25)

1. IO board status in the **SDR Device sheet**, **auto-detected** with an
   override (= upstream's detection plus "HL2 Force IO Board").
2. Wanted more detail on the HL2 page: explained, waiting on his pick.
3. **No GPSDO**: CL1 goes on the later list.
4. **No OC editing**: the filter board choice is enough.
5. **No transverter now, but wanted later**: XVTR on the later list.
6. HL2 setup **over Ethernet only, with warnings** (gateware update,
   DHCP preference, MAC, reboot). Static IP stays out unless he says so.
7. **IO board with the custom XPA125B firmware on its Pico** (the
   `hl2-xpa125b-controller` uf2): upstream sees it as "IO: Pico or SoC",
   shows the Pico's LPF status, and TUNE drives the XPA125B's tuner through
   it (`hl2_iob_set_antenna_tuner`): that part is M7.

**Picks (2026-11-25):** settings way **B** (pictures F1, F2 in
`tools/mockups/layouts8.js`); E2 scale fix **B** (widens while held, F4).
**Built the same day** (`Hl2.kt`, `android/native/app/hl2.c`): SDR Device
sheet with the HL2 section (IO board status from upstream's detection, Force
IO board, HL2+ codec, filter board RX+TX+HPF / TX only + 3 MHz HPF via
upstream's `n2adr_oc_settings[_tx]`, frequency calibration with upstream's
real `apply_ppm_ll` instead of the old stub, ADC gain calibration); HL2
setup page (Ethernet only, red warning, each action asks first and stops
the radio): reboot then start again after 15 s, prefer DHCP, alternate MAC.
**Gateware flashing not built yet**: its own step, tested with Randal.
Not run on the radio yet.

## Questions for Randal (asked 2026-11-25)

1. **Where the IO board status goes:** a small line in the info bar gaps
   (E8), in the SDR Device sheet only, or both? (pictures)
2. **An HL2 page:** put Force IO board, codec choice, filter board,
   CL1, ppm and ADC calibration together on one "HL2" page under SDR
   Device (deskHPSDR spreads them over the Radio menu)? (pictures)
3. Do you have a 10 MHz GPSDO on the HL2's CL1 input?
4. OC bits by hand: needed, or is the filter board choice enough?
5. Do you use a transverter with the HL2?
6. HL2 setup from the phone: reboot + rediscover only, or also gateware
   updates / EEPROM (DHCP preference, MAC)? Static IP stays "not doing".
7. Do you use an AH-4 (or the Pico XPA125B controller) with the HL2, or is
   the tuner your ATU-100? It decides how much of the tuner part M7 needs.
