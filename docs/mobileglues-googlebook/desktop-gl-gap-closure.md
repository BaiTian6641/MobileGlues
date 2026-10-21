# Desktop GL → MobileGlues gap-closure plan

Implementation update: the
[2026-10-21 continuation](2026-10-21-desktop-gl-bridge-ci.md) records newer
source, host and device evidence for G0/G3/G4/G5/G10 and the standalone FSR3
bridge. The row-by-row text below remains the original planning snapshot;
in particular, its “untested” labels and source line numbers must not be used
as the current execution state. Unclosed gaps retain their existing priority.

Status: **plan and verification record, not a support promotion.** Evidence date
2026-10-21. Target tuple: Minecraft 1.21.1 / NeoForge 21.1.262 / Iris
1.8.14-beta.1 / Sodium 0.8.13, HP Googlebook 14 (`C1L63708PZ`), Snapdragon
X1E80100 / Adreno X1-85, system ANGLE 2.1 over Vulkan 1.4.295, GLES 3.2.

Model used: **`deepseek/deepseek-flash`**. The runtime did not expose
`GPT-6 Astra`, so that model was not used.

This document fills the missing part of
`desktop-gl-vs-mobileglues-gap.md`: it verifies every existing row against the
current tree and probes, corrects stale classifications, and gives an exact
implementation design for each remaining gap. It does not modify the existing
gap document, any shaderpack archive, or core code.

Evidence labels used below:

- **device** — probe or game-process evidence on the X1E tuple.
- **host** — host compile/unit-test evidence in this tree.
- **source** — call-site inspection only; no run.
- **unverified** — no qualifying evidence.
- **provider hard limit** — ANGLE/Adreno does not offer the capability.
- **named deviation** — deliberately unsupported; the caller must receive an
  explicit error or a documented boundary, never a silent success.

No row below is a release qualification. The only pack rows allowed to be
called staged are the ones already recorded in `feature-matrix.md`; this plan
does not raise any of them.

---

## 1. Row-by-row verification of the existing gap document

### 1.1 “What is already supported” rows

| Existing claim | Verification | Class |
|---|---|---|
| Compute, 512 invocations, 8×8×8 | Device `compute_512_invocations`: value `1298`; `MAX_COMPUTE_WORK_GROUP_INVOCATIONS=1024` | device pass |
| Shared memory + `barrier()` | `MAX_COMPUTE_SHARED_MEMORY_SIZE=32768`; Bliss LPV requests `10*10*10` vec4 + uint (≈20 KiB) per the static audit | device limit + source |
| SSBOs, compute and vertex | `ssbo_block_binding`, `ssbo_graphics_binding`, `vertex_ssbo_atomics` pass; WSR 128/192 pass | device pass |
| Geometry shaders | `geometry_stage` compiles/links/draws `pixel=255,0,0` | device pass |
| Tessellation with TCS-written levels | `tessellation_stage` passes; default levels fail explicitly (`patch_default_levels`) | device pass + named deviation |
| Image load/store, 2D/3D | `act_volume`, `r32ui_volume`, `vertex_image_store`, `act_pingpong` pass | device pass |
| Separate sampler objects | `depth_filter` binds a sampler object, samples `0.5`, restores binding | device pass |
| Hardware shadow compare + LINEAR | `depth_raw_and_compare_linear`; live `shadowtex0` state is compare/LINEAR | device pass |
| MRT / indexed draw buffers | `framebuffer_shuffle_test` host scenarios; pack runs use multiple colortex attachments | host + device (pack) |
| Indirect and multi-draw | backend probes and `multidraw.cpp`; not re-run in this audit | existing evidence, not new |
| DSA subset | `enableExtDirectStateAccess=true`; only tested entry points claimed | existing evidence |
| Depth/stencil blits and copies | `blit_depth_match`, `copy_image_subdata_depth`, mismatch probes | device pass |

No supported row is withdrawn. The rows that are *not* in this table and are
still open are the ones in §2.

### 1.2 Numbered gap rows

| # | Existing claim | Verification result | Corrected class |
|---:|---|---|---|
| 1 | 1D textures/images unimplemented; Bliss LPV `imgBlockData` | **Confirmed.** `glTexImage1D`/`glTexStorage1D` set `GL_INVALID_OPERATION` and only write shadow fields (`gl/texture.cpp:1003`, `:1251`); `glTexSubImage1D`/`glCopyTexSubImage1D` are no-op stubs that log a warning and set no error (`gl/gl_stub.cpp:311`, `:317`); `glBindTexture` forwards `GL_TEXTURE_1D` to the driver (`gl/texture.cpp:1862`); Iris really creates 1D custom images (`TextureType.TEXTURE_1D` → `texImage1D`, and a 7-token `image.` directive selects 1D) | wrapper gap + legal lowering; **optional** for Bliss (LPV off by default) |
| 2 | Explicit `r8ui`/`r16ui`/`rg32ui` image formats lowered; device pending | **Confirmed, and broader than stated.** Commit `98058e3` adds `r8ui/r16ui→r32ui` and `rg32ui→rgba32ui`; existing code already lowers `rgba16/rg16/rg16f/rg32f`. Host tests pass. Device: storage/binding half is device-exercised by `integer_image` (`r16ui=accepted via R32UI`); the shader-qualifier half for the new formats is host-only. Completeness defects found: `r8i/r16i/r8/r16/r16f` are not lowered; `RG32F` storage is not widened; `RGBA8_SNORM` is missing from the immutable-image allowlist; `mg_image_unit_format` can return non-Table-8.27 `GL_RGBA16` | legal lowering (partially verified) + format-table defects |
| 3 | Mutable multi-level 3D images cannot extend immutable level-0 storage | **Confirmed**, and also true for mutable multi-level 2D image storage. `glTexImage2D/3D` convert only `level==0 && pixels==nullptr` to one-level `glTexStorage*` (`gl/texture.cpp:1075`, `:1167`). A later level upload reaches ANGLE against immutable storage and fails | named deviation (truthful error) + optional full-chain allocation |
| 4 | SSBO cap 128 MiB; WSR 256 blocked | **Confirmed.** `MAX_SHADER_STORAGE_BLOCK_SIZE=134217728`; WSR 128/192 map and write; WSR 256 last value reads `0` | provider hard limit |
| 5 | Provider exposes no subgroup set; MobileGlues cannot synthesize without changing algorithm | **Partially rejected.** The provider indeed exposes no `GL_KHR_shader_subgroup_*`/`GL_NV_shader_subgroup_partitioned`. But the tree **does** lower a subset: virtual 32-lane IDs, barriered shared-memory `subgroupInclusiveAdd`, quad swaps, an SPD-specific rewrite, and the NV-partition/XOR duplicate-suppression pattern (`gl/glsl/compat_subgroup.cpp`, `compat_subgroup_partition.cpp`; host tests pass). Residual constructs produce `#error MG_SUBGROUP_UNSUPPORTED`. Device semantics are unverified | legal lowering (host) + named deviations for the lowering's unsupported surface; **not** a provider-only limit |
| 6 | `glTextureBarrier`/`glTextureBarrierNV` and `glTextureView` named deviations | **Confirmed.** Provider extension list has no `GL_*_texture_barrier`, no `GL_EXT/ARB_texture_view`. The wrappers are no-op stubs that only log (`gl/gl_stub.cpp:765`, `:2213`, `:639`) | named deviation; make the error explicit |
| 7 | Default tessellation levels unsupported | **Confirmed.** `glPatchParameterfv` rejects `GL_PATCH_DEFAULT_{INNER,OUTER}_LEVEL` with `GL_INVALID_OPERATION` (`gl/gl.cpp:187`); provider has no entry point | named deviation |
| 8 | `clip_cull_distance=0`; clip/cull distance untranslatable | **Confirmed** by device log and extension list (no `GL_EXT_clip_cull_distance`). However, glslang is initialized with `maxClipDistances/maxCullDistances = 8` and the backend query may keep that, so a desktop shader can be accepted by the front-end and fail later at the driver instead of at the translation boundary (`gl/glsl/glsl_for_es.cpp:62`, `:123`, `:252-255`) | provider hard limit + truthfulness fix |
| 9 | RGBA16 unqualified; RGB8_SNORM fails FBO | **Corrected.** Null/storage `GL_RGBA16` is already lowered to `RGBA32F` by `mg_lower_16bit_image_storage` in both `glTexImage2D` (null) and `glTexStorage2D/3D`; provider `RGBA32F` is FBO-complete, so the **Bliss RGBA16 attachment question is not an open blocker**. Raw `RGBA16` is still absent from the provider probe and remains unverified. `RGB8_SNORM` is lowered to `RGBA8_SNORM` (`gl/texture.cpp:875-890`) and the device probe `rgb8_snorm_attachment` passes | legal lowering (RGBA16 memory/precision deviation; RGB8_SNORM device pass) |
| 10 | Image atomics not qualified | **Confirmed as unverified**, not as unsupported. ESSL 3.20 has core integer `imageAtomic*` functions; the provider also exposes `GL_OES_shader_image_atomic` for ES 3.1-class sources. No wrapper probe exercises image atomics. iterationRP uses `imageAtomicMax/Min` on r32ui in fragment, compute, and geometry programs | unverified; probe required |
| 11 | Bindless textures, SSO, texture views, feedback barriers not exposed | **Confirmed.** `GL_ARB_separate_shader_objects`/`GL_KHR_no_error` are deliberately not advertised (`gl/getter.cpp:269-273`); no bindless path; no view/barrier extensions | named deviations |

### 1.3 Stale statements found in the tree

These are not edited here; they are corrected by this document.

- `feature-matrix.md` “SSBO/image binding round-trip” says “no r8ui/r16ui
  lowering yet” and “R8UI/R16UI image lowering is not implemented”
  (`execution-state.md` stage 1/2 section). At HEAD (`98058e3`) the lowering
  exists and host tests pass; device validation is partial.
- The Bliss static audit says “R8UI/R16UI lowering is not implemented”. Same
  correction.
- The gap document's priority 1 (1D) is an **optional** Bliss LPV path, not a
  default-path blocker; the audit itself says LPV is off by default.
