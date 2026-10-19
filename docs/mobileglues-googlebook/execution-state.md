# MobileGlues X1E Execution State

Plan baseline: `docs/MobileGlues_Googlebook_X1E_Optimization_Plan.md` (canonical path in this workspace).
Last updated: 2026-10-19 (bridge 1.0.17).

## Current stage and active task

- Stage 0/1/2: locked X1E tuple, reproducible ARM64 build, provider and wrapper probes complete for the
  tested contracts; remaining capability/cache/ownership items stay open.
- Stage 3: both original Complementary r5.9.2 styles initialize at High without fallback; named-effect
  fidelity matrix remains open.
- Stage 4: ACT 192 staged pass on the original pack.
- Stage 5: WSR 128 and WSR 192 staged passes on the original pack; WSR 256 blocked by provider SSBO limit.
- Active task: WSR 256 negative gate and optional player reflections, then return to named-effect fidelity.
- Image binding / ACT repair (bridge 1.0.17): fixed name-to-unit registry shared by the transformer and
  Iris ProgramImages; `voxel_img` per-frame clear restored. On X1E, ACT 192 colors are distinct, WSR 192
  is stable in motion, and the earlier light ghosting/flicker is gone. Residual: the pack's half-rate
  floodfill propagation is visible as a spreading circle on light placement/world entry at low FPS.

## Repository map

| Root | Branch | HEAD | Remote | State |
| --- | --- | --- | --- | --- |
| `/home/polar/MobileGlues` | `googlebook/x1e-stage1-3` | `dcc67e8` (base `97558a6`) | `origin` = `git@github.com:BaiTian6641/MobileGlues.git` (preserved); `upstream` = `https://github.com/MobileGL-Dev/MobileGlues.git` (read baseline) | Task branch; clean except user `.vscode/` |
| `/home/polar/MobileGlues-plugin` | `googlebook/test-renderer` | `e796e12` | `origin` = `https://github.com/MobileGL-Dev/MobileGlues-plugin.git` | Parent commit updates the test core gitlink; core submodule detached at `0557189` |

Core submodules initialized at the gitlinks recorded by `97558a6`:

- `MobileGlues-cpp/3rdparty/SPIRV-Cross` = `a0fba56c34a6700f1724bf9b751da5b488a3775c`
- `MobileGlues-cpp/3rdparty/glslang` = `f5f664dee8146676b04a332a7233959fc3ce9681`
- `MobileGlues-cpp/3rdparty/xxhash` = `c2866db364b6ea3a11933e62235ddc166ba18565`
- `MobileGlues-cpp/include/ska` = `21c1cec95abee1beef827e4a7c95f692875d9594`
- `MobileGlues-cpp/3rdparty/perfetto` = `40b529923598b739b2892a536a7692eedbed5685`, **not initialized** (optional `PROFILING=OFF`).

No worktrees, no staged changes, no unfinished user changes were found. The only pre-existing untracked path was
`docs/`, containing the supplied plan.

## Target-device evidence (new)

The actual X1E Googlebook is connected as `C1L63708PZ`: HP Googlebook 14 (`quartz`), Snapdragon
X1E80100 / Adreno X1-85, Android 17 / API 37, 2880×1800@120 Hz, 4 KB pages. System EGL is ANGLE 2.1
over Vulkan 1.4.295 (Adreno driver 512.886.6). FCL 1.3.4.0 and the MobileGlues 2.0.0 plugin are
installed; FCL user data was not found at the usual shared-storage paths, so a game-process launch
has not yet been performed.

The below-wrapper probe (`MobileGlues-cpp/tools/capability_probe`) ran on the device. Machine-readable
output is `validation/2026-10-18-x1e-provider-probe.json`. Key results:

- 512-invocation compute and vertex-stage image stores (`rgba16f`) work.
- `r8ui`/`r16ui` are not legal ESSL image qualifiers on this provider, and `glBindImageTexture`
  rejects those formats. `r32ui` images work and are the first lowering candidate.
- `MAX_SHADER_STORAGE_BLOCK_SIZE` is 128 MiB. WSR 128 and WSR 192 read/write correctly; WSR 256
  (193.5 MiB) cannot be written at the end of its declared range. WSR 256 is therefore blocked on
  this provider as a single SSBO.
- `RGB8_SNORM` is texture-valid but not color-renderable; Complementary colortex1 requests it.
  `RGBA8_SNORM`, `R11F_G11F_B10F`, `RGB16F`, and `RGBA16F` render correctly.
