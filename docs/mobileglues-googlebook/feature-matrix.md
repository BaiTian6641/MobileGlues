# Feature and Qualification Matrix

Status vocabulary is the plan's: **not tested**, **fails to load**, **loads with missing effects**,
**correct for this preset**, **correct but below the performance target**, **qualified for release**.

No row is qualified: there is no X1E device and no game-process installation yet. All device rows are
`not tested`; host/source evidence is labelled separately and does not certify rendering.

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
| R32UI 3D image substitution | pass | `r32ui_image_substitution` wrote/read `0x12345678`; legal backing at 4 bytes/texel |
| WSR 128 SSBO (50,855,936 B) | pass | allocation, binding, compute write near end, mapped readback |
| WSR 192 SSBO (114,229,248 B) | pass | same test |
| WSR 256 SSBO (202,899,456 B) | **fail** | `MAX_SHADER_STORAGE_BLOCK_SIZE=134,217,728`; allocation/map succeed but the shader cannot write the last element |
| Raw + comparison depth sampling with LINEAR | pass at probe level | `depth_raw_and_compare_linear`, 32F depth; raw and compare results non-zero |
| `RGB8_SNORM` color attachment | **fail** | FBO incomplete (`36054`); Complementary colortex1 requests it |
| `RGBA8_SNORM` / `R11F_G11F_B10F` / `RGB16F` / `RGBA16F` attachments | pass | per-format FBO completeness |
| Frontend `GL_MAX_TEXTURE_IMAGE_UNITS` | fixed | provider reports 32; wrapper reports 32 after `gl/getter.cpp` repair |
| Extension admission | pass for the tested names | wrapper `extension_consistency`: no `GL_KHR_no_error`/`GL_ARB_separate_shader_objects`; SSBO/image/compute names only with backend limits |

## Game-process results (X1E, test plugin)

The test plugin APK packaged core `0557189` (NDK 27.3 debug build) and was installed
on `C1L63708PZ`. FCL launched `neoforge-21.1.262`; the log's env map points
`SDL_OPENGL_LIBRARY` / `LD_LIBRARY_PATH` at the newly installed plugin APK, so the
running renderer is this workspace's build. Iris 1.8.14-beta.1 + Sodium 0.8.13 are
present.

| Pack | Preset | Result | Evidence |
| --- | --- | --- | --- |
| Complementary Reimagined r5.9.3 | High | pipeline initializes; no `(fallback)`; 22 FPS at 2880×1585; fog/atmosphere/water reflection visible | F3 capture, post-fix log |
| Complementary Reimagined r5.9.3 | High | effect-by-effect fidelity (SSR, PBR, shadows, TAA, bloom, clouds) | **not tested** |
| Complementary Reimagined r5.9.2 (plan fixture) | High | **not tested**; not installed on the device |
| Complementary Unbound r5.9.2 | High | **not tested** |
| BSL v10.1.8 | default | **not tested** |

The first attempt (core `9a91ede`) failed at Iris `createColorFramebuffer` with
`Draw buffers [0, 1] Status: 36054` (incomplete attachment) and disabled shaders.
The RGB8_SNORM backing fix (core `0557189`) removed that failure. The post-fix log
still contains Iris custom-uniform warnings (`BIOME_PALE_GARDEN`,
`BIOME_SULFUR_CAVES`, `endFlashIntensity`) caused by the pack/Iris version pair;
these are not renderer errors but they can suppress those effects and must be
resolved by a compatible Iris/pack tuple.

## Shaderpack / effect rows

