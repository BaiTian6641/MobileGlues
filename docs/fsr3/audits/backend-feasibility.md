# FSR3.1.4 GLES backend feasibility (S2)

Status: **host-only feasibility spike complete. Target device results: NOT RUN.**
Scope: pin and audit AMD FSR3.1.4, inventory the upscaler operations, compile the
pinned shaders through a GLES 3.2 route, and probe the hardest GLES compute
operations on a host. No MobileGlues core, CMake, EGL or FSR1 file was changed.

This report makes no claim that source compilation or a host Mesa pass equals
runtime capability on the target Adreno X1-85 / ANGLE Vulkan / GLES 3.2 stack.

Artifacts:

| File | Content |
| --- | --- |
| `fsr3-source-pin.txt` | tag, commit, dates, SDK/algorithm version macros |
| `fsr3-source-manifest.sha256` | hashes of required upscaler source files |
| `fsr3-pass-bindings.txt` | per-pass SRV/UAV binding inventory |
| `fsr3-gles-probe-host.json` | raw host operation probe |
| `fsr3-port-compile.tsv` | raw 10-pass glslang/SPIRV-Cross/ESSL matrix |
| `fsr3-driver-compile-host.json` | raw Mesa GLES driver compile of the ESSL shaders |
| `fsr3-permutations.txt` | raw 64-permutation compile matrix for three passes |
| `fsr3-port-failures.txt` | captured failure messages and fixes |
| `tools/fsr3_spike/` | probe source, adaptation scripts, reproduce scripts |

## 1. Pinned source

```text
repo    https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK.git
tag     v1.1.4
commit  c6efa6bf7f2027b3ec94f28578bb5965eabb9e55
date    2025-05-08 12:18:31 +0100
subject AMD FidelityFX SDK 1.1.4
SDK     FFX_SDK_VERSION 1.1.4
FSR3    FFX_FSR3_VERSION 3.1.4
Upscaler FFX_FSR3UPSCALER_VERSION 3.1.4
```

Verification used `git ls-remote ... refs/tags/v1.1.4` and `git rev-parse HEAD`
after a depth-1 clone of that tag; both returned the commit above
(`fsr3-source-pin.txt`). SDK main was not used.

Required upscaler files (38 hashed entries in `fsr3-source-manifest.sha256`):

- 10 Vulkan GLSL pass shaders: `sdk/src/backends/vk/shaders/fsr3upscaler/*.glsl`.
- 16 upscaler GPU headers plus the shader-build manifest under
  `sdk/include/FidelityFX/gpu/fsr3upscaler/`.
- 3 host files: `sdk/src/components/fsr3upscaler/ffx_fsr3upscaler.cpp`,
  `_private.h`, `CMakeLists.txt`.
- Public host API: `sdk/include/FidelityFX/host/ffx_fsr3upscaler.h`.
- Shared GPU headers actually included by the shaders: `ffx_core.h`,
  `ffx_core_glsl.h`, `ffx_core_gpu_common.h`, `ffx_core_gpu_common_half.h`,
  `ffx_core_portability.h`, `ffx_common_types.h`, `spd/ffx_spd.h`.

License inventory: all files in the audited upscaler path carry the AMD MIT
header; the repository root and `sdk/LICENSE.txt` are MIT (Copyright (C) 2024
Advanced Micro Devices, Inc.). No LGPL/GPL code is in the FSR3 upscaler path.
The MIT notice must be retained in any port or generated-shader distribution.
Shader sources are supplied as GLSL/HLSL under that same MIT grant.

## 2. Upscaling is separable from frame generation

Two distinct components exist:

- `ffx_fsr3upscaler`: its own context, dispatch description, passes and internal
  resources. It needs no optical-flow or frame-interpolation context.
- `ffx_fsr3`: the combined component. Its CMake target privately links
  `ffx_fsr3upscaler`, `ffx_opticalflow` and `ffx_frameinterpolation`
  (`sdk/src/components/fsr3/CMakeLists.txt`), and its implementation creates
  optical-flow and frame-interpolation contexts.

Only the upscaler is in scope. The upscaler dispatch accepts color, depth,
motion, optional exposure/reactive/transparency, output, jitter, camera and
timing fields. It does not require the frame-generation present/pacing chain.
The upscaler API additionally allows caller-allocated shared resources
(`dilatedDepth`, `dilatedMotionVectors`, `reconstructedPrevNearestDepth`) for
other effects; an upscaling-only backend can own these internally.

