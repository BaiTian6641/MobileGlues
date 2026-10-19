# Feature and Qualification Matrix

Status vocabulary is the plan's: **not tested**, **fails to load**, **loads with missing effects**,
**correct for this preset**, **correct but below the performance target**, **qualified for release**.

No row is release-qualified. The X1E Googlebook (`C1L63708PZ`) and the FCL game process are available;
staged rows below are device evidence, but named-effect coverage, performance, and lifecycle gates remain
open.

The primary goal is standard desktop OpenGL behavior through MobileGlues so ordinary shaderpacks work
out of the box. Required advanced X1E coverage is Very High ACT 192 and the custom WSR 128 profile;
Ultra ACT 256/WSR 256 is optional/future X2E and `BLOCKED` on X1E by the provider storage-block limit.
The contract is a truthful core version plus individually earned extensions covering translation,
framebuffer routing, image/SSBO mapping, compute and barrier semantics; unimplemented features are
named deviations, not claims, and full GL 4.6 conformance is not promised.

This file preserves evidence captured under the historical X1E plan. Its stage labels and status
vocabulary are not the current `S0`-`S9` table; see `README.md` and `../README.md` for the mapping and
current lane status. The connected unit is an HP Googlebook 14 rather than the XPS reference
specification.

## Target delivery lane

| Field | Value |
| --- | --- |
| Minecraft | 1.21.1, Java 21 (`java-runtime-delta`), LWJGL 3.3.3 GLFW |
| Loader | NeoForge 21.1.262 (supported X1E target; 21.1.264 is an optional future patch) |
| Iris | 1.8.14-beta.1+1.21.1-neoforge, Modrinth `KduFYu4t` |
| Sodium | mc1.21.1-0.8.13-neoforge, Modrinth `uMOpc5uV` |
| Stable fallback pair | Iris 1.8.12 NeoForge (`t3ruzodq`) with Sodium 0.6.13 NeoForge (`Pb3OXVqC`); never 1.8.12 with 0.8.13 |
| Status | **STAGED/WORKING** on the supported 21.1.262 tuple (world load, render, save/quit); effect fidelity, performance and lifecycle remain open. NeoForge 21.1.264 is optional future work, not a blocker |

Every game-process row below was captured on **Minecraft 1.21.1 / NeoForge 21.1.262** with Iris
1.8.14-beta.1 and Sodium 0.8.13. That is the supported delivery-lane tuple; the 13:45 world/title
pass is the reference evidence. Effect-specific rows are not upgraded by this header. The
1.20.1/Fabric legacy and 26.3/Fabric future lanes have no game-process rows here.

## Renderer / build rows

| Renderer build | Backend/provider | Device | Level | Result | Evidence |
| --- | --- | --- | --- | --- | --- |
| core `97558a6`, ARM64 release | unknown (no device) | X1E-80-100 Googlebook | target device | not tested | pending hardware |
| core `97558a6`, ARM64 release | system EGL/GLES | x86_64 emulator API 36 | host/emulator | not tested | planned probe only |
| core `97558a6`, host x86_64 | Mesa 26.0 llvmpipe / radeonsi | development host | host execution | build only | `build/host-release/libmobileglues.so` |

## X1E provider hard gates (system ANGLE, below MobileGlues)

Evidence: `validation/2026-10-18-x1e-provider-probe.json` (SHA-256
`d7a3ba31520ff393dd2ec43cf663a4e479ec38955b65836bd3887a9c8618185d`), run on
`C1L63708PZ` against Android 17 / ANGLE 2.1 / Vulkan 1.4.295 / Adreno X1-85.