- The gap document's priority 4 (late-uniform normalization) is **implemented
  at HEAD** (`2bcdf10`) but has no host regression test and no device run.
- `image_metadata.cpp:150-151` says `compat_image` “chooses a formatless
  writeonly declaration or the float fallback”. The current
  `mg_lower_legacy_image_formats` does **not** emit formatless declarations; it
  always emits a legal qualifier. ESSL 3.10/3.20 require a format qualifier on
  every image variable, so the comment is stale and the code is right.

---

## 2. Gap-closure table

Priority classes:

- **P0** — confirmed default-path failure.
- **P1** — default-path or required-advanced seam that is unverified or only
  partially correct; must be closed before any support claim.
- **P2** — optional pack feature with a clean legal lowering.
- **P3** — provider/API hard boundary; document and fail explicitly.

| ID | Gap | Desktop construct | Current MobileGlues status | Affected packs | Exact fix or probe | Prio | Evidence |
|---|---|---|---|---|---|---|---|
| G0 | Late global `uniform` after a function | GLSL allows global declarations in any order | Core ESSL hoist implemented (`mg_hoist_late_global_uniforms`) but **untested**; earlier device build failed Bliss `deferred.fsh` at `0:948 'uniform' : syntax error` | **Bliss default S0** | Add host regression + wrapper probe; device re-run of original Bliss. If it still fails, dump Iris-patched source and isolate the exact declaration; do not add a second blind regex | **P0** | `2026-10-20-bliss-s0-fail.md`; `gl/shader_source.cpp:119`; `gl/shader.cpp:112`; no test references the function |
| G1 | Formatless image declarations | desktop image without `layout(format)`; format comes from the image unit | Core consumes `mg_image_metadata_*` and adds a qualifier; **no producer in core**. A bridge build (`92aa55cf…`) was installed on the Solas S0 run, yet the formatless declarations still produced the driver warning, so registration did not cover this source. Unregistered formatless image fails compile on device | **Solas** (default `shadowcomp`, vertex shadow image); other formatless packs | Deploy/qualify the Iris bridge as the format producer; core must emit an actionable `#error` (names) instead of relying on the driver's “No image internal format specified”. Probe ANGLE's formatless acceptance before relying on it | **P1** | `2026-10-20-solas-s0.md`; `gl/image_metadata.cpp:130`; device `formatless_image_metadata` (`unregistered_program=0`, driver errors) |
| G2 | 1D textures/images | `GL_TEXTURE_1D`, `glTexImage1D`, `glTexStorage1D`, `glTexSubImage1D`, `image1D`, `sampler1D` | Storage/binding/shader all missing; `glTexImage1D`/`glTexStorage1D` report `GL_INVALID_OPERATION`; `glBindTexture(1D)` reaches the driver; `image1D` is left in the source | **Bliss LPV** (optional); any 1D custom image | Virtualize as 2D `width×1` at the driver boundary; rewrite `image1D` to `image2D`, wrap image coordinates, and either lower `sampler1D` or fail loudly; implement 1D clear/copy/query paths | **P2** | `gl/texture.cpp:1003,1251,1338,1862,2724`; `gl/gl_stub.cpp:311,317`; Iris `TextureType.TEXTURE_1D`, `ShaderProperties` 7-token rule, `GlImage` `texImage1D`; ESSL 3.20 reserved-word list |
| G3 | Narrow/unsupported image formats | `r8ui`, `r16ui`, `rg32ui`, `rgba16`, `rg16`, `rg16f`, `rg32f`, `r8`, `r16`, `r16f`, `r8i`, `r16i`, `rgba8_snorm` | Partial lowering. `r8ui/r16ui/rg32ui/rgba16/rg16/rg16f/rg32f` are covered; `r8/r16/r16f/r8i/r16i` are not; `RG32F` storage is not widened; `RGBA8_SNORM` storage is not immutable; `mg_image_unit_format` can return `GL_RGBA16` | **iterationRP** (default rgba16/rg16/rg16f), **Solas** (r8ui), **Bliss LPV** (r16ui/rg32ui) | One shared logical→(qualifier, backing) table; complete the qualifier lowering; widen null image storage consistently; add all legal image formats to the immutable allowlist; preserve explicit binding; fail on unmappable formats | **P1** | ESSL 3.10 §4.4.7 and 3.20 §4.4.7; ES 3.1 Table 8.27/8.28; `compat_image.cpp:43`; `image_metadata.cpp:46,94,105`; `texture.cpp:541,557,564`; host tests pass; iterationRP S0 device pass with G1 core |
| G4 | Image atomics | `imageAtomicAdd/Min/Max/And/Or/Xor/Exchange/CompSwap` on `uimage2D/3D` | No wrapper probe; core ES 3.2 + provider `GL_OES_shader_image_atomic` suggest support; unverified | **iterationRP** (default), Photon/Rethinking Voxels (unverified) | Add 2D/3D compute + fragment + geometry probes with readback and `glMemoryBarrier`; expect no core change; if ESSL emission lacks the extension, add the required extension enable or report | **P1** | ESSL 3.20 §8.12 (`imageAtomic*`); provider extension list; iterationRP `BackwardAnalysis.glsl`, `Shadow.glsl`, `Textured_FS.glsl`, `Water_FS.glsl` |
| G5 | Subgroup operations | `gl_Subgroup*`, `subgroupInclusiveAdd`, quad swaps, ballot/partition/shuffle | Compute-only legal lowering exists and host tests pass; hardware extensions absent; residual constructs loud `#error`; device semantics untested | **iterationRP** (default SPD/RTW), other modern packs unverified | Device probe exercising the exact iterationRP XOR duplicate-suppression + inclusive-add patterns; read back sums/atomics. Document virtual 32-lane subgroup as a named semantic deviation. Extend only for a pack that needs ballot/broadcast/integer scans | **P1** | `compat_subgroup.cpp:580`; `compat_subgroup_partition.cpp:870`; `compat_subgroup_test`/`compat_subgroup_partition_test` pass; provider extension list; `2026-10-19-iterationrp-shader-transform.md` |
| G6 | RGBA16 unorm attachments/image storage | `GL_RGBA16` colortex, `layout(rgba16)` image | Null/storage allocations lowered to `RGBA32F`; provider `RGBA32F` FBO complete; raw `RGBA16` not in the provider probe; data-backed `RGBA16` stays `RGBA16`; image binding of that case can produce a non-Table-8.27 format | **Bliss** (colortex1/10/11/14), **iterationRP** (colortex1-5/12, `voxelData3D`) | Add `RGBA16`/`RG16` to `capability_probe` (texture/FBO/clear/image) and a mixed-MRT wrapper probe. Keep `RGBA32F` (exact, 4× memory) unless the provider probe proves `RGBA16` is color-renderable and a 2× `RGBA16F` mode is accepted; do not silently change precision | **P1** (probe) / P3 (memory optimization) | `texture.cpp:541,691`; provider `GL_EXT_texture_norm16`; EXT_texture_norm16 Table 8.13 (`RGBA16_EXT` CR=X); provider `RGBA32F` FBO pass; Bliss/iterationRP audits |
| G7 | Mutable multi-level 2D/3D image storage | level-by-level `glTexImage2D/3D` mip chain | Level-0 null image allocation becomes one-level immutable; later levels fail. Shadow fields are still overwritten after the driver error | rare; advanced pack features | Detect a level>0 upload to a wrapper-created one-level immutable image and report `GL_INVALID_OPERATION` without corrupting shadow state. Optional mode: allocate the full chain (`floor(log2(max dimension))+1`) and redirect later levels to `glTexSubImage*` | **P3** | `texture.cpp:1075,1167`; gap document item 3; `wrapper_probe image2d_mutable_bind` is mode-only (not in `all`) |
| G8 | Texture barriers and texture views | `glTextureBarrier(NV)`, `glTextureView` | No legal ES equivalent; provider lacks both extensions; stubs are no-ops that log a warning and set no error | packs that rely on feedback aliasing or view reinterpretation; not the audited default paths | Convert both stubs to an explicit `GL_INVALID_OPERATION` + forced warning; keep the named deviation. Do not claim `glMemoryBarrier` fixes ES feedback undefinedness. Document Iris `BufferFlipper` as the alternate-texture workaround | **P3** | provider extension list; `gl_stub.cpp:639,765,2213`; ES 3.1/3.2 have no barrier/view entry points; Iris `BufferFlipper.java` |
| G9 | Default tessellation levels | `glPatchParameterfv(GL_PATCH_DEFAULT_*_LEVEL)` | Explicit `GL_INVALID_OPERATION`; TCS must write levels | packs that tessellate without a TCS | Keep the explicit error. Optional future lowering: synthesize a passthrough TCS from the TES interface and inject default levels; only if a common pack needs it. Do not fake acceptance | **P3** | `gl/gl.cpp:187`; `wrapper_probe patch_default_levels`; provider `GL_EXT_tessellation_shader` but no default-level API |
| G10 | Clip/cull distance | `gl_ClipDistance`, `gl_CullDistance`, `glEnable(GL_CLIP_DISTANCEi)` | Provider lacks `GL_EXT_clip_cull_distance`; glslang limits are not gated to 0, so the failure can surface at the driver instead of the translation boundary | rare common-pack use; not established for the audited defaults | Set `maxClipDistances/maxCullDistances/maxCombinedClipAndCullDistances` to 0 when the backend extension is absent, so a shader using them fails at compile with a clear log; report 0 from the corresponding queries. No emulation | **P3** | provider extension list; `glsl_for_es.cpp:62,123,252-255`; `enable.cpp:434`; device log `clip_cull_distance=0` |
| G11 | Vertex/geometry-stage multi-image capacity | image uniforms and atomics in VS/GS | Vertex 2D RGBA16F store passes; GS 3-image/atomics and VS multi-image not probed; stage limits are queried from the backend | **iterationRP** (`PixelData_VS`, `shadow.gsh`), Complementary shadow vertex images (per backend plan) | Add `vertex_multi_image` and `geometry_image_atomics` probes; verify `GL_MAX_{VERTEX,GEOMETRY}_IMAGE_UNIFORMS` and link; no lowering change expected | **P1** | `glsl_for_es.cpp:171-215`; `wrapper_probe vertex_image2d_store`; iterationRP audit; backend plan G4 |
| G12 | SSBO size cap | SSBO > `MAX_SHADER_STORAGE_BLOCK_SIZE` | 128 MiB provider cap; WSR 128/192 pass, WSR 256 blocked | **Complementary WSR 256**, **iterationRP WSR 256** | Do not raise the limit in the wrapper and never split a block silently. Keep the truthful provider limit; Iris should reject at load. WSR 128 is the required target | **P3** | provider probe `ssbo_sizes`; `MAX_SHADER_STORAGE_BLOCK_SIZE=134217728`; Iris SSBO doc (128 MB guarantee) |
| G13 | Bindless textures, separate shader objects | `GL_ARB_bindless_texture`, `GL_ARB_separate_shader_objects` | Not exposed/implemented; SSO/no-error names deliberately not advertised | not used by the audited default paths | Keep as named deviations. Do not advertise an extension without its semantics | **P3** | `gl/getter.cpp:269-273`; provider extension list |