## 3. Pass inventory

Default dispatch schedule in `ffx_fsr3upscaler.cpp` (clears/discards excluded):

| # | Pass | Shader | Workgroup | Dispatch at 1707x1067 render / 2560x1600 output |
| ---: | --- | --- | --- | --- |
| 1 | PrepareInputs | `ffx_fsr3upscaler_prepare_inputs_pass.glsl` | 8x8x1 | 214x134 = 28,676 groups |
| 2 | LumaPyramid (SPD) | `..._luma_pyramid_pass.glsl` | 256x1x1 | 27x17 = 459 groups |
| 3 | ShadingChangePyramid (SPD) | `..._shading_change_pyramid_pass.glsl` | 256x1x1 | 27x17 = 459 groups |
| 4 | ShadingChange | `..._shading_change_pass.glsl` | 8x8x1 | 107x67 = 7,169 groups (half render extent) |
| 5 | PrepareReactivity | `..._prepare_reactivity_pass.glsl` | 8x8x1 | 214x134 = 28,676 groups |
| 6 | LumaInstability | `..._luma_instability_pass.glsl` | 8x8x1 | 214x134 = 28,676 groups |
| 7 | Accumulate | `..._accumulate_pass.glsl` | 8x8x1 | 320x200 = 64,000 groups |
| 8 | RCAS (optional) | `..._rcas_pass.glsl` | 16x16x1 | 160x100 = 16,000 groups |

Optional non-default shaders: `debug_view` (debug flag) and
`autogen_reactive` (a separate reactive-mask API, not scheduled by the normal
dispatch). `accumulate_sharpen` is the same source compiled with the sharpen
flag.

Resources and dominant operations:

| Pass | Image writes (FSR3 formats) | Notable operations |
| --- | --- | --- |
| PrepareInputs | dilated motion RG16F, dilated depth R32F, reconstructed depth R32U, farthest depth R16F, current luma R16F | `texelFetch`, `imageStore`, `imageAtomicMin/Max` on R32UI |
| LumaPyramid | SPD mips RG16F x6, frame info RGBA32F, farthest mip R16F, SPD counter R32UI | 256-thread SPD, 4x16x16 float shared arrays, `barrier`, `groupMemoryBarrier`, `imageAtomicAdd`, reads mip5 |
| ShadingChangePyramid | SPD mips RG16F x6, SPD counter R32UI | same SPD shape, plus previous luma/motion/exposure |
| ShadingChange | shading change R8 | samples SPD mip, image store |
| PrepareReactivity | dilated reactive RGBA8, new locks R8, accumulation R8 | texture fetch, image store, no atomics |
| LumaInstability | luma history RGBA16F, luma instability R16F | texture fetch, image store |
| Accumulate | internal upscaled RGBA16F, output (app format), new locks R8 | bicubic 4x4 color sampling, YCoCg, history/LUT/masks, `imageLoad` + clear of locks |
| RCAS | output (app format) | neighborhood sharpening, image store |

Full binding lists are in `fsr3-pass-bindings.txt`. The shaders use separate
`texture2D` + `sampler` objects (Vulkan style), `texelFetch`, `textureLod`,
`textureSize`, `imageLoad`, `imageStore`, `imageAtomicAdd/Min/Max`, shared
memory, workgroup barriers and coherent SSBO/image state. No
`textureGather`, `textureQueryLod`, derivatives, half-pack builtins or 64-bit
atomics are used.

## 4. GLES 3.2 format and operation requirements

ES 3.2 core storage-image formats accepted by glslang/ESSL are a strict subset:
`rgba32f, rgba16f, r32f, rgba8, rgba8_snorm` (float), plus the 32/16/8 integer
and unsigned-integer RGBA forms and `r32i/r32ui`. `r8`, `r16f`, `rg16f`,
`rg16ui` and similar are not core ES image formats. The target provider report
independently says R16F/RG16F (and R8UI/R16UI) do not bind as images; R8 was
not separately proven there, but `r8` is not expressible in ES 3.2 ESSL either.

Substitutions required by the port:

