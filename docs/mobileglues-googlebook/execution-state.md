# MobileGlues X1E Execution State

Plan baseline: `docs/01_MobileGlues_Googlebook_Architecture.md`,
`docs/02_MobileGlues_Googlebook_Implementation_Plan.md` and
`docs/03_MobileGlues_Googlebook_Validation_and_Sources.md`. Distribution locks: `compat/`.
Last updated: 2026-10-19 (bridge 1.0.17).

Stage labels below retain the historical `Stage 0`-`Stage 8` numbering used when the evidence
was captured. The approximate `S0`-`S9` mapping and lane status are in `README.md`. In current
terms: S0 partial; S1-provider partial; S1-legacy NOT RUN; S1-current STAGED/WORKING on the
supported 21.1.262 tuple (world load, render, save/quit); 21.1.264 is an optional future patch,
not a blocker; S2/S3 partial; S4 partial; S5/S6 staged, with ACT 192 and WSR 128 as the required
advanced gates; WSR 256 is optional/future X2E and `BLOCKED` on X1E by the provider limit;
S7-S9 NOT RUN.

## Delivery lane lock (2026-10-19)

The required lane is Minecraft 1.21.1 on **NeoForge 21.1.262** with Java 21
(`java-runtime-delta`), Iris 1.8.14-beta.1 NeoForge (`KduFYu4t`) and Sodium 0.8.13 NeoForge
(`uMOpc5uV`). The installed 21.1.262 instance is the supported X1E target and is staged/working:
it loaded and rendered the 1.21.1 world with the original pack and saved/quit to title.
Ordinary shaderpack ZIPs must import, select their own profile and render their enabled effects
in this lane. The stable fallback pair is Iris 1.8.12 NeoForge (`t3ruzodq`) with Sodium 0.6.13
NeoForge (`Pb3OXVqC`); Iris 1.8.12 must never be paired with Sodium 0.8.13. NeoForge 21.1.264
is an optional future patch: it is not a delivery target and not a release blocker.

The game-process evidence is the reference pass for the supported tuple. It proves the launcher,
JVM, native bridge and Iris/Sodium path can start, and that original packs can initialize and
render. It does not qualify the lane for effects, performance or lifecycle. The 21.1.264 patch,
effect fidelity, performance and lifecycle gates remain **NOT RUN/NOT COMPLETE**.

Minecraft 26.3 / Java 25 / LWJGL 3.4.3 SDL on Fabric is a future/experimental lane; its
bytecode and manifest findings stay recorded as future-lane evidence. Minecraft 1.20.1 / Java 17
Fabric remains the legacy lane and is still unbeaten.

## Current stage and active task

- Stage 0/1/2 (historical): locked X1E tuple, reproducible ARM64 build, provider and wrapper probes
  complete for the tested contracts; remaining capability/cache/ownership items stay open.
- Stage 3 (historical): both original Complementary r5.9.2 styles initialize at High without fallback;
  named-effect fidelity matrix remains open. The game process was Minecraft 1.21.1 / NeoForge
  21.1.262, the supported delivery-lane tuple; 21.1.264 is an optional future patch and this is not
  the pinned 1.20.1/Fabric legacy lane.
- Stage 4 (historical): ACT 192 staged pass on the original pack.
- Stage 5 (historical): WSR 128 and WSR 192 staged passes on the original pack. WSR 128 is the
  required advanced WSR target for the X1E release; WSR 192 is optional staged headroom. WSR 256 is
  optional/future X2E work, `BLOCKED` on X1E by the provider's 134,217,728-byte storage-block limit,
  and is not an X1E release blocker.