### 2.1 Pack impact summary

| Pack | Open desktop-GL feature gap | Not a feature gap / already handled | Do not claim |
|---|---|---|---|
| **Complementary Reimagined / Unbound** | None established for the default High path. ACT/WSR images and SSBOs are staged with the bridge; effect-by-effect fidelity is a qualification gate | Shadows, TAA, bloom, clouds, water render in the staged runs; WSR 128/192 SSBOs pass; RGB8_SNORM is lowered | Effect fidelity; WSR 256; core-only ACT/WSR |
| **BSL** | None established. The pack renders; the remaining issue is shadow/cloud comparison | Late-uniform translation is the same seam as G0 only if BSL's generated source hits it (not observed) | Full fidelity; cloud-shadow correctness |
| **Sildur Vibrant / Enhanced / Basic** | None established. RGB8/RGB10_A2 FBO formats pass on the provider | S0/S1 qualification is separate; Basic is the all-RGBA8 control | S1/S2 |
| **Bliss** | **G0** (late `uniform`) blocks default S0. LPV-only: G2 (1D) and G3 (r16ui/rg32ui) | RGBA16 is already lowered to `RGBA32F`; default has no SSBO/images | Default S0 until G0 closes; LPV |
| **Solas** | **G1** (formatless image metadata) and G3 (r8ui 3D), G11 (vertex image) | S0 pass is provisional; 3D RGBA16F/R32UI images are lowered and bindable | S1 or any image-semantics claim |
| **iterationRP** | **G3** (format table), **G5** (subgroup semantics), **G4** (image atomics), **G11** (vertex/GS images) | S0 pass with a G1 core; narrow formats compile; private port is not evidence | Original-pack support; S1 |
| **Photon / Rethinking Voxels** | Unverified. G4/G5/G11 are the prerequisite probes | No local archive/audit | Any support level |

---

## 3. Implementation designs

### G0 — Late global uniform normalization (Bliss default S0)

**Affected construct.** A global `uniform` declaration written after a function
definition. Desktop GLSL permits this; ESSL 3.10/3.20 requires declarations
before use and ANGLE's parser rejects a `uniform` token after a function body.

**Current status.**

- `mg_hoist_late_global_uniforms` is implemented at
  `MobileGlues-cpp/gl/shader_source.cpp:119` and called at
  `MobileGlues-cpp/gl/shader.cpp:112` **after** translation, on the ESSL that is
  submitted to ANGLE.
- The function scans lines, tracks comments, brace depth, conditional depth and
  the first function; it moves only plain single-line top-level `uniform ...;`
  declarations found after that function.
- **No test references it** (`grep hoist MobileGlues-cpp/tests` is empty).
- Bliss device evidence on core `c741b74d…`:
  `Shader compilation log for deferred.fsh: ERROR: 0:948: 'uniform' : syntax error`,
  then `The shaderpack failed to load!`, then shaders disabled
  (`2026-10-20-bliss-s0-fail.md`).

**Exact fix.**

1. Add host tests in `MobileGlues-cpp/tests/shader_source_test.cpp` that pin:
   - a uniform after a function is moved before the first function;
   - a uniform inside a top-level `#if` after a function is either moved as a
     whole conditional block or left with a forced failure (not split);
   - a uniform in a block comment, a string, or inside a function body is not
     moved;
   - `layout(...) uniform` image/SSBO declarations and multi-line declarations
     are not partially moved;
   - the byte content of every retained line is unchanged.
2. Add a diagnostic (release-visible `LOG_W_FORCE`/stderr) naming the moved
   declarations and their original line numbers. The current code logs only a
   count.
3. Device gate: re-run original Bliss default with the hoist build. Acceptance:
   no `'uniform' : syntax error`, `Using shaderpack: Bliss…`, pipeline active,
   no `(fallback)`.
4. If it still fails, enable Iris `enableDebugOptions` and dump
   `patched_shaders/deferred.fsh`; identify the exact declaration and its
   preprocessor depth. Do not add a broader regex until the source is known.

**Guardrails.**

- Move only declarations at `brace_depth == 0 && conditional_depth == 0`.
  Moving a declaration out of a conditional can make an inactive type
  reference active.
- Never move `layout(...)` qualified declarations, blocks, or multi-line
  declarations; a partial move is worse than the original error.
- Do not reorder across `#extension` directives; insert before the first
  function, which is already after the header.
- The bridge's `ShaderSourceCompat.hoistLateGlobalUniforms` operates on the
  desktop source before Iris compiles. It cannot fix a uniform that
  glslang/SPIRV-Cross emits late in the generated ESSL. The core hoist is the
  correct layer; do not rely on the bridge hoist as evidence.

**Probe and acceptance.**

| Probe | Where | Acceptance |
|---|---|---|
| `late_uniform_hoist` (new) | host `shader_source_test`, and `wrapper_probe` with a compiled ESSL shader | uniform compiles; no source-line reordering of unrelated lines |
| Bliss `deferred.fsh` S0 | X1E game process, original ZIP | no `'uniform' : syntax error`; pipeline active; no fallback |
| BSL/Complementary regression | X1E game process | logs unchanged; no new compile errors |

**Risk to other packs.** Low. The transform is a no-op when no late uniform
exists. The main risk is moving a declaration whose type is defined after the
first function; the guard above (only simple declarations, no blocks) and the
tests bound it.

---

### G1 — Formatless image metadata production (Solas default)

**Affected construct.** Desktop GLSL allows an image uniform without a format
layout qualifier; the format is taken from the image unit at bind time.
ESSL 3.10 and 3.20 require a format layout qualifier on every image variable
(§4.4.7: “Any image variable must specify a format layout qualifier”).

**Current status.**

- `gl/image_metadata.h` defines the producer contract:
  `mg_image_metadata_begin/add/end` before `glShaderSource`.
- `mg_image_metadata_take()` is consumed at `gl/shader.cpp:74`;
  `mg_normalize_image_qualifiers` (`gl/image_metadata.cpp:130`) adds
  `layout(<legal qualifier>) highp` for names with metadata.
- The **core has no producer**. The Iris bridge (separate repository) is the
  producer. A bridge build (`92aa55cf…`) was installed in the Solas S0 run,
  yet the shadow programs still logged the warning. Either those names were
  not registered, or registration happened after the source was consumed.
  This is a bridge-seam defect (or an unsupported source shape), not evidence
  that the core can resolve the format itself.
- Device probe `formatless_image_metadata` shows the negative case:
  `unregistered_program=0` and driver errors
  `'uimage3D' : No precision specified` /
  `'No image internal format specified'`. The positive case (metadata
  registered) returns `12345` on device.
- Solas S0 passed but logged repeated
  `WARNING: 'No image internal format specified' : layout qualifier` for
  `shadowcomp.csh` and shadow programs (`2026-10-20-solas-s0.md`).

**Exact fix.**

1. Qualify the support boundary explicitly: original-pack support for
   formatless images requires the Iris bridge as the metadata producer. The
   bridge must register the **logical** format
   (`mg_image_metadata_add(name, GL_R8UI)` etc.) before the `glShaderSource`
   that carries the declaration; core owns the logical→ESSL mapping. Solas
   exercises two shapes the installed registration must cover: a one-line
   multi-declarator `writeonly uniform image3D floodfill_img,
   floodfill_img_copy;` and a vertex-stage `voxel_img` declaration. A bridge
   that only scans compute sources or only the first declarator would explain
   the S0 warnings.
2. Make the core failure actionable: after
   `mg_normalize_image_qualifiers`, scan for remaining `u?image*` declarations
   that still lack a `layout` format and, if the name is used by
   `imageLoad/imageStore/imageAtomic/imageSize`, emit
   `#error MG_IMAGE_FORMAT_REQUIRED: <names>` plus a forced log. This turns
   the current driver-side message into a named failure. If a formatless
   declaration is unused, leave it (do not fail a pack for dead code).
3. Do not add name-based guessing. The existing comment in
   `image_metadata.h` is a contract: no format is guessed from a variable name.
4. Probe ANGLE's formatless acceptance before choosing the fallback: run a
   write-only formatless image with an explicit binding format on X1E. If ANGLE
   accepts it with only a warning, document it as an implementation extension,
   not as a portable substitute for metadata.

**Guardrails.**

- Metadata is per-source and consumed once (`mg_image_metadata_take`). Do not
  carry a map across shaders, programs, or reloads.
- Never emit a qualifier that is not in the ESSL grammar; the format table in
  G3 is the only source of truth.
- Preserve explicit `layout(binding=N)` and access qualifiers.

**Probe and acceptance.**

| Probe | Where | Acceptance |
|---|---|---|
| `formatless_image_metadata` positive/negative | device wrapper probe | positive compute write/read `12345`; negative program fails with the named `#error`/driver message |
| Solas `shadowcomp.csh` + shadow programs | X1E game process, original ZIP | no `No image internal format specified`; image read/write semantics verified by a fixed route |
| A→B pack switch | device | no format map reuse across packs (see G14) |

