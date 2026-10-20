#!/bin/sh
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
NDK=${ANDROID_NDK_HOME:-${ANDROID_NDK_ROOT:-$HOME/Android/Sdk/ndk/28.2.13676358}}
HOST=$(ls "$NDK/toolchains/llvm/prebuilt" | head -n1)
CXX="$NDK/toolchains/llvm/prebuilt/$HOST/bin/aarch64-linux-android26-clang++"
"$CXX" -O2 -std=c++20 -fPIE -pie -static-libstdc++ -o "$HERE/direct_probe_arm64" "$HERE/main.cpp" -ldl
