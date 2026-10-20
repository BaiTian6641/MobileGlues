# Core FSR1 audit and S1 spatial foundation

Status: **host PASS; ARM64 build PASS; device NOT RUN; full S1 gate NOT CLAIMED** (adapter
dependency, below).

- Base commit: `355d797eeaf63d1ddfc8d2199671b061d9c6462f`
- Functional result commit: `5f0c421fc934ec463530b395e0e951e91b755bef`
- Result commit: `0100e0fdaee26a34a749046924808ad8a8dc55ff` (the difference from the
  functional commit is a guard-rationale comment only) on branch `googlebook/fsr-core-s1`,
  worktree `/tmp/mg-fsr-core`
- Independent-review fix commit: `3314e39` on the same branch, on top of `72f2ad1`. Sections 2,
  4, 5 and 6 below describe the tree at `3314e39`; the claims this document made before that
  commit about viewport restoration, first-frame content and draw-FBO tracking were wrong and
  are corrected in place. The commit containing this documentation update is a child of
  `3314e39`.
- Scope: `MobileGlues-cpp/gl/FSR1/`, plus `MobileGlues-cpp/tests/fsr1_core_test.cpp` and the
  single test-runner line in `MobileGlues-cpp/tests/run.sh`. The review fixes also touch
  `gl/framebuffer.cpp`/`gl/framebuffer.h` for one narrow republish entry point
  (`mg_framebuffer_republish_draw_binding`) and the dual draw-FBO tracker, and extend
  `tests/framebuffer_shuffle_test.cpp`; no other shared interface changed.
- No public FSR ABI was added or changed. ABI v1 remains
  `/home/polar/MobileGlues/docs/fsr3/frame-contract.md`.

## 1. Reproduction of the audited defects

All five findings were confirmed in the base source before editing.

| Finding | Base evidence | Reproduction |
| --- | --- | --- |
| EASU result discarded | `FSRShaderSource.h` `main()` computed `FsrEasuF(color, ip, ...)` and then wrote only `sharpenedColor`, which came from `FsrRcasF(...)`. The only use of `color` was its declaration. | CPU model in `tests/fsr1_core_test.cpp` (`test_synthetic_edge_dataflow`): running RCAS on the scene texture at target coordinates produces a different image from EASU-then-RCAS; the test asserts both the correct result and the difference. |
| RCAS samples the original texture | Same shader: `FsrRcasLoadF` did `texelFetch(uInputTex, p)`, and `ApplyFSR` never rebound a different texture between EASU and RCAS. | Same synthetic-edge test; plus `kPasses`/`PlanIsCoherent` require RCAS's input stage to be EASU's output stage. |
| Target derived from surface/viewport, then multiplied | `CheckResolutionChange` assigned the queried EGL surface size to `g_renderWidth/g_renderHeight`, then called `CalculateTargetResolution(preset, width, height)`, which multiplied by 1.3/1.5/1.7/2.0. `OnResize` compared against the *render* size; the `glViewport` implementation set `g_resolutionChanged` whenever the requested viewport exceeded the pending size. | `test_sizing`: for a 1920x1080 output the new plan must be target 1920x1080 and render 1280x720 (Quality), not target 2880x1620. `PlanIsCoherent` rejects target != output. |
| RGBA8 at init, RGBA32F on recreate | `InitFSRResources` allocated `GL_RGBA8`; `RecreateFSRFBO` recreated the same texture as `GL_RGBA32F`. | Format policy is now one table (`kStageFormats`, all `RGBA8`) and one constant pair in `FSR1.cpp`; `test_dataflow` checks all three stages and a resize keep the same format. No `RGBA32F` remains in `gl/FSR1/`. |
| Missing injected-pass state coverage | Base `ApplyFSR` guard covered program, VAO, texture, framebuffer only. It changed viewport, clear colour, blend, depth, cull, colour mask and left them changed. | `kApplyFSRState` in `FSR1Plan.h` is the mask used by `ApplyFSR`; `test_state_mask` asserts every required bit is present. The guard now saves/restores all listed state. |
| Hidden failures looked successful | Shader compile/link errors used `LOG_F`, which is compiled out unless debug logging is on; no `glCheckFramebufferStatus` was called for any FSR FBO. | `CompileFSRShader` now force-logs and deletes on failure; every FBO creation checks `GL_FRAMEBUFFER_COMPLETE`; build/resize failure latches the path off and stops redirecting framebuffer 0. |