**Risk.** A pack with an unused formatless image would fail if the core errors
unconditionally; the usage scan avoids that. Enabling the bridge changes the
qualified boundary and must be recorded as such, not as “core-only support”.

---

### G2 — One-dimensional textures and images (Bliss LPV)

**Affected constructs.**

- API: `GL_TEXTURE_1D`, `glTexImage1D`, `glTexStorage1D`, `glTexSubImage1D`,
  `glCopyTexImage1D`, `glCopyTexSubImage1D`, `glBindTexture(GL_TEXTURE_1D)`,
  `glTexParameter*`, `glGetTexLevelParameter*`, `glClearTexImage`,
  `glGetTexImage`, `glGenerateMipmap`.
- GLSL: `image1D`, `uimage1D`, `iimage1D`, `sampler1D`/`usampler1D`/
  `isampler1D`, `imageLoad/imageStore/imageAtomic/imageSize`, `texelFetch`,
  `textureSize`.

**Why it is real.** ES 3.2 has no 1D texture target; image units accept only
`TEXTURE_2D`, `TEXTURE_3D`, `TEXTURE_2D_ARRAY`, `TEXTURE_CUBE_MAP`,
`TEXTURE_2D_MULTISAMPLE`, `TEXTURE_2D_MULTISAMPLE_ARRAY`, `TEXTURE_BUFFER`
(ES 3.1 §8.22 target list). ESSL 3.20 reserves `image1D` and `sampler1D` as
unsupported names. Iris does create 1D custom images: `TextureType.TEXTURE_1D`
calls `texImage1D` with null pixels, and a 7-token `image.` directive selects
1D (`ShaderProperties` 1.21.1 branch; the 1.21.9 branch is identical).

**Current call-site evidence.**

- `glTexImage1D` (`gl/texture.cpp:1003`): sets `GL_INVALID_OPERATION`, then
  writes width/height/depth into the shadow object; no driver storage.
- `glTexStorage1D` (`:1251`): same pattern.
- `glCopyTexImage1D` (`:1338`): same.
- `glTexSubImage1D`/`glCopyTexSubImage1D`: no-op stubs that log a warning
  and set no error (`gl/gl_stub.cpp:311`, `:317`).
- `glBindTexture` (`:1862`): passes the raw target to `GLES.glBindTexture`;
  `GL_TEXTURE_1D` is not a GLES target.
- `glTexParameteri` (`:2150`), `glGetTexLevelParameteriv` (`:1703`),
  `glGetTexParameter*`, `glGetTexImage` (`:2004`), `glClearTexImage` (`:2724`)
  all use the raw target or a 2D-only attachment path.
- `glBindImageTexture` already lowers `layered=TRUE` for `TEXTURE_1D`
  (`gl/drawing.cpp:322`), which is correct once the driver object is 2D.
- Shader transforms (`compat_image.cpp:43`, `glsl_for_es.cpp:869`,
  `shader.cpp:83`) match `image1D` in the declaration regex but never rewrite
  the type or call sites.

**Legal ES substitution.**

Virtualize each 1D texture as a 2D texture of `width × 1` and keep the logical
target in the wrapper's `TextureObject`:

| Logical | Driver |
|---|---|
| `GL_TEXTURE_1D` | `GL_TEXTURE_2D` |
| `GL_PROXY_TEXTURE_1D` | `GL_PROXY_TEXTURE_2D` |
| `image1D` / `uimage1D` / `iimage1D` | `image2D` / `uimage2D` / `iimage2D` |
| `imageLoad(img, x)` | `imageLoad(img, ivec2(x, 0))` |
| `imageStore(img, x, v)` | `imageStore(img, ivec2(x, 0), v)` |
| `imageAtomic*(img, x, ...)` | `imageAtomic*(img, ivec2(x, 0), ...)` |
| `imageSize(img)` | `imageSize(img).x` (returns `int`) |
| `texelFetch(s, x, lod)` | `texelFetch(s, ivec2(x, 0), lod)` |
| `textureSize(s, lod)` | `textureSize(s, lod).x` |

**Files/functions to change.**

1. `gl/texture.cpp`
   - Add a driver-target helper specific to 1D (do **not** reuse
     `map_tex_target` blindly: it also maps 3D→2D at `gl/mg.cpp:89`). Map only
     `GL_TEXTURE_1D`/`GL_PROXY_TEXTURE_1D` to their 2D equivalents at driver
     call boundaries.
   - Implement `glTexImage1D` as a 2D `width×1` allocation, including the
     immutable image path: convert the format/type, lower unorm/narrow image
     formats exactly as `glTexImage2D` does, and call `glTexStorage2D` for
     null level-0 image formats. Record `TextureObject::target =
     TEXTURE_1D`, `height=1`, `depth=1`.
   - Implement `glTexStorage1D` as `GLES.glTexStorage2D(…,width,1)` with the
     same format conversion.
   - Implement `glTexSubImage1D` and `glCopyTexSubImage1D` through a shared
     internal helper that takes the already-resolved `TextureObject*` and
     calls the 2D driver entry point with `height=1, yoffset=0`. Do not route
     through the public 2D functions, which would look up the 2D binding slot.
   - Map the driver target in `glBindTexture`, `glTexParameteri/iv/fv`,
     `glGetTexParameter*`, `glGetTexLevelParameter*`, `glGetTexImage`,
     `glGenerateMipmap`, and `glClearTexImage`. In `glClearTexImage`, add
     `TextureTarget::TEXTURE_1D` to the 2D attachment switch and attach with
     `GL_TEXTURE_2D`.
   - Keep the logical target in the binding slot so `glGetTexParameter*` and
     image binding see `TEXTURE_1D`.
2. `gl/drawing.cpp`
   - `glBindImageTexture` needs no new target mapping once the driver object is
     a 2D texture. Keep `mg_lower_2d_image_layering` as is.
   - If a later sampler-path implementation is added, the driver 2D binding on
     the same unit can conflict with an application 2D texture. See the
     guardrail below.
3. `gl/glsl/compat_image.cpp`
   - Add a 1D pass that runs after the format-qualifier normalization:
     token-aware declaration rewrite (`image1D`→`image2D`) and call-site
     rewrites. Use balanced-parenthesis scanning, not a regex over argument
     text.
   - The pass must operate on the desktop source before glslang and on the
     direct-ESSL path; `mg_lower_legacy_image_formats` is called from both
     (`glsl_for_es.cpp:869`, `shader.cpp:83`), so put the rewrite there or in a
     function called from both.
4. `gl/gl_stub.cpp`
   - Remove the silent `glTexSubImage1D`/`glCopyTexSubImage1D` stubs once the
     real implementations exist.
5. `gl/glsl/glsl_for_es.cpp` / `gl/shader.cpp`
   - Ensure the 1D pass runs for both translated and direct ESSL sources.

**Guardrails.**

- Do not rewrite identifiers in comments/strings; use the existing
  comment/string mask pattern (`compat_subgroup.cpp:36` is a good model).
- Preserve `layout(binding=N)` and access qualifiers exactly.
- If a call shape is not recognized, emit
  `#error MG_IMAGE1D_UNREWRITTEN: <name>` rather than leaving `image1D`
  (which cannot compile in ESSL).
- `sampler1D` is a separate lowering. Phase 1 may claim **image1D only**: if a
  1D custom image has a `samplerName`, or the source declares `sampler1D`,
  fail loudly. Phase 2 implements `sampler1D` by the same 2D virtualization
  plus coordinate wrapping; the driver 2D binding conflict must be handled
  (reserved unit or draw-time rebind), otherwise a 1D sampler can silently
  sample a 2D texture bound on the same unit.
- Keep `glGetTexLevelParameteriv(GL_TEXTURE_HEIGHT)` answering `1`.
- Keep the 1D proxy state width-only; do not report a fake height from the
  proxy path.

**Probe and acceptance.**

| Probe | Where | Acceptance |
|---|---|---|
| `texture1d_image` (new) | wrapper probe, device | RG32UI 1D width 2048; `glTexImage1D`, `glBindImageTexture`, compute `imageStore` at x=0 and x=2047, `imageLoad` readback; `imageSize().x == 2048`; no GL error |
| `texture1d_clear` (new) | wrapper probe, device | `glClearTexImage` clears the whole 1D level; readback zero |
| `texture1d_query` (new) | wrapper probe | `GL_TEXTURE_WIDTH=2048`, `GL_TEXTURE_HEIGHT=1`, internal format preserved |
| `compat_image` 1D cases | host test | declaration and call rewrites pinned; unrecognized shape produces `#error` |
| Bliss LPV enabled | X1E game process, original ZIP | no compile error; LPV floodfill visible; default LPV-off path unchanged |

**Risk to other packs.** Low for image-only 1D (Bliss LPV). The driver 2D
binding alias is the main hazard; it only matters if an application binds a 1D
texture and a 2D texture on the same unit and samples both. The Phase-1
sampler1D prohibition prevents a silent wrong sample.

---

### G3 — Complete the legal image-format table

**Affected constructs.** Explicit `layout(<format>)` and formatless custom
images using any desktop format that is not in ES Table 8.27/8.28 or the ESSL
grammar.

**Authoritative sets.**

- ESSL 3.10/3.20 image format qualifiers: `rgba32f`, `rgba16f`, `r32f`,
  `rgba8`, `rgba8_snorm`; `rgba32i`, `rgba16i`, `rgba8i`, `r32i`;
  `rgba32ui`, `rgba16ui`, `rgba8ui`, `r32ui`.
- ES 3.1 Table 8.27 image unit formats: `RGBA32F`, `RGBA16F`, `R32F`,
  `RGBA32UI`, `RGBA16UI`, `RGBA8UI`, `R32UI`, `RGBA32I`, `RGBA16I`,
  `RGBA8I`, `R32I`, `RGBA8`, `RGBA8_SNORM`.
- ES 3.1 §8.22: a GL-allocated texture's image-unit format must be compatible
  by size (Table 8.28) unless it matches exactly. This is why widening the
  *storage* is mandatory, not optional: `R16UI` (16-bit) + `R32UI` image unit
  (32-bit) is a size mismatch; `RG32UI` (64-bit) + `RGBA32UI` (128-bit) is a
  size mismatch.

