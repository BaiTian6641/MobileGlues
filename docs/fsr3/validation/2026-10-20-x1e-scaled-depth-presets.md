# Scaled depth and preset spot check on X1E

Status: **Balanced/Quality/Performance visual spot PASS; performance and final-uniform gates open.**

Device: `C1L63708PZ`, X1E80100 / Adreno X1-85, Android 17, ANGLE Vulkan.
Pack: `AuroraLumen-Mobile-v2.zip`, profile `MOBILE_WSR (+1 option changed by user)`.
Settings: shaders ON, FSR3 history absent, motion zero, RCAS 0.4.

## Tested pair

- APK `e7d655bd…`
- Core `af74d676…`
- Bridge `cd2c2781…`

Previous NativeAA rollback remains APK `538455b7…`, core `5733b7bb…`,
bridge `1f759cfe…`.

## Trace finding

Before the fix, a Balanced world draw was:

- color attachment: 1694×932 FBO
- depth attachment: **2880×1585** external main depth

The hand used the same scaled color target, while the final output remained
2880×1585. This mixed depth/color domain is the supported cause of the
hand/light-shadow overlay; the scaled-preset trace is retained at
`fixtures/experiment-b/balanced-depth-trace.jsonl`.

The bridge now allocates a managed scene-depth texture at the scaled extent and
passes it into Iris `RenderTargets`; it is destroyed with the pipeline. The
external Minecraft depth remains native for final/UI. After the change, the
same trace shows:

- world color 1694×932 and depth 1694×932
- hand color 1694×932 and depth 1694×932
- `noHand`/`noTranslucents` remain scaled copies
- shadow target remains 1024×1024
- final output remains 2880×1585

## Visual spot checks

- Balanced, night, pause menu: single hand, no duplicated translucent overlay,
  native centered menu.
- Quality, night: single hand, native HUD, no duplicated hand/light overlay.
- Performance, night: single hand, native HUD, no duplicated hand/light overlay.
- Screenshots: `fixtures/experiment-b/{balanced-night,quality-f3,performance-f3}.png`.

F3 spot values: **Quality 29 FPS, Performance 40 FPS**. These are single
night-scene observations, not matched benchmark or thermal evidence.

## Open

- The final program now receives `viewWidth=2880,viewHeight=1585` through the
  native final-scope override; the earlier 1694×932 observation is fixed.
- A black/garbled world was captured after the first scaled-depth build. The
  bridge-owned depth clear then failed because of an incorrect LWJGL
  `glGenFramebuffers` signature. The corrected depth-only clear is installed
  and visually verified in the night frame; see
  [depth-clear evidence](2026-10-20-x1e-depth-clear-fixed.md). Temporal motion
  remains unverified.
- UltraPerformance and a post-change NativeAA regression were not rerun.
- Active pack archive SHA-256/option map remain missing.
- No matched frame-time, star-rotation, resize, or shader-reload matrix in this
  checkpoint.
