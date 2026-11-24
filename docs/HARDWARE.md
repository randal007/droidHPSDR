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
- Traps from the Zeus HL2+ work (station-engine, 2026-09): the FPGA CW keyer
  only arms in a CW mode, and the paddle goes in the KEY jack, not MIC.

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
