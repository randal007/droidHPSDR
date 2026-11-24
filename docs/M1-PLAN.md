# M1 — headless core: timeboxed plan

**M1 goal (from CLAUDE.md):** cross-compile WDSP, FFTW3 and the protocol layer
for arm64. Discover, stream, print S-meter values to logcat. No UI.

**M1 acceptance:** S-meter values in logcat visibly track a real signal — they
move when an antenna is connected and disconnected, and change with band.

---

## Progress

**2026-11-23**
- Fork rebased onto upstream deskHPSDR `9f49c21` (2026-11-16, 224 commits newer,
  WDSP 2.11). `src/` identical to upstream. Pre-rebase state kept on branch
  `backup/pre-rebase-2026-11-23`. Not yet pushed (needs a force-push).
- **Boxes 4 and 5 done, with NR3/NR4 included instead of excluded:**
  `android/deps/build-deps.sh` cross-compiles FFTW 3.3.11 (double + float,
  NEON on both), DL1BZ's rnnoise and libspecbleach (from git.bzsax.de, same
  as upstream's `update_libs.sh`), and WDSP 2.11 (83 objects, zero warnings,
  out of tree, `wdsp-2.11/` untouched) into `build-android/arm64-v8a/`. Full
  build takes about 7 min, most of it the 56 MB rnnoise model download.
- Box 5's test program is `android/deps/wdsp-smoke.c` (`build-deps.sh smoke`).
  Links with no missing symbols; needs only libc/libm/libdl. It checks USB
  demod, LSB rejection, S-meter tracking (+-20 dB), NR2/NR3/NR4/NNR and speed.
  **All pass on the desktop and on the phone** (S26 Ultra SM-S948W, Android 16,
  Snapdragon SM8850, 8 cores; run 2026-11-23 over adb from /data/local/tmp).
  Phone numbers: FFTW wisdom computed in **11.4 s** (vs ~7.5 min on the x86
  desktop, which has far more SIMD variants to time); plain USB 53x real time,
  NR2 41x, NR3 55x, NR4 152x, NNR 6.5x. Demod, LSB rejection and S-meter
  results are identical to the desktop's.
- Learned from the desktop run:
  - **FFTW wisdom took ~7.5 min** on this desktop but 11.4 s on the phone
    (cached afterwards in the given directory). A short first-start
    "preparing DSP" moment is enough; no need to ship pre-computed wisdom.
  - WDSP's I/Q sign: a tone above the VFO is `(cos, -sin)`. `(cos, +sin)`
    lands on the LSB side.
  - rnnoise (NR3) silences a steady pure tone; NNR cuts it ~25 dB. Expected
    for speech denoisers, so the test only checks they run.
  - **M0 done over USB Ethernet** (the radio direct to the phone, APIPA): see
    CLAUDE.md "Verified on the S26 Ultra". Main lesson: sockets must be bound
    to the Ethernet Network, or unicast to the radio goes out over WiFi.
  - Speed: plain USB ~45x real time on the desktop, 53x on the phone; NNR is the
    heaviest (~7x desktop, 6.5x phone, single-threaded) and still fine.

**2026-11-23, later: Box 2 done.** `docs/PORTING-NOTES.md` and
`docs/HARDWARE.md`. `old_protocol.c` makes no GTK calls (3 GLib functions);
`receiver.c` only 10, all display. The IO board and HL2+ codec logic are inside
`old_protocol.c`. Box 3 evidence points to option B: a small compat header
set (opaque GTK/Cairo types, pthread-backed GMutex/GThread, `g_idle_add` →
own queue) so the upstream files compile unchanged; no GLib port. `radio.c`
(56 GUI calls + WebKit) is replaced for M1 by a headless state file.

**2026-11-23, night: M1 receive working on the phone (Boxes 3, 6, 8; Box 7
skipped, see below).**
- Box 3 decided: option B. `android/native/compat/` holds stand-in
  `glib.h`, `gtk/gtk.h`, `gdk/gdk.h`, `cairo.h` + `glib_compat.c` (pthreads,
  a callback queue for `g_idle_add`, no-op widget calls). No GLib port.
