# FSR3.1.4 GLES dispatch gate (S2-E)

Status: **EXECUTED on target.** One adapted FSR3.1.4 pass (RCAS) and the full
eight-pass upscaler chain ran on the reserved HP Googlebook 14 `C1L63708PZ`
through ANGLE/Adreno, with pixel readback validated against declared expected
results. A cleared or merely changed buffer would not satisfy these checks.

This report distinguishes four evidence classes:

| Class | Meaning |
| --- | --- |
| **EXECUTED + VERIFIED** | A real adapted pass or the full chain dispatched on the target; output and selected state read back and compared with a declared expected result or a published host reference hash. |
| **EXECUTED (parity only)** | The pass ran and its readback matched the host reference, but the specific value has no independent analytic model (e.g. the shading-change pyramid on a step frame). |
| **COMPILE-ONLY** | Compiled/linked on the target, not dispatched here (all eight passes are dispatched; the two extra output-format variants from the earlier gate remain compile-only). |
| **NOT RUN** | Multi-frame history, reset/resource-generation behaviour, ABI wiring, production submission and timing. |

Commits:

| Role | Commit |
| --- | --- |
| Base (integration state) | `1c96533` |
| Single-pass harness + evidence method | `0524d56` |
| Full-chain harness | `d77dd8c` |
| This report + raw evidence | `ae2b4c3` |

No MobileGlues core, CMake, EGL, FSR1, ABI or integration-branch file was
changed. No SDK source or binary is vendored; the harness regenerates the ESSL
from the pinned tree.

## 1. Target and host

Target (`C1L63708PZ`):

- `GL_RENDERER`: `ANGLE (Qualcomm, Vulkan 1.4.295 (Adreno (TM) X1-85
  (0x43050C01)), Qualcomm Technologies Inc. Adreno Vulkan Driver-512.886.6)`
- `GL_VERSION`: `OpenGL ES 3.2 (ANGLE 2.1 git hash: 89ef25f8f92e103b5bec64e19e36f308aac15164)`
- `GL_VENDOR`: `Qualcomm`

Host reference (Mesa, not a capability claim):

- `GL_RENDERER`: `AMD Radeon Graphics (radeonsi, gfx1151, ACO, DRM 3.64, 7.0.0-38-generic)`
- `GL_VERSION`: `OpenGL ES 3.2 Mesa 26.0.0-devel`

## 2. Pinned source and generated shaders

All shaders come from the pinned AMD tree
`c6efa6bf7f2027b3ec94f28578bb5965eabb9e55` (SDK v1.1.4, FSR3.1.4). The
`make_shaders.sh` script refuses to run on any other commit. Route:
`inline_adapt.py` (committed) -> glslang Vulkan 1.2 -> local SPIRV-Cross
`--es --version 320` -> glslang ESSL validation.

Build flags are the SDK's candidate set:
`-DFFX_FSR3UPSCALER_OPTION_UPSAMPLE_SAMPLERS_USE_DATA_HALF=0`,
`..._ACCUMULATE_...=0`, `..._REPROJECT_...=1`,
`..._POSTPROCESSLOCKSTATUS_...=0`, `..._UPSAMPLE_USE_LANCZOS_TYPE=2`, and
`-DFFX_SPD_NO_WAVE_OPERATIONS=1` for both SPD pyramids.

Adaptations (from the S2 compile study, unchanged): inlined includes, removed
Vulkan descriptor `set=`, R8->RGBA8, R16F/RG16F->RGBA16F,
RGBA8/RGBA16F concrete output image, read-write float images split into
coherent readonly/writeonly aliases at additional image units, luma history
declared `rgba16f` to match the allocation, formatless image extension removed.

Exact generated ESSL hashes (SHA-256, also embedded per pass in the chain JSON):

| Pass | SHA-256 |
| --- | --- |
| prepare_inputs | `a624b575604835ed99cc82bd65c43bcdc952db74437ffd4029c8417b20669960` |
| luma_pyramid | `25c6aebaae4835eff3bcc3362c5bb73ab20018b0a3dd9cf4358cc36862c03c56` |
| shading_change_pyramid | `3bfd9cd89b747ce735cd4e222d78c63db1cfe509ecdf977492e930ebb5b3f130` |
| shading_change | `0835f5e00e38652d7f4470dbe1f8acccc015f09b9b5375bbc03e08594e25a052` |
| prepare_reactivity | `821a384220c16449a89eff8a4d97463c91c1cfbe1e16d2a0263dc4552076374f` |
| luma_instability | `b8425c8503ca6371a8f37e640262aba614c08ec201469c4d535529963ee2a153` |
| accumulate | `f02e2a4f97841efa1ed124066e353091e0aca5fc043d0471489cdc2c45f22b16` |
| rcas | `b64ac08df15ef7ae92176881cd29696e92e02acecbb2e11f60f19aecc6aeaf7d` |

