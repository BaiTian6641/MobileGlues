# Desktop OpenGL vs MobileGlues: gap analysis for Minecraft shaderpacks

Updated: 2026-10-20. **See [`desktop-gl-gap-closure.md`](desktop-gl-gap-closure.md) for the
row-by-row verification, corrected classifications, exact implementation designs,
and probe matrix.** Evidence levels are separated:

- **wrapper**: implemented in MobileGlues and exercised by a host/device probe.
- **lowered**: the desktop feature is rewritten to a legal ES/ANGLE form.
- **named deviation**: deliberately not implemented; callers receive an error or
  the pack is known not to work.
- **unverified**: no qualifying evidence yet.
- **provider limit**: ANGLE/Adreno does not offer the capability.

MobileGlues is not a desktop GL implementation. It is a GL 4.0 frontend over
GLSL-to-ESSL translation (glslang + SPIRV-Cross) and an OpenGL ES 3.2 backend
provided by ANGLE over Vulkan. A feature can therefore be missing at three
different layers: the shader translator, the GL API wrapper, or the provider.

## What is already supported

These are repeatedly mistaken for the missing pieces because they are desktop
features, but the pinned X1E tuple probes them successfully:

| Feature | MobileGlues evidence |
|---|---|
| Compute shaders, 512 invocations, 8x8x8 | `compute_512_invocations`; `MAX_COMPUTE_WORK_GROUP_INVOCATIONS=1024` |
| Shared memory + `barrier()` | 32 KiB compute-shared budget; Bliss uses 20 KiB |
| SSBOs, compute and vertex | probes pass; rebinding and vertex atomics pass |
| Geometry shaders | probe compiles/links/draws |
| Tessellation control/evaluation | probe passes when TCS writes levels; default patch levels are a named deviation |
| Image load/store, 2D/3D | 3D R32UI allocation/clear/bind passes; WSR/ACT images pass |
| Separate sampler objects | sampler-object depth filtering probe passes |
| Hardware shadow compare + LINEAR | live `shadowtex0` state: `COMPARE_REF_TO_TEXTURE`, `LEQUAL`, linear min/mag |
| MRT / indexed draw buffers | colortex/FBO shuffle tests pass |
| Indirect and multi-draw | multiindirect/unroll backends; base-vertex order |
| DSA subset | `enableExtDirectStateAccess=true`; only tested entry points are claimed |
| Depth/stencil blits and copies | D24S8/D32F same-format probes pass |

## Actual gaps

### 1. One-dimensional textures and images

Desktop GL has `GL_TEXTURE_1D`, `sampler1D`, and `image1D`. ES 3.2 has no 1D
texture or image target. MobileGlues currently leaves `glTexImage1D` and
`glTexStorage1D` unimplemented (`GL_INVALID_OPERATION`), and `image1D` has no
lowering.

Impact: **Bliss LPV `imgBlockData`** (RG32UI, 1D, 2048) cannot work. This is
optional in Bliss, not its default graphics path.

Legal fix: virtualize 1D as 2D `width x 1`, rewrite `uimage1D` to `uimage2D`,
and rewrite `imageLoad/img, x` and `imageStore/img, x, value` coordinates. This
is a semantic lowering, not a conformance claim.

### 2. Explicit storage-image formats absent from ES

ES 3.1/3.2 image qualifier sets omit `r8ui`, `r16ui`, and `rg32ui`. The provider
rejects the raw qualifiers and `glBindImageTexture` rejects the matching
formats. MobileGlues now lowers:

- `layout(r8ui)` and `layout(r16ui)` to `layout(r32ui)`;
- `layout(rg32ui)` to `layout(rgba32ui)`;
- `GL_R8UI`/`GL_R16UI` texture storage to `GL_R32UI`;
- `GL_RG32UI` texture storage and image unit format to `GL_RGBA32UI`.

The host qualifier test passes. Device validation is pending.

Impact: **Bliss `imgVoxelMask`** (R16UI 3D), **Bliss `imgBlockData`**
(RG32UI 1D), and other packs using narrow integer images.

### 3. Mutable multi-level 3D image textures

The level-0 3D image allocation is converted to one-level immutable
`glTexStorage3D` storage so ANGLE can bind it for load/store. A later level > 0
upload cannot extend that texture. One-level Iris custom images are the tested
path.

Impact: a pack that builds a mutable mip chain level by level for a 3D storage
image. Common packs generally use one-level Iris custom images, so this is a
boundary rather than a first-order blocker.

### 4. SSBO size cap

The X1E provider reports `GL_MAX_SHADER_STORAGE_BLOCK_SIZE=134,217,728` bytes
(128 MiB). Complementary WSR 256 requests 202,899,456 bytes and cannot write its
last element. WSR 128 (50,855,936 bytes) and WSR 192 (114,229,248 bytes) pass.

Impact: **Complementary ACT/WSR Ultra**, **iterationRP WSR 256**. Not an X1E
release blocker; X2E/optional.

### 5. Subgroup operations

The provider exposes no usable `GL_KHR_shader_subgroup_quad/arithmetic/ballot`
set for iterationRP's SPD path (`GL_KHR_shader_subgroup_quad/arithmetic/ballot`
are rejected). MobileGlues cannot synthesize subgroup semantics with a shader
rewrite without changing algorithm behavior.

Impact: **iterationRP** (SPD and related compute), not Complementary/BSL/Sildur
default paths.

