#!/usr/bin/env bash
#
# Cross-compile WDSP and everything it links against for Android arm64-v8a.
#
#   FFTW 3.3.11 (double + float)  ->  libfftw3.a, libfftw3f.a
#   rnnoise   (DL1BZ's fork)      ->  librnnoise.a     (WDSP rnnr.c, NR3)
#   libspecbleach (DL1BZ's fork)  ->  libspecbleach.a  (WDSP sbnr.c, NR4)
#   WDSP (wdsp-2.11 from upstream deskHPSDR)  ->  libwdsp.a
#
# Same libraries and versions upstream's build-fftw.sh and update_libs.sh use,
# built with the NDK instead of the host compiler. Nothing in wdsp-*/ is
# edited: WDSP is compiled out of tree from the source list in its Makefile.
#
# Output:  build-android/arm64-v8a/{fftw-double,fftw-float,wdsp-libs,wdsp}
# Sources: build-android/src
#
# Usage:   android/deps/build-deps.sh [all|fftw|rnnoise|specbleach|wdsp|smoke]

set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
NDK="${ANDROID_NDK_HOME:-$HOME/Android/Sdk/ndk/27.3.13750724}"
API=31
ABI=arm64-v8a
TRIPLE=aarch64-linux-android
WDSP_DIR="$REPO/wdsp-2.11"
FFTW_VERSION=3.3.11
FFTW_MD5=40ec8d0447d03b8f01f8c90aa77bd16f   # fftw.org/fftw-3.3.11.tar.gz.md5sum

SRC="$REPO/build-android/src"
OUT="$REPO/build-android/$ABI"
TC="$NDK/toolchains/llvm/prebuilt/linux-x86_64"
CC="$TC/bin/${TRIPLE}${API}-clang"
AR="$TC/bin/llvm-ar"
RANLIB="$TC/bin/llvm-ranlib"
STRIP="$TC/bin/llvm-strip"
JOBS="$(nproc)"

[[ -x "$CC" ]] || { echo "NDK compiler not found: $CC" >&2; exit 1; }
mkdir -p "$SRC" "$OUT"

log() { printf '\n=== %s ===\n' "$*"; }

build_fftw() {
  local tarball="$SRC/fftw-$FFTW_VERSION.tar.gz"
  if [[ ! -f "$tarball" ]]; then
    log "Downloading FFTW $FFTW_VERSION"
    curl -fsSL -o "$tarball.part" "https://www.fftw.org/fftw-$FFTW_VERSION.tar.gz"
    mv "$tarball.part" "$tarball"
  fi
  echo "$FFTW_MD5  $tarball" | md5sum -c --quiet
  [[ -d "$SRC/fftw-$FFTW_VERSION" ]] || tar -xzf "$tarball" -C "$SRC"

  local prec extra
  for prec in double float; do
    log "FFTW $FFTW_VERSION $prec"
    extra=(--enable-neon)
    [[ $prec == float ]] && extra+=(--enable-float)
    rm -rf "$SRC/fftw-build-$prec" "$OUT/fftw-$prec"
    mkdir -p "$SRC/fftw-build-$prec"
    (
      cd "$SRC/fftw-build-$prec"
      CC="$CC" AR="$AR" RANLIB="$RANLIB" STRIP="$STRIP" CFLAGS="-O3 -fPIC" \
        "$SRC/fftw-$FFTW_VERSION/configure" --host=$TRIPLE \
        --disable-shared --enable-static --disable-fortran --with-pic \
        "${extra[@]}" --prefix="$OUT/fftw-$prec" >configure.log
      make -j"$JOBS" >make.log
      make install >install.log
    )
  done
}

build_rnnoise() {
  log "rnnoise"
  local d="$SRC/rnnoise"
  [[ -d "$d" ]] || git clone -q --depth=1 https://git.bzsax.de/dl1bz/rnnoise.git "$d"
  # autogen.sh fetches the model with wget, which this desktop lacks.
  local shim="$SRC/.wget-shim"
  mkdir -p "$shim"
  printf '#!/bin/sh\nexec curl -fsSL -O "$@"\n' >"$shim/wget"
  chmod +x "$shim/wget"
  (
    cd "$d"
    [[ -x configure ]] || PATH="$shim:$PATH" ./autogen.sh >autogen.log
    [[ -f Makefile ]] && make distclean >/dev/null || true
    CC="$CC" AR="$AR" RANLIB="$RANLIB" STRIP="$STRIP" CFLAGS="-O2 -fPIC" \
      ./configure --host=$TRIPLE --disable-shared --enable-static \
      --disable-examples --disable-doc --prefix="$OUT/wdsp-libs" >configure.log
    make -j"$JOBS" >make.log
    make install >install.log
  )
}

