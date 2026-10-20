# Minecraft/Iris reduced-resolution scene adapter (experimental)

Status: **implemented, host build PASS, device NOT RUN.** This is the S4 adapter
slice. It is deliberately Minecraft/Iris-specific and optional; the MobileGlues
upscaler and config remain the authority.

## What changed

| Layer | Change | Evidence |
| --- | --- | --- |
| Iris adapter | `RenderTargetsMixin` modifies the constructor and `resizeIfNeeded` width/height through `Fsr3Bridge.sceneWidth/Height` | `integrations/minecraft/fsr3-bridge` build PASS; runtime NOT RUN |
| Native scale | `Fsr3Bridge_nativeScale` reads `fsr3Quality`; returns 1.0 when FSR3 is off | host/ARM64 build PASS |
| Viewport | `glViewport` and `glScissor` wrappers scale only when the draw FBO color attachment is below the EGL surface extent | host/ARM64 build PASS; runtime NOT RUN |
| Final pass | `FinalPassRendererMixin` resets the viewport to native EGL extents at `HEAD` | bridge build PASS; runtime NOT RUN |
| Double-present | direct swap-path FSR3 skips exactly the swap after a bridge submit via a per-frame flag | host/ARM64 build PASS; runtime NOT RUN |

## Why this shape

The wrapper already owns the OpenGL viewport and FBO objects, so it can compare
the requested viewport with the actual draw attachment instead of applying a
global scale. Shadow maps (attachment smaller than the surface, viewport equal
to the attachment) and native final/HUD targets are skipped. This is the
smallest hook that keeps the upscaler in MobileGlues while allowing the Iris
adapter to request a real reduced scene.

## Known risks to verify on device

- Iris screen-size uniforms and `texelFetch` consumers may still see the
  display extent rather than the scaled target extent.
- Decals, particles, hands, shadows and Create geometry may use their own
  extents or viewports; they are not automatically covered.
- The final pass is still native, and its sampler input is the already-composed
  image in the bridge path. This is not yet a pre-final scene-color FSR input.
- `RenderTargets` may be constructed before the config/adapter is ready; the
  mixin then leaves the target native for that generation.
- No correctness/performance claim is made until the X1E run records the exact
  log lines and frames.

## Package identity

- Core `libmobileglues.so` SHA-256: `472576b7fe303d06362277a4e9f2864dd1077177bebfe54e615dc1a38816024f`
- APK SHA-256: `025850a0180eead1d4434d35cf9f9d5ac5bfd09503572b7bde8a927f3df64ce9`
- Bridge JAR SHA-256: `7e171274b9e86619e1455ec8849a297ca244615c5d1244dfa6794babff9724dc`
- APK: `arm64-v8a` only, 16 KB ELF/archive alignment, debug-key signature
- Files: `/home/polar/MobileGlues-fixtures/MobileGlues-scene-arm64.apk` and
  `/home/polar/MobileGlues-fixtures/mgfsr3bridge-0.1.0.jar`

## Test log signatures

Expected when the adapter activates at Quality (1.5x) on a 2880x1800 surface:

```text
RenderTargetsMixin loaded
FSR3 bridge: experimental submit 1920x1200 -> 2880x1800 (render 1920x1200); camera_motion=1, Iris scene adapter
```

The bridge log now says `Iris scene adapter`; the adapter extent must still be
confirmed from the actual run.

## Next executable task

Device-run the APK + bridge pair with FSR3 off first, then Quality, then
Performance. Record F3, screenshots of world and HUD, and `mgfsr3.log`/logcat.
If the world is cropped or HUD blurs, stop and inspect the viewport/FBO trace
before changing quality.


## Independent review corrections

The first review found two blocking integration defects, both now fixed:

- `resizeIfNeeded` args for Iris 1.8.14 are `(depthBufferVersion, depthTextureId,
  width, height, ...)`; the handlers now use ordinals 2/3. The previous 0/1
  would have reverted targets on the first per-frame resize and could corrupt
  depth attachments.
- Iris binds its gbuffer FBO without a viewport call, so the viewport hook now
  also runs at the end of the app-facing `glBindFramebuffer`.
- The direct-present suppression is a per-frame flag, not a 250 ms window.
- The bridge captures/restores read/draw FBO and viewport at entry through an
  RAII guard, including early returns after aux allocation.

Host semantic tests pass with these changes. Device runtime remains NOT RUN.

