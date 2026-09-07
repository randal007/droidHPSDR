#!/bin/sh
# Build m0-probe for the phone (arm64) and, with -host, for this desktop.
#
# The real CMake build arrives in M1 Box 6; this stays a two-line shell script
# on purpose, so the probe can be built before any of that exists.
set -e

NDK="${ANDROID_NDK_HOME:-$HOME/Android/Sdk/ndk/27.3.13750724}"
API=31
OUT="$(dirname "$0")"

if [ "$1" = "-host" ]; then
    cc -O2 -Wall -Wextra -o "$OUT/m0-probe-host" "$OUT/m0_probe.c"
    echo "built $OUT/m0-probe-host"
    exit 0
fi

CC="$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android$API-clang"
[ -x "$CC" ] || { echo "NDK compiler not found: $CC" >&2
                  echo "Set ANDROID_NDK_HOME, or install the NDK." >&2; exit 1; }

"$CC" -O2 -Wall -Wextra -o "$OUT/m0-probe" "$OUT/m0_probe.c"
echo "built $OUT/m0-probe"
file "$OUT/m0-probe" 2>/dev/null || true

cat <<'USAGE'

Push and run:
  adb push m0-probe /data/local/tmp/ && adb shell chmod 755 /data/local/tmp/m0-probe
  adb shell /data/local/tmp/m0-probe -l           # list interfaces
  adb shell /data/local/tmp/m0-probe -i wlan0     # WiFi path
  adb shell /data/local/tmp/m0-probe -i eth0 -w   # USB Ethernet path
USAGE