build_specbleach() {
  log "libspecbleach"
  local d="$SRC/libspecbleach"
  [[ -d "$d" ]] || git clone -q --depth=1 https://git.bzsax.de/dl1bz/libspecbleach.git "$d"
  local cross="$SRC/android-$ABI.meson"
  cat >"$cross" <<EOF
[binaries]
c = '$CC'
ar = '$AR'
strip = '$STRIP'
pkg-config = 'pkg-config'

[properties]
pkg_config_libdir = '$OUT/fftw-float/lib/pkgconfig'

[host_machine]
system = 'android'
cpu_family = 'aarch64'
cpu = 'armv8-a'
endian = 'little'
EOF
  rm -rf "$d/build-android"
  (
    cd "$d"
    meson setup build-android --cross-file "$cross" --buildtype=release \
      --prefix="$OUT/wdsp-libs" --libdir=lib -Ddefault_library=static \
      -Db_staticpic=true >"$SRC/specbleach-setup.log"
    meson compile -C build-android >"$SRC/specbleach-compile.log"
    meson install -C build-android >"$SRC/specbleach-install.log"
  )
}

build_wdsp() {
  log "WDSP ($(basename "$WDSP_DIR"))"
  local obj="$OUT/wdsp/obj"
  rm -rf "$OUT/wdsp"
  mkdir -p "$obj"
  # The source list comes from WDSP's own Makefile, so it follows upstream.
  local sources
  sources="$(printf 'print-sources:\n\t@echo $(SOURCES)\n' |
    make -s --no-print-directory -C "$WDSP_DIR" -f Makefile -f - print-sources 2>/dev/null |
    tail -1)"
  [[ -n "$sources" ]] || { echo "could not read SOURCES from $WDSP_DIR/Makefile" >&2; exit 1; }
  # Same flags as wdsp-2.11/Makefile, minus -mcpu=native (that is the build host).
  export CC OBJ="$obj" WDSP_DIR
  export WDSP_CFLAGS="-O2 -fPIC -fno-strict-aliasing -pthread -D_GNU_SOURCE \
-Wno-parentheses -Wcast-align -I$WDSP_DIR -I$OUT/wdsp-libs/include \
-I$OUT/fftw-double/include"
  # shellcheck disable=SC2016
  printf '%s\n' $sources | xargs -P"$JOBS" -I{} sh -c \
    '$CC $WDSP_CFLAGS -c -o "$OBJ/$(basename {} .c).o" "$WDSP_DIR/{}" 2>>"$OBJ/../warnings.log" || { echo "FAILED: {}"; exit 255; }'
  "$AR" rcs "$OUT/wdsp/libwdsp.a" "$obj"/*.o
  "$RANLIB" "$OUT/wdsp/libwdsp.a"
  echo "$(ls "$obj" | wc -l) objects, warnings in $OUT/wdsp/warnings.log"
}

build_smoke() {
  log "wdsp-smoke (test program, see android/deps/wdsp-smoke.c)"
  "$CC" -O2 -Wall -I"$WDSP_DIR" -I"$OUT/fftw-double/include" \
    -o "$OUT/wdsp-smoke" "$REPO/android/deps/wdsp-smoke.c" \
    "$OUT/wdsp/libwdsp.a" "$OUT/wdsp-libs/lib/librnnoise.a" \
    "$OUT/wdsp-libs/lib/libspecbleach.a" "$OUT/fftw-double/lib/libfftw3.a" \
    "$OUT/fftw-float/lib/libfftw3f.a" -lm -pthread
  echo "$OUT/wdsp-smoke"
  echo "run: adb push $OUT/wdsp-smoke /data/local/tmp/ && adb shell /data/local/tmp/wdsp-smoke"
}

summary() {
  log "Result"
  local f
  for f in fftw-double/lib/libfftw3.a fftw-float/lib/libfftw3f.a \
           wdsp-libs/lib/librnnoise.a wdsp-libs/lib/libspecbleach.a wdsp/libwdsp.a; do
    if [[ -f "$OUT/$f" ]]; then
      printf '%-34s %6s KB  %s\n' "$f" "$(( $(stat -c %s "$OUT/$f") / 1024 ))" \
        "$("$TC/bin/llvm-readelf" -h "$OUT/$f" | grep -m1 Machine | sed 's/ *Machine: *//')"
    else
      printf '%-34s MISSING\n' "$f"
    fi
  done
}

case "${1:-all}" in
  all)        build_fftw; build_rnnoise; build_specbleach; build_wdsp; build_smoke; summary ;;
  fftw)       build_fftw ;;
  rnnoise)    build_rnnoise ;;
  specbleach) build_specbleach ;;
  wdsp)       build_wdsp; summary ;;
  smoke)      build_smoke ;;
  *)          echo "usage: $0 [all|fftw|rnnoise|specbleach|wdsp|smoke]" >&2; exit 1 ;;
esac
