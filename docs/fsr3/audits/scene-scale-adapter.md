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
| Double-present | direct swap-path FSR3 skips while the bridge has presented in the last 250 ms | host/ARM64 build PASS; runtime NOT RUN |

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

- Core `libmobileglues.so` SHA-256: `f437a53472540d8a6661edbdbdfc6819e946bfae700fc26a378150605eb095ca`
- APK SHA-256: `34ed362e405420c9038a871bcebdacece719b135445a792093a2b465c599ab02`
- Bridge JAR SHA-256: `096111baf99ce7ab4508c680695cb76f076c0364d2082fc6d3e4591974b0b609`
- APK: `arm64-v8a` only, 16 KB ELF/archive alignment, debug-key signature
- Files: `/home/polar/MobileGlues-fixtures/MobileGlues-scene-arm64.apk` and
  `/home/polar/MobileGlues-fixtures/mgfsr3bridge-0.1.0.jar`

## Test log signatures

Expected when the adapter activates at Quality (1.5x) on a 2880x1800 surface:

```text
RenderTargetsMixin loaded
FSR3 bridge: experimental submit 1920x1200 -> 1920x1200 (render 1920x1200); synthetic motion, scene native
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