- Box 6: `android/native/CMakeLists.txt` (NDK toolchain). Upstream
  `old_protocol.c old_discovery.c discovered.c message.c nw_toolset.c band.c
  mode.c filter.c receiver.c` compile **unchanged**, zero warnings.
  `headless/radio_state.c` is generated from upstream definitions (defaults
  kept; RX-ONLY overrides marked), `headless/stubs.c` covers the rest.
- Box 7 (moving files into `core/`) is no longer needed: upstream files stay
  in `src/` untouched and everything droidHPSDR-specific lives in
  `android/native/`, which keeps cherry-picks trivial.
- Box 8: `headless/m1_main.c` → `m1-rx`, run with `android/native/run-m1.sh`
  (looks up the Ethernet network handle, binds with
  `android_setprocnetwork`). **RECEIVE ONLY**: `radio_is_transmitting()` is
  a stub returning 0 (checked in the disassembly), the radio's CW keyer and
  PA bits stay off, drive level 0, no WDSP TX channel.
- Result over USB Ethernet, 23:00 local: streams with 0 lost packets, clean
  stop. S-meter follows the bands: 40 m FT8 -76..-84 dBm (moving with FT8
  slots), 30 m -80, 10 m -97 (dead at night). **"HL2 IO board detected on
  I2C2"** from the unchanged protocol code. HL2+ codec bit set; whether audio
  reached the HL2+ speaker is still to be confirmed by ear.
- Bug found on the way: `old_protocol.c` reads `receiver[1]` unconditionally,
  so (like radio.c) both receivers must exist even when only one is used.
- 60 s on 40 m FT8: the S-meter rises ~9 dB for exactly 12.5 s every 30 s
  (11.0-23.6 s, 41.1-53.6 s), i.e. the FT8 transmissions of one slot parity.
  Randal saw the amplifier switch bands with each run (IO board band
  outputs work) and **heard the FT8 audio clearly on the HL2+ speaker**.
- **M1 accepted (Randal, 2026-11-23): "it's working perfectly fine".** The
  antenna off/on step was dropped as unnecessary given the above.
- **M2, speaker half: done** by the same run (demodulated audio in the EP2
  slots reaches the HL2+ codec). Mic half (EP6 mic samples) is left for the
  TX milestone; no TX until Randal asks for it.

## What a "box" is

One box = **one session, one branch, one commit, one testable outcome.**

Sized so it fits in a single context window without compaction. If a box needs
more than roughly a dozen source files read, it is two boxes.

Every box below states its **done** condition as something observable, not as
"the code is written". A box that cannot be demonstrated is not finished, and
the next box does not start.

Boxes 0–2 are deliberately cheap and de-risking. They buy information before any
restructuring commitment is made.

---

## Survey findings (already done, 2026-09-07)

Measured against the fork at `dcf7832`. These numbers change the plan's shape,
so they are recorded here rather than rediscovered:

| Fact | Number | Why it matters |
|---|---|---|
| `src/*.c` + `*.h` | 219 files, ~95k LOC | Full port is out of scope; M1 needs a subset |
| Files with no GTK reference | 79 of 219 | Larger clean surface than expected |
| `src/old_protocol.c` | 3470 lines, **13** GTK hits | P1 framing is effectively clean |
| — of those 13 | 1 × `#include <gtk/gtk.h>`, 12 × `g_idle_add` | All are UI *notifications*, not UI *logic* |
| `src/old_discovery.c` | 621 lines, **1** real hit | Just the include; logic is portable as-is |
| `src/discovered.c/.h` | 147 lines, GTK-free | Radio-descriptor struct usable unchanged |
| `src/discovery.c` | 752 lines, 118 `gtk_` calls | This is the discovery **dialog** — not needed for M1 |
| `src/radio.c` | 3557 lines, 170 hits | Heavily coupled. The real work of the port |
| WDSP external deps | pthread, semaphore, math, stdio | All present in the NDK |
| WDSP optional dep | `libspecbleach` in `sbnr.c` only (NR4) | Single file — excludable from the first build |
| FFTW | vendored `build-fftw.sh`, needs `libfftw3.a` **and** `libfftw3f.a` | Double *and* float, both required |
| Makefile | 1214 lines, hard-requires WebKitGTK | Desktop-only; confirms CMake is a named task |