| FSR3 internal | ES image? | Port format | Bytes/pixel compact -> port | Note |
| --- | --- | --- | --- | --- |
| R8_UNORM | no (`r8`) | RGBA8 | 1 -> 4 | accumulation x2, new locks, shading change |
| R16_FLOAT | no (`r16f`) | RGBA16F | 2 -> 8 | luma x2, intermediate, farthest mip, luma instability |
| R16G16_FLOAT | no (`rg16f`) | RGBA16F | 4 -> 8 | dilated motion, SPD mips |
| R32_UINT | yes (`r32ui`) | R32UI | 4 -> 4 | SPD counter, reconstructed depth |
| R32_FLOAT | yes (`r32f`) | R32F | 4 -> 4 | dilated depth |
| RGBA8 | yes | RGBA8 | 4 -> 4 | dilated reactive, optional output |
| RGBA16F | yes | RGBA16F | 8 -> 8 | internal upscaled, luma history, output |
| RGBA32F | yes | RGBA32F | 16 -> 16 | 1x1 frame info |
| R16_SNORM | texture only | R16_SNORM texture | - | 128x1 Lanczos LUT, not a storage image |

R16G16_UNORM is not used anywhere in the FSR3.1.4 upscaler source: the only
occurrences in the pinned tree are DXGI/tooling declarations. Frame generation
and optical flow are separate and use R16G16_SINT and RGBA16F resources, so no
upscaler substitution is needed for those.

The substitution is value-exact for the components FSR3 reads/writes (R8,
R16 and RG16 values occupy the same component positions; no filtering is
applied to the storage images). The cost is allocation and bandwidth, not precision loss.

Additional operation requirements:

- **Read-write formatted images**: ES requires `readonly` or `writeonly` on
  formatted float images except `r32f`; `r32ui` read-write is accepted. FSR3
  reads and writes new-locks (R8), frame-info (RGBA32F) and SPD mips (RG16F) in
  the same shader. The port splits each into a `coherent readonly` alias and a
  `coherent writeonly` alias bound to two image units of the same texture.
  Version 1.1.4 also declares luma history `rgba8` while the host allocates
  RGBA16F; the port must declare `rgba16f` (HLSL has no format).
- **Formatless output image**: FSR3 declares the application output without a
  format. ES/Mesa reject formatless images without
  `GL_EXT_shader_image_load_formatted`, which ANGLE does not expose. The port
  must generate a declaration matching the actual output format (`rgba8` or
  `rgba16f`); both variants compile.
- **SPD mip binding shape**: FSR3 binds six separate image uniforms for mip
  levels 0-5. A GLES backend binds the corresponding level of one mipmapped
  RGBA16F texture to each unit (and the readonly/writeonly aliases to further
  units); it must not allocate six unrelated images if the later sampling pass
  expects a mip chain.
- **Atomics**: `imageAtomicAdd` (SPD counter) and `imageAtomicMin/Max`
  (reconstructed depth) on `r32ui`, plus one `atomicAdd` on a coherent uint
  SSBO path if used. R32UI images are supported by the target provider.
- **Shared memory/barriers**: four 16x16 float arrays (~4 KiB) plus a counter;
  `groupMemoryBarrier()` and `barrier()`. Target shared memory is 32 KiB.
- **Workgroups**: 256x1 and 8x8/16x16; target invocation limit is 1024.
- **Subgroups**: SPD's fast path uses `subgroupQuadSwap*`, which the target
  provider does not expose. The SDK's `FFX_SPD_NO_WAVE_OPERATIONS` LDS fallback
  removes all subgroup ops; both pyramid shaders then compile with zero
  `GroupNonUniform` capabilities. This is the required SPD variant.
- **Separate samplers**: FSR3 GLSL uses `texture2D` plus `sampler`; GLES has
  combined samplers only. SPIRV-Cross combines them, or the port must generate
  combined declarations. The source-level `GL_EXT_samplerless_texture_functions`
  extension is a port artifact, not a target extension.
