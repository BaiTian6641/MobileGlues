# Complementary performance model and X1E polish plan

Date: 2026-10-19  
Device: HP Googlebook 14 (`C1L63708PZ`), X1E80100, Adreno X1-85 (3.8 TFLOPS class),
ANGLE Vulkan / ES 3.2 through MobileGlues. Pack: Complementary Reimagined r5.9.3
(SHA-256 `fed6c879…b484b38`), original, 2880×1585.

## Measured profile cost

User-run F3 values; shaders-off reference ≈500 FPS (≈2.0 ms/frame). Frame
budgets below are derived, not GPU-counter measurements.

| Profile | FPS | Frame ms | GPU work above shaders-off |
|---|---:|---:|---:|
| POTATO | 55 | 18.2 | 16.2 ms |
| VERYLOW | 48 | 20.8 | 18.8 ms |
| LOW | 35 | 28.6 | 26.6 ms |
| HIGH | 20 | 50.0 | 48.0 ms |

Feature deltas implied by the pack profile table (`shaders.properties:2-9`):

| Delta | Approx. cost |
|---|---:|
| Shadows at 1024 / 64 (VERYLOW vs POTATO) | ~2.6 ms |
| WATER_REFLECT 1→2, BLOCK 0→1, DETAIL 0→2, CLOUD 1→2 (LOW vs VERYLOW) | ~7.8 ms |
| Shadow 0/96→2/192, BLOCK 1→3, LIGHTSHAFT 0→2, ENTITY_SHADOW + FXAA (HIGH vs LOW) | ~21.4 ms |

The High-specific 21 ms block is shadows, light shafts, block reflections, entity
shadows and FXAA. Water/cloud/detail changes cost about 8 ms going from VERYLOW
to LOW. The pack source matches this: High uses a 2048² shadow map at 192 blocks,
30-step SSR plus 6 refinement steps at 0.5 reflection scale
(`reflections.glsl:83-89`), 12 cloud samples (`reimaginedClouds.glsl:47-51`),
10 light-shaft samples (`volumetricLight.glsl:88-94`) and an 8-neighbourhood TAA
path (`taa.glsl:111`).

## Hardware expectation

- Adreno X1-85 is a ~3.8 TFLOPS class GPU; a Radeon 780M is ~8.9 TFLOPS
  (published comparison: <https://www.notebookcheck.net/Qualcomm-Adreno-X1-85-3-8-TFLOPS-GPU-Benchmarks-and-Specs.763558.0.html>,
  <https://technical.city/en/gpu/GeForce-GTX-1060-mobile-vs-Radeon-780M>).
- The ratio is ~0.43. 20 FPS on X1-85 therefore projects to roughly 45 FPS on a
  780M at the same resolution/settings. The current High result is near hardware
  parity; a 2× MobileGlues-only gain is not available without reducing work.
- Shaders-off ≈500 FPS shows the JVM/Sodium/MobileGlues submission path is not
  the 20 FPS limiter. The limiter is GPU shader/fill work.
- The realistic X1E targets are 30 FPS at 1920×1200 on a Balanced profile and
  native 2560×1600 only after effect cuts. 40–60 FPS at High/2880×1585 is not a
  realistic X1-85 target.

## Recommended X1E Balanced profile (existing pack options, no shader edits)

```
SHADOW_QUALITY=1
shadowDistance=128
WATER_REFLECT_QUALITY=1
BLOCK_REFLECT_QUALITY=1
LIGHTSHAFT_QUALI_DEFINE=1
CLOUD_QUALITY=1
DETAIL_QUALITY=2
FXAA_DEFINE=1
SSAO_QUALI_DEFINE=2
ENTITY_SHADOW=1
RAIN_PUDDLES=0
COLORED_LIGHTING=0
WORLD_SPACE_REFLECTIONS=-1
```

Expected position: between LOW (35 FPS) and HIGH (20 FPS) at 2880×1585; at
1920×1200 the pixel count is 0.505×, so a 30 FPS target is plausible. This is a
measurement plan, not a claim.

## Optimization order

1. **Resolution** to 1920×1200 or a 0.67–0.75 render scale. Largest lever.
2. **Balanced profile** above; measure each group (shadows, light shafts,
   reflections, clouds) separately.
3. **Wrapper A/B**: `enableExtDirectStateAccess=false` vs true. Iris detects DSA
   from the wrapper; disabling it selects classic bind/restore paths and can
   change CPU cost. Also compare with image-bind diagnostic probing disabled.
4. **Pack shader specialization** only if needed: clamp bounded SSR/voxel loops
   or reflection resolution under an explicit Performance option, never silently.
5. **Hardware RT** is not available through GLES/ANGLE. The X1E Vulkan device
   exposes `VK_KHR_ray_query` and `VK_KHR_acceleration_structure`, but no
   ray-tracing pipeline and no ANGLE GLES frontend. A separate Vulkan renderer
   would be required; it is outside the MobileGlues port.

## Next gate

At 1920×1200, record: shaders off, High, Balanced, and one-effect-off variants
(shadows/light shafts/clouds/reflections), 10-minute steady state. Keep WSR and
player reflections off for the X1E standard target; WSR128 remains the advanced
staged target.

## FP16 / mediump assessment

Device probe on `C1L63708PZ` (raw ANGLE ES context, not MobileGlues):

- `highp` contract: range 127, precision 23 (FP32).
- `mediump` contract: range 15, precision 10 (FP16).
- `GL_EXT_shader_explicit_arithmetic_types_float16` is **not** exposed.
- X1-85 published peak: 4.61 TFLOPS FP32 / 9.22 TFLOPS FP16 for the 4.6 TFLOPS
  variant; X1E-80-100 is the lower 3.8/7.6-class SKU.
- A dependent vec4 ALU loop (200k iterations) through ANGLE:
  - `highp`: **7.35 ms**
  - `mediump`: **3.19 ms**
  - ratio ≈ **2.3×**

### What this means

FP16 arithmetic is real and can reach the advertised ~2× ALU rate, but it is
not a global switch:

- Shaderpacks predominantly use `highp`/FP32. MobileGlues and SPIRV-Cross emit
  highp for world-space math; changing that wholesale is unsafe.
- FP16 has 10-bit mantissa and ±15 exponent range (~65504 max). It is unsafe
  for world/view positions, depth reconstruction, shadow coordinates, matrices,
  ray origins/directions, temporal history, ACT/WSR voxel indices, atomic
  indices and packed-ID buffers.
- Texture/fill/bandwidth-limited work does not speed up from FP16 ALU. RGBA16F
  storage is already used by the packs; storage format is not the same as
  arithmetic precision.
- `mediump` is permission, not a guarantee; the compiler may promote some
  expressions. Per-pass measurement is required.

### Recommended implementation route

1. Preserve shaderpack-authored `mediump`/`lowp`; do not force highp over it.
2. Add an **opt-in precision mode** with an explicit allowlist: full-screen
   color/tonemap/bloom/composite accumulation first; exclude vertex, depth,
   shadows, TAA, SSR/WSR, ACT voxelization and packed-ID passes.
3. Apply per-variable/per-expression `mediump` via AST or a validated source
   pass; never rewrite all `highp` tokens.
4. A/B each pass for FPS and visual reference; rollback per pass.
5. Treat FP16 as a polish optimization, not a substitute for resolution,
   shadow/cloud/SSR cuts. Expected overall gain is pass-dependent and below the
   ALU ratio because High is also fill/texture-bound.
