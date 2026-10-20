#!/bin/bash
# Reproduce the FSR3.1.4 GLES feasibility spike.
# Requires: git, cmake, ninja or make, glslangValidator, EGL/GLES3.2, python3.
# Does not contact a device. Does not vendor the SDK into the repository.
set -euo pipefail
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=${MG_ROOT:-/home/polar/MobileGlues}
SPIKE=${SPIKE:-/tmp/fsr3-spike}
PIN=c6efa6bf7f2027b3ec94f28578bb5965eabb9e55
SDK=$SPIKE/FidelityFX-SDK

mkdir -p "$SPIKE/out/logs"
if [ ! -d "$SDK/.git" ]; then
  git clone --depth 1 --branch v1.1.4 https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK.git "$SDK"
fi
test "$(git -C "$SDK" rev-parse HEAD)" = "$PIN" || { echo "SDK pin mismatch" >&2; exit 1; }

if [ ! -x "$SPIKE/build-spirv-cross/spirv-cross" ]; then
  cmake -S "$ROOT/MobileGlues-cpp/3rdparty/SPIRV-Cross" -B "$SPIKE/build-spirv-cross" \
    -DSPIRV_CROSS_CLI=ON -DSPIRV_CROSS_ENABLE_TESTS=OFF -DCMAKE_BUILD_TYPE=Release
  cmake --build "$SPIKE/build-spirv-cross" -j"${JOBS:-4}"
fi

SPIKE="$SPIKE" bash "$HERE/study/build_port_study.sh"
if [ "${PERM:-0}" = "1" ]; then
  SPIKE="$SPIKE" bash "$HERE/study/perm_matrix.sh" | tee "$SPIKE/out/permutation-matrix.txt"
fi

make -C "$HERE/probe"
"$HERE/probe/fsr3_gles_probe" > "$SPIKE/out/probe-host.json"
"$HERE/probe/fsr3_gles_probe" --compile-shader "$SPIKE"/out/shaders/*_essl320.glsl \
  > "$SPIKE/out/driver-compile-host.json" || true

"$HERE/probe/build_android.sh"

# Device packaging only. Run run_device.sh only when this worker owns a device.
PKG=$SPIKE/device-package
rm -rf "$PKG"; mkdir -p "$PKG/shaders"
cp "$HERE/probe/fsr3_gles_probe_arm64" "$PKG/"
cp "$SPIKE"/out/shaders/*_essl320.glsl "$PKG/shaders/"
cp "$HERE/probe/run_device.sh" "$PKG/"
cp "$SPIKE/out/probe-host.json" "$PKG/expected-host-probe.json"
sha256sum "$PKG"/* "$PKG"/shaders/* > "$PKG/sha256sums.txt" 2>/dev/null || true

echo "host probe:   $SPIKE/out/probe-host.json"
echo "port matrix:  $SPIKE/out/port-matrix.tsv"
echo "device pkg:   $PKG (device results NOT RUN)"