| Gate | Result | Evidence |
| --- | --- | --- |
| 512-invocation compute (local 8×8×8) | pass | `compute_512_invocations` wrote the expected value; `MAX_COMPUTE_WORK_GROUP_INVOCATIONS=1024` |
| Vertex-stage image store | pass with `rgba16f` | provider `vertex_image_store`; wrapper `vertex_image_store` (image value 42, read back through compute) |
| R8UI / R16UI images, raw provider | **fail** | ESSL rejects `r8ui`/`r16ui` qualifiers; `glBindImageTexture` returns `GL_INVALID_VALUE` |
| R8UI/R16UI metadata bridge | pass | wrapper registers the real format before `glShaderSource`; formatless `uimage3D` is rewritten to `layout(r32ui) highp`, R16UI texture/binding is backed by R32UI, and a compute write/read returns 12345 |
| Iris compute image binding bridge | pass | `act_volume` fills and samples a 192×96×192 RGBA16F volume (24×12×24 groups of 8³); `act_pingpong` shows dynamic image units alias (`values=22,22`) while explicit `layout(binding=0/1)` separates them (`values=11,22`); mixin logs `floodfill_img=0, floodfill_img_copy=1` |
| ACT 192 dynamic colored lighting (original r5.9.2) | pass (staged) | Very High, bridge installed: sea lantern cyan and glowstone warm pools, stable over an 8 s sample (mean blue-left 24.1±0.1, warm-right 77.8±0.1); log `mclo.gs/Dz8wXmm` |
| GLSL 330 `binding` layout | pass | core enables `GL_ARB_shading_language_420pack` before glslang for pre-420 sources; original `composite.fsh` parses |
| `noperspective` / Iris precision | pass | core strips `noperspective` before glslang and marks `iris_*` float uniforms highp in ESSL |
| 3D image allocation/clear | pass | core uses immutable `glTexStorage3D` for null-pixel 3D image formats, and skips FBO clears of layered/3D textures that crash ANGLE; WSR 128 binds 128×64×128 R32UI |
| Iris SSBO zero-fill | pass | bridge 1.0.5 zero-fills the 50,855,936 B WSR buffer through core `glBufferSubData` when `glClearBufferSubData` is unavailable |
| R32UI 3D image substitution | pass | `r32ui_image_substitution` wrote/read `0x12345678`; legal backing at 4 bytes/texel |
| WSR 128 SSBO (50,855,936 B) | pass — **required advanced WSR target** | allocation, binding, compute write near end, mapped readback; staged pack scenes pass |
| WSR 192 SSBO (114,229,248 B) | pass (optional staged headroom) | same test |
| WSR 256 SSBO (202,899,456 B) | **fail — optional/future X2E only** | `MAX_SHADER_STORAGE_BLOCK_SIZE=134,217,728`; allocation/map succeed but the shader cannot write the last element. Not an X1E release blocker; never claimed as supported |
| Raw + comparison depth sampling with LINEAR | pass at probe level | `depth_raw_and_compare_linear`, 32F depth; raw and compare results non-zero |
| `RGB8_SNORM` color attachment | **fail** | FBO incomplete (`36054`); Complementary colortex1 requests it |
| `RGBA8_SNORM` / `R11F_G11F_B10F` / `RGB16F` / `RGBA16F` attachments | pass | per-format FBO completeness |
| Frontend `GL_MAX_TEXTURE_IMAGE_UNITS` | fixed | provider reports 32; wrapper reports 32 after `gl/getter.cpp` repair |
| Extension admission | pass for the tested names | wrapper `extension_consistency`: no `GL_KHR_no_error`/`GL_ARB_separate_shader_objects`; SSBO/image/compute names only with backend limits |

## Game-process results (X1E, test plugin)

Delivery-lane evidence on the supported NeoForge 21.1.262 tuple. The test plugin
APK packaged core `0557189` (NDK 27.3 debug build) and was installed
on `C1L63708PZ`. FCL launched `neoforge-21.1.262`; the log's env map points
`SDL_OPENGL_LIBRARY` / `LD_LIBRARY_PATH` at the newly installed plugin APK, so the
running renderer is this workspace's build. Iris 1.8.14-beta.1 + Sodium 0.8.13 are
present.