**Current mapping (source).**

| Logical | Backing (`mg_image_backing_format`) | Qualifier (`mg_image_qualifier`) | Storage widening | Status |
|---|---|---|---|---|
| `R8UI`, `R16UI` | `R32UI` | `r32ui` | `internal_convert` → `R32UI` | lower + widen; device binding half proven |
| `R8I`, `R16I` | `R32I` | `r32i` | `internal_convert` → `R32I` | **qualifier not lowered by `compat_image`** |
| `R8`, `R16`, `R16F` | `R32F` | `r32f` | none | **qualifier not lowered; storage not widened → size mismatch if used as an image** |
| `RG16F` | `RGBA16F` | `rgba16f` | null/storage → `RGBA16F` | data-backed storage mismatch |
| `RGBA16F` | `RGBA16F` | `rgba16f` | none | ok |
| `RGBA16` | `RGBA32F` | `rgba32f` | null/storage → `RGBA32F` | ok for images; data-backed `RGBA16` can yield an invalid unit format |
| `RG16` | `RGBA32F` | `rgba32f` | null/storage → `RGBA32F` | ok |
| `RG32F` | `RGBA32F` | `rgba32f` | **not widened** | **qualifier/storage size mismatch** |
| `RG32UI` | `RGBA32UI` | `rgba32ui` | `internal_convert` → `RGBA32UI` | ok |
| `RGBA32UI` | `RGBA32UI` | `rgba32ui` | none | ok |
| `RGBA8` | `RGBA8` | `rgba8` | none | ok |
| `RGBA8_SNORM` | `RGBA8_SNORM` | `rgba8_snorm` | allowlist missing | **null image uses mutable storage** |
| `RGBA16UI`, `RGBA8UI`, `RGBA16I`, `RGBA8I` | unchanged | **nullptr** | allowlist missing | legal ESSL formats but formatless metadata path cannot add them |

**Exact fix.**

1. Make `mg_image_backing_format`/`mg_image_qualifier` a single exhaustive
   table for every format the wrapper is willing to accept. Add the missing
   entries above. Return `nullptr` only for formats that have no legal image
   representation; callers must then fail.
2. Extend `mg_lower_legacy_image_formats` (`compat_image.cpp:43`) to lower:
   - `r8`, `r16`, `r16f` → `r32f`;
   - `r8i`, `r16i` → `r32i`;
   - keep `r8ui/r16ui → r32ui`, `rg32ui → rgba32ui`,
     `rgba16/rg16 → rgba32f`, `rg16f → rgba16f`, `rg32f → rgba32f`.
   The test `compat_image_test.cpp` currently pins `r16f-untouched`; that pin
   is wrong for ESSL and must be changed to `r16f → r32f`.
3. Add `RGBA8_SNORM`, `RGBA16UI`, `RGBA8UI`, `RGBA16I`, `RGBA8I` to
   `mg_is_image_storage_format` (`texture.cpp:557`) so null image allocations
   take the immutable path. Remove `RG32F` from the allowlist unless
   `mg_lower_16bit_image_storage` widens it to `RGBA32F`; otherwise the
   allowlist promises an image-legal storage that cannot be bound.
4. Add `RG32F → RGBA32F` and `R8/R16/R16F → R32F` to
   `mg_lower_16bit_image_storage` (`texture.cpp:541`) for null/storage image
   allocations. Do not widen data-bearing storage without converting the data
   (see guardrails). These formats are not legal image formats; if a pack uses
   them as images, the qualifier must be `r32f` and the storage must be 32-bit.
5. `mg_image_unit_format` (`image_metadata.cpp:94`) must never return a
   format outside Table 8.27. Today it can return the texture's
   `GL_RGBA16` for a data-backed `RGBA16` texture. Change it to:
   - return the exact storage format only when it is in Table 8.27;
   - otherwise return the table's widening for the logical format, and have
     `image_unit_bind_format` verify size compatibility; if it cannot, report
     `GL_INVALID_OPERATION` instead of passing an invalid format.
6. Replace the `colorimg8` name heuristic in `compat_image.cpp` with
   target-aware resolution if the source makes it available: Iris emits
   `const int colortex<N>Format = …;` for render targets, and `colorimg<N>`
   aliases `colortex<N>`. Parse the constant and use that backing format;
   otherwise keep the current narrow exception, documented as a named
   heuristic.
7. Add a shared table test that enumerates all Table 8.27 formats and asserts
   the qualifier is in the ESSL grammar and the backing is one of the storage
   formats the wrapper allocates.

**Guardrails.**

- Never emit an illegal qualifier. If the logical format is not mapped, emit
  `#error MG_IMAGE_FORMAT_UNMAPPABLE: <name> (<format>)` rather than leaving
  the source and letting the driver fail later. `rgba16_snorm`, `rgb10_a2`,
  and `r11f_g11f_b10f` have no Table 8.27 representation and are not among
  the widenings above, so they belong in this category unless a later legal
  widening is added
  (<https://registry.khronos.org/OpenGL/specs/es/3.1/es_spec_3.1.pdf>,
  Table 8.27).
- Preserve explicit `binding` and access qualifiers.
- Do not widen data-bearing storage (`glTexImage*` with `pixels != nullptr`,
  `glTexSubImage*`) unless the pixel data is converted to the new component
  layout. Today `internal_convert` rewrites `R16UI → R32UI` and
  `RG32UI → RGBA32UI` for every path; a `glTexSubImage2D` into a texture
  allocated that way is then presented with a mismatched client type/format.
  Either add integer/2→4-channel encoders to `mg_upload_fix_t` or restrict the
  rewrite to null allocations and report a bind failure for data-backed
  images. The latter is smaller and truthful.
- Keep the low-bit preservation claim for R8UI/R16UI→R32UI only for integer
  values in `[0, 255]`/`[0, 65535]`; it is a widening, not a bit-pattern
  reinterpretation.

**Probe and acceptance.**

| Probe | Where | Acceptance |
|---|---|---|
| `image_format_matrix` (new) | wrapper probe, device | one program per logical format (`r8ui`, `r16ui`, `rg32ui`, `rgba16`, `rg16`, `rg16f`, `rg32f`, `r8`, `r16`, `r16f`, `r8i`, `r16i`, `rgba8_snorm`, `rgba16ui`) compiles; store/load round-trip preserves values; no `GL_INVALID_VALUE` |
| `image_format_table` (new) | host test | every Table 8.27 entry has a legal qualifier/backing pair; unmappable formats return nullptr |
| `metadata_image_format` / `formatless_image_metadata` | device (existing) | still pass; formatless path uses the new table |
| iterationRP S0 | X1E game process, original ZIP | no illegal qualifier; pipeline active (already passes S0 with a G1 core) |
| Solas S0 | X1E game process | R8UI 3D image binds; no format warning |

**Risk.** The main risk is data-bearing uploads into narrowed integer
textures; the guard above prevents a silent misread. Memory increases
(`RGBA16/RG16 → RGBA32F`, `RG16F → RGBA16F`, `RG32F → RGBA32F`) are real and
must be recorded; for a 192³ RGBA16 volume this is 56.6 MB (54 MiB) →
113.2 MB (108 MiB).

---

### G4 — Image atomics

**Affected construct.** `imageAtomicAdd/Min/Max/And/Or/Xor/Exchange/CompSwap`
on `uimage2D/3D` (r32ui) and `iimage2D/3D` (r32i).

**Current status.** ESSL 3.20 has core integer `imageAtomic*` functions; ESSL
3.10 has none, which is why the provider's `GL_OES_shader_image_atomic`
extension matters for any ES 3.1-class source. The provider exposes that
extension on this tuple. The wrapper has `glMemoryBarrier` pass-through
(`gl/drawing.cpp:502`) and image binding works for R32UI (device). **No probe
exercises image atomics.** iterationRP uses them in fragment, compute and
geometry stages; Photon/Rethinking Voxels are unverified.

**Exact fix / probe.**

1. Add `wrapper_probe image_atomic_2d`:
   - compute shader `layout(r32ui, binding=0) uniform uimage2D img;`
   - `imageAtomicMax(img, ivec2(0), 7u)`, `imageAtomicMin(img, ivec2(0), 3u)`,
     `imageAtomicAdd(img, ivec2(1), 5u)`;
   - `glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT)`;
   - readback through a second compute load or `glGetTexImage`; expect
     `3, 5`.
2. Add `image_atomic_3d` with an R32UI 3D image and `imageAtomicMax` at a
   non-zero layer.
3. Add `image_atomic_fragment` (fragment `imageAtomicMin`) and
   `image_atomic_geometry` (geometry `imageAtomicMax` on `uimage3D`) once the
   stage probes exist.
4. If the translated ESSL fails, check whether SPIRV-Cross emits
   `imageAtomic*` at `#version 320 es`; if it emits the OES names, add
   `#extension GL_OES_shader_image_atomic : enable` (provider exposes it) in
   the ESSL prelude. Do not silently drop the atomic.

**Guardrails.**

- Do not report a pass from a shader that compiled but never executed the
  atomic; the probe must read back the value.
- Keep the barrier; atomics without a barrier can read stale data.
- Do not lower an atomic to a load/store pair.

**Acceptance.** Expected min/max/sum values on device for 2D, 3D, fragment and
geometry; no GL error; iterationRP shadow/RTW paths do not produce imageAtomic
compile errors.

**Risk.** If Adreno's R32UI storage image supports load/store but not atomics,
this becomes a provider limit for iterationRP's RTW/voxel path. That would be
a real default-path failure and must be recorded, not hidden.

---

### G5 — Subgroup lowering

**Affected construct.** `gl_SubgroupInvocationID`, `gl_SubgroupSize`,
`gl_SubgroupID`, `gl_NumSubgroups`, `subgroupInclusiveAdd`,
`subgroupQuadSwap*`, `subgroupShuffleXor`, `subgroupBallot*`,
`subgroupPartition*`.

**Current status.**

- Hardware: provider exposes no `GL_KHR_shader_subgroup_*` or
  `GL_NV_shader_subgroup_partitioned` (device extension list); the capability
  probe records `quad=no arith=no ballot=no`.