## 1a. Independent-review findings: fixes and evidence

All six findings were reproduced against `72f2ad1`. The fix commit is `3314e39`.

| # | Severity | Finding in `72f2ad1` | Fix (post-`3314e39` file:line) | Evidence |
| --- | --- | --- | --- | --- |
| 1 | HIGH | `kResourceCreationState` had no `STATE_VIEWPORT`; `InitFSRResources` and `RecreateFSRTargets` set `GL_VIEWPORT` to the render extent after building, and the guard could not restore the application's viewport. The claim in the previous revision that "`RecreateFSRTargets` likewise restores it" was false. | `STATE_VIEWPORT` added to `kResourceCreationState` and to the new `kRecreateState` (`gl/FSR1/FSR1Plan.h:234-241`). The two post-build bind/viewport writes were removed because the guard always undid them anyway (`gl/FSR1/FSR1.cpp:600-609`, `:617-670`); a future body that sets a viewport is now covered by the mask. `ApplyFSR`'s per-frame guard is unchanged except for the sampler bit. | `test_state_mask` asserts `STATE_VIEWPORT` in both creation masks; `test_source_invariants` asserts the three masks are the ones the bodies construct guards with. |
| 2 | MEDIUM | Init's render-FBO bind was undone by its own guard, and `ApplyFSR` upscaled unconditionally, so the first frame after resources were built sampled an unwritten render texture. | `g_dirty` is now the "render target primed" flag. `gl/framebuffer.cpp:185-192` sets it only when a draw bind of framebuffer 0 is actually redirected to a nonzero render FBO. `ApplyFSR` returns without touching GL unless it is set (`gl/FSR1/FSR1.cpp:691-706`), clears it after a successful present (`:763-765`), and every build/failure/recreate path clears it (`:592`, `:599`, `:642`, `:668`, `:797`). It is stored per context (`gl/FSR1/FSR1.h:63-69`, `FSR1.cpp:859-861`, `:898`, `:925`). An unprimed frame presents the surface exactly as drawn and logs once that it is waiting, so FSR1 is not reported as active. | Host tests cannot run GL, so the flag's set/clear sites are checked by `test_source_invariants` (g_dirty gate and clear present), and the "no redirect, no priming" arm is driven against the real `gl/framebuffer.cpp` in `framebuffer_shuffle_test` scenario 9. |
| 3 | MEDIUM | `RecreateFSRTargets` updated `gl_state->current_draw_fbo` directly; `gl/framebuffer.cpp`'s per-context `current_draw_fbo` kept naming the deleted render FBO. | New narrow entry point `mg_framebuffer_republish_draw_binding` (`gl/framebuffer.cpp:216-230`, declared at `gl/framebuffer.h:110-118`) writes both trackers. `RecreateFSRTargets` calls it with the new render FBO on success and with 0 on failure (`gl/FSR1/FSR1.cpp:638`, `:660`), only when the tracked binding was the redirect. No direct `set_gl_state_current_draw_fbo` call remains in `gl/FSR1/`. | `framebuffer_shuffle_test` scenario 9 drives the real tracker: redirect to 42, mark record 42 all-NONE, republish to 43, then require `gl_state->current_draw_fbo == 43` **and** `mg_draw_framebuffer_all_none() == false` (a stale per-context tracker would still find record 42 and answer true); then republish 0 and require 0. `test_source_invariants` also forbids a direct field write in `FSR1.cpp`. |
| 4 | MEDIUM | A sampler object bound on unit 0 overrides the FSR1 textures' min/mag filter, wrap and compare mode; EASU samples through it. | The guard saves unit 0's `GL_SAMPLER_BINDING` (`gl/FSR1/FSR1.cpp:116-122`), restores it (`:180-191`), and the internal passes bind sampler 0 (`:729`). `STATE_SAMPLER` (`gl/FSR1/FSR1Plan.h:214-219`) is in `kApplyFSRState` (`:224-227`). | `test_state_mask` requires `STATE_SAMPLER`; `test_source_invariants` requires the guard query and `GLES.glBindSampler(0, 0);` in `ApplyFSR`. Device behavior of a compare sampler is still unexercised (section 6). |
| 5 | LOW | A resize failure deleted the targets but kept the program and quad; the failure path implied full cleanup. | `CheckResolutionChange` now calls `DestroyProgramAndQuad()` on the failure arm as well (`gl/FSR1/FSR1.cpp:786-800`); the init failure arm already did. No FSR1 object survives a latched failure. | `test_source_invariants` requires at least two `DestroyProgramAndQuad();` call sites; `DestroyProgramAndQuad` zeroes the program, quad, buffer and uniform locations. |
| 6 | LOW (tests) | The state-mask test omitted `STATE_STENCIL`, `STATE_RASTERIZER_DISCARD` and the sampler bit; nothing tied the masks to the bodies. | `tests/fsr1_core_test.cpp:349-426`: required bits extended, creation/recreate masks asserted, source-invariant test added (masks in use, no direct draw-FBO field write, republish function writes both trackers, cleanup call sites). Shader check strengthened: exactly one sampler uniform, EASU `textureGather(uInputTex, ...)`, RCAS `texelFetch(uInputTex, ...)` (`:329-333`). | `sh MobileGlues-cpp/tests/run.sh` exit 0 (section 4). |

