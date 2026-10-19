#!/bin/sh
# Build the below-wrapper capability probe for Android arm64.
#
#   ANDROID_NDK=/path/to/ndk sh MobileGlues-cpp/tools/capability_probe/build-arm64.sh
#
# The probe does not link MobileGlues or any project static library. It needs
# only the NDK sysroot and the platform EGL/GLESv2 stubs.
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
OUT="${OUT:-$PWD/capability_probe_arm64}"
$CXX -std=c++20 -O2 -fPIE -pie -Wall -Wextra -Wno-unused-parameter \
    -static-libstdc++ -o "$OUT" probe.cpp -lEGL -lGLESv2 -llog
echo "built $OUT"
