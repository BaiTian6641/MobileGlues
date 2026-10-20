# iterationRP shader-transform compatibility layer

Date: 2026-10-19  
Target: Android 17 / X1E80100 / Adreno X1-85 / ANGLE Vulkan / ES 3.2.

## Implemented wrapper transforms (no shaderpack edits)

- `gl/glsl/compat_image.*`: `layout(rgba16)`/`layout(rg16)` storage-image
  qualifiers become `rgba32f`/`rg32f`; whole-qualifier matching preserves
  `rgba16f`/`rg16f`. Null-data unorm16 image storage is allocated as
  `RGBA32F`/`RG32F`; the same mapping feeds `mg_image_backing_format`.
- `gl/glsl/compat_subgroup.*`: for compute shaders with no hardware subgroup
  extension:
  - virtual subgroup width 32; `gl_SubgroupInvocationID`, `gl_SubgroupSize`,
    `gl_SubgroupID`, `gl_NumSubgroups` map to local-invocation arithmetic;
  - `subgroupInclusiveAdd` becomes a barriered shared-memory 32-lane scan;
  - generic quad swaps become shared-memory swaps;
  - FidelityFX SPD's `SpdReduceQuad` and mip 3/4/5 definitions are replaced so
    every invocation reaches the barriers and only owners load/store;
  - all `GL_KHR_shader_subgroup_*` / `GL_NV_shader_subgroup_partitioned`
    extension lines are removed;
  - unsupported residual constructs produce `#error MG_SUBGROUP_UNSUPPORTED`.
- `gl/glsl/compat_subgroup_partition.*`: rewrites the NV partition /
  KHR-XOR-shuffle BackwardAnalysis pattern to a shared open-addressed partition
  table with uniform barriers and leader-only image atomics.
- Pre-pass ordering: image formats, const-transpose lowering, reserved
  `active` rename, then subgroup/NV-partition lowering.

## Verification

- Host tests: `sh MobileGlues-cpp/tests/run.sh` PASS, including
  `compat_image`, `compat_subgroup`, and `compat_subgroup_partition`.
- Full host sweep of the **unmodified original** pack: all **70/70** compute
  shaders compile after include inlining + the actual C++ transforms
  (`glslangValidator`), including `composite.csh` and `composite72.csh`.
- Wrapper GPU probe: 58/58 PASS.
- Device note: the previous `composite72.csh` syntax error was reproduced as a
  pre-pass bug (`rgba16f` became `rgba32ff` because the replacement matched a
  prefix). It is fixed and covered by `compat_image_test`; device confirmation
  remains the next gate.
- Known semantic gap: `rgba16` is lowered to `rgba32f`, but a pack can declare
  `rgba16` over a target that is actually `RGBA16F` (original Bloom's
  `FBIMG_ALT_OUTPUT`). The qualifier and backing format then disagree. The next
  fix is target-aware format resolution or a write-only formatless image path;
  do not claim pack support until it is device-tested.

## Coverage against common shaderpack needs

| Feature | Status |
|---|---|
| Compute dispatch, SSBO, image load/store, barriers | real forwarding; probes pass |
| Custom images (2D/3D), RGBA16F/R32F/R32UI/RGBA32F, immutable image storage | real; 2D/3D fixed |
| Desktop unorm16 image formats | lowered to rg32f variants |
| Geometry shader stage + array varyings | stage plumbing; geometry probe passes; pack GS resource mix unvalidated |
| `textureGather`, `textureGrad`, `texelFetch`, depth filtering | real; probes pass |
| MRT, draw buffers, depth copies, blits, per-buffer blend | real; probes pass |
| Subgroup arithmetic/quad/partition used by iterationRP | lowered to shared-memory workgroup equivalents |
| FSR2/SR/DLSS and optional debug paths | not qualified; not default |
