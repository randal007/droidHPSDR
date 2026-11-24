#!/usr/bin/env bash
# Push and run the M1 headless receive test on the phone.
#   android/native/run-m1.sh [m1-rx options...]     e.g.  -f 7074000 -t 60
# Uses $ADB_SERIAL if set (e.g. 192.168.88.110:5555 for adb over WiFi).
# Looks up the Ethernet network's handle so unicast to the radio goes out
# eth0 instead of WiFi (see CLAUDE.md, "Verified on the S26 Ultra").
set -euo pipefail
REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ADB=("$HOME/Android/Sdk/platform-tools/adb")
[[ -n "${ADB_SERIAL:-}" ]] && ADB+=(-s "$ADB_SERIAL")
BIN="$REPO/build-android/cmake/m1-rx"
[[ -x "$BIN" ]] || { echo "build first: cmake --build build-android/cmake" >&2; exit 1; }

handle="$("${ADB[@]}" shell dumpsys connectivity |
  grep -m1 'ni{Ethernet CONNECTED' | grep -oE 'handle\{[0-9]+\}' | grep -oE '[0-9]+' || true)"
if [[ -z "$handle" ]]; then
  echo "no connected Ethernet network on the phone; running without -H (WiFi radio only)" >&2
else
  echo "Ethernet network handle: $handle"
fi
"${ADB[@]}" push -q "$BIN" /data/local/tmp/m1-rx
"${ADB[@]}" shell chmod 755 /data/local/tmp/m1-rx
exec "${ADB[@]}" shell /data/local/tmp/m1-rx ${handle:+-H "$handle"} "$@"
