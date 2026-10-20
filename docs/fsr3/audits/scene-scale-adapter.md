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

- Core `libmobileglues.so` SHA-256: `c6b4cd518c9d49b314e3ab8a2b20bb313724bc4ed58c77cafd352185fa563119`
- APK SHA-256: `eeb3a2f26ecd8a1ba6a49d45acc32a8c638468f02332cf78fbb7d10453c29e50`
- Bridge JAR SHA-256: `fdf7c1485340a18c520743cbe245b594f14dcfbcef546d031df9ac3ecf811f16`
- APK: `arm64-v8a` only, 16 KB ELF/archive alignment, debug-key signature
- Files: `/home/polar/MobileGlues-fixtures/MobileGlues-scene-arm64.apk` and
  `/home/polar/MobileGlues-fixtures/mgfsr3bridge-0.1.0.jar`

## Test log signatures

Expected when the adapter activates at Quality (1.5x) on a 2880x1800 surface:

```text
RenderTargetsMixin loaded
FSR3 bridge: experimental submit 1920x1200 -> 1920x1200 (render 1920x1200); synthetic motion, scene native
```

The final log text still says `scene native`; the code now uses the adapter
extent. Both the adapter activation and the actual target dimensions must be
confirmed from the run before editing that wording.

## Next executable task

Device-run the APK + bridge pair with FSR3 off first, then Quality, then
Performance. Record F3, screenshots of world and HUD, and `mgfsr3.log`/logcat.
If the world is cropped or HUD blurs, stop and inspect the viewport/FBO trace
before changing quality.
