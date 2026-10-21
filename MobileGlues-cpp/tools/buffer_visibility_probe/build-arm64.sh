#!/bin/sh
# Below-wrapper Android probe: links platform EGL/GLES, never MobileGlues.
set -eu
cd "$(dirname "$0")"
NDK="${ANDROID_NDK:-${ANDROID_NDK_HOME:-$HOME/Android/Sdk/ndk/28.2.13676358}}"
CXX="$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android26-clang++"
OUT="${OUT:-/tmp/mg-buffer-visibility-arm64}"
"$CXX" -std=c++20 -O2 -fPIE -pie -Wall -Wextra -Werror -static-libstdc++ \
    main.cpp -o "$OUT" -lEGL -lGLESv3 -ldl
printf 'Built provider-only probe %s\n' "$OUT"