## 2. Implemented behavior

### Two coherent passes

One program, explicit `uPass` (0 = EASU, 1 = RCAS). No unused EASU path remains.

1. **EASU**: samples `g_renderTexture` (render extent) and writes `g_easuTexture`
   (`g_easuFBO`, target extent). `gl_FragCoord` is the output pixel. `FsrEasuCon` receives
   `uInputSize` as both input viewport and input resource size, and `uOutputSize` as the output
   extent. Output extent is never derived from `textureSize()`.
2. **RCAS**: samples `g_easuTexture` (bound to unit 0 for this pass), writes
   `g_targetTexture` (`g_targetFBO`, target extent) at 1:1 coordinates. `FsrRcasLoadF` clamps
   its 3x3 cross taps to the last real texel, because `texelFetch` is undefined outside the
   image.
3. **Present**: `glBlitFramebuffer` from `g_targetFBO` to framebuffer 0 with
   `GL_NEAREST`, source and destination both `0..target`, i.e. the real surface.

### Output-first sizing

`FSR1Plan::MakePlan` owns the rule:

```
output   = EGL surface extent (authority)
target   = output
intermediate = target
render   = round(output / preset scale), clamped to [1, output]
```

- `CalculateTargetResolution` is deleted. The `glViewport` implementation is now a pure
  pass-through and no longer marks a resize. Resize comes only from `eglQuerySurface`
  (`CheckResolutionChange` -> `OnResize`), compared against the output extent.
- Integer rational scales (13/10, 3/2, 17/10, 2/1) make host and renderer agree on rounding.
- Odd surface extents stay odd; the target is never snapped or multiplied past the surface.

### Format and completeness

- One policy: `GL_RGBA8`/`GL_RGBA`/`GL_UNSIGNED_BYTE` for the render texture and both
  intermediate textures, at first allocation and at every resize.
- Render FBO: colour texture + `GL_DEPTH24_STENCIL8` renderbuffer (the application believes it
  is drawing to the window). EASU and target FBOs: colour only.
- Every FBO is checked with `glCheckFramebufferStatus`; every allocation path verifies the
  generated names and extents. A failed initial build deletes partial objects, latches
  `g_initFailed`, keeps `fsrInitialized == false` and leaves framebuffer 0 unredirected. A
  failed resize deletes the partial target set, republishes **both** draw-FBO trackers as 0,
  destroys the program and quad, disables the redirect and latches off. Neither case presents
  as a working upscale, and neither leaves an FSR1 object behind.

### Priming and the first frame

Resources can be built before the application's next bind of framebuffer 0 (the shader-compile
hook) or at present time if the surface was not yet current. In both cases the render texture
starts unwritten. `FSR1_Context::g_dirty` is the "render target primed" flag:

- `gl/framebuffer.cpp` sets it only when it redirects a **draw** bind of framebuffer 0 to a
  nonzero `g_renderFBO`; a bind of 0 that is not redirected (FSR1 off/unbuilt) is the surface
  and does not set it.
- `ApplyFSR` does nothing to GL unless the flag is set: the frame is presented on the surface
  exactly as the application drew it, with a one-time forced log saying upscaling is waiting.
  This is a correct image, not a missing one.
