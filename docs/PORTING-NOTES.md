# Porting notes — M1 Box 2 survey

Measured 2026-11-23 against upstream `9f49c21` (deskHPSDR, 2026-11-16, WDSP
2.11). Method, so it can be repeated after an upstream update: compile every
`src/*.c` on the desktop with the Makefile's defines (`-DAUDIO_RINGBUFFER
-DMINIAUDIO -Iminiaudio`, WebKit flags for `radio.c`), build a symbol → file
map from `nm --defined-only`, then read each file's `nm -u` against it.
105 of 111 files compile that way; the 6 that don't (`ozyio`, `tci`,
`rtmidi_midi`, `udplistener`, `bootldrsim`, `version`) are not needed.

## Headline

**The Protocol 1 code makes no GTK calls.** `old_protocol.c` (3579 lines)
uses three GLib functions (`g_thread_new`, `g_malloc_n`, `g_idle_add`) and
otherwise talks to the rest of the program through ~90 global radio settings
and a handful of sample hand-off functions. The HL2 IO board (I2C 0x1D, Pico
LPF detection, tuner) and the HL2+ AK4951 codec handling are both *inside*
`old_protocol.c`, so keeping that file unchanged brings both for free.

## GUI coupling per file

Calls into GTK/GDK/Cairo/WebKit, and distinct GLib functions used:

| File | Lines | GUI calls | GLib fns | M1 plan |
|---|---:|---:|---:|---|
| `old_protocol.c` | 3579 | 0 | 3 | **compile unchanged** |
| `old_discovery.c` | 683 | 0 | 7 | **compile unchanged** |
| `discovered.c` | 27 | 0 | 0 | **compile unchanged** |
| `message.c` | 118 | 0 | 3 | **compile unchanged** (`t_print` → logcat later) |
| `nw_toolset.c` | 185 | 0 | 0 | **compile unchanged** |
| `band.c` | 937 | 0 | 1 | **compile unchanged** |
| `mode.c` | 36 | 0 | 0 | **compile unchanged** |
| `filter.c` | 801 | 0 | 1 | **compile unchanged** |
| `iambic.c` | 626 | 0 | 1 | later (CW, M5) |
| `receiver.c` | 2630 | 10 | 16 | **compile unchanged + display stubs** |
| `transmitter.c` | 3099 | 56 | 17 | stub for M1; real one in M5 |
| `vfo.c` | 2994 | 32 | 3 | stub for M1 (`vfo[]` + 3 functions) |
| `radio.c` | 3711 | 56 | 29 | **replace for M1** with a headless state file |
| `discovery.c` | 797 | 52 | 9 | not needed (the discovery *dialog*) |
| `main.c` | 1158 | 57 | 21 | not needed (new headless main) |
| `buffered_audio.c` | 1373 | 0 | 11 | not needed (local PC audio; HL2+ path skips it) |
| `rigctl.c` | 6350 | 0 | 26 | later (CAT for digital-mode apps) |

## What `old_protocol.c` needs from other files

From `nm -u` against the symbol map:

- **radio.c** — ~90 globals: `device`, `radio`, `receiver[]`, `receivers`,
  `active_receiver`, `transmitter`, `adc[]`, `n_adc`, `filter_board`,
  `hl2_audio_codec`, `force_iob`, `enable_hl2_atu_gateware`, `hl2_cl1_input`,
  `duplex`, `tune`, the CW keyer settings, PA / Alex / Atlas settings, and
  functions `radio_is_transmitting`, `radio_ptt`, `radio_set_hardware_tx_inhibit`,
  `radio_start_auto_tune`, `sequence_error_add`, `apply_ppm_ll`.
- **receiver.c** — `rx_add_iq_samples`, `rx_add_div_iq_samples`, `rx_get_mode_dc_offset`.
- **transmitter.c** — `tx_add_mic_sample`, `tx_add_ps_iq_samples`.
- **vfo.c** — `vfo[]`, `vfo_get_tx_mode`, `vfo_get_tx_vfo`.
- **ext.c** — `ext_vfo_update`, `ext_mox_update` (only as `g_idle_add` targets).
- **main.c** — `fatal_error` (only as a `g_idle_add` target).
- small ones: `band_get_band` (band.c), `nw_settings` (nw_toolset.c),
  `keyer_event` (iambic.c), `tx_off_cancel` (tx_off.c), `tci_audio_tx_enabled`
  (tci_audio.c), `rigctl_debug` (rigctl.c), `t_print`/`t_perror` (message.c),
  `audio_get_next_mic_sample`/`audio_open_input`/`audio_reset_mic_buffer`
  (buffered_audio.c — only used when `transmitter->local_microphone` is set).

## Plan questions from M1-PLAN Box 2

1. **Which files does M1 need?** Compile unchanged: `old_protocol.c`,
   `old_discovery.c`, `discovered.c`, `message.c`, `nw_toolset.c`, `band.c`,
   `mode.c`, `filter.c`, `receiver.c`. New, droidHPSDR-only: a headless
   radio-state file (the ~90 globals with radio.c's defaults, plus the few
   radio functions), stubs for transmitter/vfo/ext/display/menu calls, and the
   headless `main`.
2. **The `g_idle_add` sites.** 12 in `old_protocol.c`; targets are
   `ext_vfo_update`, `ext_mox_update`, `fatal_error`. All are "tell the UI"
   notifications. The compat `g_idle_add` queues the callback onto our own
   event queue (logging stub in M1, the JNI bridge to the Compose UI later).
3. **GLib-without-GTK for Android?** Not needed. The headers use GTK, GDK and
   Cairo types only as pointers or struct members (`GtkWidget *`,
   `GdkPixbuf *`, `cairo_surface_t *`), and GLib as `GMutex`, `GThread` and a
   few helpers. A small compat header set (`android/compat/gtk/gtk.h`,
   `glib.h`, `gdk/gdk.h`) with opaque types and pthread-backed
   `g_mutex_*`/`g_thread_*` lets the unchanged upstream files compile. That
   is Box 3 option B, and it avoids adding GLib as a dependency.
   Every file in a build must see the same compat definitions, since `GMutex`
   is embedded by value in the RECEIVER/TRANSMITTER structs.
4. **Path from an EP6 frame to the S-meter.**
   `old_protocol.c` receive thread → `process_ozy_byte()` (line ~1898)
   unpacks I/Q → `rx_add_iq_samples()` (receiver.c:1677) fills
   `iq_input_buffer` → at `buffer_size` samples `rx_full_buffer()`
   (receiver.c:1609) runs noise blankers, `fexchange0()` and `Spectrum0()` →
   `rx_process_buffer()` (receiver.c:1502) hands the audio to
   `old_protocol_audio_samples()`. The S-meter is
   `rx_get_smeter()` (receiver.c:1909) = `GetRXAMeter(rx->id, RXA_S_AV)`
   (or `RXA_S_PK`), polled by the GUI. Headless main polls the same call.

## Upstream merges

Files compiled unchanged can take upstream fixes by cherry-pick with no
conflict risk from us. Everything droidHPSDR-specific lives outside `src/`
(compat headers, state file, stubs, main), so a `src/` cherry-pick only breaks
the build when upstream adds a new global or call that the stubs lack, which
the linker reports by name.