- Raw and comparison depth sampling with LINEAR works in the probe's 32F case.
- The provider reports `GL_MAX_TEXTURE_IMAGE_UNITS=32`; the wrapper still doubles it to 64.

These are **device provider** results, not shaderpack qualification. No pack has been loaded in the
game process.

## Confirmed progress

- Source inspection confirms the audit's `glShaderSource` hazard in `MobileGlues-cpp/gl/shader.cpp`:
  the concatenated ESSL is submitted as one pointer while the original `count` is passed, and the original
  `length` array is dropped (`GLES.glShaderSource(shader, count, s, nullptr)`).
- The compile/link failure masking paths remain in `gl/shader.cpp` and `gl/program.cpp`
  (`ignore_error` rewrites `GL_COMPILE_STATUS`/`GL_LINK_STATUS`); `glGetError` still always reports
  `GL_NO_ERROR`.
- `glShaderStorageBlockBinding` was a no-op stub in `gl/gl_stub.cpp`; it is now implemented in
  `gl/program.cpp` (source rewrite + frontend relink) and tested on the X1E device.
- The translator/limit/image/SSBO observations in the plan are being re-audited against this exact checkout.
- A real FCL game process was launched on `C1L63708PZ` (Minecraft 1.21.1 / NeoForge 21.1.262,
  9 mods) with the installed MobileGlues 2.0.0 renderer. The main menu rendered, but the installed
  build's logcat rejected `#version 400` shaders and generated `iris_Normal` syntax errors. This is
  smoke evidence for the launcher/provider path, not pack qualification.
- Host semantic tests pass unchanged: `sh MobileGlues-cpp/tests/run.sh` → 0 failures.
- Reproducible ARM64 build succeeds with NDK r28c; artifact is 16 KB-aligned and ThinLTO is active.

### Stage 1/2 implementation evidence (X1E, wrapper level)

The below-wrapper provider probe and a new wrapper-level harness
(`MobileGlues-cpp/tools/wrapper_probe`) ran on `C1L63708PZ`. Results are in
`validation/2026-10-19-x1e-wrapper-stage1-stage2.json` (SHA-256
`385e8dab0120ad164b051e7608ea553d5dad9060a41df30a037deae467928964`).

Fixed and verified:

- `glShaderSource` now concatenates with exact lengths and submits **one** string
  with an explicit length. Before the fix, a three-string submission segfaulted
  the X1E process; after it, both explicit-length and lengthless multi-string
  submissions compile.
- `glGetError` reports frontend errors instead of always returning `GL_NO_ERROR`.
  An invalid enum is now reported as `1280` (`GL_INVALID_ENUM`).
- Compile/link/validate statuses are no longer rewritten by `ignore_error`.
  Invalid shaders and interface-mismatch links report failure with
  `enableNoError=0`, `1`, and `2` (`None`, `Partial`, `Full`), and the driver log
  is preserved.
- `glCheckFramebufferStatus` no longer rewrites incomplete framebuffers.
- `GL_MAX_TEXTURE_IMAGE_UNITS` is no longer doubled; the frontend reports the
  provider's 32.
- Shader and program state is keyed by share group and object name, not one
  process-global last-shader record. Per-program `glBindFragDataLocation`
  bindings survive interleaved shader sourcing, relinking, and an existing
  `layout(location=...)` qualifier; `glGetFragDataLocation` round-trips the
  stored binding.
- Backend refusal of an `r16ui` image binding is now visible to the application
  (`GL_INVALID_VALUE`); an `r32ui` image binding is accepted. The actual
  r8ui/r16ui lowering is not implemented yet.
- glslang resource limits are queried from the backend. Vertex-image shaders now
  compile and execute through the wrapper; a 512-invocation compute shader runs.
- `glShaderStorageBlockBinding` is implemented by resolving the block name,
  rewriting the attached shader source, and relinking through the frontend.
  Compute and vertex-stage SSBOs rebound from 0 to 3 are read back correctly.
- The translated-source cache key includes a schema revision, stage, backend ES
  version, compute/DSA settings, and environment hiding level. Failed translation
  now returns empty instead of raw desktop GLSL.

Still open in these stages:

- Extension admission is still not derived from a single capability contract;
  several desktop extensions remain unconditional.
- Post-link reflection is queried live from the backend for SSBO block binding,
  but is not retained as a frontend table for cache identity or relinking.