### 6. Texture barriers and texture views

- `glTextureBarrier`/`glTextureBarrierNV`: no legal ES equivalent for
  render-to-texture feedback. Named deviation.
- `glTextureView`: ES has no texture views; the provider lists no
  `GL_EXT_texture_view`. Named deviation.

Iris works around the common cases with BufferFlipper alternate textures, so
these are not usually first-order pack blockers. They matter to packs or
features that explicitly rely on feedback barriers or format/level aliasing.

### 7. Default tessellation levels

Tessellation stages work when the TCS writes `gl_TessLevel*`. ES 3.2 exposes
`glPatchParameteri(GL_PATCH_VERTICES)` but not `glPatchParameterfv`; valid
desktop default-inner/outer-level calls report `GL_INVALID_OPERATION` rather
than being silently accepted.

Impact: packs that use tessellation without writing levels. Most common packs
do not enable tessellation by default.

### 8. Clip/cull distance

The capability probe reports `clip_cull_distance=0` on this tuple. Shaders that
write `gl_ClipDistance`/`gl_CullDistance` cannot rely on desktop semantics.
Common packs rarely require this; it is a named translation boundary rather than
a supported feature.

### 9. Color/render-target formats

| Format | Status | Impact |
|---|---|---|
| RGBA16F | pass | common HDR buffers |
| R11F_G11F_B10F | pass | common final/bloom buffers |
| RGBA8 | pass | common LDR buffers |
| RGBA16 (unorm) | unqualified | **Bliss** requests it for gbuffer buffers |
| RGB8_SNORM | fail at FBO completeness | some Complementary configurations |
| RGBA8_SNORM | pass | — |
| RGB16F | pass | — |

`RGBA16` is the most relevant open format question for Bliss once its source
ordering failure is resolved.

### 10. Image atomics and other image operations

Image load/store is supported for the probed formats and targets. Image atomics
are **not qualified** by the current evidence; the feature matrix records vertex
SSBO atomics, not a general image-atomic contract. Packs that use
`imageAtomic*` need a dedicated probe before a support claim.

### 11. Bindless textures, separate shader objects, and other desktop-only APIs

Not exposed/implemented: bindless texture handles, separate shader objects,
texture views, and desktop-only feedback barriers. These are not used by the
common pack set considered here, but they are real differences from desktop GL.

## Shaderpack impact matrix

| Pack | Default path | Features that matter | Current status |
|---|---|---|---|
| Complementary Reimagined 5.9.3 | Legacy graphics + deferred/composite | shadows, TAA, bloom, clouds; ACT/WSR images/SSBO | High renders; effect-by-effect gates open; WSR256 capped |
| BSL 10.1.8 | Legacy graphics | shadow maps, cloud shadows, deferred/composite | Renders; current compile failure is Iris-generated late `uniform`, not a GPU feature |
| Sildur Vibrant 2.02 | Legacy 120/330 | shadows, water, bloom | provisional S1 |
| Sildur Enhanced Default 1.20 | Legacy 120 | shadows, fog, bloom | S0/import only; exact fixture S1 not captured |
| Sildur Basic 2.7 | Legacy 120 | shadows, fog | exact fixture renders; no compile errors |
| Bliss 2.1.2 | Legacy graphics; LPV off by default | LPV compute/images: R16UI 3D, RGBA8 3D, RG32UI 1D, vertex imageStore | default blocked by source ordering; LPV blocked by 1D + explicit image format lowering (format lowering now implemented) |
| Solas 3.0 | Legacy graphics + shadowcomp | 3D images, vertex image, compute | provisional S0; metadata/format verification open |
| iterationRP 0.8.31 | Modern compute/image/SSBO | subgroups, rgba16/rg16 images, 3D images, SSBO/WSR/ACT | original blocked; private port only |
| Photon / Rethinking Voxels | Modern compute/image/atomics | gather, temporal, geometry storage/atomics | unverified |

## Priority order

1. **1D image virtualization** (Bliss LPV).
2. **Device-validate the new R16UI/RG32UI lowering**; add image-atomic probe.
3. **RGBA16 color-attachment qualification** (Bliss gbuffer).
4. **Late-uniform source normalization** at the driver boundary; this currently
   blocks Bliss before any GPU feature is reached. It is an Iris/ANGLE source
   compatibility issue, not a desktop-GL feature.
5. **Subgroups** only if iterationRP remains a target; otherwise document as
   provider-bound.
6. **WSR 256** remains X2E/optional; do not raise limits in the wrapper.

## Sources

- Iris custom images: https://shaders.properties/current/reference/buffers/custom_images
- Iris SSBOs: https://shaders.properties/current/reference/buffers/ssbo
- Iris feature flags: https://shaders.properties/current/reference/shadersproperties/flags/
- OptiFine shader development: https://optifine.readthedocs.io/shaders_dev.html
- OptiFine `shaders.txt`: https://github.com/sp614x/optifine/blob/master/OptiFineDoc/doc/shaders.txt
- Khronos OpenGL ES 3.1/3.2 specs: https://registry.khronos.org/OpenGL/specs/es/3.1/es_spec_3.1.pdf
- Khronos OpenGL 4.6 core spec: https://registry.khronos.org/OpenGL/specs/gl/glspec46.core.pdf
- Local evidence: `feature-matrix.md`, `execution-state.md`,
  `backend-support-fixing-plan.md`, and the shaderpack audits under
  `shaderpack-support/audits/`.
