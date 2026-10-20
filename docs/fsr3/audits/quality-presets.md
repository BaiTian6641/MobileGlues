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

The JNI bridge now builds a coherent scene-extent input set: it downsamples the
pre-HUD final color to the scene extent, copies the valid lower-left scene depth
sub-rect through the motion/depth compute pass, and reconstructs to the native
output. This is no longer a native-extent final-image-only experiment.

## Limits and claims

- The direct presentation path (no Iris adapter) still downsamples the final
  image and does not reduce scene work.
- The Iris scene adapter is implemented but **device-NOT-RUN**. It scales real
  scene target allocation, color/depth extents, and matching viewports; screen
  uniforms, decals/texelFetch consumers, and pack-specific jitter ownership are
  still unverified.
- The bridge now feeds FSR3 at the scene extent. Its color input is a downsample
  of the already-composed pre-HUD image, not raw scene color; the pack final TAA
  still runs. This is not yet a clean reconstruction-boundary replacement.
- No temporal-quality claim: camera-only motion exists, jitter is not captured,
  independently moving objects are static, and the game final TAA remains enabled.
- The previous native-scene measurement was 13 FPS with FSR3 on versus 26 FPS
  off at 2880x1585. The slider does not remove the missing scene-scale hook.

## Packaging

Scene-adapter test package:

- APK: `/home/polar/MobileGlues-fixtures/MobileGlues-scene-arm64.apk`
- Bridge: `/home/polar/MobileGlues-fixtures/mgfsr3bridge-0.1.0.jar`
- Package: `com.fcl.plugin.mobileglues`
- ABI: `arm64-v8a` only
- Core SHA-256: `4a60051434ef1955a23680eb6a551967e985cf16f94206f295368e2401ce8471` (16 KB-aligned ELF)
- APK SHA-256: `1d0da3ce2172ee1ffd13779cfe2bb40492c924c9775bfe03a1369225e1f729af`
- Bridge JAR SHA-256: `8aecd174f3d6c258899223ffe6717f71833ae017031678728d6a8e6e85030939`
- Archive: `zipalign -P 16 4` + debug-key signature

## Next gate

The quality control is complete only when the scene-scale hook scales Iris
render targets and viewports coherently. That hook must keep HUD at native
resolution and match depth/screen uniforms before any performance claim is made.