- The wrapper still reports a desktop GL 4.0 profile while `enableExtComputeShader`
  defaults to false; this is a loader-capability decision, not a proven feature
  level.
- R8UI/R16UI image lowering is not implemented. WSR 256 remains blocked by the
  provider's 128 MiB `GL_MAX_SHADER_STORAGE_BLOCK_SIZE`.
- The plan fixture **Complementary Reimagined r5.9.2** is now installed in the
  Forge 21.1.262 instance and active at **High, no fallback**. F3 confirms
  `ComplementaryReimagined_r5.9.2.zip`, `Profile: HIGH (+0 options changed)`,
  Adreno X1-85 / ANGLE / Vulkan, MobileGlues 2.0.0. The paired shaders-off
  reference is 569 FPS; the shader pass is 21 FPS at 2880×1585. Water
  reflections, fog and clouds are visible; a 12-second motion sample was
  accepted as visually fine by the user. This is a visual spot-check, not an
  effect-by-effect fidelity gate.
- The earlier r5.9.3 High run also initialized without fallback (22 FPS). It
  remains a separate fixture row.
- **Complementary Unbound r5.9.2** is also active at High without fallback:
  20 FPS at 2880×1585, paired shaders-off reference 574 FPS, water
  reflections/fog/clouds visible, and shader reload works. F3 and log
  `mclo.gs/hH2OIs5` identify the pack/profile/provider.
- The mobile-optimized `Reimagined_MobileGL_r5.9.3_ACT_WSR.zip` reported
  `(fallback)` on Iris 1.8.14; the log shows custom-uniform failures and
  `The shaderpack failed to load!`. That is a pack/Iris version mismatch, not a
  renderer failure; it is not used as a reference.
- The installed test plugin APK (core `0557189`, debug signing) is the renderer
  named in the game log's `SDL_OPENGL_LIBRARY` and `LD_LIBRARY_PATH`. The original
  installed 2.0.0 APK is preserved for rollback.
- WSR 128 core/plugin commits: core `1deeef6`, plugin submodule `e5e0131`,
  plugin parent `6e4b230` (`googlebook/test-renderer`). Final test APK
  SHA-256 `771f2fd3aa51eb3b7982faaff8cc70b1633ed1c5fac28d8c93921fe47fe89593`;
  bridge 1.0.5 SHA-256
  `e7cedffca73153fc70799b8cc483506deeec33d9f9bd33ffc15c658684f0511b`.
- **Bridge 1.0.17** (`2e1c083c841f795717569536591d91e10b83ac20a1ea68e9af29acb9bad86397`) fixes the image
  unit contract: shader `layout(binding=N)` and Iris `ProgramImages` now consume one fixed
  pack-order name → unit map, populated inside the transformer before layout rewriting. Disabling
  `voxel_img` clear was the ghosting source; restoring Iris's per-frame clear removed it while the
  fixed units kept colored lights distinct.
- **WSR 192 passes on the original pack** with custom `COLORED_LIGHTING=192`,
  `WORLD_SPACE_REFLECTIONS=1`. The bridge zero-filled the 114,229,248 B
  buffer; water shows terrain/island reflections. Log `mclo.gs/FMnnMkC`;
  captures `2026-10-19-r592-wsr192-water.{png,mp4}` and room frame.
- Latest commits on `googlebook/x1e-stage1-3`: `d9789fa` shader source/per-object
  state, `9a91ede` compiler limits/SSBO binding, `0557189` RGB8_SNORM backing,
  `468c4bb` game evidence, `e3b2661` artifact hashes, `d22b187` wrapper version
  fixtures, `dcc67e8` branch/handoff records, `bc34198` extension admission and
  image-metadata bridge.
- Latest standalone ARM64 artifact SHA-256:
  `ef515913cd2abb9fb62f5d708d012136cb939d6d9e6957f94098d1d053648fda`.
- Final test plugin: core `a524755`, plugin commit `59de4b5`, APK SHA-256
  `e728b6e9bd0d290df4059a401aed3aa7bd48c6f55d1d3bccb7cb0af4fbd72162`,
  packaged library SHA-256
  `85f6dcf2d29ac6e7bbe82fe70f660daaba44f6018f1849a86118991ced3cd3a0`.
  Installed on the device.
