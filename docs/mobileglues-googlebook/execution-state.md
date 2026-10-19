# MobileGlues X1E Execution State

Plan baseline: `docs/MobileGlues_Googlebook_X1E_Optimization_Plan.md` (canonical path in this workspace).
Last updated: 2026-10-18.

## Current stage and active task

- Stage 0 (lock/build/identify provider): in progress on host work; **X1E device gate pending**.
- Stage 1 (observable failures and source-contract repairs): active implementation work.
- Active task: fix the audited `glShaderSource` multi-string submission contract, then replace global
  last-shader state and remove compile/link failure masking, with host regressions.

## Repository map

| Root | Branch | HEAD | Remote | State |
| --- | --- | --- | --- | --- |
| `/home/polar/MobileGlues` | `main` | `97558a69157b033b890958b1e49258c5b51a80c6` | `origin` = `git@github.com:BaiTian6641/MobileGlues.git` (preserved); `upstream` = `https://github.com/MobileGL-Dev/MobileGlues.git` (read baseline) | Task work; plan/records are untracked until committed |
| `/home/polar/MobileGlues-plugin` | detached | `f5105e4ddd1752cccdcc6421c402960d40ec3d9d` | `origin` = `https://github.com/MobileGL-Dev/MobileGlues-plugin.git` | Clean clone; core submodule not initialized; no edits |

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
- An original-looking Complementary Reimagined **r5.9.3** archive is now active in
  the game process at High (not the plan's r5.9.2; r5.9.3 joins a new fixture row).
  Iris no longer reports `(fallback)` after the RGB8_SNORM backing fix, and the
  F3 overlay shows the pack name and High profile. The measured menu/world frame
  rate was 22 FPS at 2880×1585, below the plan's 60 FPS target and not yet a
  controlled performance run.
- The installed test plugin APK (core `0557189`, debug signing) is the renderer
  named in the game log's `SDL_OPENGL_LIBRARY` and `LD_LIBRARY_PATH`. The original
  installed 2.0.0 APK is preserved for rollback.
- Latest commits: `d9789fa` shader source/per-object state, `9a91ede` compiler
  limits/SSBO binding, `0557189` RGB8_SNORM backing, `468c4bb` records.
- Latest standalone ARM64 artifact SHA-256:
  `fd37ab2f71801b6619b937af1895234c318dab9e3dcf29f8f41993d4577d73a8`.
- The plugin APK (debug) SHA-256 is
  `ed3f3c9d5f7b010e2d92d2313ad1e85f2a81f6f651a869f3bea8d313ec5a32ba`; its
  packaged `libmobileglues.so` SHA-256 is
  `bc23304770683b167fc5ebe328dee4e12e668f39bcbd034d66551a37c9178cac`.

## Blockers

1. **No X1E Googlebook connected.** `adb devices` exposes two x86_64 emulators (`sdk_gphone64_x86_64`,
   API 36/37). No target-device/provider, visual, or performance gate can be closed. Do not relabel emulator
   results as Googlebook results.
2. **No launcher/JVM tuple in this workspace.** The plugin targets FCL/Zalith/Pojav-style launchers, but no
   launcher checkout or Android ARM64 JRE is pinned here. Game-tuple rows remain pinned at artifact level;
   launcher/JVM rows are explicitly unknown.
3. **Plugin/core gitlink mismatch.** The audited plugin pins core `f56af31`; this workspace builds core
   `97558a6` (the audit baseline, which contains `f56af31` plus the multidraw staging fix). The plugin
   submodule must be updated in a separate reviewed parent/child change when integration begins.

## Latest test evidence

- Host: `sh MobileGlues-cpp/tests/run.sh` → all checks passed (0 failures), 2026-10-18.
- ARM64: `cmake --build build/android-arm64-release -j 24` → success, 102/102 targets.
- Artifact: `build/android-arm64-release/libmobileglues.so`,
  SHA-256 `884582a269a44bdab4641259bfa5b330335df13ca08ad8cfafd88ac59ab2501e`.
- ELF: `ELF64 AArch64`, Android 26, NDK r28c, `LOAD` align `0x4000` (16 KB).

## Exact next action

Run a controlled visual session on `C1L63708PZ` with the test plugin and the
active Complementary r5.9.3 High pack: set a fixed time/weather, capture a still
and a short pan, then toggle shaders off for a paired reference. Check water SSR,
rough/specular PBR, shadow edges, clouds/fog, bloom, and TAA/history behavior;
record each as pass/fail with the exact pack/settings. Then acquire/install the
original r5.9.2 archive and repeat. Address the Iris custom-uniform warnings by
selecting a compatible Iris/pack tuple. R8UI/R16UI lowering and WSR 256 remain
separate gates; the latter is blocked by the provider's 128 MiB SSBO limit.