- Wrapper: compute-only lowering to a virtual 32-lane subgroup:
  - IDs map to `gl_LocalInvocationIndex` arithmetic;
  - `subgroupInclusiveAdd` is a barriered shared-memory scan (float/vec2/3/4);
  - generic quad swaps use shared memory;
  - FidelityFX SPD `SpdReduceQuad` and mips 3/4/5 are replaced with a
    workgroup port;
  - the NV partition / XOR duplicate-suppression pattern is rewritten to a
    shared partition table (`mg_lower_nv_partition_pattern`), including the
    KHR-only XOR loop that survives Iris preprocessing on Adreno
    (`MC_GL_VENDOR_NVIDIA` is false, `MC_GL_VENDOR_OTHER` is defined);
  - residual constructs produce `#error MG_SUBGROUP_UNSUPPORTED`.
- Host tests pass (`compat_subgroup_test`, `compat_subgroup_partition_test`).
- Device compile/link of iterationRP's compute graph passed with a G1 core
  (`2026-10-20-iterationrp-original-s0.md`); **semantics** (prefix sums,
  leader-only atomics) are not device-verified.
- iterationRP's non-NVIDIA `BackwardAnalysis.glsl` branch uses
  `subgroupShuffleXor` in a loop with `imageAtomicMax`; the partition pass
  recognizes exactly this shape.

**Exact fix / probe.**

1. Add a device `subgroup_lowered` probe that runs:
   - the inclusive-add scan (expected prefix sums over 128 invocations);
   - the XOR duplicate-suppression loop with a workgroup-shared write count
     (expected one leader per duplicate key);
   - the SPD quad reduce (expected reduced values).
2. Document the semantic deviation: a hardware subgroup can be 32 or 64 wide
   and quad lanes follow hardware layout; the lowering uses 32 consecutive
   local invocations and a linear-lane XOR. This is faithful for packs that
   query `gl_SubgroupSize` and treat lanes as a linear group (iterationRP),
   and a deviation for packs that assume hardware quad layout.
3. Keep the loud `#error` for ballot/broadcast/elect/integer scans and for
   subgroup use outside compute. Extend only when a target pack requires it.
4. Barrier guardrail: the injected scan functions contain `barrier()`. A call
   inside non-uniform control flow is invalid. The wrapper cannot prove
   uniformity; the host transform must refuse patterns it cannot prove (the
   partition pass already checks for enclosing conditionals and early
   returns).

**Acceptance.** Device probe values match the CPU reference; iterationRP S1
color path still requires its own image-unit/format validation (G3/G14), so a
subgroup probe pass alone does not promote iterationRP.

**Risk.** Virtual 32-lane semantics can change results for packs that rely on
64-wide subgroups or hardware quad layout. That is a named deviation, not a
bug to hide.

---

### G6 — RGBA16 attachments and unorm16 image storage

**Affected construct.** `GL_RGBA16` / `GL_RG16` color attachments; `layout(rgba16)`
/ `layout(rg16)` storage images.

**Current status.**

- `internal_convert` keeps `RGBA16` when `GL_EXT_texture_norm16` is present,
  otherwise `RGBA16F` (`texture.cpp:691-700`).
- `mg_lower_16bit_image_storage` maps `RGBA16` and `RG16` to `RGBA32F` for
  null `glTexImage2D/3D` allocations and for all `glTexStorage2D/3D` calls
  (`texture.cpp:541`, `:1075`, `:1167`, `:1278`, `:1305`). On this tuple
  (`GL_EXT_texture_norm16` present) the Bliss RGBA16 colortex attachments are
  allocated as `RGBA32F`, which the provider probe marks FBO-complete. On a
  provider without norm16, `internal_convert` first produces `RGBA16F` and
  the same attachment is 2× rather than 4×.
- Raw `RGBA16`/`RG16` are absent from the provider probe table
  (`capability_probe` `kFormats`); their FBO complete/clear/image-binding
  status is unverified.
- `GL_EXT_texture_norm16` **is exposed** by the provider, and the extension
  spec marks `RGBA16_EXT` color-renderable (`CR=X`) in Table 8.13. So raw
  RGBA16 may pass; the probe must decide, not the extension list alone.
- Data-backed `RGBA16` textures are not widened; if they are image-bound,
  `mg_image_unit_format` can return `GL_RGBA16`, which is not a legal image
  unit format.

**Exact fix / probe.**

1. Add `RGBA16` and `RG16` entries to `capability_probe` with
   `GL_RGBA`/`GL_UNSIGNED_SHORT` and `GL_RG`/`GL_UNSIGNED_SHORT`; record
   `texture2d`, `texture3d`, `framebuffer`, `clear`, `image_binding`.
2. Add a `wrapper_probe rgba16_attachment` that creates a mixed MRT
   (`RGBA16`, `RGBA16F`, `R11F_G11F_B10F`), checks FBO completeness, clears,
   samples, and queries `GL_TEXTURE_INTERNAL_FORMAT`.
3. Decision:
   - If raw `RGBA16` is FBO-complete and image binding is not required, the
     wrapper could keep `RGBA16` for attachments (1× memory). But a texture
     can be both a colortex and a `colorimgN` image; the wrapper cannot know
     at allocation. Therefore the safe default is the current lowering to
     `RGBA32F` (exact, 4× memory).
   - `RGBA16F` is the only 2× alternative that is both renderable and a legal
     image backing (`rgba16f`). It is not bit-exact for unorm16. Do not switch
     silently; if a 2× mode is desired, make it an explicit setting and
     record the precision deviation.
4. Fix `mg_image_unit_format` so a data-backed `RGBA16` image binding either
   returns a Table 8.27 format with size-compatible storage or fails
   explicitly (see G3).

**Acceptance.** Provider probe records raw RGBA16/ RG16; wrapper mixed-MRT
probe FBO complete; Bliss S0 after G0; iterationRP `voxelData3D` RGBA16 image
allocates and binds with `rgba32f`; memory measured.

**Risk.** Switching the default backing changes precision and memory for every
RGBA16 colortex and image. Keep the current exact widening unless a pack-level
gate requires otherwise.

---

### G7 — Mutable multi-level 2D/3D images

**Affected construct.** Building a mip chain level by level for a texture that
is also an image.

**Current status.** `glTexImage2D` converts `level==0 && pixels==nullptr &&
GL_TEXTURE_2D && image_storage_format` to `glTexStorage2D(…,1,…)`
(`texture.cpp:1075`). `glTexImage3D` does the same for `GL_TEXTURE_3D`
(`:1167`). A later `glTexImage2D/3D` for level>0 targets immutable one-level
storage and fails at ANGLE. The wrapper still writes `tex->width/height/depth`
after the failed driver call, which can corrupt later queries.

**Exact fix.**

1. Record on `TextureObject` that the wrapper created one-level immutable
   image storage (`immutable_image_levels = 1`).
2. In `glTexImage2D/3D`, if the target level is >0 and the texture has wrapper
   immutable one-level storage:
   - report `GL_INVALID_OPERATION` with a forced log naming the texture and
     level;
   - do **not** update the shadow dimensions/format.
3. Optional full-chain mode (only if a pack needs it): when converting level 0,
   allocate `levels = floor(log2(max(width,height,depth))) + 1` and redirect a
   later same-format `glTexImage*` level>0 to `glTexSubImage*`. This preserves
   desktop level-by-level uploads at ~14% extra storage for a cubic 3D image,
   but increases memory for every null image allocation. Default off.
4. Add the same detection for 2D image storage; the gap is not 3D-only.

**Probe and acceptance.**

| Probe | Where | Acceptance |
|---|---|---|
| `image3d_mutable_mip` (new) | wrapper probe, device | level 0 null, level 1 upload; either explicit `GL_INVALID_OPERATION` (default) or a correct level-1 readback (full-chain mode); no shadow state corruption |
| `image2d_mutable_mip` (new) | wrapper probe | same for 2D |
| Existing `image2d_mutable_bind` | mode-only probe | document its result; it is not in `all` |

**Risk.** Full-chain mode increases memory for every Iris custom 3D image by
~14% (more for non-cubic volumes) and can push a 192³ RGBA32F volume from
108 MiB to ~123 MiB (~129 MB). Keep it off unless required.

---

### G8 — Texture barriers and texture views

**Affected construct.** `glTextureBarrier`, `glTextureBarrierNV`,
`glTextureView`.

**Current status.** Provider has neither `GL_ARB/EXT/NV_texture_barrier` nor
`GL_EXT/ARB_texture_view`. The wrappers are stubs that only log
(`gl_stub.cpp:765`, `:2213`, `:639`). `glTextureView` leaves the application's
new texture name without storage; `glTextureBarrier` returns without any
synchronization.

**Exact fix.**

- Replace both with an explicit unsupported result:
  `mg_set_gl_error(GL_INVALID_OPERATION)` plus a forced one-time warning.
  For `glTextureView`, do not define storage for the view name.
- Keep the named deviation in the capability contract. Do not advertise
  `GL_ARB_texture_barrier`/`GL_ARB_texture_view`.
- Do not claim `glMemoryBarrier`/`glFlush`/`glFinish` fixes ES feedback
  undefinedness; they cannot.
- Document Iris `BufferFlipper` as the software workaround the pinned path
  uses (it swaps main/alternate textures; it does not call a barrier).

**Probe and acceptance.** `texture_barrier_contract` and
`texture_view_contract` in a host/device wrapper probe: a valid desktop call
returns `GL_INVALID_OPERATION` and no device loss; a pack that never calls
them is unaffected. Acceptance: explicit error, no silent success.

**Risk.** An application that calls `glTextureBarrier` unconditionally and does
not check errors will now see an error state; that is the truthful contract.
Iris 1.8.14's pinned path does not call it.

---

### G9 — Default tessellation levels

**Affected construct.** `glPatchParameterfv(GL_PATCH_DEFAULT_INNER_LEVEL /
OUTER_LEVEL)`; desktop TES without a TCS uses these defaults.