- Active task: close the required advanced gates (ACT 192 and WSR 128) with effect-specific evidence,
  then return to standard named-effect fidelity. Optional player reflections and WSR 256 remain
  future work, never claimed as supported on X1E.
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
- `MAX_SHADER_STORAGE_BLOCK_SIZE` is 128 MiB. WSR 128 (required advanced target) and WSR 192
  (optional headroom) read/write correctly; WSR 256 (193.5 MiB) cannot be written at the end of its
  declared range. WSR 256 is therefore `BLOCKED` on this provider as a single SSBO; it is
  optional/future X2E work and not an X1E release blocker.
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
  Iris 1.8.14-beta.1, Sodium 0.8.13, 9 mods) with the installed MobileGlues 2.0.0 renderer. The main menu
  rendered, but the installed build's logcat rejected `#version 400` shaders and generated `iris_Normal`
  syntax errors. This is staged smoke evidence for the delivery lane's launcher/provider path on the
  supported NeoForge tuple, not pack qualification and not a release claim.
- Host semantic tests pass unchanged: `sh MobileGlues-cpp/tests/run.sh` → 0 failures.
- Reproducible ARM64 build succeeds with NDK r28c; artifact is 16 KB-aligned and ThinLTO is active.
- Explicit image bindings are now authoritative: the transformer preserves `layout(binding=N)` from
  the pack or the Iris bridge, and only assigns a fallback unit when none exists; that fallback map is
  scoped per share group instead of process-global. X1E `wrapper_probe explicit_image_binding` wrote
  and read `0x12345678` through `layout(binding=5)` with no `glUniform1i` call. Build SHA-256
  `86a73e18…9093`; result `b9c94bfc…4fad`. This is wrapper-level evidence, not pack qualification.
- Per-context logical image bindings are now replayed through the current program's unit map on
  `glUseProgram`, relink and image-uniform updates. The X1E `wrapper_probe all` suite passes 31/31,
  including `act_pingpong` (`values=11,22`, `program_values=11,22`) with images bound before the
  program and uniforms set after it. Build SHA-256 `cc2b5e2f…8252`; result
  `2026-10-19-x1e-wrapper-all-final.json` (`cb092e6b…4f80`).
- Review follow-up: restored the depth-size guard in `glCopyTexSubImage2D` that commit `be5e9a1`
  introduced; its uncommitted removal would have re-exposed the ANGLE device-loss path. Removed the
  `WORLD_SPACE_PLAYER_REF` source rewrite: Iris preprocesses the option with jcpp before
  `glShaderSource` and the translator regenerates ESSL, so the guard could never match. The option
  remains unsupported and is not silently disabled.