- The installed test plugin packages core `a524755` (extension admission and
  image bridge); the Iris runtime still does not call the metadata begin/add
  hook. The companion Iris bridge `mgirisbridge-1.0.0` now supplies explicit
  bindings/formats for **compute** image declarations. This is required because
  dynamic image-unit assignment through `glUniform1i` is not honoured on this
  ANGLE path: two floodfill images aliased and produced the flashing ACT mask.
- Wrapper probe `act_volume` passes full 192×96×192 RGBA16F dispatch and sampler
  readback. `act_pingpong` fails with dynamic units (`values=22,22`) and passes
  with explicit bindings (`values=11,22`).
- Original Complementary Reimagined r5.9.2 at **Very High** now runs with
  stable colored lighting: sea lantern cyan, glowstone warm, visible diffusion
  process, no flashing over an 8 s sample. Log `mclo.gs/Dz8wXmm`, F3 and clip
  archived. This closes the staged ACT 192 scene, not the full Stage 4 matrix.
- **WSR 128 passes on the original pack** with custom `COLORED_LIGHTING=128`,
  `WORLD_SPACE_REFLECTIONS=1`. WSR images are 128×64×128; the SSBO is
  50,855,936 B. Log `mclo.gs/adG0NiA`, water and room captures archived.
- Core fixes required for that pass:
  - pre-420 GLSL gets `GL_ARB_shading_language_420pack` for `layout(binding=)`;
  - `noperspective` is stripped and Iris scalar uniforms get highp in ESSL;
  - null-pixel 3D image formats allocate immutable storage (`glTexStorage3D`)
    instead of `glTexImage3D`, which left ANGLE images invalid;
  - FBO clears of layered/3D textures are skipped because ANGLE's `GL_Clear`
    crashes on wsr imagery;
  - bridge 1.0.5 zero-fills SSBOs and gives compute image pairs explicit
    bindings.

## Blockers

1. **WSR 256 provider limit.** `MAX_SHADER_STORAGE_BLOCK_SIZE` is 134,217,728 B; the pack requests
   202,899,456 B. This is a provider boundary, not a wrapper regression. WSR 128/192 pass.
2. **Named-effect fidelity is incomplete.** PBR, shadow edges, bloom, TAA/history isolation, and player
   reflections are not yet effect-by-effect qualified on both original styles.
3. **Performance/lifecycle gates pending.** Sustained 60 FPS at 1920×1200, 20-minute thermal run,
   resize/focus/context recovery, packaging/release, and X2E onboarding remain open.

## Latest test evidence

- Host: `sh MobileGlues-cpp/tests/run.sh` → all checks passed (0 failures), 2026-10-18.
- ARM64: `cmake --build build/android-arm64-release -j 24` → success, 102/102 targets.
- Artifact: `build/android-arm64-release/libmobileglues.so`,
  SHA-256 `884582a269a44bdab4641259bfa5b330335df13ca08ad8cfafd88ac59ab2501e`.
- ELF: `ELF64 AArch64`, Android 26, NDK r28c, `LOAD` align `0x4000` (16 KB).

## Handoff artifacts

Recoverable patches are in `/home/polar/MobileGlues-fixtures/handoff/`:
`core-patches/0001..0011` from base `97558a6` to `a524755`, and
`plugin-patches/0001..0002` from `f5105e4` to `59de4b5`. Hashes are in
`handoff/hashes.txt`. The WSR/ACT bridge work is additionally exported as
`handoff/wsr128/{core,core-submodule,plugin}/*.patch` with
`handoff/wsr128/hashes.txt`; the reconstructed research tree is at
`/home/polar/snapdragon-shader-upload/extracted/snapdragon-shader` (host
fixture, not a release input). The core submodule commit `0557189` is present in the
local core repository and in the plugin's local submodule clone; upstream does
not contain it, so the patches are the portable handoff. No push or PR was
performed.

## Exact next action

Run the **Ultra WSR 256** negative gate on the original pack: expect the
provider's 134,217,728 B `MAX_SHADER_STORAGE_BLOCK_SIZE` to reject the
202,899,456 B request; record the exact loader/pipeline behavior, not a
shaderpack edit. In parallel, qualify **optional player reflections**
(`WORLD_SPACE_PLAYER_REF=1`) at WSR 128/192, and add a native core
`glClearBufferSubData` implementation so the Iris zero-fill bridge is no
longer required. Keep named-effect Stage 3 fidelity, graphics-stage image
formats, performance, and lifecycle gates separate.