**Current status.** Explicit `GL_INVALID_OPERATION`
(`gl/gl.cpp:187`); `glPatchParameteri(GL_PATCH_VERTICES)` is forwarded. A TCS
that writes `gl_TessLevel*` works on device.

**Exact fix.** Keep the explicit error and the named deviation. If a common
pack is later found to require defaults, the only faithful lowering is to
synthesize a passthrough TCS from the TES interface:

1. At link, detect a program with a TES and no TCS and non-default
   `glPatchParameterfv` state.
2. Parse the TES inputs (per-vertex and per-patch) and generate a TCS with
   `layout(vertices = patch_vertices) out;`, copying every input from `gl_in`
   to `gl_out`, copying patch inputs, writing the stored
   `gl_TessLevelOuter/Inner`, and forwarding the vertex position.
3. Recompile/link; if any interface cannot be reproduced, fail explicitly.

This is a large, high-risk change; do not implement it without a pack gate.
**Acceptance for the current plan:** `patch_default_levels` still reports
`GL_INVALID_OPERATION`; no silent acceptance.

**Risk.** A synthesized TCS can change derivatives/primitive behavior and
interact with arrayed inputs; an incorrect synthesis is worse than a clear
error. Keep it optional.

---

### G10 — Clip/cull distance truthfulness

**Affected construct.** `gl_ClipDistance`, `gl_CullDistance`,
`glEnable(GL_CLIP_DISTANCEi)`.

**Current status.** Provider lacks `GL_EXT_clip_cull_distance`
(device log `clip_cull_distance=0`). `enable.cpp:434` already avoids the
driver call when the extension is absent. However, `InitResources` starts with
`maxClipDistances = 8`, `maxCullDistances = 8`,
`maxCombinedClipAndCullDistances = 8`, and only queries
`GL_MAX_*` when `GLES.glGetIntegerv` exists. If the provider reports 8 even
without the extension, glslang accepts a shader that SPIRV-Cross cannot emit
for ESSL 3.20.

**Exact fix.**

1. In `glsl_for_es.cpp`, set these three limits to 0 when
   `!g_gles_caps.GL_EXT_clip_cull_distance` (after the backend query), so
   glslang rejects `gl_ClipDistance`/`gl_CullDistance` at the translation
   boundary with a clear compile error.
2. Make the frontend `glGetIntegerv` answers for
   `GL_MAX_CLIP_DISTANCES`/`GL_MAX_CULL_DISTANCES`/
   `GL_MAX_COMBINED_CLIP_AND_CULL_DISTANCES` (`gl/getter.cpp`) truthful (0) on
   this tuple; do not report 8 if the shader cannot use it.
3. No emulation. A fragment-discard approximation changes clipping semantics,
   depth, and derivatives; it is not a legal substitution.

**Probe and acceptance.** `clip_distance_gate` (new): a desktop shader writing
`gl_ClipDistance[0]` must fail to compile with a log naming the limit, not
produce an ESSL builtin that ANGLE rejects later. Acceptance: compile failure
is visible through `glGetShaderiv(GL_COMPILE_STATUS)`; the query reports 0.

**Risk.** An app that queries the limit and chooses a fallback now sees 0,
which is correct. An app that assumed 8 and did not check would fail earlier
and visibly.

---

### G11 — Vertex/geometry-stage multi-image capacity

**Affected construct.** Image uniforms and image atomics in vertex/geometry
shaders (desktop supports them; ES 3.2 supports them subject to stage limits).

**Current status.** `InitResources` queries `GL_MAX_VERTEX_IMAGE_UNIFORMS`,
`GL_MAX_FRAGMENT_IMAGE_UNIFORMS`, `GL_MAX_GEOMETRY_IMAGE_UNIFORMS` and
tessellation-stage limits (`glsl_for_es.cpp:171-215`). Device
`vertex_image2d_store` passes for RGBA16F; iterationRP `PixelData_VS` uses one
RG16F image and `shadow.gsh` uses three R32UI images plus `imageAtomicMax`.
There is no multi-image vertex or geometry probe.

**Exact fix / probe.**

1. `vertex_multi_image`: two RGBA16F/R32UI images, write both from the vertex
   stage, read back.
2. `geometry_image_atomics`: three R32UI 3D images, `imageAtomicMax` per
   invocation, read back.
3. Verify the glslang limits match the backend; a zero limit must produce a
   compile/link failure, not a driver crash.

**Acceptance.** Both probes pass on X1E; distinct units; atomics visible. If
geometry image atomics fail, record a provider limit for iterationRP's
`shadow.gsh` path; do not weaken the probe.

**Risk.** Low if the limits are truthful; the main risk is a driver crash on
multi-image GS, which the probe is designed to catch with a fence/readback.

---

### G12 — SSBO size cap

**Affected construct.** `bufferObject.<n>` > `GL_MAX_SHADER_STORAGE_BLOCK_SIZE`.

**Current status.** Provider cap 128 MiB; WSR 128 (50,855,936 B) and WSR 192
(114,229,248 B) pass; WSR 256 (202,899,456 B) maps but the last element reads
0. Iris documents a guaranteed 128 MB, so a pack asking for more is outside
the guaranteed contract.

**Exact fix.** None in the wrapper. Do not split a block silently, do not
raise the advertised limit, and do not claim WSR 256. Keep the provider limit
visible to Iris so it can reject at load. If a pack asks for a block larger
than the limit, the truthful outcome is a load/compile failure, not a
truncated buffer.

**Acceptance.** `ssbo_sizes` remains `wsr128 pass`, `wsr192 pass`,
`wsr256 fail`; WSR 128 remains the required advanced target.

**Risk.** None; this is a provider/lane limit.

---

### G13 — Bindless textures and separate shader objects

**Affected construct.** `GL_ARB_bindless_texture`, `GL_ARB_separate_shader_objects`.

**Current status.** Not implemented/advertised; the extension list deliberately
omits SSO and `GL_KHR_no_error` (`getter.cpp:269-273`). No audited pack uses
them.

**Exact fix.** Keep as named deviations. Do not advertise an extension name
without the corresponding semantics. If a future pack requires SSO, implement
it as a program-pipeline layer with its own tests; it is not a desktop-GL
feature gap for the current pack set.

---

### G14 — Image-unit determinism and lifecycle (related seam, not a desktop-GL feature)

This is not a desktop-GL capability, but it gates the same packs and must not
be confused with the rows above.

- Current evidence: the core has explicit-binding preservation, duplicate-
  formatless separation, per-share-group fallback maps, and program/array
  uniform routing (`feature-matrix.md` rows; device suites 32/32-58/58).
- Open: the bridge was required for the ACT/WSR staged passes; a core-only
  proof with the bridge removed is still G2 in
  `backend-support-fixing-plan.md`; lifecycle maps across pack switch/reload
  are G6 there.
- Do not use the bridge-assisted ACT/WSR staged passes as evidence that the
  desktop-GL rows above are closed.

---

## 4. Priority and sequencing

### P0 — confirmed default-path failure

1. **G0 late-uniform hoist.** Bliss is the only confirmed default-path S0
   failure in the current evidence. Add the missing host test, then re-run the
   original Bliss default on X1E. This must be closed before any Bliss claim
   and before using Bliss to test RGBA16/1D.

### P1 — default-path or required-advanced seams

2. **G3 format table + G6 RGBA16 probe.** Finish the legal-format table,
   fix the `RG32F` storage mismatch and the `r16f/r8/r16/r8i/r16i` omissions,
   add the immutable allowlist entries, and run the device format matrix.
   This is the default path for iterationRP and Solas; it also removes the
   remaining uncertainty around Bliss LPV formats.
3. **G1 formatless metadata.** Qualify the Iris bridge as the producer and
   make the core failure named. Solas is the default-path witness.
4. **G4 image atomics + G5 subgroup semantics + G11 vertex/GS images.**
   These are iterationRP's default-path compute features. Device probes first;
   code changes only if a probe fails.

### P2 — optional features

5. **G2 1D virtualization.** Optional for Bliss LPV, but a clean, bounded
   legal lowering with a small surface (image-only phase 1). Do it after the
   default-path rows so it cannot mask a default-path failure.
6. **G7 mutable multi-level images** (only if a pack needs it; prefer the
   explicit error).
7. **G9 default tessellation levels** (only with a pack gate; TCS synthesis is
   a large change).

### P3 — provider/API hard boundaries

8. **G8 texture barriers/views**, **G10 clip/cull distance**, **G12 SSBO cap**,
   **G13 bindless/SSO**. Make the failures explicit, document the deviations,
   and do not spend wrapper complexity on emulation that changes semantics.

### Explicit non-goals

- Do not raise the GL version or advertise extensions to make Iris choose a
  path the backend cannot execute.
- Do not edit original shaderpack archives.
- Do not use the private iterationRP ports (which widen formats, rewrite
  subgroups/SH, and change BackwardAnalysis) as original-pack evidence.
- Do not treat the bridge-assisted ACT/WSR passes as desktop-GL feature
  evidence.

---

## 5. Probe matrix

| Probe | Type | New/existing | What it proves | Acceptance |
|---|---|---|---|---|
| `late_uniform_hoist` | host + wrapper | new | late uniform moved before first function | compiles; unrelated lines byte-identical |
| Bliss `deferred.fsh` S0 | device game process | existing fixture | G0 closed | no `'uniform' : syntax error`; no fallback |
| `formatless_image_metadata` | device wrapper | existing | metadata positive/negative | positive `12345`; negative named failure |
| Solas `shadowcomp.csh` | device game process | existing fixture | G1 closed | no `No image internal format specified`; image semantics verified |
| `texture1d_image` | device wrapper | new | 1D image storage/bind/store/load | x=0/2047 round-trip; `imageSize().x=2048` |
| `texture1d_clear` / `texture1d_query` | device wrapper | new | 1D clear/get | zero readback; width/height correct |
| `compat_image` 1D cases | host | new | declaration/call rewrite | pinned strings; unrecognized shape `#error` |
| `image_format_matrix` | device wrapper | new | G3 lowering | every listed format compiles; values preserved |
| `image_format_table` | host | new | qualifier/backing legality | all Table 8.27 entries map; unmappable → nullptr |
| `rgba16_attachment` | device wrapper | new | RGBA16 mixed MRT | FBO complete; internal format reported |
| capability_probe `RGBA16/RG16` | device provider | new | raw provider formats | texture/FBO/clear/image recorded |
| `image_atomic_2d/3d/fragment/geometry` | device wrapper | new | G4 | expected min/max/sum; barrier; no error |
| `subgroup_lowered` | device wrapper | new | G5 semantics | prefix sums/leaders match CPU reference |
| `vertex_multi_image` / `geometry_image_atomics` | device wrapper | new | G11 | distinct units; atomics read back |
| `image3d_mutable_mip` / `image2d_mutable_mip` | device wrapper | new | G7 | explicit error or correct full-chain upload |
| `texture_barrier_contract` / `texture_view_contract` | host/device | new | G8 | explicit `GL_INVALID_OPERATION` |
| `clip_distance_gate` | device wrapper | new | G10 | compile failure visible; query 0 |
| `patch_default_levels` | device wrapper | existing | G9 | explicit `GL_INVALID_OPERATION` |
| `ssbo_sizes` | device provider | existing | G12 | 128/192 pass; 256 fail |

