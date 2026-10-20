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

## Iris bridge path

The JNI bridge performs the same final-color downsample only when the selected
extent is below the final target. At `NativeAA` it keeps the existing direct
attachment path and does not allocate the extra color/depth textures. At lower
presets it uses a synthetic depth texture with the same reduced extent.

## Limits and claims

- Neither path reduces Iris/Minecraft render-target allocation, viewports,
  depth extents, or screen-size uniforms. Scene shading and fill remain at the
  display resolution.
- The downsampled final image is a **reconstruction-quality experiment**, not a
  scene-performance feature. At lower presets, added downsample traffic can
  make frame time worse even though the FSR3 internal chain is smaller.
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
