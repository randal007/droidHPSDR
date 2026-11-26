#!/usr/bin/env bash
# Put the demo band's IQ recording on the phone (the app's own folder, where
# Radio.iqFile() looks). Default: RFSPACE's 20 m demo recording, converted
# to 192 kHz by tools/iq_convert.py into build-android/iq/ (not in git: the
# file has no licence, it's for testing only; remove it before a release).
#   ADB_SERIAL=<ip:port> tools/push-demo-iq.sh [file.wav]
set -euo pipefail
REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ADB=("$HOME/Android/Sdk/platform-tools/adb")
[[ -n "${ADB_SERIAL:-}" ]] && ADB+=(-s "$ADB_SERIAL")
F="${1:-$REPO/build-android/iq/demo_14225000Hz.wav}"
[[ -f "$F" ]] || { echo "no $F (see tools/iq_convert.py)" >&2; exit 1; }
DIR=/sdcard/Android/data/io.github.randal007.droidhpsdr/files
"${ADB[@]}" shell mkdir -p "$DIR"
"${ADB[@]}" push "$F" "$DIR/"
"${ADB[@]}" shell ls -l "$DIR"
