# X1E player-reflection functional report

Date: 2026-10-19  
Level: user-reported game-process observation on the X1E Googlebook test laptop.  
Not a wrapper probe, not an archived log, and not a release qualification.

## Tuple

- HP Googlebook 14 (`quartz`), Snapdragon X1E80100 / Adreno X1-85, Android 17 / API 37.
- Minecraft 1.21.1, NeoForge 21.1.262, Iris 1.8.14-beta.1, Sodium 0.8.13.
- Complementary Reimagined r5.9.2, WSR 128, `WORLD_SPACE_REFLECTIONS=1`,
  `WORLD_SPACE_PLAYER_REF=1`.
- MobileGlues test core `e7795ad54fb379a489ac21ab2ef5db45f633f78979522f716b4bf2536716c54b`
  (packaged in APK `7767091dd8c753f9477bc80e6030b4c7365d232026fb670a418cdcd4c10a33d3`).
- Iris bridge jar `0b862474575676759ccec89fea049eef887163c50c72fd0ede90853362386f1f`.

## Observation

Player reflections now render without the previous crash. The option consumes too much power and
visibly drags GPU/system performance. No FPS, frame-time, power, or thermal capture was archived.

## Gate

- Functional: **staged pass, user-reported**.
- Performance/power: **FAIL / NOT QUALIFIED**.
- Release claim: **none**. WSR 128 without player reflections remains the supported advanced
  target; player reflections remain an optional performance-failing path until measured and tuned.
