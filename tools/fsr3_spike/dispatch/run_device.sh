#!/bin/sh
# Device-run script for the dispatch gate. ONLY run with an assigned device.
# usage: ./run_device.sh [serial]
#
# Runs:
#   rcas-dispatch-320x180.json      single RCAS pass, sharpness 0.2
#   rcas-dispatch-640x360.json      single RCAS pass, sharpness 1.0
#   chain-flat-160x90.json          8-pass chain, flat frame (invariant check)
#   chain-step-160x90.json          8-pass chain, step pattern (parity)
#   chain-flat-320x180.json         8-pass chain at 1.5x, flat frame
#   chain-step-320x180.json         8-pass chain at 1.5x, step pattern
#
# The device is not modified beyond /data/local/tmp/fsr3_dispatch_* scratch.
set -eu
SERIAL=${1:-}
ADB=${ADB:-adb}
if [ -n "$SERIAL" ]; then ADB="$ADB -s $SERIAL"; fi
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
SHADER_DIR=${SHADER_DIR:-/tmp/fsr3-spike/out/shaders}
$ADB wait-for-device
$ADB shell getprop ro.product.model
$ADB push "$HERE/fsr3_dispatch_arm64" /data/local/tmp/fsr3_dispatch
$ADB shell chmod 755 /data/local/tmp/fsr3_dispatch

$ADB push "$SHADER_DIR/rcas_essl320.glsl" /data/local/tmp/fsr3_shaders/rcas_essl320.glsl 2>/dev/null || {
  $ADB shell mkdir -p /data/local/tmp/fsr3_shaders
  $ADB push "$SHADER_DIR/rcas_essl320.glsl" /data/local/tmp/fsr3_shaders/rcas_essl320.glsl
}
$ADB shell "/data/local/tmp/fsr3_dispatch --shader /data/local/tmp/fsr3_shaders/rcas_essl320.glsl --width 320 --height 180 --sharpness 0.2" \
  > "$HERE/rcas-dispatch-320x180.json"
$ADB shell "/data/local/tmp/fsr3_dispatch --shader /data/local/tmp/fsr3_shaders/rcas_essl320.glsl --width 640 --height 360 --sharpness 1.0" \
  > "$HERE/rcas-dispatch-640x360.json"

$ADB shell rm -rf /data/local/tmp/fsr3_shaders
$ADB shell mkdir -p /data/local/tmp/fsr3_shaders
for p in prepare_inputs luma_pyramid shading_change_pyramid shading_change \
         prepare_reactivity luma_instability accumulate rcas; do
  $ADB push "$SHADER_DIR/${p}_essl320.glsl" /data/local/tmp/fsr3_shaders/
done
for spec in "flat 160x90" "step 160x90" "flat 320x180" "step 320x180"; do
  set -- $spec
  pat=$1; ext=$2
  $ADB shell "/data/local/tmp/fsr3_dispatch --mode chain --shaders /data/local/tmp/fsr3_shaders --pattern $pat --render $ext --output $( \
    case "$ext" in
      160x90) echo 320x180;;
      320x180) echo 480x270;;
    esac)" > "$HERE/chain-${pat}-${ext}.json"
done
echo "wrote rcas-dispatch-*.json and chain-*.json"
