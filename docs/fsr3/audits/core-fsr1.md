# Core FSR1 audit and S1 spatial foundation

Status: **host PASS; ARM64 build PASS; device NOT RUN; full S1 gate NOT CLAIMED** (adapter
dependency, below).

- Base commit: `355d797eeaf63d1ddfc8d2199671b061d9c6462f`
- Functional result commit: `5f0c421fc934ec463530b395e0e951e91b755bef`
- Result commit: `0100e0fdaee26a34a749046924808ad8a8dc55ff` (the difference from the
  functional commit is a guard-rationale comment only) on branch `googlebook/fsr-core-s1`,
  worktree `/tmp/mg-fsr-core`
- Scope: `MobileGlues-cpp/gl/FSR1/`, plus `MobileGlues-cpp/tests/fsr1_core_test.cpp` and the
  single test-runner line in `MobileGlues-cpp/tests/run.sh`.
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
  failed resize deletes the partial set, republishes the tracked draw FBO as 0, disables the
  redirect and latches off. Neither case presents as a working upscale.

### State preservation

`GLStateGuard` now covers, and `ApplyFSR` restores: program, VAO, array buffer, active texture
and unit 0 `GL_TEXTURE_2D` binding, read FBO, draw FBO, renderbuffer (resource-creation guard),
viewport, scissor box, blend enable, blend source/destination factors (RGB and alpha), blend
equations (RGB and alpha), depth test/write/function, stencil test, cull enable/mode, colour
mask, clear colour and rasterizer discard.

- Program, draw FBO and the enable bits are answered from this layer's shadows
  (`gl_state->current_program`, `gl_state->current_draw_fbo`, `mg_enable_get`) and are not
  driver queries. The enable bits are *changed* through the frontend `glEnable/glDisable`, so
  the shadow stays truthful for nested guards; restoring the same way never skips a needed
  driver call.
- VAO, read FBO, active unit, unit 0 texture, viewport, scissor box, blend factors/equations,
  depth mask/function, cull mode, colour mask and clear colour have no trustworthy shadow and
  are queried once per presented frame.
- `ApplyFSR` no longer forces the viewport back to render size; the guard restores the
  application's viewport. `RecreateFSRTargets` likewise restores it.

### Off path

With `fsr1_setting == Disabled`, `presentSurface` still calls `eglSwapBuffers` directly;
`ApplyFSR`, `InitFSRResources` and `CheckResolutionChange` are not reached. `glViewport`
forwards exactly as before (its old resize bookkeeping had no consumer while disabled). No
allocation, copy, or pass is added to the disabled path.

## 3. Changed files

| File | Change |
| --- | --- |
| `MobileGlues-cpp/gl/FSR1/FSR1Plan.h` (new) | Pure sizing/pass/format/state-mask contract. No GL, no allocation. |
| `MobileGlues-cpp/gl/FSR1/FSR1.cpp` | Two-pass execution, output-first sizing, RGBA8 creation, FBO checks, expanded state guard, failure latching, pass-through viewport, context-state fields for the new objects. |
| `MobileGlues-cpp/gl/FSR1/FSR1.h` | Uniform and EASU/texture object declarations; output extent. |
| `MobileGlues-cpp/gl/FSR1/FSRShaderSource.h` | Two-pass fragment entry with `uPass`/`uInputSize`/`uOutputSize`; EASU output returned by pass 0; RCAS reads the bound intermediate with clamped taps. |
| `MobileGlues-cpp/tests/fsr1_core_test.cpp` (new) | Sizing grid, dataflow, synthetic 1px-edge executable model, format policy, state mask, shader-interface assertions. |
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

Not run: any device install, game launch, screenshot, timing or FPS measurement. No claim of
device PASS.

## 5. Acceptance status

| Requirement | Status | Evidence |
| --- | --- | --- |
| No unused EASU output; RCAS reads the EASU result | PASS (source + host model) | Shader pass 0 returns `vec4(color, 1.0)`; pass 1 binds `g_easuTexture` and RCAS reads it; `test_synthetic_edge_dataflow` distinguishes EASU-then-RCAS from RCAS-on-source and requires the intermediate's edge at target extent. |
| Input/render/target extents explicit and correct | PASS (host) | `FSR1Plan` + `test_sizing` (presets, odd, tiny, 0) and `PlanIsCoherent`. |
| Hidden compile/link/FBO failures cannot appear successful | PASS (source + build) | Force-logged compile/link errors, `glCheckFramebufferStatus` on all FBOs, failure latch and unredirect, uniform-location check for the pass selector and size uniforms. |
| Off path unchanged | PASS (source review) | Disabled still swaps directly; no FSR call on that path; `glViewport` remains a forwarder. |
| Host tests built and run | PASS | `sh MobileGlues-cpp/tests/run.sh`, exit 0. |
| ARM64 compile | PASS | Release `libmobileglues.so` built with NDK r28c. |
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
8. **Multiple contexts.** New object/extent/init-failure state is stored per context in the
   existing `mg_fsr1_bind_context` table; context switching itself was not exercised.

Next dependency: the Minecraft adapter (or an agreed viewport/screen-size policy) to make the
application render its scene at the render extent, then device qualification of shader
compilation, image correctness at the edges, resize and the disabled path.