All new device probes must run against the same provider library and EGL
context as the game process, and must read back values (no compile-only
passes). A probe that returns `GL_NO_ERROR` without a readback is not
evidence.

---

## 6. Claims rejected or not promoted

| Claim | Why it is rejected / not promoted |
|---|---|
| “MobileGlues cannot synthesize subgroup semantics with a shader rewrite without changing algorithm behavior” | The tree has a compute-only legal lowering (IDs, inclusive add, quad swaps, SPD, NV/XOR partition pattern) with host tests. Device semantics are unverified, and unsupported constructs fail loudly. This is not a blanket inability. |
| “RGBA16 attachments are an open Bliss blocker” | Null/storage `RGBA16` is already lowered to `RGBA32F`, which the provider probe marks FBO-complete. Raw `RGBA16` remains unprobed, but it is not the current Bliss S0 blocker; the confirmed blocker is the late `uniform`. |
| “1D images block Bliss default” | Bliss LPV is disabled by default (`lib/settings.glsl:746`, `shaders.properties` disables `setup`/`shadowcomp`). 1D is an optional LPV feature. |
| “R8UI/R16UI lowering is absent” | Implemented at HEAD (`98058e3`); host tests pass. Storage/binding half is device-exercised; the new qualifier half is not device-validated. Rows in `feature-matrix.md`/`README.md`/`decisions.md` saying otherwise are stale. |
| “Image atomics are unsupported” | Not established. ESSL 3.20 has core integer image atomics; the provider exposes `GL_OES_shader_image_atomic` for ES 3.1-class sources; no probe has been run. |
| “RGB8_SNORM still fails as an attachment” | It is lowered to `RGBA8_SNORM`; device `rgb8_snorm_attachment` passes. |
| “RG32F lowering is complete” | The qualifier is lowered to `rgba32f`, but `RG32F` storage is not widened to `RGBA32F`, so the image-unit format and storage are size-incompatible. This is a format-table defect (no audited default pack uses it, but the test/comment claims completeness). |
| “Texture barriers/views can be emulated with `glMemoryBarrier`” | ES has no feedback-aliasing guarantee; `glMemoryBarrier` cannot make the ES feedback loop defined. Views alias storage; a copy is not a view. Named deviations. |
| “WSR 256 can be fixed by raising the wrapper limit” | Provider `MAX_SHADER_STORAGE_BLOCK_SIZE=134,217,728`; the last element of a 202,899,456-byte block reads 0. Provider/lane limit, not a wrapper constant. |
| “Bliss is blocked by RGBA16/format limits” | The confirmed S0 failure is the late `uniform`; RGBA16 is lowered and the format path is not reached. Do not attribute the failure to a GPU feature. |
| “The bridge hoist fixes Bliss” | The bridge hoist runs on the desktop source before Iris compiles; the failing uniform is in the translated ESSL. The core ESSL hoist is the correct layer and is device-unverified. |
| “Solas default is format-clean” | The device log contains repeated `No image internal format specified`; the image-metadata seam is open. |
| “IterationRP is supported” | S0 passed with a G1 core; S1 fails (split viewport/FSR and brown/black lighting). No promotion. |
| “Complementary/BSL/Sildur effect fidelity is qualified” | Unchanged from `feature-matrix.md`: spot checks only; effect-by-effect gates remain NOT RUN. |

---

## 7. Sources

Primary specifications (read for this document):

- ESSL 3.10 (ES 3.1): <https://registry.khronos.org/OpenGL/specs/es/3.1/GLSL_ES_Specification_3.10.pdf>
  — §4.4.7 image format qualifiers; reserved 1D sampler/image names.
- ESSL 3.20 (ES 3.2): <https://registry.khronos.org/OpenGL/specs/es/3.2/GLSL_ES_Specification_3.20.pdf>
  — §4.4.7 same qualifier set; §8.12 `imageAtomic*`.
- OpenGL ES 3.1: <https://registry.khronos.org/OpenGL/specs/es/3.1/es_spec_3.1.pdf>
  — §8.22 image units and target list, Table 8.27 formats, Table 8.28
  size/class compatibility.
- OpenGL 4.6 core: <https://registry.khronos.org/OpenGL/specs/gl/glspec46.core.pdf>
  — desktop 1D targets, `glPatchParameterfv`, texture barriers/views.
- `EXT_texture_norm16`: <https://registry.khronos.org/OpenGL/extensions/EXT/EXT_texture_norm16.txt>
  — `RGBA16_EXT` color-renderable (`CR=X`) and texture format.
- `OES_shader_image_atomic`: <https://registry.khronos.org/OpenGL/extensions/OES/OES_shader_image_atomic.txt>
  — image atomic functions against ESSL 3.10.
- Khronos image load/store wiki: <https://www.khronos.org/opengl/wiki/Image_Load_Store>

Iris documentation and source:

- Custom images: <https://shaders.properties/current/reference/buffers/custom_images/>
- SSBOs (128 MB guarantee): <https://shaders.properties/current/reference/buffers/ssbo/>
- Feature flags: <https://shaders.properties/current/reference/shadersproperties/flags/>
- `TextureType` (1D `texImage1D`): <https://github.com/IrisShaders/Iris/blob/1.21.1/common/src/main/java/net/irisshaders/iris/gl/texture/TextureType.java>
- `ShaderProperties` (7-token image → `TEXTURE_1D`): <https://github.com/IrisShaders/Iris/blob/1.21.1/common/src/main/java/net/irisshaders/iris/shaderpack/properties/ShaderProperties.java>
- `GlImage` (null `apply`, `glClearTexImage`): <https://github.com/IrisShaders/Iris/blob/1.21.1/common/src/main/java/net/irisshaders/iris/gl/image/GlImage.java>
- `StandardMacros` (`MC_GL_VENDOR_OTHER` for Adreno): <https://github.com/IrisShaders/Iris/blob/1.21.9/common/src/main/java/net/irisshaders/iris/gl/shader/StandardMacros.java>
- `BufferFlipper` (no texture barrier): <https://github.com/IrisShaders/Iris/blob/1.21.9/common/src/main/java/net/irisshaders/iris/targets/BufferFlipper.java>

OptiFine:

- `shaders.txt` (custom image aliases `colorimg0-5`, image load/store in all
  programs): <https://github.com/sp614x/optifine/blob/master/OptiFineDoc/doc/shaders.txt>
- Shader development guide: <https://optifine.readthedocs.io/shaders_dev.html>

Local evidence:

- `docs/mobileglues-googlebook/feature-matrix.md`
- `docs/mobileglues-googlebook/execution-state.md`
- `docs/mobileglues-googlebook/backend-support-fixing-plan.md`
- `docs/mobileglues-googlebook/validation/2026-10-18-x1e-provider-probe.json`
- `docs/mobileglues-googlebook/validation/2026-10-19-x1e-iterationrp-caps.json`
- `docs/mobileglues-googlebook/validation/2026-10-19-iterationrp-shader-transform.md`
- `docs/mobileglues-googlebook/validation/2026-10-20-bliss-s0-fail.md`
- `docs/mobileglues-googlebook/validation/2026-10-20-solas-s0.md`
- `docs/mobileglues-googlebook/validation/2026-10-20-iterationrp-original-s0.md`
- `docs/mobileglues-googlebook/validation/2026-10-20-g1-rg16f-rg32f-lowering.md`
- `docs/mobileglues-googlebook/validation/2026-10-20-cross-pack-render-check.md`
- `docs/mobileglues-googlebook/validation/2026-10-20-x1e-sildur-format-probe.md`
- `docs/mobileglues-googlebook/shaderpack-support/audits/2026-10-20-bliss-static-audit.md`
- `docs/mobileglues-googlebook/shaderpack-support/audits/2026-10-20-solas-static-audit.md`
- `docs/mobileglues-googlebook/shaderpack-support/audits/2026-10-20-iterationrp-static-audit.md`
- Source: `MobileGlues-cpp/gl/{texture.cpp,image_metadata.cpp,drawing.cpp,shader.cpp,shader_source.cpp}`, `MobileGlues-cpp/gl/glsl/{compat_image.cpp,compat_subgroup.cpp,compat_subgroup_partition.cpp,glsl_for_es.cpp}`, `MobileGlues-cpp/gl/image_metadata.h`
- Host tests: `MobileGlues-cpp/tests/{compat_image_test.cpp,image_metadata_test.cpp,compat_subgroup_test.cpp,compat_subgroup_partition_test.cpp,shader_source_test.cpp}`; `sh MobileGlues-cpp/tests/run.sh` passed on 2026-10-21.
- Probes: `MobileGlues-cpp/tools/capability_probe/probe.cpp`, `MobileGlues-cpp/tools/wrapper_probe/main.cpp`.

Version caveat: the pinned Iris 1.8.14 source is not published under a matching
GitHub tag; the Iris source links above are the nearest published 1.21.1/1.21.9
branches. The 1D allocation and parsing code is identical in those branches and
is the code path described by the Bliss audit. The pinned jar remains the
delivery artifact; no Iris source was modified.