| Pack | Preset | Result | Evidence |
| --- | --- | --- | --- |
| Complementary Reimagined r5.9.3 | High | pipeline initializes; no `(fallback)`; 22 FPS at 2880×1585; fog/atmosphere/water reflection visible | F3 capture, post-fix log |
| Complementary Reimagined r5.9.2 (plan fixture) | High | **pipeline active, no fallback**, 21 FPS at 2880×1585; paired shaders-off reference 569 FPS; water reflections, fog, clouds visible; motion accepted by user | `2026-10-19-r592-high-f3.png`, `2026-10-19-r592-high-shaders-off.png`, log `mclo.gs/z3qIi68` |
| Complementary Reimagined r5.9.2 | High | effect-by-effect fidelity (PBR, shadow edges, bloom, TAA/history isolation) | **not tested** |
| Complementary Unbound r5.9.2 | High | **pipeline active, no fallback**, 20 FPS at 2880×1585; paired shaders-off reference 574 FPS; water reflections, fog, clouds visible; reload works | `2026-10-19-unbound-r592-high-f3.png`, `2026-10-19-unbound-r592-high-shaders-off.png`, log `mclo.gs/hH2OIs5` |
| Complementary Unbound r5.9.2 | High | effect-by-effect fidelity (PBR, shadow edges, bloom, TAA/history isolation) | **not tested** |
| Complementary Reimagined r5.9.3 | High | effect-by-effect fidelity (SSR, PBR, shadows, TAA, bloom, clouds) | **not tested** |

| BSL v10.1.8 | default | **not tested** |

The first attempt (core `9a91ede`) failed at Iris `createColorFramebuffer` with
`Draw buffers [0, 1] Status: 36054` (incomplete attachment) and disabled shaders.
The RGB8_SNORM backing fix (core `0557189`) removed that failure. The post-fix
logs for both r5.9.3 and r5.9.2 contain Iris custom-uniform warnings
(`BIOME_PALE_GARDEN`, `BIOME_SULFUR_CAVES`, `endFlashIntensity`) caused by the
pack/Iris version pair; these are not renderer errors but they can suppress
those effects and must be resolved by a compatible Iris/pack tuple.

The mobile-optimized `Reimagined_MobileGL_r5.9.3_ACT_WSR.zip` was selected for
comparison and reported `(fallback)`. Its log shows the same custom-uniform
failures and then `The shaderpack failed to load!`; this is a pack/Iris version
mismatch, not evidence of a MobileGlues pipeline failure. It is not used as a
fidelity reference.

## Shaderpack / effect rows

