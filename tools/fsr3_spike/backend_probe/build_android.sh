#!/bin/sh
# Build the production-backend probe for ARM64; links only against the public
# library and system EGL/GLES loader headers.
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$HERE/../../.." && pwd)
NDK=${ANDROID_NDK_HOME:-${ANDROID_NDK_ROOT:-$HOME/Android/Sdk/ndk/28.2.13676358}}
HOST=$(ls "$NDK/toolchains/llvm/prebuilt" | head -n1)
CXX="$NDK/toolchains/llvm/prebuilt/$HOST/bin/aarch64-linux-android26-clang++"
"$CXX" -O2 -std=c++20 -fPIE -pie -static-libstdc++ -I"$ROOT/MobileGlues-cpp" \
  -o "$HERE/backend_probe_arm64" "$HERE/main.cpp" -ldl -llog
file "$HERE/backend_probe_arm64"