## 3. Harness

`tools/fsr3_spike/dispatch/fsr3_dispatch.cpp` (test harness, not a backend):

- `--mode rcas`: single RCAS pass.
- `--mode chain`: the eight-pass schedule from
  `ffx_fsr3upscalerDispatch`, frame 0 with reset.
- Links the adapted ESSL by path; does not embed or translate it.
- Parses each shader's `layout(binding=...)` image/UBO declarations and
  sampler names, resolves them against a resource table built with the SDK
  frame-0 ping-pong choices, and records every resolved binding in the JSON.
- Creates legal ES 3.2 resources only. UBOs are std140; combined samplers use
  texture units; storage images use the declared image units; read-only and
  write-only aliases of one texture bind at distinct units.
- After every dispatch: `glMemoryBarrier(GL_ALL_BARRIER_BITS)`, `glFinish()`.
  The production backend must use tighter barriers; the harness uses the
  conservative set deliberately.
- Reads back through an FBO + `glReadPixels`. **This path exists only for the
  test harness; no production design or performance claim uses CPU readback.**

Build and run (host):

```sh
make -C tools/fsr3_spike/dispatch
tools/fsr3_spike/dispatch/make_shaders.sh                 # needs the pinned tree
tools/fsr3_spike/dispatch/fsr3_dispatch \
  --shader /tmp/fsr3-spike/out/shaders/rcas_essl320.glsl --width 320 --height 180
tools/fsr3_spike/dispatch/fsr3_dispatch --mode chain \
  --shaders /tmp/fsr3-spike/out/shaders --pattern flat --render 160x90 --output 320x180
```

Build and run (target; serializes all device use):

```sh
tools/fsr3_spike/dispatch/build_android.sh
tools/fsr3_spike/dispatch/run_device.sh C1L63708PZ
```

`run_device.sh` produced the six committed device JSON objects through the
repository's own harness; no ad-hoc commands were used for the evidence.

## 4. Single pass: RCAS

Reference: a CPU fp32 port of `FsrRcasF` from the pinned
`ffx_fsr3upscaler_rcas.h` (`-ffp-contract=off`), including the exact
`ffxApproximateReciprocalMedium` bit trick and the ESSL operation order.
Input, resources and bindings:

| Item | Value |
| --- | --- |
| Input | synthetic RGBA8, hard vertical step + horizontal step + ramp + constant patch, 320x180 or 640x360 |
| RCAS input sampler | `GL_RGBA8`, NEAREST, unit 0 |
| Exposure sampler | `GL_R32F` 1x1 = 1.0, unit 1 |
| Output image | `rgba8 writeonly` on `GL_RGBA8`, image unit 2, seeded with a magenta sentinel |
| `cbFSR3UPSCALER_t` | std140, binding 3, 160 B (block data size 148 on ANGLE, 160 on Mesa) |
| `cbRCAS_t` | std140, binding 4, 16 B; `con.x = asuint(exp2(-(2-2*sharpness)))` |
| Dispatch | `(ceil(W/16), ceil(H/16), 1)`, local size 64x1 (each thread writes four 2x2-RCAS pixels) |

Comparison region: interior pixels, one-pixel border excluded because
out-of-range `texelFetch` is undefined in ES. Declared tolerance: every
interior pixel within 1 8-bit level and >=99.9% byte-exact. Additional declared
invariants: a constant 5-neighborhood reproduces its value exactly; alpha is
255; the pass changes the step neighborhood; a repeated dispatch is
byte-identical.

Results on `C1L63708PZ`:

| Run | Interior | Exact | Max diff | Repeat | Output SHA-256 |
| --- | ---: | ---: | ---: | --- | --- |
| 320x180, sharpness 0.2 | 56,604 | 56,604 (100%) | 0 | identical | `4463cdf65617df8a889aa0331e68777a56d0b76811218ea9a094a08f0af71819` |
| 640x360, sharpness 1.0 | 228,404 | 228,404 (100%) | 0 | identical | `6e654a8766f7cdb4a752bd59cc4769144227f3bba8a9396af71e6b71f944540e` |

The host Mesa run produced the same two hashes byte-for-byte. No GL errors were
reported at any stage. **EXECUTED + VERIFIED.**

## 5. Full eight-pass chain

### 5.1 Schedule, extents, dispatch groups

Order: PrepareInputs -> LumaPyramid -> ShadingChangePyramid -> ShadingChange ->
PrepareReactivity -> LumaInstability -> Accumulate -> RCAS. Bar: full memory
barrier and `glFinish` after each pass.