| Pack | Archive SHA-256 | Preset | Effect | Build/device/provider | Status | Evidence |
| --- | --- | --- | --- | --- | --- | --- |
| Complementary Reimagined r5.9.2 | `7ae5f1d6…a278` | High | water/surface SSR | ARM64 / X1E / unknown | not tested | fixture not yet acquired |
| Complementary Reimagined r5.9.2 | `7ae5f1d6…a278` | High | rough/specular PBR | ARM64 / X1E / unknown | not tested | — |
| Complementary Reimagined r5.9.2 | `7ae5f1d6…a278` | High | shadows / AO / fog / clouds / bloom / TAA | ARM64 / X1E / unknown | not tested | — |
| Complementary Reimagined r5.9.2 | `7ae5f1d6…a278` | custom (ACT 128, WSR 1) | WSR 128 + ACT 128 | ARM64 / X1E / ANGLE | pass (staged; **required advanced target**) | original pack, bridge 1.0.5; log `mclo.gs/adG0NiA`; water/room captures |
| Complementary Reimagined r5.9.2 | `7ae5f1d6…a278` | Very High | ACT 192 colored lighting | ARM64 / X1E / ANGLE | pass (staged, one emissive scene; **required advanced target**) | F3 `2026-10-19-r592-act192-bridge-f3.png`, 8 s clip, log `mclo.gs/Dz8wXmm` |
| Complementary Reimagined r5.9.2 | `7ae5f1d6…a278` | custom (ACT 128, WSR 1) | WSR 128 | ARM64 / X1E / ANGLE | pass (staged; **required advanced target**) | original pack; WSR images 128×64×128; SSBO 50,855,936 B; log `mclo.gs/adG0NiA` |
| Complementary Reimagined r5.9.2 | `7ae5f1d6…a278` | custom (ACT 192, WSR 1) | WSR 192 + ACT 192 | ARM64 / X1E / ANGLE | pass (staged, bridge 1.0.17) | original pack; distinct cyan/warm ACT; WSR stable in motion; bridge hash `2e1c083c…6397`; residual: visible half-rate floodfill circle on light placement/world entry |
| Complementary Reimagined r5.9.2 | `7ae5f1d6…a278` | Ultra | WSR 256 | ARM64 / X1E / ANGLE | blocked — **optional/future X2E; not an X1E release blocker** | provider `MAX_SHADER_STORAGE_BLOCK_SIZE=134,217,728`; request 202,899,456 B; never claimed as supported on X1E |
| Complementary Reimagined r5.9.2 | `7ae5f1d6…a278` | WSR 128 + player refs | player reflections (binding 3) | ARM64 / X1E / ANGLE | pass functional (staged, user-reported); **performance/power FAIL** | `validation/2026-10-19-x1e-playerref-user-report.md`; core `e7795ad5…54b`, bridge `0b862474…6f1f`. Renders without the previous crash; GPU draw/power unacceptable. No archived FPS/power capture. Six binding-3 atomics and a 2D RGBA8 vertex image store pass as separate probes |
| Complementary Unbound r5.9.2 | `2b4e9c71…21d2` | High | standard fidelity suite | ARM64 / X1E / unknown | not tested | — |
| BSL v10.1.8 | `36b0a50f…30b5` | default | standard fidelity suite | ARM64 / X1E / unknown | not tested | — |
| BSL v10.1.8 | `36b0a50f…30b5` | multicolored block light | ACT image/compute | ARM64 / X1E / unknown | not tested | — |
| Photon `5c79f54` | source snapshot | default | gather/temporal/PBR/compute | ARM64 / X1E / unknown | not tested | — |
| Rethinking Voxels `1fd788f2` | source snapshot | default | geometry storage/atomics stress | ARM64 / X1E / unknown | not tested | — |

## Host semantic gates