**The headline:** the P1 protocol and discovery path is far cleaner than
`radio.c` suggests. M1 can reach its acceptance test while touching very little
GTK-coupled code, provided `radio.c` is bypassed rather than ported.

---

## Box 0 — toolchain on the desk

No repository code. Purely the build host and phone.

- Install Android NDK (r27 or later) and `platform-tools` for `adb`.
  `cmake` and `ninja` are already present on this box; the NDK and `adb` are not.
- Enable developer mode + USB debugging on the S26 Ultra, confirm `adb devices`.
- Cross-compile a hello-world C file with `aarch64-linux-android31-clang`,
  `adb push` to `/data/local/tmp`, run it.

**Done:** an arm64 binary built on the desktop prints to the phone's shell.

**Not in this box:** anything from `src/`.

---

## Box 1 — `tools/m0-probe`

The highest value-per-line box in the whole milestone. Proves toolchain,
transport, and the phone's network configuration in one shot, before any of the
expensive cross-compiles.

One C file. No WDSP, no GLib, no UI. Target well under 300 lines.

- UDP socket, Protocol 1 discovery broadcast to `255.255.255.255:1024`, print
  every reply with MAC, board ID and firmware version.
- `-u <ip>` unicast mode for the wired path, defaulting to `169.254.19.221`.
- **Bind to the chosen interface explicitly.** With cellular up, the default
  route is not the one you want — this is the trap the probe exists to catch.
- Crib the packet layout from `src/old_discovery.c`; do not include it.

**Done — this is also the M0 acceptance test:** a discovery reply from the radio
over WiFi. (The USB Ethernet half was dropped 2026-11-23: Randal already uses
the adapter with this radio; the app always allows typing the IP.)

**Do not write:** any link-local range scan. With no address on `eth0` there is
no route and the scan cannot leave the phone. It is not a workaround for a
missing local address.

---

## Box 2 — porting survey → `docs/PORTING-NOTES.md`

Reading only. **No file is moved or edited in this box.** CLAUDE.md is explicit
that nothing gets removed until this document exists.

Start from the survey table above rather than re-measuring. Questions to close:

1. Which of the 79 GTK-free files does M1 actually need? Produce the list.
2. What do the 12 `g_idle_add` sites in `old_protocol.c` actually notify —
   `ext_vfo_update`, `ext_mox_update`, `fatal_error`. Can all three be a
   function-pointer callback the headless build sets to a logging stub?
3. Does GLib-without-GTK cross-compile for Android, and is it even needed if
   the answer to (2) is yes?
4. Where does the S-meter value originate, and what is the shortest path from
   an EP6 frame to that number?

**Ride-along (same reading pass, so it costs almost nothing extra):** CLAUDE.md
first-task 3 — locate the LOCAL/RADIO audio routing and the EP2/EP6 audio slot
handling, and write `docs/HARDWARE.md`. It is M2 material, but it is the same
files and the same context load. Doing it now saves a whole session later.

**Done:** both documents committed. Still zero changes under `src/`.

---

## Box 3 — the GLib decision

Close question (3) from Box 2 with a build, not an opinion. Two candidates:

- **A — keep GLib, drop the widget layer.** CLAUDE.md says to try this first.
  Cost: cross-compiling GLib for Android.
- **B — a ~50-line event queue.** Replace `g_idle_add` with a callback the
  headless build points at a logging stub. Given only 12 call sites in the
  protocol layer, this may be cheaper than building GLib at all.

