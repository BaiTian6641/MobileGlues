#!/bin/sh
# Build the wrapper-level contract harness for Android arm64.
#   ANDROID_NDK=/path/to/ndk sh MobileGlues-cpp/tools/wrapper_probe/build-arm64.sh
set -eu
cd "$(dirname "$0")"
NDK="${ANDROID_NDK:-${ANDROID_NDK_HOME:-}}"
if [ -z "$NDK" ]; then
    for candidate in "$HOME/Android/Sdk/ndk/28.2.13676358" "$HOME/Android/Sdk/ndk/27.3.13750724"; do
        if [ -d "$candidate" ]; then NDK="$candidate"; break; fi
    done
fi
if [ -z "$NDK" ] || [ ! -d "$NDK" ]; then
    echo "ANDROID_NDK is not set and no default NDK was found" >&2
    exit 1
fi
HOST="$NDK/toolchains/llvm/prebuilt/linux-x86_64"
CXX="$HOST/bin/aarch64-linux-android26-clang++"
ROOT="$(cd ../../.. && pwd)"
OUT="${OUT:-$PWD/wrapper_probe_arm64}"
$CXX -std=c++20 -O2 -fPIE -pie -Wall -Wextra -Wno-unused-parameter \
    -I"$ROOT/MobileGlues-cpp/include" \
    -static-libstdc++ -o "$OUT" main.cpp -ldl -llog
echo "built $OUT"