| Gate | Status | Evidence |
| --- | --- | --- |
| Existing pixel-size tests | pass | `tests/run.sh` |
| Existing framebuffer-shuffle tests | pass | `tests/run.sh` |
| `glShaderSource` multi-string / explicit-length regression | pass | host `tests/run.sh`; X1E `wrapper_probe source_multi`, `source_multi_implicit` |
| RGB8_SNORM render-target backing | pass | X1E `wrapper_probe rgb8_snorm_attachment`: both TexImage2D and TexStorage2D FBOs complete |
| Desktop GLSL version normalization | pass for simple shaders | X1E `wrapper_probe desktop_glsl_120/150/330/400/460` |
| Full wrapper contract suite | pass (25/25 historical; 55/55 current) | `validation/2026-10-19-x1e-wrapper-image-bridge.json`; `validation/2026-10-19-x1e-wrapper-all-translation55.json` |
| Failed translation replaces backend source | pass | X1E `translation_failure_not_stale`: valid source compiles, then rejected desktop source leaves backend source length 0 and compile status false |
| Shared fragment shader program ownership | pass | X1E `frag_data_shared_shader`: program A patches location 5; program B links the restored original with no location 5 |
| Initialized uniform array suffix | pass | X1E `initialized_uniform_array`: `uniform float weights[4] = float[4](...)` becomes `uniform highp float weights[4];` |
| Link-assigned fragment output location | pass | X1E `frag_data_linked_location`: `layout(location = 3)` queried as 3 after link |
| Storage-block qualifier preservation | pass | X1E `ssbo_qualifier_preserved`: rebinding keeps `readonly buffer` and adds `binding = 2` |
| Vertex-stage SSBO atomics on binding 3 | pass | X1E `vertex_ssbo_atomics`: six `atomicMin/Max` on the 5,376-byte block, `min=-10,-10,-10 max=10,10,10`, fence valid |
| 2D RGBA8 vertex image store | pass | X1E `vertex_image2d_store`: `imageStore` from the vertex stage reads back `64,128,191`, fence valid |
| Translated image qualifier order | pass | X1E `translated_image_decl`: desktop `layout(binding=0,r32ui) ... uimage3D` + `uniform writeonly image2D` translated to explicit bindings 0/1 with `writeonly highp` preserved |
| Image-format metadata rewrite | pass | X1E `metadata_image_format`: bare `uniform writeonly image2D playerAtlas_img` + `GL_RGBA8` metadata translated to `layout(binding = 0, rgba8)` |
| Legal matching depth blit | pass | X1E `blit_depth_match`: D24S8→D24S8 depth+stencil blit, `blit_error=0`, fence valid |
| `glCopyTexImage2D` depth mismatch | pass (no device loss) | X1E `copy_image_depth_mismatch`: D24S8 source / D24 level skipped, `copy_error=0`, fence valid |
| 3D image-store clear level isolation | pass | X1E `clear_tex_level`: 2×2×2 R32UI, level 0 remains `7`, cleared level 1 reads `0` |
| Compute depth sampler override | pass | X1E `depth_filter`: LINEAR sampler object bound to unit 0; dispatch samples `0.5` and restores the sampler binding |
| Geometry shader stage | pass | X1E `wrapper_probe geometry_stage`: desktop `#version 400 core` vertex/geometry/fragment compiled, linked, drew `pixel=255,0,0` through ANGLE ES 3.2 |
| Tessellation control/evaluation stages | pass with TCS-written levels | X1E `wrapper_probe tessellation_stage`: `#version 400 core` VS/TCS/TES/FS, `GL_PATCHES`, `pixel=255,0,0`; `GL_ARB_tessellation_shader` advertised. Default tessellation levels are a named deviation |
| Depth-copy format mismatch | pass (no device loss) | X1E `depth_copy_mismatch`: D24S8 source to D24 destination skipped, `copy_error=0`, fence valid; `blit_depth_mismatch`: `GL_INVALID_OPERATION` (1282), fence valid |
| Multisample framebuffer resolve | pass | X1E `wrapper_probe blit_multisample_resolve`: 4x MSAA RGBA8 cleared RGB `0.25/0.5/0.75`, blitted to single-sample texture, sampled `0.251/0.502/0.749` |
| Logical attachment invalidation after draw-buffer shuffle | pass | Host `framebuffer_shuffle_test` scenario 5b: invalidating logical COLOR1 reaches physical COLOR0; `glInvalidateFramebuffer`/`SubFramebuffer` wrappers |
| Depth texture filter completeness | pass | X1E `wrapper_probe depth_filter_complete`: D32F app filters read back 9986/9729, sampled `0.5`; host `depth_filter_test` 31 checks |
| 64-bit timestamp queries | pass | X1E `wrapper_probe timer_query64`: two increasing 64-bit timestamps; EXT entry points loaded and timer extension advertised only when they exist |
| Same-format depth image copy | pass | X1E `wrapper_probe copy_image_subdata_depth`: D32F `glCopyImageSubData`, sampled `0.5`; format-substitution copies remain untested |
| Program/array image-uniform routing | pass | X1E `program_uniform_image_binding` and `image_array_uniform_iv`; non-image `1iv` writes preserve caller bytes |
| Explicit image bindings preserved / fallback scoped per share group | pass | X1E `wrapper_probe explicit_image_binding`, `layout(binding=5)` write/read `0x12345678`; build `86a73e18…9093`; `validation/2026-10-19-x1e-wrapper-explicit-image.json` |
| Dynamic image units across bind-before-program and program switch | pass | X1E `wrapper_probe act_pingpong`: `values=11,22`, `program_values=11,22`; logical bindings replayed on `glUseProgram` and image-uniform updates; build `cc2b5e2f…8252` |
| Duplicate/default image binding alias | pass | X1E `wrapper_probe duplicate_image_binding`: repeated formatless `binding=0` separated to units 0/1, values `11,22`; whole-source parser build `a3f5d9d9…0f0c`; `validation/2026-10-19-x1e-wrapper-all-global.json` |
| Failed compile/link status remains observable | pass | X1E `wrapper_probe compile_failure`, `link_failure` at `enableNoError=0/1/2` |
| Per-program frag-data bindings / interleaving | pass | X1E `wrapper_probe frag_data_*`; `glGetFragDataLocation` round-trip |
| Shader/program state keyed by share group | implemented, not multi-context tested | source + X1E single-context tests |
| SSBO block binding (compute and vertex) | pass | X1E `wrapper_probe ssbo_block_binding`, `ssbo_graphics_binding`: declared 0 rebound to 3, value read back |
| 512-invocation compute through wrapper | pass | X1E `wrapper_probe compute_512`, value 0x512 |
| Vertex image store through wrapper | pass | X1E `wrapper_probe vertex_image_store`, `rgba16f` image value 42 |
| glslang limits derived from backend | pass for tested limits | vertex image shaders compile; `maxVertexImageUniforms` no longer 0 |
| SSBO/image binding round-trip | **partial** | r16ui refusal visible; r32ui accepted; no r8ui/r16ui lowering yet |
| Cache identity invalidation | pass | Host `cache_key_test`: distinct keys for source, stage, ESSL, backend, texture-buffer emulation, texture-query-LOD, compute, DSA, hide level and PATCH; device suite 38/38 |

