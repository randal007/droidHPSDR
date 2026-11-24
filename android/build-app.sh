#!/bin/bash
# Build (and optionally install) the droidHPSDR app.
#   android/build-app.sh            debug APK -> android/app/build/outputs/apk/debug/
#   android/build-app.sh install    ... and install it on the phone (ADB_SERIAL or the only device)
# Needs: JDK 21 (mise: java@temurin-21), the Android SDK in ~/Android/Sdk with
# platforms;android-37.2, build-tools;37.0.0, cmake;4.1.2, ndk;27.3.13750724,
# and the native deps from android/deps/build-deps.sh.
set -e
cd "$(dirname "$0")"
export ANDROID_HOME=${ANDROID_HOME:-$HOME/Android/Sdk}
if [ -z "$JAVA_HOME" ] || ! "$JAVA_HOME/bin/java" -version 2>&1 | grep -q '"21'; then
  JAVA_HOME=$(mise where java@temurin-21 2>/dev/null || ls -d "$HOME"/.local/share/mise/installs/java/temurin-21* | tail -1)
  export JAVA_HOME
fi
[ -f local.properties ] || echo "sdk.dir=$ANDROID_HOME" > local.properties
./gradlew --no-daemon -q assembleDebug
apk=app/build/outputs/apk/debug/app-debug.apk
echo "$apk"
if [ "$1" = install ]; then
  "$ANDROID_HOME/platform-tools/adb" ${ADB_SERIAL:+-s $ADB_SERIAL} install -r "$apk"
fi
