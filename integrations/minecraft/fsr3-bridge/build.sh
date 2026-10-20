#!/bin/sh
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
IRIS_JAR="${IRIS_JAR:-/tmp/iris-1.8.14.jar}"
MIXIN_JAR="${MIXIN_JAR:-/home/polar/MobileGlues-plugin/iris-image-bridge/.cache/mixin-0.8.7.jar}"
NDK="${ANDROID_NDK_HOME:-${ANDROID_NDK_ROOT:-$HOME/Android/Sdk/ndk/28.2.13676358}}"
HOST=$(ls "$NDK/toolchains/llvm/prebuilt" | head -n1)
CXX="$NDK/toolchains/llvm/prebuilt/$HOST/bin/aarch64-linux-android26-clang++"
[ -f "$IRIS_JAR" ] || { echo "missing IRIS_JAR=$IRIS_JAR" >&2; exit 1; }
[ -f "$MIXIN_JAR" ] || { echo "missing MIXIN_JAR=$MIXIN_JAR" >&2; exit 1; }
rm -rf "$HERE/build"
mkdir -p "$HERE/build/classes" "$HERE/build/native/arm64-v8a"
find "$HERE/src/main/java" -name '*.java' > "$HERE/build/sources.txt"
javac --release 17 -proc:none -cp "$IRIS_JAR:$MIXIN_JAR" -d "$HERE/build/classes" @"$HERE/build/sources.txt"
cp -R "$HERE/src/main/resources/." "$HERE/build/classes/"
"$CXX" -O2 -std=c++17 -fPIC -shared -static-libstdc++ \
  -o "$HERE/build/native/arm64-v8a/libmgfsrhelper.so" "$HERE/src/main/cpp/mgfsrhelper.cpp" -ldl
mkdir -p "$HERE/build/classes/native/arm64-v8a"
cp "$HERE/build/native/arm64-v8a/libmgfsrhelper.so" "$HERE/build/classes/native/arm64-v8a/"
printf "Manifest-Version: 1.0\nMixinConfigs: mixins.mgfsrbridge.json\n\n" > "$HERE/build/MANIFEST.MF"
mkdir -p "$HERE/dist"
jar --create --file "$HERE/dist/mgfsr3bridge-0.1.0.jar" --manifest "$HERE/build/MANIFEST.MF" -C "$HERE/build/classes" .
sha256sum "$HERE/dist/mgfsr3bridge-0.1.0.jar"
