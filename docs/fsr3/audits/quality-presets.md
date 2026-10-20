# FSR3 quality presets and slider

Status: implemented in the core setup parser, the MobileGlues FSR3 direct path,
and the optional Iris bridge path. **Image-space only; scene work is still
native.** This is the quality-control slice, not the low-resolution-scene gate.

## Configuration

`fsr3Quality` is an integer enum, independent of `fsr3Setting`:

| Value | Preset | Scale | Internal render extent at 2880x1800 |
| --- | --- | --- | --- |
| 0 | NativeAA | 1.0x | 2880x1800 |
| 1 | Quality | 1.5x | 1920x1200 |
| 2 | Balanced | 1.7x | 1694x1059 |
| 3 | Performance | 2.0x | 1440x900 |
| 4 | Ultra Performance | 3.0x | 960x600 |

The plugin writes and reads the key in
`MGConfigCodec`; the native settings parser validates it and logs
`Fsr3Quality`. The existing `fsr3Setting` remains the master switch. Both
``MiuixSettings`` and ``MaterialSettings`` expose a discrete slider; changing it
requires relaunching the game because the core reads settings at process start.

## Native presentation path

`FSR3Direct` now allocates its color/depth/motion inputs at
`ceil(output / scale)` and copies the default framebuffer into the color input
with a linear blit. The eight-pass backend reconstructs to the native output
extent, then presents that output. Depth is still synthetic constant depth and
motion is zero.

## Iris scene adapter

`RenderTargetsMixin` scales the Iris main scene targets at construction and at
`resizeIfNeeded` using the same preset. Color, alternate-color and depth targets
therefore share one extent. The core `glViewport`/`glScissor` wrappers scale a
native-sized viewport only while the bound draw FBO has a smaller color
attachment; native FBOs (final composition, HUD, shadows) are left alone. The
final-pass hook resets the viewport to the EGL surface before Iris composes to
the native main target.

The JNI bridge no longer downsamples the final image. At this stage it consumes
the native final image for the FSR experiment; the scene reduction comes from
the scaled Iris targets. This avoids applying the quality ratio twice.

## Limits and claims

- The direct presentation path (no Iris adapter) still downsamples the final
  image and does not reduce scene work.
- The Iris scene adapter is implemented but **device-NOT-RUN**. It scales real
  scene target allocation, color/depth extents, and matching viewports; screen
  uniforms, decals/texelFetch consumers, and pack-specific jitter ownership are
  still unverified.
- The final image enters FSR3 at native extent in the bridge path. Therefore the
  FSR3 temporal cost is additional; only the scene-target reduction can pay for
  it. No performance claim is valid until the device matrix runs.
- No temporal-quality claim: depth is constant, motion is zero, and the game
  final TAA remains enabled.
- The previous native-scene measurement was 13 FPS with FSR3 on versus 26 FPS
  off at 2880x1585. The slider does not remove the missing scene-scale hook.

## Packaging

Debug APK with the ARM64 core and slider:
`/tmp/MobileGlues-quality-arm64.apk` (also copied to
`/home/polar/MobileGlues-fixtures/MobileGlues-quality-arm64.apk`).

- Package: `com.fcl.plugin.mobileglues`
- ABI: `arm64-v8a` only
- Core SHA-256: `1acf176ab1cafa90b3fe8e60c8e6fcadaaa6e8e0f6dc337dd7acb5e81db3bba8` (16 KB-aligned ELF, `LOAD ... 0x4000`)
- APK SHA-256: `adb844c934c32f86a304cf1e36fe31634b9787a82c0f9efa73c2359a0c223b9d`
- Archive: `zipalign -P 16 4` + debug-key signature

## Next gate

The quality control is complete only when the scene-scale hook scales Iris
render targets and viewports coherently. That hook must keep HUD at native
resolution and match depth/screen uniforms before any performance claim is made.
