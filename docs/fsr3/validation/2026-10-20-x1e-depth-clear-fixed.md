# X1E scaled-depth clear fix

Status: **Balanced night visual PASS for depth overlay; temporal ghosting not yet tested.**

Installed pair:

- APK `e7d655bd…`
- Core `af74d676…`
- Bridge `24677cd5…` (source `4640947`)

Settings: `AuroraLumen-Mobile-v2.zip`, profile `MOBILE_WSR (+1)`, Balanced,
shaders ON, history absent, motion zero, RCAS 0.4.

## Failure and fix

The first scaled-depth build allocated the replacement scene depth but failed
to create the clear framebuffer:

```text
scene depth FBO creation failed:
java.lang.NoSuchMethodException: GL30.glGenFramebuffers(int,[I)
```

LWJGL exposes `glGenFramebuffers()` returning one name and
`glDeleteFramebuffers(int)`. The bridge now uses those calls, clears only its
managed scene depth at `beginLevelRendering` RETURN, and restores the draw FBO,
scissor and depth-mask/clear state.

## Confirmed trace

Balanced night run:

| Role | Extent |
|---|---|
| World color/depth | 1694×932 / 1694×932 |
| Hand color/depth | 1694×932 / 1694×932 |
| Shadow color/depth | 1024×1024 / 1024×1024 |
| Final output | 2880×1585 |
| Final `viewWidth/viewHeight` | 2880×1585 |

The captured frame shows the night world, aurora, block lighting, a single hand
and centered native HUD; the black/garbled world from the pre-clear build is
gone.

Evidence:

- `MobileGlues-fixtures/experiment-b/final-depth-clear-night.png`
- `MobileGlues-fixtures/experiment-b/final-depth-clear-trace.jsonl`

## Limits

- Static frame proves depth/color ownership and the clear; it does not prove
  temporal ghosting behavior during camera/item motion.
- FSR3 history and motion remain OFF.
- UltraPerformance and a post-change NativeAA regression remain pending.
- Active pack archive hash/option map remain missing.