## Performance track (implemented, measured on X1E)

| Change | Status | Evidence |
| --- | --- | --- |
| Depth-guard scan skip when no sampler / no sampler on unit; no per-draw Moved zero-fill | measured on X1E | plain dispatch **780.4 → 588.6 ns**; guard path **862.6 → 713.0 ns** (`performance.md`) |
| Per-physical-unit image-binding dedupe | measured on X1E | identical image bind **54.74 → 23.80 ns** (`performance.md`) |
| Image-bind backend error probe gated to lowered/unknown formats | implemented, included in bind measurement | source; not separately isolated |
| Uniform fast path for programs without image uniforms | implemented, unmeasured | source; perfetto pending |

## Game-process smoke (X1E, installed renderer 2.0.0)

Staged evidence for the supported delivery-lane tuple.
FCL 1.3.4.0 launched its existing NeoForge 1.21.1 instance
(`neoforge-21.1.262`, 9 mods) under the installed MobileGlues 2.0.0 renderer.
The Minecraft main menu rendered and the process stayed alive. Logcat from that
launch showed the installed build rejecting `#version 400` shaders, reporting
unsupported `GL_ARB_shader_image_load_store` / `GL_ARB_gpu_shader5` /
`GL_ARB_shading_language_packing` / `GL_ARB_shader_storage_buffer_object`
warnings, and failing generated `iris_Normal` syntax. Those logs are from the
**installed 2.0.0 library**, not this workspace's build. No shaderpack was loaded.
The game-process smoke therefore proves only that the launcher/provider path
starts on the supported 21.1.262 tuple; it closes no fidelity, Iris, or pack gate.
NeoForge 21.1.264 is an optional future patch, not a blocker.

## Game-process runs (X1E, workspace plugin)

A later launch with the workspace plugin reached terrain and then crashed. FCL's uploaded log
`https://mclo.gs/KgWJ5oC` shows two separate facts:

- The renderer mapped all four active image uniforms to core unit 0, aliasing the pack's images.
  A duplicate-binding heuristic now separates repeated formatless bindings while preserving unique
  explicit bridge bindings; the wrapper suites pass 32/32 and `duplicate_image_binding` returns
  `11,22`.
- The same log contains a `SodiumServiceModLocator` duplicate-class `LinkageError` and repeated
  `Failed to create fence object`. These are not established as renderer defects; the Sodium/mod
  instance must be isolated before the crash is attributed.

This run is a failure, not a pass. It does not qualify the delivery lane or any shaderpack.

### 13:45 staged pass (NeoForge 21.1.262)