- Game-process run (13:14, NeoForge 21.1.262): reached terrain, then FCL showed its crash screen.
  Uploaded log `https://mclo.gs/KgWJ5oC`. The renderer log showed every active image uniform mapped
  to core unit 0 (Iris's default repeated binding), so the pack aliased its images. A duplicate-
  binding heuristic now overrides repeated formatless bindings while preserving unique bridge
  bindings; X1E `wrapper_probe duplicate_image_binding` passes `11,22`, and the full suite is 32/32.
  The same log contains an independent `SodiumServiceModLocator` duplicate-class `LinkageError` and
  `Failed to create fence object`; the crash cause must remain separated from the renderer alias.
  This run is a failure, not a pass.
- A resolver build logged `relinked to separate duplicate image units` for every affected program
  but the reflected units remained 0; uploaded log `https://mclo.gs/PeqII57` shows the same Sodium
  and fence failures. The line-based parser was not seeing multi-declaration/multi-line sources.
  It has been replaced with a whole-source declaration parser that stores name -> unit per shader
  and feeds reflection directly. The X1E suite passes 32/32; build `a3f5d9d9…0f0c`; result
  `validation/2026-10-19-x1e-wrapper-all-global.json`. The in-game retry is the deciding check.
- In-game result (13:45, Minecraft 1.21.1 / NeoForge 21.1.262): the 1.21.1 world loaded and rendered
  with the original pack, logcat showed distinct image units 0-3 for programs 435/438/256/441/433,
  and Save and Quit to Title succeeded. Captures
  `2026-10-19-1.21.1-neoforge-21.1.262-world.png` and `...-title.png`. A 26.2 world downgrade prompt
  was canceled; that world was not loaded. This is the staged/working reference pass for the
  supported 21.1.262 tuple. Effect-by-effect fidelity, performance and lifecycle remain NOT RUN;
  NeoForge 21.1.264 is an optional future patch and not a blocker.
- Desktop-parity increments after the reference pass:
  - `glProgramUniform1i/1iv` and `glUniform1iv` now route image units through the target program's
    map; image-array elements are registered by location/unit run. X1E
    `program_uniform_image_binding` and `image_array_uniform_iv` pass.
  - Depth completeness port: application-visible depth filters are preserved while the driver gets
    ES-legal filters; a draw-time guard covers sampler-object overrides. Host `depth_filter_test`
    passes 31 checks; X1E `depth_filter_complete` samples `0.5` and reads back 9986/9729.
  - 64-bit timer queries are wired to `GL_EXT_disjoint_timer_query`; extension advertising is gated
    on loaded entry points. X1E `timer_query64` returns increasing timestamps.
  - Same-format D32F `glCopyImageSubData` passes on X1E. Full wrapper suite: **37/37**; evidence
    `validation/2026-10-19-x1e-wrapper-all-final2.json`, build `59a9b230…f699`.
  - Named deviations remain: `glTextureBarrier`/NV and `glTextureView` have no legal/available ES
    implementation; `glInvalidateFramebuffer` attachment mapping and copy format substitutions
    remain backlog.

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

1. **Delivery tuple is staged, not release-qualified.** The supported tuple is the installed
   Minecraft 1.21.1 / NeoForge 21.1.262 / Iris 1.8.14-beta.1 / Sodium 0.8.13 instance. It loaded,
   rendered the original pack and saved/quit to title, but effect-specific, performance and
   lifecycle gates remain open. NeoForge 21.1.264 is an optional future patch, not a blocker.
2. **Named-effect fidelity is incomplete.** Standard PBR, shadow edges, bloom, TAA/history isolation,
   SSR, and the required ACT 192 and WSR 128 advanced sets are not yet effect-by-effect qualified on
   both original styles. This is the primary release work.
3. **Pack-load diagnostics in the supported tuple.** Custom-uniform warnings (`BIOME_PALE_GARDEN`,
   `BIOME_SULFUR_CAVES`, `endFlashIntensity`) appear with the 21.1.262 / Iris 1.8.14-beta.1 pair and can
   suppress effects; resolve with a compatible Iris/pack tuple rather than editing packs.
4. **Performance/lifecycle gates pending.** Sustained 60 FPS at 1920×1200, 20-minute thermal run,
   resize/focus/context recovery, packaging/release, and X2E onboarding remain open.

### Non-blocking provider limit (optional/future)

`MAX_SHADER_STORAGE_BLOCK_SIZE` is 134,217,728 B on the X1E provider; Ultra requests 202,899,456 B.
This is a provider boundary, not a wrapper regression, and it is not a blocker for the X1E release.
WSR 128 passes staged and is the supported advanced WSR target. Ultra ACT 256/WSR 256 remains
`BLOCKED` on X1E, optional/future X2E work only, and is never claimed as supported.

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

Close the required advanced gates on the original pack: qualify **Very High ACT 192** and the
**custom WSR 128 profile** (`COLORED_LIGHTING=128`, `WORLD_SPACE_REFLECTIONS=1`) effect by effect,
then add a native core `glClearBufferSubData` implementation so the Iris zero-fill bridge is no
longer required. WSR 128 is the advanced release gate. Standard named-effect fidelity remains the
primary release work.

Ultra ACT 256/WSR 256 is **optional/future X2E** work only. If it is characterized, record the
provider's 134,217,728 B `MAX_SHADER_STORAGE_BLOCK_SIZE` rejecting the 202,899,456 B request as a
known deviation; do not edit packs and do not present it as an X1E release gate. Optional player
reflections (`WORLD_SPACE_PLAYER_REF=1`) remain future work. Keep graphics-stage image formats,
performance, and lifecycle gates separate.
