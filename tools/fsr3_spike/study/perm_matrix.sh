#!/bin/bash
# 64-permutation compile matrix for three representative passes.
set -u
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
SPIKE=${SPIKE:-/tmp/fsr3-spike}
SDK=${FSR3_SDK:-$SPIKE/FidelityFX-SDK}
CROSS=${SPIRV_CROSS:-$SPIKE/build-spirv-cross/spirv-cross}
G=${GLSLANG:-glslangValidator}
OUT=${OUT:-$SPIKE/out}
base="-DFFX_FSR3UPSCALER_OPTION_UPSAMPLE_SAMPLERS_USE_DATA_HALF=0 -DFFX_FSR3UPSCALER_OPTION_ACCUMULATE_SAMPLERS_USE_DATA_HALF=0 -DFFX_FSR3UPSCALER_OPTION_REPROJECT_SAMPLERS_USE_DATA_HALF=1 -DFFX_FSR3UPSCALER_OPTION_POSTPROCESSLOCKSTATUS_SAMPLERS_USE_DATA_HALF=0 -DFFX_FSR3UPSCALER_OPTION_UPSAMPLE_USE_LANCZOS_TYPE=2"
mkdir -p "$OUT/shaders" "$OUT/logs" "$SPIKE/adapted"
for p in accumulate prepare_inputs luma_pyramid; do
  S="$SDK/sdk/src/backends/vk/shaders/fsr3upscaler/ffx_fsr3upscaler_${p}_pass.glsl"
  extra=""; case "$p" in luma_pyramid) extra="-DFFX_SPD_NO_WAVE_OPERATIONS=1";; esac
  python3 "$HERE/inline_adapt.py" "$S" \
    "$SDK/sdk/include/FidelityFX/gpu" "$SDK/sdk/include/FidelityFX/gpu/fsr3upscaler" rgba8 \
    > "$SPIKE/adapted/${p}.glsl"
  ok=0; fail=0
  for lut in 0 1; do for hdr in 0 1; do for lo in 0 1; do for ji in 0 1; do
  for inv in 0 1; do for sh in 0 1; do
    "$G" -V --target-env vulkan1.2 -DFFX_GLSL=1 -DFFX_GPU=1 $base $extra \
      -DFFX_FSR3UPSCALER_OPTION_REPROJECT_USE_LANCZOS_TYPE=$lut \
      -DFFX_FSR3UPSCALER_OPTION_HDR_COLOR_INPUT=$hdr \
      -DFFX_FSR3UPSCALER_OPTION_LOW_RESOLUTION_MOTION_VECTORS=$lo \
      -DFFX_FSR3UPSCALER_OPTION_JITTERED_MOTION_VECTORS=$ji \
      -DFFX_FSR3UPSCALER_OPTION_INVERTED_DEPTH=$inv \
      -DFFX_FSR3UPSCALER_OPTION_APPLY_SHARPENING=$sh \
      -S comp "$SPIKE/adapted/$p.glsl" -o "$OUT/shaders/perm.spv" \
      > "$OUT/logs/perm-$p.log" 2>&1 || { fail=$((fail+1)); continue; }
    "$CROSS" "$OUT/shaders/perm.spv" --es --version 320 \
      > "$OUT/shaders/perm.glsl" 2> "$OUT/logs/perm-essl-$p.log" || { fail=$((fail+1)); continue; }
    "$G" -S comp "$OUT/shaders/perm.glsl" \
      > "$OUT/logs/perm-val-$p.log" 2>&1 || { fail=$((fail+1)); continue; }
    ok=$((ok+1))
  done; done; done; done; done; done
  echo "$p permutations_ok=$ok permutations_fail=$fail"
done