- After a successful present the flag is cleared; `InitFSRResources`, a successful
  `RecreateFSRTargets` (new texture), the failure arms and the resize-failure latch all clear
  it. The flag is stored and loaded per context with the objects it describes.
- `InitFSRResources` no longer binds the render FBO or sets a viewport after building: its
  guard restored both, so the bind had no effect and the viewport write was the leak in review
  finding 1.

### State preservation

`GLStateGuard` covers, and `ApplyFSR` restores: program, VAO, array buffer, active texture,
unit 0 `GL_TEXTURE_2D` binding, unit 0 sampler object (`GL_SAMPLER_BINDING`), read FBO, draw
FBO, renderbuffer (resource-creation guard), viewport, scissor box, blend enable, blend
source/destination factors (RGB and alpha), blend equations (RGB and alpha),
depth test/write/function, stencil test, cull enable/mode, colour mask, clear colour and
rasterizer discard.

- Program, draw FBO and the enable bits are answered from this layer's shadows
  (`gl_state->current_program`, `gl_state->current_draw_fbo`, `mg_enable_get`) and are not
  driver queries. The enable bits are *changed* through the frontend `glEnable/glDisable`, so
  the shadow stays truthful for nested guards; restoring the same way never skips a needed
  driver call.
- VAO, read FBO, active unit, unit 0 texture and sampler bindings, viewport, scissor box, blend
  factors/equations, depth mask/function, cull mode, colour mask and clear colour have no
  trustworthy shadow and are queried once per presented frame.
- The internal passes bind sampler 0 on unit 0 before either pass, so a sampler object the
  application left bound cannot change EASU's filtering, wrap or compare state. `STATE_SAMPLER`
  is part of `kApplyFSRState`.
- `ApplyFSR` never forces the viewport back to render size; the guard restores the
  application's viewport. Resource creation and resize use sets that include `STATE_VIEWPORT`,
  and they no longer write a viewport that the guard would immediately undo.

### Off path

With `fsr1_setting == Disabled`, `presentSurface` still calls `eglSwapBuffers` directly;
`ApplyFSR`, `InitFSRResources` and `CheckResolutionChange` are not reached. `glViewport`
forwards exactly as before (its old resize bookkeeping had no consumer while disabled). No
allocation, copy, or pass is added to the disabled path.

## 3. Changed files

| File | Change |
| --- | --- |
| `MobileGlues-cpp/gl/FSR1/FSR1Plan.h` (new) | Pure sizing/pass/format/state-mask contract: sampler bit, viewport in both creation masks. No GL, no allocation. |
| `MobileGlues-cpp/gl/FSR1/FSR1.cpp` | Two-pass execution, output-first sizing, RGBA8 creation, FBO checks, expanded state guard including unit 0's sampler, dirty/primer gating, dual-tracker republish, failure latching and full cleanup, pass-through viewport, context-state fields for the new objects. |
| `MobileGlues-cpp/gl/FSR1/FSR1.h` | Uniform and EASU/texture object declarations; output extent; `g_dirty` documented as the per-context primed flag. |
| `MobileGlues-cpp/gl/FSR1/FSRShaderSource.h` | Two-pass fragment entry with `uPass`/`uInputSize`/`uOutputSize`; EASU output returned by pass 0; RCAS reads the bound intermediate with clamped taps. |
| `MobileGlues-cpp/gl/framebuffer.cpp` | `g_dirty` set only for a real redirect; new `mg_framebuffer_republish_draw_binding` that updates this file's per-context draw binding and `gl_state->current_draw_fbo` together. |
| `MobileGlues-cpp/gl/framebuffer.h` | Declaration of the republish entry point and why both trackers exist. |
| `MobileGlues-cpp/tests/fsr1_core_test.cpp` (new) | Sizing grid, dataflow, synthetic 1px-edge executable model, format policy, complete state mask (stencil, rasterizer discard, sampler, creation/recreate viewport), source-level invariants, strengthened shader-interface assertions. |
| `MobileGlues-cpp/tests/framebuffer_shuffle_test.cpp` | Adds the dual-tracker republish scenario against the real `gl/framebuffer.cpp`. |
| `MobileGlues-cpp/tests/run.sh` | Builds and runs the new host test. |

## 4. Commands actually run and observed results

All in `/tmp/mg-fsr-core`.

