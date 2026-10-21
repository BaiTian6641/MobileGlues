#!/bin/sh
# Build separate provider-only and real-buffer.cpp adapters. No device access.
set -eu
cd "$(dirname "$0")"
ROOT="$(cd ../.. && pwd)"
OUT="${OUT_DIR:-/tmp/mg-buffer-visibility}"
mkdir -p "$OUT"
CXX="${CXX:-c++}"
"$CXX" -std=c++20 -O2 -Wall -Wextra -Werror main.cpp -o "$OUT/provider" -lEGL -lGLESv2 -ldl
# The project headers/TU have baseline warnings; new probe main is built strictly.
"$CXX" -std=gnu++20 -O2 -w -ffunction-sections -fdata-sections \
    -I"$ROOT" -I"$ROOT/include" -I"$ROOT/3rdparty/xxhash" \
    -c "$ROOT/gl/buffer.cpp" -o "$OUT/buffer.o"
"$CXX" -std=gnu++20 -O2 -w -ffunction-sections -fdata-sections \
    -I"$ROOT" -I"$ROOT/include" -I"$ROOT/3rdparty/xxhash" \
    -c wrapper_adapter.cpp -o "$OUT/adapter.o"
"$CXX" -std=gnu++20 -O2 -w -ffunction-sections -fdata-sections \
    -I"$ROOT" -I"$ROOT/include" -I"$ROOT/3rdparty/xxhash" \
    -c "$ROOT/gl/getter.cpp" -o "$OUT/getter.o"
# Keep this translation unit's public symbols from interposing unrelated GLES
# operations (for example VAO setup). Calls through BufferApi still use its real
# implementation; no source is replaced or copied into the test.
nm --defined-only -g --format=posix "$OUT/buffer.o" "$OUT/getter.o" | \
    awk '$1 ~ /^gl[A-Z]/ { print $1, "probe_" $1 }' | sort -u > "$OUT/symbols"
objcopy --redefine-syms="$OUT/symbols" "$OUT/buffer.o"
objcopy --redefine-syms="$OUT/symbols" "$OUT/adapter.o"
objcopy --redefine-syms="$OUT/symbols" "$OUT/getter.o"
"$CXX" -std=c++20 -O2 -Wall -Wextra -Werror -DMG_BUFFER_WRAPPER -Wl,--gc-sections \
    main.cpp "$OUT/adapter.o" "$OUT/buffer.o" "$OUT/getter.o" -o "$OUT/real-buffer" -lEGL -lGLESv2 -ldl
printf 'Built %s/provider and %s/real-buffer\n' "$OUT" "$OUT"