With the whole-source image parser build, FCL launched the same Minecraft 1.21.1 / NeoForge 21.1.262
instance. The 1.21.1 world loaded and rendered with the original pack; logcat showed distinct image
units 0–3 for programs 435, 438, 256, 441 and 433. The world was saved and quit to title. Captures:
`2026-10-19-1.21.1-neoforge-21.1.262-world.png` and `...-title.png`. A 26.2 world downgrade prompt was
canceled; no 26.2 world was loaded. The earlier Sodium/fence failure did not reproduce in this run.

This is the staged/working reference pass for the supported NeoForge 21.1.262 tuple.
Effect-by-effect fidelity, performance, endurance and lifecycle gates remain NOT RUN;
NeoForge 21.1.264 is an optional future patch and is not a gate.

## Named deviations and untested boundaries

These are not advertised as capabilities and are not release passes. The pinned Iris 1.8.14/Sodium
0.8.13 builds do not call the texture-barrier/texture-view paths; this prevents accidental support
claims, not future compatibility work.

- **`glTextureBarrier` / `glTextureBarrierNV`:** no legal GLES equivalent for render-to-texture
  feedback; `glMemoryBarrier`/`glFlush`/`glFinish` cannot remove the ES feedback undefinedness.
  Remains a named deviation, not advertised. Iris uses its BufferFlipper alternate-texture scheme.
- **`glTextureView`:** ES has no texture views and the X1E provider lists no
  `GL_EXT_texture_view`; whole-level EGL image aliasing cannot express mip/layer ranges or format
  reinterpretation. Not advertised; no pinned call site.
- **`glPatchParameterfv` default tessellation levels:** ES 3.2 exposes
  `glPatchParameteri` for `GL_PATCH_VERTICES` only. ANGLE exports no
  `glPatchParameterfv`/`EXT` entry point, so valid desktop default-inner/outer-level calls report
  `GL_INVALID_OPERATION`; they are never silently accepted. Tessellation works when the TCS writes
  `gl_TessLevel*`.
- **Depth-filter scan boundary:** draw-time sampler override handling covers 2D depth textures;
  arrays/cubes/3D get depth-legal texture filters at set/allocation but a sampler override on those
  targets is not rewritten. Stencil-index mode is not modelled. One-level textures with a
  mip-requiring min filter remain mip-incomplete, matching desktop.
- **Mutable multi-level 3D image textures:** the level-0 `glTexImage3D` image-format allocation is converted to one-level immutable `glTexStorage3D` storage to keep ANGLE image load/store usable. A later level > 0 upload therefore cannot extend that texture; a caller that supplies a mutable mip chain level by level is not yet supported. One-level Iris custom images are the tested path.
- **Copy format substitutions:** same-format D32F copy passes; R16UI→R32UI / RGB8_SNORM→RGBA8_SNORM
  compatibility-class changes are not yet validated for `glCopyImageSubData`.

## Device gates (all pending)

| Gate | Required evidence |
| --- | --- |
| Supported delivery tuple boot (1.21.1 / NeoForge 21.1.262 / Iris 1.8.14-beta.1 / Sodium 0.8.13) | STAGED/PASS for the recorded world/title run; keep the game-process log and captures. Optional 21.1.264 patch is not a gate |
| Unmodified pack ZIP import and profile selection | F3 capture plus Iris log for an original r5.9.3 ZIP |
| Same provider in probe and game process | provider library paths, EGL vendor/version, GLES renderer |
| Vertex image stores / 512-invocation groups | behavioral probe pass/fail on X1E |
| R8UI/R16UI image representation | allocation, integer read/write, sampling, clear/copy |
| 50,855,936-byte WSR 128 SSBO binding 0 (required advanced WSR gate) | allocation plus access near the end of range |
| WSR 128 visual/temporal reference scenes (required) | staged water/room captures and resource logs; effect-specific evidence still open |
| 202,899,456-byte Ultra SSBO (optional/future X2E) | optional characterization only; `BLOCKED` on X1E and not a release gate |
| 60 FPS at 1920 × 1200 (target) | frame-time distribution, features constant |
| 20-minute AC/battery sustained run | thermal/frequency data, no failure or unbounded memory |
| Lifecycle: resize/maximize/sleep/resume/provider fallback | launcher/game-process evidence |
