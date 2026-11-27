# Hardware notes — HL2, HL2+ codec, IO board

How deskHPSDR (upstream `9f49c21`) drives the hardware droidHPSDR must
support. All of it is in `src/old_protocol.c` (Protocol 1), which droidHPSDR
compiles unchanged, so this is a map for debugging, not a to-do list.

## The radio on this bench

Hermes Lite 2, gateware 75.0, MAC `00:1c:c0:a2:13:dd`, HL2+ companion board
(AK4951 codec) and N2ADR IO board both installed. Direct to the phone over USB
Ethernet it sits at APIPA `169.254.19.221` (see CLAUDE.md "Verified on the S26
Ultra").

## HL2+ audio codec (AK4951)

Setting: `hl2_audio_codec` (radio.h: `HL2_CODEC_OFF` 0, `HL2_CODEC_AK4951` 1,
`HL2_CODEC_SQUARESDR2` 2), chosen in deskHPSDR's Radio menu.

- **Speaker / phones (EP2, PC → radio).** `rx_process_buffer()` (receiver.c)
  calls `old_protocol_audio_samples(left, right)` for the active receiver.
  That writes 16-bit L/R into the TX ring (old_protocol.c ~2139). On a plain
  HL2 the audio bytes are forced to zero, because the stock gateware reuses
  them for extended register writes; with `hl2_audio_codec` set they carry
  real audio to the codec.
- **"Codec present" bit.** With `HL2_CODEC_AK4951`, the C3 dither bit
  (`LT2208_DITHER_ON`) is forced on in every frame (old_protocol.c ~2524).
  The HL2+ gateware reads it as "codec present". On a SQUARE SDR 2 the same
  bit means "speaker on", so it is *not* forced there.
- **Mic (EP6, radio → PC).** Mic samples arrive in the receive frames,
  decimated to 48 kHz (old_protocol.c ~2023) and passed to
  `tx_add_mic_sample()`. Unless `transmitter->local_microphone` is set, the
  radio's own mic is used and Android audio is never involved.
- **Mic jack (V4.x board):** 3.5 mm TRS, mic on the tip, PTT ring to
  sleeve (a mono plug = continuous TX). Analog mic amp fixed at 18 dB;
  deskHPSDR's Mic Boost bit = +20.25 dB in the codec's digital block. The
  "Line In" bit is **not** a line input: since gateware 74p2 it switches
  the HL2+'s AH-4 ATU support. Details and sources: docs/M7-INVENTORY.md.
- Traps from the Zeus HL2+ work (station-engine, 2026-09): the FPGA CW keyer
  only arms in a CW mode, and the paddle goes in the KEY jack, not MIC.
- **CW (read 2026-11-27, not yet checked on the radio):** the stock HL2 has
  no keyer (the KEY tip is a straight key, the gateware shapes the
  carrier); the HL2+ gateware adds an iambic keyer and "electronic bug"
  mode, with sidetone on PHONES and the speaker. old_protocol.c arms it
  with 0x1E C1 bit 0 (HL2 register 0x0f bit 24) in CW modes and sends
  speed / mode / weight / spacing / reversed (0x16), sidetone level and
  PTT delay (0x1E), hang time and sidetone frequency (0x20). Only the hang
  time is in the HL2 wiki; which of the rest the HL2+ follows is a radio
  check. The radio reports PTT (held through the hang time) and the dot
  bit; old_protocol.c keeps dot/dash in statics. droidHPSDR's gate: see
  CLAUDE.md's CW trap and `android/native/app/cw.c`.

## N2ADR IO board

All in old_protocol.c's HL2 command round-robin (~3040-3200), sent in spare
C0-C4 slots so it doesn't disturb normal traffic:

- **Detection:** PCA9536 / IO-board probe on I2C2 (address `0x1D`, register
  0x41), plus a separate Pico LPF-controller probe on both buses.
  `force_iob` skips detection. `hl2_iob_is_present()` /
  `hl2_pico_is_present()` report the result.
- **Per-slot traffic once detected:** TX / RX1 / RX2 frequency codes (coarse,
  so RIT/offset are ignored), antenna selection (`alex_antenna`, PureSignal
  aware), RF input mode, Reg7 fast-poll for tuner status.
- **Tuner:** `hl2_iob_set_antenna_tuner()` and the TUNE_IOB action; AH-4
  support behind `__AH4IOB__`.

droidHPSDR needs only the right settings in the headless radio state (for
example `hl2_audio_codec = HL2_CODEC_AK4951`, filter board) and a UI for the
IO board status. The wire protocol comes with the unchanged file.

## Found on the air in M7 (2026-11-25)

- **The HL2 unkeys by itself when the host goes away**: force-stopping the app
  or pulling the USB Ethernet adapter while transmitting ends the
  transmission (Randal, HL2 → XPA125B → antenna). old_protocol.c sends 20 ms
  PTT hang and 40 ms TX latency (12 ms in CW) in register 0x2E.
- TUNE with the Pico (XPA125B controller) present also writes IO board
  register 7 = 1 (deskHPSDR's TUNE); TUNE itself was fine on the air.
- The HL2's own power meter reads the HL2's output (the XPA125B's input):
  3.2 W at tune drive 18 %; SSB reads low on it (averaged).

## Found while building M6 (2026-11-25)

- **Filter board OC bits.** On an HL2 the N2ADR filter board's relays
  follow the OC bits deskHPSDR sets per band (`n2adr_oc_settings()`: 160 m
  1, 80 m 66, 60/40 m 68, 30/20 m 72, 17/15 m 80, 12/10 m 96; "TX only"
  mode puts the 3 MHz HPF bit 64 on RX). droidHPSDR sent 0 until
  2026-11-25. Radio check: relays click on band changes.
- **Randal's IO board runs the custom XPA125B firmware on its Pico**
  (`hl2-xpa125b-controller/` in the repo). Upstream reports it as
  "IO: Pico or SoC", shows the Pico's LPF status, and TUNE drives the
  XPA125B's tuner through `hl2_iob_set_antenna_tuner()` (M7).
- **RX antenna.** Any band RX antenna but ANT1 (`alexRxAntenna`) sets the
  IO board's RF mode 1 (its separate RX input); 2 with PureSignal. The ANT
  menu offers ANT1 or "RX in" (EXT1).
- **HL2 setup** talks to UDP 1025 (EEPROM register 6 flags: 0x20 prefer
  DHCP, 0x40 alternate MAC in 12-13, 0x80 static IP in 8-11). Static IP is
  not offered (breaks WiFi discovery).
