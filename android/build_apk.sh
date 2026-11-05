#!/bin/bash
# Builds the APK from the arm64 native build: no Gradle, only the Android SDK and NDK.
# usage: build_apk.sh <arm64 build dir> <SDL3 source dir> <output dir>
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
SDK=${ANDROID_HOME:-/home/claude/android/sdk}; NDK=${ANDROID_NDK:-/home/claude/android/android-ndk-r28c}
NATIVE=${1:-/home/claude/bf2-work/arm64}; SDL=${2:-/home/claude/build-android/_deps/sdl3-src}; OUT=${3:-$NATIVE/apk}
SDLLIB=${SDLLIB:-/home/claude/build-android/_deps/sdl3-build/libSDL3.so}
BT=$SDK/build-tools/36.0.0; JAR=$SDK/platforms/android-34/android.jar
LLVM=$NDK/toolchains/llvm/prebuilt/linux-x86_64
rm -rf "$OUT"; mkdir -p "$OUT/classes" "$OUT/dex" "$OUT/lib/arm64-v8a"
"$BT/aapt2" link -o "$OUT/base.apk" -I "$JAR" --manifest "$HERE/AndroidManifest.xml"
javac -nowarn -encoding UTF-8 -XDstringConcat=inline -source 8 -target 8 -bootclasspath "$JAR" -d "$OUT/classes" \
  $(find "$HERE/src" "$SDL/android-project/app/src/main/java" -name '*.java')
"$BT/d8" --release --min-api 29 --lib "$JAR" --output "$OUT/dex" $(find "$OUT/classes" -name '*.class')
cp "$NATIVE/libmain.so" "$NATIVE/bin_portable/libRendDX9.so" "$NATIVE/bin_portable/libBF2Audio.so" \
   "$NATIVE/bin_portable/libSwiffPlayer.so" \
   "$NATIVE/openal-build/libopenal.so" "$NATIVE/bin_linux/libdice_py.so" \
   "$NATIVE/deps/libdxvk_d3d9.so" "$NATIVE/deps/libvkd3d-shader.so" "$SDLLIB" \
   "$NATIVE"/drv/at-build/src/hook/lib*_hook.so "$NATIVE/drv/at-build/src/hook/libhook_impl.so" \
   "$NATIVE/drv/libvulkan_freedreno.so" \
   "$LLVM/sysroot/usr/lib/aarch64-linux-android/libc++_shared.so" "$OUT/lib/arm64-v8a/"
"$LLVM/bin/llvm-strip" --strip-unneeded "$OUT"/lib/arm64-v8a/*.so
(cd "$OUT/dex" && zip -q ../base.apk classes.dex)
(cd "$OUT" && zip -q -r -0 base.apk lib)
"$BT/zipalign" -f -p 16 "$OUT/base.apk" "$OUT/aligned.apk"
KS=${KEYSTORE:-/home/claude/moxnatiy/openbattlefield2/.debug.keystore}
[ -f "$KS" ] || keytool -genkeypair -keystore "$KS" -storepass android -keypass android -alias mackiller -keyalg RSA -keysize 2048 -validity 10000 -dname "CN=mackillerGR"
"$BT/apksigner" sign --ks "$KS" --ks-pass pass:android --out "$OUT/Battlefield2_by_mackillerGR.apk" "$OUT/aligned.apk"
"$BT/apksigner" verify "$OUT/Battlefield2_by_mackillerGR.apk"
ls -la "$OUT/Battlefield2_by_mackillerGR.apk"