| Pass | Local size | Groups at 160x90->320x180 | Groups at 320x180->480x270 |
| --- | --- | ---: | ---: |
| prepare_inputs | 8x8 | 20x12 | 40x23 |
| luma_pyramid | 256x1 | 3x2 (6 groups) | 5x3 (15 groups) |
| shading_change_pyramid | 256x1 | 3x2 | 5x3 |
| shading_change | 8x8 | 10x6 | 20x12 |
| prepare_reactivity | 8x8 | 20x12 | 40x23 |
| luma_instability | 8x8 | 20x12 | 40x23 |
| accumulate | 8x8 | 40x23 | 60x34 |
| rcas | 64x1 | 20x12 | 30x17 |

SPD constants follow `ffxSpdSetup`: work-group offset 0, `numWorkGroups` =
groupsX*groupsY, `mips` = min(floor(log2(max(renderW,renderH))),12) (7 and 8
for the two sizes). The frame-0 constants are the host formulas: previous
frame sizes 0, jitter 0, `deviceToViewDepth` from near 0.1 / far 1000 / 60 deg
FOV, `jitterPhaseCount = int(8 * (upscaleWidth/renderWidth)^2)`, delta time
0.016, delta pre-exposure 1, accumulation added per frame 1/3, minimum
disocclusion accumulation -1/3, frame index 0.

### 5.2 Resources and formats

All internal resources use the documented ES substitutions; the JSON records
the concrete format and access of every binding per pass.

| SDK resource | Port format | Size | Notes |
| --- | --- | --- | --- |
| input color | `GL_RGBA8` | render | synthetic frame |
| input depth | `GL_R32F` | render | constant 0.5 |
| input motion | `GL_RG16F` | render | zero |
| exposure | `GL_R32F` 1x1 | 1.0 |
| reactive / transparency | `GL_R8` 1x1 | zero (default masks) |
| reconstructed previous depth | `GL_R32UI` | render | cleared to float-bits 1.0 |
| dilated motion | `GL_RGBA16F` (R16G16F substitution) | render |
| dilated depth | `GL_R32F` | render |
| accumulation 1/2 | `GL_RGBA8` (R8 substitution) | render | ping-pong, cleared |
| luma 1/2 | `GL_RGBA16F` (R16F substitution) | render | cleared |
| luma history 1/2 | `GL_RGBA16F` | render | cleared; declared `rgba16f` |
| intermediate fp16x1 | `GL_RGBA16F` | render | farthest depth / luma instability alias |
| internal upscaled color 1/2 | `GL_RGBA16F` | upscale | ping-pong |
| new locks | `GL_RGBA8` (R8) | upscale | cleared |
| dilated reactive masks | `GL_RGBA8` | render | 4 channels: reactive, disocclusion, shading change, accumulation |
| shading change | `GL_RGBA8` (R8) | render/2 |
| SPD mips | `GL_RGBA16F` (RG16F) | render/2 | one texture, 6 bound levels, full mip chain allocated |
| farthest depth mip1 | `GL_RGBA16F` | render/2 |
| SPD atomic counter | `GL_R32UI` 1x1 | cleared |
| frame info | `GL_RGBA32F` 1x1 | cleared to (-1,1,0,0) |
| output | `GL_RGBA8` | upscale |

Read-only/write-only image aliases are used exactly where the port requires
them (`r_rw_*` + `rw_*` on the same texture/level): frame info, new locks,
SPD mip levels 0-5. The frame-0 ping-pong selection matches the SDK
(`ACCUMULATION_1` source / `_2` destination, `INTERNAL_UPSCALED_COLOR_1` source
/ `_2` destination, `LUMA_HISTORY_1` source / `_2` destination, luma 1 current
/ luma 2 previous, RCAS input = internal upscaled color destination).

### 5.3 Declared expected results

The chain has no CPU model of the whole algorithm. Two declared references are
used, both stated in the JSON:

1. **Analytic flat-frame invariants** (independent of any GPU):
   - reconstructed depth bits exactly `0x3f000000` (depth 0.5) everywhere;
   - dilated depth exactly 0.5, dilated motion exactly 0;
   - current luma = `dot(rgb, (0.2126,0.7152,0.0722))` within 2e-3;
   - farthest depth = `y/(0.5-x) * viewSpaceToMeters` = 0.19998 m within 2e-3;
   - SPD mip5 = `(max(6.1e-5, log(luma)), luma)` within 5e-3, atomic counter
     reset to 0 by the last work group;
   - farthest-depth mip1 = 0.19998 m;
   - each shading-change pyramid mip is flat (spread 0);
   - final output must reproduce the input color: channel spread <= 1 and
     channel difference <= 3 over all 320x180 / 480x270 pixels.
2. **Differential reference**: the same adapted ESSL executed on host Mesa GLES.
   Output and five state buffers are SHA-256 hashed; the target must match the
   host bytes. This is *not* an independent Vulkan reference; it proves
   implementation parity, while (1) provides the independent value checks.

