# droidHPSDR native build: the headless receive test (m1-rx, milestone 1)
# and the app's JNI library (libdroidhpsdr.so, built by Gradle from android/app).
#
# Configure (from the repo root):
#   cmake -S android/native -B build-android/cmake -G Ninja \
#     -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake \
#     -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=31 -DCMAKE_BUILD_TYPE=Release
#   cmake --build build-android/cmake
#
# Needs the libraries from android/deps/build-deps.sh in build-android/arm64-v8a.
# Upstream deskHPSDR files are compiled unchanged from ../../src.

cmake_minimum_required(VERSION 3.22)
project(droidhpsdr C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_EXTENSIONS ON)   # upstream uses GNU C (e.g. `linux` macro in WDSP headers)

get_filename_component(REPO ${CMAKE_CURRENT_SOURCE_DIR}/../.. ABSOLUTE)
set(DEPS ${REPO}/build-android/${ANDROID_ABI})
set(UPSTREAM ${REPO}/src)

# deskHPSDR files compiled unchanged (see docs/PORTING-NOTES.md).
set(UPSTREAM_SOURCES
  old_protocol.c old_discovery.c discovered.c message.c nw_toolset.c
  band.c mode.c filter.c receiver.c
  vfo.c store.c property.c appearance.c)   # M4: VFO A/B, band stacks, RIT/CTUN, steps, memories, deskHPSDR's props file
list(TRANSFORM UPSTREAM_SOURCES PREPEND ${UPSTREAM}/)

add_library(droidcore STATIC
  ${UPSTREAM_SOURCES}
  compat/glib_compat.c
  headless/radio_state.c
  headless/stubs.c
  headless/net_target.c
  headless/n2adr.c)
# compat/ first so <gtk/gtk.h>, <glib.h>, <gdk/gdk.h>, <cairo.h> are the shims.
target_include_directories(droidcore PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/compat
  ${CMAKE_CURRENT_SOURCE_DIR}/headless
  ${UPSTREAM}
  ${REPO}/wdsp-2.11
  ${DEPS}/fftw-double/include)
# Same audio defines as upstream's Linux build, so struct layouts match its headers.
target_compile_definitions(droidcore PUBLIC __linux__ AUDIO_RINGBUFFER MINIAUDIO)
target_compile_options(droidcore PRIVATE -Wno-unused-parameter)
set_target_properties(droidcore PROPERTIES POSITION_INDEPENDENT_CODE ON)   # linked into libdroidhpsdr.so

add_library(wdsp STATIC IMPORTED)
set_target_properties(wdsp PROPERTIES IMPORTED_LOCATION ${DEPS}/wdsp/libwdsp.a)
target_link_libraries(droidcore PUBLIC
  wdsp
  ${DEPS}/wdsp-libs/lib/librnnoise.a
  ${DEPS}/wdsp-libs/lib/libspecbleach.a
  ${DEPS}/fftw-double/lib/libfftw3.a
  ${DEPS}/fftw-float/lib/libfftw3f.a
  android log m)

# m1-rx stays RECEIVE ONLY by construction: headless/tx_null.c is its transmit side.
add_executable(m1-rx headless/m1_main.c headless/display_null.c headless/audio_null.c headless/tx_null.c)
target_link_libraries(m1-rx droidcore)

# The app: radio core + upstream display hook + GPU renderer + JNI bridge.
add_library(droidhpsdr SHARED
  ${UPSTREAM}/buffered_audio.c   # deskHPSDR's local audio engine, unchanged
  ${UPSTREAM}/transmitter.c      # M7: deskHPSDR's transmitter (WDSP TX channel), unchanged
  ${UPSTREAM}/tx_off.c           #     graceful MOX off (the speech tail goes out first)
  ${UPSTREAM}/vox.c
  ${UPSTREAM}/sintab.c
  app/transmit.c                 #     radio.c's transmit functions + droidHPSDR's safety
  app/audio_aaudio.c
  app/autogain.c
  app/demo.c
  app/dsp.c
  app/radio_core.c
  app/display_bridge.c
  app/gl_render.c
  app/jni_bridge.c
  app/hl2.c
  app/playback.c)
target_include_directories(droidhpsdr PRIVATE app)
target_compile_options(droidhpsdr PRIVATE -Wall -Wno-unused-parameter)
target_link_libraries(droidhpsdr droidcore GLESv3 aaudio)
# playback.c: no HL2+ audio while an IQ recording plays (receiver.c's call goes through playback.c)
target_link_options(droidhpsdr PRIVATE -Wl,--wrap=old_protocol_audio_samples)
# 16 KB page devices (Android 15+): NDK 27 still links for 4 KB by default.
target_link_options(droidhpsdr PRIVATE "-Wl,-z,max-page-size=16384")