```sh
git submodule update --init MobileGlues-cpp/3rdparty/SPIRV-Cross \
  MobileGlues-cpp/3rdparty/glslang MobileGlues-cpp/3rdparty/xxhash \
  MobileGlues-cpp/include/ska
# result: each submodule checked out at its gitlink; perfetto deliberately not initialized.

g++ -std=gnu++20 -w -I. -I./includes -I./include -I./3rdparty/xxhash \
  -o /tmp/mg_fsr1_core_test tests/fsr1_core_test.cpp && /tmp/mg_fsr1_core_test
# result: "fsr1_core_test: all checks passed"

sh MobileGlues-cpp/tests/run.sh
# result: exit 0; first line "fsr1_core_test: all checks passed"; every existing
# host test (pixel, framebuffer, shader source, depth filter, cache key, compat image,
# compat subgroup) still reports all checks passed.

# Shader sources extracted verbatim from FSRShaderSource.h and validated as GLSL 450:
glslangValidator -S vert /tmp/FSR_VSSource.glsl   # exit 0
glslangValidator -S frag /tmp/FSR_FSSource.glsl   # exit 0

NDK=$HOME/Android/Sdk/ndk/28.2.13676358
cmake -S MobileGlues-cpp -B build/android-arm64-fsr -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-26 \
  -DANDROID_STL=c++_static -DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build/android-arm64-fsr -j 24
# result: exit 0; [111/111] Linking CXX shared library libmobileglues.so
#   build/android-arm64-fsr/CMakeFiles/mobileglues.dir/gl/FSR1/FSR1.cpp.o (450,484 bytes)
#   build/android-arm64-fsr/libmobileglues.so sha256
#     ce1d0816bdc8c6af90546d93eada0d7c8bd931c2aa27ee1a1117ed0ef335198c
```

After the review fixes (`3314e39`):

```sh
sh MobileGlues-cpp/tests/run.sh
# result: exit 0. fsr1_core_test: all checks passed (including the extended state mask
#   and source invariants); framebuffer_shuffle_test: all checks passed (0 failures),
#   including scenario 9 "republishing a replaced draw binding moves both trackers";
#   pixel, shader source, depth filter, cache key, compat image and compat subgroup
#   all still pass.

cmake --build build/android-arm64-fsr -j 24
# result: exit 0; [10/10] Linking CXX shared library libmobileglues.so
#   build/android-arm64-fsr/CMakeFiles/mobileglues.dir/gl/FSR1/FSR1.cpp.o (452,452 bytes)
#   build/android-arm64-fsr/libmobileglues.so sha256
#     481513939d5a4bca0c6e1b1a24019ce982ca1429eff981fddc01cd56d23e9fc0
```

Not run after the review fixes: the glslang validation (no shader source changed), any device
install, game launch, screenshot, timing or FPS measurement. No claim of device PASS.

## 5. Acceptance status

| Requirement | Status | Evidence |
| --- | --- | --- |
| No unused EASU output; RCAS reads the EASU result | PASS (source + host model) | Shader pass 0 returns `vec4(color, 1.0)`; pass 1 binds `g_easuTexture` and RCAS reads it; `test_synthetic_edge_dataflow` distinguishes EASU-then-RCAS from RCAS-on-source and requires the intermediate's edge at target extent. |
| Input/render/target extents explicit and correct | PASS (host) | `FSR1Plan` + `test_sizing` (presets, odd, tiny, 0) and `PlanIsCoherent`. |
| Hidden compile/link/FBO failures cannot appear successful | PASS (source + build) | Force-logged compile/link errors, `glCheckFramebufferStatus` on all FBOs, failure latch and unredirect, uniform-location check for the pass selector and size uniforms. |
| Off path unchanged | PASS (source review) | Disabled still swaps directly; no FSR call on that path; `glViewport` remains a forwarder. |
| Host tests built and run | PASS | `sh MobileGlues-cpp/tests/run.sh`, exit 0. |
| ARM64 compile | PASS | Release `libmobileglues.so` built with NDK r28c; rebuilt at `3314e39` (section 4). |
| Review 1: creation/recreate cannot leave the application viewport changed | PASS (mask + source) | `STATE_VIEWPORT` in `kResourceCreationState`/`kRecreateState`; tests assert both; the writes the guards used to undo are gone. |
| Review 2: no upscale of an unprimed render target | PASS (host source/adapter check) | `g_dirty` set only by a redirect (`framebuffer.cpp:191`), gated in `ApplyFSR`, cleared after present and on every build/failure path, per context. Device frame ordering NOT RUN. |
| Review 3: both draw-FBO trackers name the recreated FBO / 0 | PASS (host test against real `framebuffer.cpp`) | `framebuffer_shuffle_test` scenario 9 plus source invariant forbidding direct field writes. |
| Review 4: unit 0 sampler cannot override the passes | PASS (mask + source) | `STATE_SAMPLER`, driver query/restore, sampler 0 bound for the passes; compare-sampler behavior NOT RUN on device. |
| Review 5: resize failure leaves no FSR1 objects | PASS (source) | `DestroyProgramAndQuad()` on the resize-failure arm; test requires both cleanup sites. |
| Device behavior | NOT RUN | Device reserved by coordinator; no install/launch. |

