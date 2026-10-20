#!/bin/bash
# Generate the adapted ESSL used by the dispatch harness from the PINNED AMD
# source. Nothing is vendored into the repository: the pinned tree is expected
# at $FSR3_SDK (default /tmp/fsr3-spike/FidelityFX-SDK at commit c6efa6b...).
#
# usage: ./make_shaders.sh [outdir]
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
SPIKE=${SPIKE:-/tmp/fsr3-spike}
SDK=${FSR3_SDK:-$SPIKE/FidelityFX-SDK}
CROSS=${SPIRV_CROSS:-$SPIKE/build-spirv-cross/spirv-cross}
G=${GLSLANG:-glslangValidator}
OUT=${1:-$SPIKE/out/shaders}
PIN=c6efa6bf7f2027b3ec94f28578bb5965eabb9e55
test "$(git -C "$SDK" rev-parse HEAD)" = "$PIN" || { echo "SDK pin mismatch" >&2; exit 1; }
mkdir -p "$OUT"
BASE="-DFFX_FSR3UPSCALER_OPTION_UPSAMPLE_SAMPLERS_USE_DATA_HALF=0 -DFFX_FSR3UPSCALER_OPTION_ACCUMULATE_SAMPLERS_USE_DATA_HALF=0 -DFFX_FSR3UPSCALER_OPTION_REPROJECT_SAMPLERS_USE_DATA_HALF=1 -DFFX_FSR3UPSCALER_OPTION_POSTPROCESSLOCKSTATUS_SAMPLERS_USE_DATA_HALF=0 -DFFX_FSR3UPSCALER_OPTION_UPSAMPLE_USE_LANCZOS_TYPE=2"
# The eight passes scheduled by ffx_fsr3upscalerDispatch.
for p in prepare_inputs luma_pyramid shading_change_pyramid shading_change \
         prepare_reactivity luma_instability accumulate rcas; do
  S="$SDK/sdk/src/backends/vk/shaders/fsr3upscaler/ffx_fsr3upscaler_${p}_pass.glsl"
  extra=""
  case "$p" in luma_pyramid|shading_change_pyramid) extra="-DFFX_SPD_NO_WAVE_OPERATIONS=1";; esac
  python3 "$HERE/../study/inline_adapt.py" "$S" \
    "$SDK/sdk/include/FidelityFX/gpu" "$SDK/sdk/include/FidelityFX/gpu/fsr3upscaler" rgba8 \
    > "$OUT/${p}_adapted.glsl"
  "$G" -V --target-env vulkan1.2 -DFFX_GLSL=1 -DFFX_GPU=1 $BASE $extra \
    -S comp "$OUT/${p}_adapted.glsl" -o "$OUT/${p}_adapted.spv" >/dev/null
  "$CROSS" "$OUT/${p}_adapted.spv" --es --version 320 > "$OUT/${p}_essl320.glsl"
  "$G" -S comp "$OUT/${p}_essl320.glsl" >/dev/null
  echo "$OUT/${p}_essl320.glsl $(sha256sum "$OUT/${p}_essl320.glsl" | cut -d' ' -f1)"
done