- **Includes/descriptor sets**: `GL_GOOGLE_include_directive` requires an
  include resolver (MobileGlues' glslang path has none); `set=0` is Vulkan
  only and must be removed. Shader generation must pre-inline includes.
- **Uniforms**: std140 UBOs and std430 SSBOs are reachable through the existing
  glslang/SPIRV-Cross route; the ESSL output uses bound UBOs and combined
  samplers.
- **State**: the internal pass must bind programs, images, UBO/SSBO ranges,
  dispatch, issue the right memory barriers, and restore caller state.
- **Translation path**: the generated ESSL is compiled with the provider's raw
  GLES entry points. It must not be fed through the desktop-GLSL-to-ESSL
  translator a second time; that path exists for application shaders.

## 5. Compiler study

Route studied: pinned Vulkan GLSL -> glslang (Vulkan 1.2 SPIR-V) -> local
SPIRV-Cross -> ESSL 320 -> glslang ESSL validation -> Mesa GLES driver compile.
This mirrors MobileGlues' `glsl_for_es` route, except that the FSR3 sources are
pre-inlined and adapted because the FSR3 distribution is not a desktop GL pack.

Adaptation performed by `tools/fsr3_spike/study/inline_adapt.py`:

1. Recursively inline `#include` (no include resolver in the MobileGlues route).
2. Remove `set = 0,` from layout declarations.
3. Map R8 -> RGBA8; R16F/RG16F -> RGBA16F; correct luma history to RGBA16F.
4. Add the real format to the output image (`rgba8`, or `rgba16f` for HDR).
5. Split read-write float images into coherent readonly/writeonly aliases.
6. Remove `GL_EXT_shader_image_load_formatted`.
7. Compile SPD with `-DFFX_SPD_NO_WAVE_OPERATIONS=1`.

Exact commands (also in `tools/fsr3_spike/study/build_port_study.sh`):

```sh
git clone --depth 1 --branch v1.1.4 \
  https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK.git /tmp/fsr3-spike/FidelityFX-SDK
test "$(git -C /tmp/fsr3-spike/FidelityFX-SDK rev-parse HEAD)" = \
  c6efa6bf7f2027b3ec94f28578bb5965eabb9e55

glslangValidator -V --target-env vulkan1.2 -DFFX_GLSL=1 -DFFX_GPU=1 $BASE $EXTRA \
  -S comp adapted/<pass>.glsl -o out/shaders/<pass>_adapted.spv
spirv-cross out/shaders/<pass>_adapted.spv --es --version 320 \
  > out/shaders/<pass>_essl320.glsl
glslangValidator -S comp out/shaders/<pass>_essl320.glsl

# BASE includes the SDK's base arguments:
#   -DFFX_FSR3UPSCALER_OPTION_UPSAMPLE_SAMPLERS_USE_DATA_HALF=0
#   -DFFX_FSR3UPSCALER_OPTION_ACCUMULATE_SAMPLERS_USE_DATA_HALF=0
#   -DFFX_FSR3UPSCALER_OPTION_REPROJECT_SAMPLERS_USE_DATA_HALF=1
#   -DFFX_FSR3UPSCALER_OPTION_POSTPROCESSLOCKSTATUS_SAMPLERS_USE_DATA_HALF=0
#   -DFFX_FSR3UPSCALER_OPTION_UPSAMPLE_USE_LANCZOS_TYPE=2
# EXTRA for the two SPD pyramids: -DFFX_SPD_NO_WAVE_OPERATIONS=1
```

Raw results (`fsr3-port-compile.tsv`): all 10 shaders produced
`glslang=0, spirv-cross=0, essl_validate=0`. Raw driver results
(`fsr3-driver-compile-host.json`): all 12 ESSL files (10 plus RGBA8/RGBA16F
output variants of Accumulate/RCAS) compiled and linked on Mesa OpenGL ES 3.2
(AMD Radeon gfx1151, Mesa 26.0.0-devel). Permutations: 64/64 for Accumulate,
64/64 for PrepareInputs and 64/64 for LumaPyramid across reprojection Lanczos
type, HDR, low-resolution motion, jittered motion, inverted depth and
sharpening.

This is compile evidence only. It does not show that a pass executes or
produces correct output.

## 6. Host GLES compute probe

Source: `tools/fsr3_spike/probe/probe.cpp`. Build and run:

```sh
make -C tools/fsr3_spike/probe
tools/fsr3_spike/probe/fsr3_gles_probe > out/probe-host.json
tools/fsr3_spike/probe/fsr3_gles_probe --compile-shader out/shaders/*_essl320.glsl
```

Host: Mesa 26.0.0-devel, AMD Radeon gfx1151 (radeonsi), OpenGL ES 3.2.
This is **not** the target Adreno/ANGLE stack.

| Test | Status | Observed |
| --- | --- | --- |
| compute limits | PASS | workgroup 1024^3, invocations 1024, compute images 16, max image units 256 |
| highp float query (fragment proxy) | PASS | range 127, precision 23 |
| R8 image store/load | FAIL | ESSL: `r8` not a core ES image format |
| R16F image store/load | FAIL | ESSL: `r16f` not a core ES image format |
| RG16F image store/load | FAIL | ESSL: `rg16f` not a core ES image format |
| RGBA16F roundtrip + load->SSBO | PASS | exact 0.5/0.25/0.75/1.0 |
| RGBA32F roundtrip + load->SSBO | PASS | exact values |
| R32F roundtrip + load->SSBO | PASS | 0.5 in R |
| RGBA8 roundtrip + load->SSBO | PASS | quantized as expected |
| R32UI image atomics + shared 256 + SSBO atomic | PASS | add=256, max=255, min=0, count=256, shared sum=32640 |
| readonly/writeonly aliases on one texture | PASS | read 0.25, wrote 1.25 to a second texel |
| formatless writeonly image | FAIL | "all image uniforms must have a format layout qualifier" |
| sampler `texelFetch`/`textureLod`/`textureSize` + std140 UBO | PASS | correct fetch, size and UBO values |
| subgroup quad swap | FAIL | `GL_KHR_shader_subgroup_quad` absent |
| 256x1 workgroup | PASS | 256 invocations |

Interpretation: the probe directly exercises the hardest *substituted*
operations. The three failures are handled by the port (format substitution,
explicit output format, SPD no-wave path). Formatless images and hardware
subgroup quad are not required. Still, none of this has executed on the target.

## 7. Device-run package (NOT RUN)

Prepared at `/tmp/fsr3-spike/device-package/`:

- `fsr3_gles_probe_arm64` (NDK r28c, Android API 29 arm64-v8a).
- `shaders/` (10 adapted ESSL shaders plus two HDR output variants).
- `run_device.sh`, `expected-host-probe.json`, `fsr3-port-failures.txt`,
  `sha256sums.txt`.

The exact device commands are:

```sh
adb push fsr3_gles_probe_arm64 /data/local/tmp/fsr3_gles_probe
adb shell chmod 755 /data/local/tmp/fsr3_gles_probe
adb shell /data/local/tmp/fsr3_gles_probe > device-probe.json
adb push shaders /data/local/tmp/fsr3_shaders
adb shell "/data/local/tmp/fsr3_gles_probe --compile-shader /data/local/tmp/fsr3_shaders/*.glsl" \
  > device-shader-compile.json
```

**Device results are NOT RUN.** The reserved C1L63708PZ was not touched. The
first target gate must confirm, on the actual provider: R32UI image atomics,
RGBA16F/RGBA8 alias image units, shared-memory + barriers, 256-thread SPD,
RGBA16F/R32F/RGBA32F image load/store, and driver compile of all 12 ESSL
shaders.

## 8. Cost hypotheses (HYPOTHESIS, not measurements)

Dispatch counts at a 2560x1600 output:

| Mode | Render size | Prepare/Instability/Reactivity | SPD pyramids | ShadingChange | Accumulate | RCAS |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| Quality (1.5) | 1707x1067 | 3 x 28,676 | 2 x 459 | 7,169 | 64,000 | 16,000 |
| Balanced (1.7) | 1506x941 | 3 x 22,302 | 2 x 360 | 5,605 | 64,000 | 16,000 |
| Performance (2.0) | 1280x800 | 3 x 16,000 | 2 x 260 | 4,000 | 64,000 | 16,000 |

All groups are 64 threads except the two SPD pyramids (256). At Quality this is
about 11.3 million invocations per frame, or ~2.8 per output pixel. Eight
dispatches plus four clears/discards per frame are not expected to be a
submission bottleneck, but this must be measured with the provider's timer
queries or Perfetto.

Persistent internal allocation at Quality with the ES substitutions is about
216 MB (including the shared dilated resources); Balanced is about 186 MB and
Performance about 157 MB. Format expansion adds about 62.5 MB at Quality:
luma x2 ~21.9 MB, intermediate ~10.9 MB, accumulation x2 ~10.9 MB, new locks
~12.3 MB, shading change ~1.4 MB, SPD mips ~2.4 MB, farthest mip ~2.7 MB.
These numbers assume max render/upscale size equals the current frame; larger
max sizes increase them.

Likely cost centers, in order:

1. **Accumulate** (full output extent): 4.1M pixels, 8x8 groups, 4x4 bicubic
   color sampling plus history, motion, luma, instability, LUT and lock reads.
   A 20-30 fetch/pixel estimate is ~330-500 MB of reads per frame before cache
   reuse; at 60 fps this is ~20-30 GB/s. This is the dominant hypothesis.
2. **RCAS** (full output, optional): neighborhood sharpening, single-digit
   fetches per pixel, roughly 100-200 MB/frame.
3. **PrepareInputs / PrepareReactivity / LumaInstability** (render extent):
   each touches 3-9 textures and writes 2-5 images. Each is tens of MB/frame;
   together they are comparable to Accumulate at reduced scale.
4. **SPD pyramids**: only quarter/half of render extent in mip 0, but use
   256-thread groups, 4 KiB shared memory, `imageAtomicAdd` and workgroup
   barriers. The no-wave path is slower than the subgroup path by design; the
   absolute work is small, but synchronization and atomics can serialize.
5. **ShadingChange**: half-render extent, one 4-byte write per pixel; small.

No bandwidth, frame-time, power or thermal claim is made. The project's
performance gate still requires measurements against native and bilinear
controls at the same scene resolution.

## 9. ABI v1 mapping

The frozen contract is `/home/polar/MobileGlues/docs/fsr3/frame-contract.md`.
No competing contract is introduced here.

| ABI v1 field | FSR3 use | Conversion / validation |
| --- | --- | --- |
| `color` | input color texture (render res) | sample as float; HDR/LDR is selected by `color_space` and the context's HDR flag |
| `depth` | input depth texture | 32-bit depth expected; `depth_reversed` maps to inverted-depth flag, `depth_infinite_far` to infinite flag |
| `motion` | input motion texture | current-to-previous, matching FSR3; pixel units convert to `motionVectorScale = 1/render_width, 1/render_height`; `motion_includes_jitter` selects jitter cancellation |
| `exposure` | optional 1x1 exposure | absent -> default exposure with positive `pre_exposure`; `frame_delta_seconds` -> `frameTimeDelta * 1000` |
| `reactive` | optional reactive mask | absent -> 1x1 zero/default reactive; do not silently mark every pixel reactive |
| `transparency` | optional composition/alpha | absent -> default 1x1 mask |
| `output` | output image | format must be RGBA8 or RGBA16F and image-bindable for the studied variants; `0` -> backend-owned output; other formats need their own declared variant; alpha policy explicit |
| `render_*`, `output_*` | dispatch sizes | FSR3 has no active-rectangle parameter; non-zero active origins require a view/copy or a backend-side crop before dispatch |
| `color_active`, `output_active` | active rectangles | backend must enforce these; do not infer from allocation |
| `jitter_x/y` | jitter | FSR3 expects render-pixel jitter in [-1, 1]; contract matches |
| `motion_scale_x/y` | not passed through | backend derives scale from pixel/UV conversion; the ABI fields are producer-side metadata, not FSR3 parameters |
| `camera_near/far/fov_y` | DeviceToViewDepth, TanHalfFov | radians and world units; FSR3 computes the depth transform |
| `frame_delta_seconds` | DeltaTime | FSR3 field is milliseconds |
| `pre_exposure` | DeltaPreExposure | must be > 0 |
| `reset_history`, `resource_generation` | context/accumulation reset | reset clears history; a generation change requires reset |
| `motion_confidence` | adapter policy | camera-only (1) is experimental per contract; FSR3 does not invent entity motion |
| `color_space` | HDR permutation | changing it requires a matching pipeline variant/context |
| `alpha_premultiplied`, `output_alpha_mode` | output policy | not interpreted by the algorithm; backend composition must state its convention |

The backend-owned intermediates, history, aliasing and reset handling are
consistent with the contract's "Backend ownership" section. FSR3's optional
shared `dilatedDepth`/`dilatedMotionVectors`/`reconstructedPrevNearestDepth`
resources can stay backend-private for this upscaling-only milestone.

## 10. Bounded decision

**Decision: continue the GLES backend to the target-dispatch gate; do not switch
to Vulkan interoperation yet.** The target now passes the compile/operation gate,
but the passes have not executed and no cost has been measured. It is the bounded
next step before either GLES continuation or the Vulkan chain can be selected.
No host-visible requirement failed that the
documented substitutions cannot express. Specifically:

- The three unsupported image formats have exact RGBA8/RGBA16F replacements.
- The required R32UI atomics and shared-memory path pass on the host GLES 3.2
  driver (target NOT RUN).
- The missing subgroup-quad path has an SDK-owned no-wave fallback.
- All 10 upscaler shaders and 64 permutations of the three heaviest compile
  through the candidate route and on the host GLES driver (target NOT RUN).
- The ES read-write-image restriction has a working alias-unit shape on host.

This is not a capability proof. The next gate is entirely on the target:

1. Run `fsr3_gles_probe` and the 12-shader `--compile-shader` package on the
   assigned device. Mark each operation PASS/FAIL; do not treat compile as
   runtime.
2. If alias images or R32UI atomics fail on ANGLE/Adreno, record the exact
   operation. A bounded restructuring exists before giving up: move SPD mips
   6+ into a second small pass, clear new-locks in a dedicated pass, and move
   the 1x1 frame-info read/write to a coherent SSBO. If those still fail, stop
   the GLES route. The fallback chain to prove is: provider
   GL texture <-> `AHardwareBuffer`/`EGLImage` import into a Vulkan FSR3
   upscaler, matching formats for color/depth/motion/output, native-fence
   ownership, and no CPU readback or device-wide wait.
3. If the device probe passes, compile and execute one real pass with synthetic
   inputs (RCAS or ShadingChange first), then the full 8-pass chain, comparing
   against the project's Vulkan reference. Only then measure per-pass cost.
4. Correct the v1.1.4 luma-history declaration (`rgba8` -> `rgba16f`) and keep
   the correction visible in the port's source manifest.
5. Do not claim FSR3 availability until the full chain, image quality and cost
   gates have target evidence.

Remaining known risks: ANGLE validation of aliased coherent image units;
Adreno scheduling of 256-thread SPD with 4 KiB shared memory; bandwidth from
the format expansion; and the absence of any target timing. These are gates,
not proven blockers.

## 11. Target-device probe and shader compile (2026-10-20)

The coordinator ran the packaged probe on the reserved HP Googlebook 14
`C1L63708PZ` after stopping the game process. The first binary failed to load
because it linked the NDK's shared libc++; the build script now uses
`-static-libstdc++`, and the results below are from that corrected binary.

- Renderer: `ANGLE (Qualcomm, Vulkan 1.4.295 (Adreno (TM) X1-85 (0x43050C01)),
  Qualcomm Technologies Inc. Adreno Vulkan Driver-512.886.6)`
- Version: `OpenGL ES 3.2 (ANGLE 2.1 git hash: 89ef25f8f92e103b5bec64e19e36f308aac15164)`
- Probe: **14/22 PASS**, 8 expected FAIL. The failures are the three unsupported
  image qualifiers (`r8`, `r16f`, `rg16f`, each store and load), formatless
  writeonly images, and the absent subgroup-quad extension. All required
  substituted operations PASS: RGBA8/RGBA16F/R32F/RGBA32F image round trips,
  R32UI image atomics, 256-thread shared-memory reduction, SSBO atomics,
  readonly/writeonly image aliases on one texture, combined samplers,
  texelFetch/textureLod/textureSize, std140 UBO, and a 256x1 workgroup.
- Shader compile: **12/12 PASS**. The 10 base ESSL 320 pass shaders plus the
  RGBA8 and RGBA16F output variants of Accumulate/RCAS all compiled and linked
  through ANGLE/Adreno.

Raw evidence:

- `docs/fsr3/validation/2026-10-20-x1e-gles-probe.json`
  SHA-256 `e12a6e9fa13f460421beb25c069e8f6feb667a3f3931e255ce1f71c2b19fb50c`
- `docs/fsr3/validation/2026-10-20-x1e-fsr3-shader-compile.json`
  SHA-256 `f9c6f207039216c50bc26e8d82178f6035e998e0df976ea48f5599d25ebf607c`

This closes the **target compile/operation** portion of the S2 gate, not the
dispatch gate. No FSR3 pass has executed and no image has been reconstructed on
the device. The device package's generated ESSL files are not committed; the
harness regenerates them, and the committed JSON binds the exact files that
were compiled.
