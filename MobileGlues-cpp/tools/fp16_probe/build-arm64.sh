#!/bin/sh
# Build the X1E FP16 ALU probe.
set -eu
cd "$(dirname "$0")"
NDK="${ANDROID_NDK:-$HOME/Android/Sdk/ndk/28.2.13676358}"
CC="$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android26-clang"
$CC -O2 fp16_probe.c -lEGL -lGLESv3 -o fp16_probe_arm64
echo "built $(pwd)/fp16_probe_arm64"