## 6. Unresolved risks and blockers

1. **Adapter dependency (blocks the on-device S1 gate, not the core work).** The EGL surface
   remains the output and is not lied about, and the application's `glViewport` is deliberately
   not rescaled. A renderer that draws its scene with a surface-sized viewport into the reduced
   render texture will be cropped, and screen-size uniforms still describe the surface. Full S1
   ("no crop", correct reduced scene) therefore needs the adapter to render at the render
   extent, or a viewport/scissor policy agreed with it. This was the coordinator's explicit
   condition; full S1 is not claimed. A one-time warning is emitted when a viewport larger than
   the render target is issued while drawing into the FSR render FBO.
2. **GLSL conversion path unverified on device.** The modified shader validates as desktop
   GLSL 450 with host `glslangValidator`, and the C++ ARM64 target links, but the runtime
   `glsl_for_es` conversion plus Adreno compilation of this exact EASU/RCAS pair was not run.
3. **Format/FBO support unverified on the target.** `GL_DEPTH24_STENCIL8` renderbuffer and
   RGBA8 completeness are checked truthfully, but an unsupported combination would now disable
   FSR1 (logged) and present the surface directly; that fallback has not been exercised on the
   device.
4. **RCAS border policy.** Border taps are clamped to the edge texel. This is defined and
   tested for addressability, but not compared against AMD's reference output.
5. **Alpha and colour space.** EASU sets alpha to 1.0 and RCAS does not pass source alpha
   through (`FSR_RCAS_PASSTHROUGH_ALPHA` is not enabled); the path assumes LDR perceptual
   colour and does not handle `GL_FRAMEBUFFER_SRGB`/HDR explicitly. Unchanged from the original
   intent, but not validated for translucent scene output.
6. **Active region.** EASU treats the whole render texture as the scene (`FsrEasuCon`, not
   `FsrEasuConOffset`). Black-bar/letterbox or partial-scene inputs need a later policy.
7. **Performance.** Two target-extent passes (EASU + RCAS) replace the single broken pass.
   Cost on X1E/Adreno is unmeasured.
8. **Multiple contexts.** New object/extent/init-failure state, including the `g_dirty` primed
   flag, is stored per context in the existing `mg_fsr1_bind_context` table; context switching
   itself was not exercised.
9. **Priming is bind-granular, not draw-granular (device gap).** `g_dirty` is set by a
   redirect bind of framebuffer 0, as specified, not by observing a draw into the render
   target. An application that binds 0, draws nothing there, then draws into its own FBO and
   presents would have the previous (or first-frame empty) render-target content upscaled. The
   usual bind-0-then-draw pattern is correct; this adversarial ordering is not handled and the
   first frame is only guaranteed safe when the bind happened before any resource existed.
10. **Applications that bind framebuffer 0 once, before FSR1's resources exist, never prime
    it.** They keep drawing on the surface (correct native image) while FSR1 logs once that it
    is waiting and stays inactive. Whether real Minecraft/Adreno titles rebind 0 per frame is
    a device question, not verified here.
11. **Sampler compare on device.** The guard restores `GL_SAMPLER_BINDING` and the passes bind
    sampler 0, but no device run has exercised an application compare sampler (or a sampler
    with non-LINEAR filters) left on unit 0 at present time.

Next dependency: the Minecraft adapter (or an agreed viewport/screen-size policy) to make the
application render its scene at the render extent, then device qualification of shader
compilation, image correctness at the edges, resize and the disabled path.