### 5.4 Results

All six target runs are **PASS**; the flat-frame numeric checks passed at both
sizes, and every published state/output hash equals the host reference.

| Run | Target status | Output SHA-256 | Host match |
| --- | --- | --- | --- |
| 160x90 -> 320x180, flat | PASS (18/18 checks) | `010e45072c02a4ab360a2d54758c096b6d5eadeaf78ec73559e66d64748daee2` | yes |
| 160x90 -> 320x180, step | PASS | `3babf435ebe710883253ec2aadb0c1e83bd0d4f071e5e8efd8debe45a1a371a4` | yes |
| 320x180 -> 480x270, flat | PASS | `b2a26fa9b66da16c3f558b7ad33a02d57e633d258127d0c58e84e3025dcae8ec` | yes |
| 320x180 -> 480x270, step | PASS | `d32ad243eb38c7720a18b8d47ee10f8f7ade114196eee39b45b867516c9666bc` | yes |

Shared state hashes at 160x90 (identical in flat and step runs, and on both
implementations):

```text
reconstructed_depth 864166db82d567116e650fbee347d086a29071398c9386a3b59e00d21df66ee9
dilated_motion      d8b443032200e143b1c49820b6d78c32d519553a3f63a38f48e8bc31da084f0a
spd_mip5            374708fff7719dd5979ec875d56cd2286f6d3cf7ec317a3b25632aab28ec37bb
shading_change      9202e68fbe5587e30738f2f118e87e10202a6eabd7297e1299978ed53c3a8f46
dilated_reactive    6d6f368a78a3311505f9333eb9e51c24577d66124672d53a4f4d338752eb23c8
```

The flat output was `96,128,191..192` for an input of `96,128,192` (spread 1,
max channel difference 1). The step run changed pixels between 16 and 239, so
the chain is not a pass-through; its exact pixels match the host reference but
have no independent analytic model (parity-only for those values).

## 6. Raw evidence

| File | Content | SHA-256 |
| --- | --- | --- |
| `docs/fsr3/validation/2026-10-20-x1e-rcas-dispatch.json` | two device RCAS runs, full JSON | `9728b3fb43bba715cdfba4a38a4e8860979af20564641ab3ed36fb2a8c171d92` |
| `docs/fsr3/validation/2026-10-20-x1e-fsr3-chain-dispatch.json` | four device chain runs with per-pass bindings, checks and hashes | `b3bf613728ce64be36ab651736b8131d4460a1639b14eef8d936dd70290f2ad3` |
| `docs/fsr3/validation/2026-10-20-host-reference.json` | declared host reference hashes/statuses | `65af1eb0201b348d822ce7c5932e7efd91f8b4d93475f92438d94ceba1ed640a` |

Each device JSON contains the renderer string, driver, shader path and hash,
resource bindings per pass (texture format, sampler unit, image unit/level/
access), UBO bindings, constants, dispatch groups, local sizes, barrier,
readback format and every check with its measured values. Raw logs from the
runs are the program's stderr (per-resource first bytes) and are reproducible
with `run_device.sh`.

## 7. Interpretation and limits

- **Production readback**: none. The harness readback is FBO + `glReadPixels`
  and is excluded from any backend or performance claim.
- **Execution is not image quality.** The chain executes and matches the host
  implementation, but no Vulkan FSR3 reference frame has been compared, so this
  is not an image-quality or API-conformance result.
- **History**: only frame 0 with reset. Multi-frame accumulation, ping-pong
  rotation across frames, exposure/history convergence, reset on resource
  generation and jitter sequences were **NOT RUN**.
- **ABI**: no `frame-contract.md` field was consumed by this harness; the
  resources were sized and initialized by the test, not by MobileGlues.
- **Sizes**: the chain was exercised at 2x (160x90) and 1.5x (320x180 render)
  with synthetic content. Real 2560x1600 output, real depth/motion data and
  reactive/transparency masks were **NOT RUN**.
- **Timing/brandwidth**: no timer query or Perfetto capture was taken. The
  conservative per-pass `glFinish` makes these runs unsuitable for cost
  measurement.
- **Blockers**: none for the dispatch gate itself. The next dependency is a
  production backend path (no readback, tighter barriers, per-frame history and
  resource-generation reset, ABI-driven formats before dispatch) plus a
  Vulkan-reference or IQ comparison before any quality claim.

## 8. Verdict

The GLES compute route can run the pinned FSR3.1.4 upscaler: all eight adapted
passes execute on the target, the single RCAS pass is byte-exact against a CPU
model, the full chain satisfies independent flat-frame invariants at two
extents, and the target reproduces the host implementation bit-for-bit on the
published state and output hashes. The remaining risk has moved from
"does it execute" to history/reset semantics, production integration, image
quality against a Vulkan reference, and cost.