Note the survey favours B more strongly than the original assumption did. Decide
on evidence and record the reasoning in `PORTING-NOTES.md`.

**Done:** decision written down, with whichever proof actually compiles.

---

## Box 4 — FFTW3 for arm64

Adapt the existing `build-fftw.sh`. Both precisions — the Makefile wants
`libfftw3.a` and `libfftw3f.a`, and WDSP uses both.

**Done:** two static libraries, `file` reporting `ARM aarch64`, in a build
directory the CMake in Box 6 can find.

---

## Box 5 — WDSP for arm64

`wdsp-2.10`. Exclude `sbnr.c` and the NR4 path for now — it is the only file
pulling `libspecbleach`, and cross-compiling that library is not on M1's
critical path. Record the exclusion; it is a deliberate, reversible decision.

**Done:** `libwdsp.a` for arm64-v8a, plus a throwaway test that opens and closes
a WDSP channel on the phone and exits 0. Proves the library links and its
pthread/semaphore usage survives Bionic.

---

## Box 6 — CMake for the M1 subset

The named task from CLAUDE.md. **Convert only what M1 needs**, not the 1214-line
Makefile — that Makefile hard-requires WebKitGTK and will never cross-compile.

Input is the file list from Box 2, question 1.

**Done:** `cmake --build` on the desktop produces an arm64 executable linking
WDSP and FFTW, which runs on the phone without crashing.

---

## Box 7 — `core/` restructure, moves only

Placed here deliberately: after Boxes 2–6 have established which files are
actually needed, and before the new code grows around the old layout.

- `git mv` into `core/protocol`, `core/radio`, `core/transport`, `core/dsp`
  per the layout in CLAUDE.md.
- **Moves and edits in separate commits.** A move plus a rewrite in one commit
  destroys the ability to follow either.
- No reformatting. Ever. A style pass makes every future cherry-pick conflict.
- Preserve per-file GPL copyright headers.

**Done:** tree matches the CLAUDE.md layout, Box 6's build still succeeds,
`git log --follow` still works on a moved file.

---

## Box 8 — headless main, S-meter to logcat

The payoff box. Likely splits into 8a (stream running) and 8b (S-meter correct)
— split it the moment it stops fitting one context window.

- Discovery → start the P1 stream → feed EP6 samples to WDSP → read the S-meter
  → `__android_log_print`.
- Bypass `radio.c`. Construct the minimum receiver state directly. Porting
  `radio.c`'s 3557 lines and 170 GTK hits is M3/M4 work, not M1.
- Threads own their own loop from day one. There is no render loop yet, and
  there must never be one driving DSP — retrofitting that means rewriting the
  threading model a second time.

**Done — M1 acceptance:** `adb logcat` shows S-meter values that move when the
antenna is connected and disconnected, and change with band.

---

## Ordering and risk

The sequence is chosen so the cheap boxes retire the expensive risks first:

- Box 1 costs a few hundred lines and can invalidate the phone's whole network
  story. It must not come after two days of cross-compiling.
- Box 2 is free (reading) and reshapes Boxes 3, 6 and 7.
- Boxes 4–6 are grind, not risk — known-shaped work, just slow.
- Box 8 is the only box where genuinely new logic gets written.

**Open unknowns that could reorder this,** carried from CLAUDE.md:

- Whether GLib-without-GTK builds clean for Android. Box 3 answers it.
- Thermal behaviour under sustained WDSP load. Not an M1 question, but note
  anything alarming during Box 8.

## Standing guardrails

Repeated here because these are the ones a long milestone erodes:

- No Oboe, no AAudio, no Android audio dependency. If a task seems to need one,
  the design is being misread.
- No network interface configuration code. It needs root; the phone is not
  rooted.
- No dependencies added without asking. Every one has to cross-compile.
- Don't refactor beyond the box at hand.
- Don't edit `vendor/` or the `wdsp-*/` trees — patch via build scripts.
