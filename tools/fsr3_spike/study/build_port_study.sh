#!/bin/bash
# Compile the pinned FSR3.1.4 Vulkan GLSL through a candidate GLES 3.2 route.
# This is a compilation study, not a runtime port and not a capability claim.
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
SPIKE=${SPIKE:-/tmp/fsr3-spike}
SDK=${FSR3_SDK:-$SPIKE/FidelityFX-SDK}
CROSS=${SPIRV_CROSS:-$SPIKE/build-spirv-cross/spirv-cross}
G=${GLSLANG:-glslangValidator}
OUT=${OUT:-$SPIKE/out}
mkdir -p "$OUT/shaders" "$OUT/logs" "$SPIKE/adapted"
BASE="-DFFX_FSR3UPSCALER_OPTION_UPSAMPLE_SAMPLERS_USE_DATA_HALF=0 -DFFX_FSR3UPSCALER_OPTION_ACCUMULATE_SAMPLERS_USE_DATA_HALF=0 -DFFX_FSR3UPSCALER_OPTION_REPROJECT_SAMPLERS_USE_DATA_HALF=1 -DFFX_FSR3UPSCALER_OPTION_POSTPROCESSLOCKSTATUS_SAMPLERS_USE_DATA_HALF=0 -DFFX_FSR3UPSCALER_OPTION_UPSAMPLE_USE_LANCZOS_TYPE=2"
: > "$OUT/port-matrix.tsv"
for p in accumulate autogen_reactive debug_view luma_instability luma_pyramid \
         prepare_inputs prepare_reactivity rcas shading_change shading_change_pyramid; do
  S="$SDK/sdk/src/backends/vk/shaders/fsr3upscaler/ffx_fsr3upscaler_${p}_pass.glsl"
  extra=""
  case "$p" in luma_pyramid|shading_change_pyramid) extra="-DFFX_SPD_NO_WAVE_OPERATIONS=1";; esac
  python3 "$HERE/inline_adapt.py" "$S" \
    "$SDK/sdk/include/FidelityFX/gpu" "$SDK/sdk/include/FidelityFX/gpu/fsr3upscaler" rgba8 \
    > "$SPIKE/adapted/$p.glsl"
  "$G" -V --target-env vulkan1.2 -DFFX_GLSL=1 -DFFX_GPU=1 $BASE $extra \
    -S comp "$SPIKE/adapted/$p.glsl" -o "$OUT/shaders/${p}_adapted.spv" \
    > "$OUT/logs/${p}_adapted_glslang.log" 2>&1; g=$?
  "$CROSS" "$OUT/shaders/${p}_adapted.spv" --es --version 320 \
    > "$OUT/shaders/${p}_essl320.glsl" 2> "$OUT/logs/${p}_essl.err"; s=$?
  "$G" -S comp "$OUT/shaders/${p}_essl320.glsl" \
    > "$OUT/logs/${p}_essl_validate.log" 2>&1; v=$?
  printf "%s\t%s\t%s\t%s\n" "$p" "$g" "$s" "$v" | tee -a "$OUT/port-matrix.tsv"
done
