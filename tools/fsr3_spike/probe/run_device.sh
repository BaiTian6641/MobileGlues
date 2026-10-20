#!/bin/sh
# Device-run package. Execute ONLY when the target device is assigned to this work.
# The reserved C1L63708PZ must not be used by this worker without coordinator permission.
#
# Produces:
#   device-probe.json          operation probe (formats, atomics, shared, limits)
#   device-shader-compile.json driver compile of the adapted FSR3 ESSL shaders
set -eu
SERIAL=${1:-}
ADB=${ADB:-adb}
if [ -n "$SERIAL" ]; then ADB="$ADB -s $SERIAL"; fi
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
$ADB wait-for-device
$ADB shell getprop ro.product.model
$ADB shell getprop ro.build.version.sdk
$ADB push "$HERE/fsr3_gles_probe_arm64" /data/local/tmp/fsr3_gles_probe
$ADB shell chmod 755 /data/local/tmp/fsr3_gles_probe
$ADB shell rm -rf /data/local/tmp/fsr3_shaders
$ADB shell mkdir -p /data/local/tmp/fsr3_shaders
for f in "$HERE"/shaders/*_essl320.glsl; do
  $ADB push "$f" /data/local/tmp/fsr3_shaders/
done
$ADB shell /data/local/tmp/fsr3_gles_probe > "$HERE/device-probe.json"
FILES=""
for f in "$HERE"/shaders/*_essl320.glsl; do
  FILES="$FILES /data/local/tmp/fsr3_shaders/$(basename "$f")"
done
$ADB shell /data/local/tmp/fsr3_gles_probe --compile-shader $FILES > "$HERE/device-shader-compile.json"
echo "wrote device-probe.json and device-shader-compile.json"
