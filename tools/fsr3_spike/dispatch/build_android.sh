#!/bin/sh
# Build the arm64 Android dispatch harness. Does not contact a device.
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$HERE"
NDK=${ANDROID_NDK_HOME:-${ANDROID_NDK_ROOT:-}}
if [ -z "$NDK" ]; then
  for c in "$HOME/Android/Sdk/ndk/28.2.13676358" "$HOME/Android/Sdk/ndk/27.3.13750724" /opt/android-ndk; do
    [ -d "$c" ] && NDK=$c && break
  done
fi
[ -n "$NDK" ] || { echo "NDK not found; set ANDROID_NDK_HOME" >&2; exit 1; }
HOST=$(ls "$NDK/toolchains/llvm/prebuilt" | head -n1)
CXX="$NDK/toolchains/llvm/prebuilt/$HOST/bin/aarch64-linux-android29-clang++"
[ -x "$CXX" ] || { echo "missing $CXX" >&2; exit 1; }
"$CXX" -O2 -std=c++17 -Wall -Wextra -ffp-contract=off -static-libstdc++ \
  -o fsr3_dispatch_arm64 fsr3_dispatch.cpp -lEGL -lGLESv3 -lm
file fsr3_dispatch_arm64