| Pack | Archive SHA-256 | Preset | Effect | Build/device/provider | Status | Evidence |
| --- | --- | --- | --- | --- | --- | --- |
| Complementary Reimagined r5.9.2 | `7ae5f1d6…a278` | High | water/surface SSR | ARM64 / X1E / unknown | not tested | fixture not yet acquired |
| Complementary Reimagined r5.9.2 | `7ae5f1d6…a278` | High | rough/specular PBR | ARM64 / X1E / unknown | not tested | — |
| Complementary Reimagined r5.9.2 | `7ae5f1d6…a278` | High | shadows / AO / fog / clouds / bloom / TAA | ARM64 / X1E / unknown | not tested | — |
| Complementary Reimagined r5.9.2 | `7ae5f1d6…a278` | Ultra | ACT colored lighting 128 | ARM64 / X1E / unknown | not tested | — |
| Complementary Reimagined r5.9.2 | `7ae5f1d6…a278` | Ultra | ACT colored lighting 192 | ARM64 / X1E / unknown | not tested | — |
| Complementary Reimagined r5.9.2 | `7ae5f1d6…a278` | Ultra | WSR 128 | ARM64 / X1E / unknown | not tested | — |
| Complementary Reimagined r5.9.2 | `7ae5f1d6…a278` | Ultra | WSR 256 | ARM64 / X1E / unknown | not tested | — |
| Complementary Reimagined r5.9.2 | `7ae5f1d6…a278` | Ultra | player reflections (binding 3) | ARM64 / X1E / unknown | not tested | — |
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
| Full wrapper contract suite | pass (25/25) | `validation/2026-10-19-x1e-wrapper-image-bridge.json` |
| Failed compile/link status remains observable | pass | X1E `wrapper_probe compile_failure`, `link_failure` at `enableNoError=0/1/2` |
| Per-program frag-data bindings / interleaving | pass | X1E `wrapper_probe frag_data_*`; `glGetFragDataLocation` round-trip |
| Shader/program state keyed by share group | implemented, not multi-context tested | source + X1E single-context tests |
| SSBO block binding (compute and vertex) | pass | X1E `wrapper_probe ssbo_block_binding`, `ssbo_graphics_binding`: declared 0 rebound to 3, value read back |
| 512-invocation compute through wrapper | pass | X1E `wrapper_probe compute_512`, value 0x512 |
| Vertex image store through wrapper | pass | X1E `wrapper_probe vertex_image_store`, `rgba16f` image value 42 |
| glslang limits derived from backend | pass for tested limits | vertex image shaders compile; `maxVertexImageUniforms` no longer 0 |
| SSBO/image binding round-trip | **partial** | r16ui refusal visible; r32ui accepted; no r8ui/r16ui lowering yet |
| Cache identity invalidation | pending implementation | cache-key audit identifies stage/profile inputs as omitted |

## Game-process smoke (X1E, installed renderer 2.0.0)

FCL 1.3.4.0 launched its existing NeoForge 1.21.1 instance
(`neoforge-21.1.262`, 9 mods) under the installed MobileGlues 2.0.0 renderer.
The Minecraft main menu rendered and the process stayed alive. Logcat from that
launch showed the installed build rejecting `#version 400` shaders, reporting
unsupported `GL_ARB_shader_image_load_store` / `GL_ARB_gpu_shader5` /
`GL_ARB_shading_language_packing` / `GL_ARB_shader_storage_buffer_object`
warnings, and failing generated `iris_Normal` syntax. Those logs are from the
**installed 2.0.0 library**, not this workspace's build. No shaderpack was loaded
and the instance is not one of the plan's Fabric lanes. The game-process smoke
therefore proves only that the launcher/provider path starts; it closes no
fidelity, Iris, or pack gate.

## Device gates (all pending)

| Gate | Required evidence |
| --- | --- |
| Same provider in probe and game process | provider library paths, EGL vendor/version, GLES renderer |
| Vertex image stores / 512-invocation groups | behavioral probe pass/fail on X1E |
| R8UI/R16UI image representation | allocation, integer read/write, sampling, clear/copy |
| 202,899,456-byte SSBO binding 0 | allocation plus access near the end of range |
| WSR 128 / WSR 256 independent | visual reference scenes and resource logs |
| 60 FPS at 1920 × 1200 (target) | frame-time distribution, features constant |
| 20-minute AC/battery sustained run | thermal/frequency data, no failure or unbounded memory |
| Lifecycle: resize/maximize/sleep/resume/provider fallback | launcher/game-process evidence |