## S3 multi-frame probe status

`tools/fsr3_spike/backend_probe/main.cpp` already submits three frames
(reset, history, reset) and checks output/dispatch state on the target. It was
built for ARM64, not host.

A host build was attempted as an additional check:

```sh
g++ -O2 -std=c++20 -I MobileGlues-cpp \
  -o /tmp/mg_fsr3_backend_probe tools/fsr3_spike/backend_probe/main.cpp -ldl -lEGL -lGLESv2
/tmp/mg_fsr3_backend_probe build/host-release/libmobileglues.so
```

Result: **FAIL/BLOCKED before dispatch**. `dlopen` runs the core static
initializer, which calls the frontend `glGetIntegerv` before a current context
has populated the GLES dispatch table; the process recurses and segfaults.
This is a host-harness initialization limitation, not a backend result. Do not
record it as a multi-frame PASS. The ARM64 multi-frame probe remains NOT RUN
until a device run is authorized.


## Camera-only motion (experimental)

The bridge now captures Iris `CapturedRenderingState.getGbufferModelView()` and
`getGbufferProjection()` through reflection at the final-pass hook and sends the
two column-major matrices to the core with `nativeCamera`. The core keeps the
previous pair and, when both frames exist, runs an 8x8 compute shader that:

1. reconstructs a view-space/world position from the final depth attachment;
2. projects it through the previous view-projection;
3. writes `(previous_uv - current_uv) * extent` as RGBA16F motion.

This is **camera-only**: independently moving entities, hands, particles,
foliage, water and Create contraptions are not represented. Matrices may include
Iris/pack projection jitter because no separate jitter uniform is captured yet;
`motion_includes_jitter` stays 0. Depth is assumed GL 0..1, non-reversed, and the
final attachment must be a 2D depth texture. Shader/pipeline failure falls back
to zero motion, and history is reset every frame on that fallback.

The bridge no longer resets history on every frame once camera motion executes;
this is the first temporal-history experiment and is expected to ghost moving
objects. No pass/quality claim exists until device screenshots and motion-debug
captures are reviewed.

The package hashes below supersede the prior static-motion pair.


## Host checks for semantic motion

`tests/fsr3_motion_math_test.cpp` verifies the column-major multiply, identity
and translation inverse, and singular-matrix rejection used by the camera-only
reprojection pass. `tests/fsr3_viewport_test.cpp` verifies the scene-viewport
rule. Both pass in `sh MobileGlues-cpp/tests/run.sh`. Runtime matrix extraction
from Iris and the GLSL motion dispatch remain device-NOT-RUN.


## Second review corrections (camera/depth inputs)

The camera-motion review found four material defects, now corrected:

- Camera translation was omitted. The bridge now also captures
  `CameraUniforms.getUnshiftedCameraPosition()` and passes
  `current - previous` to the reprojection shader.
- The motion/depth pass now runs at the scene extent and samples the valid
  lower-left depth sub-rect at scene coordinates; it writes a scene-extent R32F
  depth input and RGBA16F motion.
- FSR3 now consumes scene-extent color/depth/motion and reconstructs to the
  native output. The pre-HUD final image is downsampled to that extent.
- Image units 0 and 1 are saved/restored and the MobileGlues image-binding
  shadow is invalidated after the compute pass.
- Camera history is invalidated after a >250 ms capture gap, a >64-block camera
  jump, a reflection failure, or an Iris pipeline create/destroy (world or
  shader generation change).
- The shader stores zero motion on invalid reprojection/projection instead of
  leaving stale texels.

Still unverified on device: ANGLE feedback sampling of the main depth texture,
Iris matrix values, exact scene sub-rect placement, and motion quality.


## APK packaging requirement (2026-10-20)

The first device attempt crashed before rendering:

```text
UnsatisfiedLinkError: dlopen failed:
.../com.fcl.plugin.mobileglues.../lib/arm64/libmobileglues.so not found
```

The test APK had `extractNativeLibs=false`; FCL resolves the renderer through
`nativeLibraryDir`/`legacyNativeLibraryDir`, so the separately packaged core was
never extracted. The test APK must use `android:extractNativeLibs="true"`
(AGP `jniLibs.useLegacyPackaging = true`). The corrected APK has
`legacyNativeLibraryDir`, and the extracted `libmobileglues.so` is present.
This is a packaging/install property, not an FSR3 runtime result.
