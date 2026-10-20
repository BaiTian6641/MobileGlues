# Scene adapter + camera-only motion, X1E first successful frame

Status: **rendering PASS (single run); performance and temporal quality NOT RUN.**

## Tuple

- Device: HP Googlebook 14 `C1L63708PZ`, X1E80100 / Adreno X1-85.
- OS: Android 17 / API 37; ANGLE Vulkan 1.4.295, ES 3.2.
- Minecraft 1.21.1 / NeoForge 21.1.262 / Iris 1.8.14-beta.1 / Sodium 0.8.13.
- Shaderpack: `AuroraLumen-Mobile-v2.zip` (not Complementary).
- FSR3 setting: enabled, quality **Quality**.
- APK SHA-256: `858743c7012c80f3349dea9a2573ccd0d89096573f3e4907d10b796c10316707`.
- Core SHA-256: `83372740ffceb7f2b0c95cc7daa42b2e9c73b8a02c393b42df40d8a3364c6a14`.
- Bridge SHA-256: `a7b9263993208d33dba51fdc0809c35ec71f0bd3ec62fca6b9029f2295e836c8`.
- Screenshot FSR3 Quality: `/home/polar/MobileGlues-fixtures/2026-10-20-fsr3-quality-camera-motion.png`
- Log FSR3 Quality: `/home/polar/MobileGlues-fixtures/2026-10-20-fsr3-quality-camera-motion.log`
- Screenshot FSR3 off baseline: `/home/polar/MobileGlues-fixtures/2026-10-20-fsr3-off-baseline.png`

## Observed

```text
FSR3 native viewport 2880x1585
FSR3 bridge: experimental submit 1920x1057 -> 2880x1585
              (render 1920x1057, surface 2880x1585)
FSR3 bridge: output 2880x1585 frame=1 motion=0
FSR3 bridge: camera motion active at frame 2
```

- World fills the full game view; no lower-left crop.
- Hotbar/HUD are native-size and sharp; no observed HUD blur.
- First frame reports `motion=0` (no previous camera); frame 2 reports camera motion active.
- F3 overlay is present. Early FSR3 Quality frames were ~22 FPS and one
  FSR3-off frame ~30 FPS, but a different turned viewpoint measured ~18 FPS
  off. Those cameras/areas are not matched. **Net performance is NOT
  MEASURED**; no pass/fail conclusion is valid until the same camera, pack,
  resolution and time window are captured in both modes.

## What this proves

- The corrected bridge JAR loads all three mixins and the FSR3 final-pass path.
- FSR3 consumes a 1920x1057 scene extent and returns a 2880x1585 output.
- Camera-only reprojection initializes after the first frame.
- Native HUD composition no longer breaks at the tested tuple.

## Not proven

- That the Iris scene targets themselves, rather than only the bridge inputs, are
  reduced; a one-time target-size log is still needed.
- Net performance versus FSR3 off at a matched camera; the current samples
  are not comparable.
- Temporal quality, ghosting, jitter ownership, independent entity motion.
- Complementary or r5.9.2 behavior; ACT/WSR/player reflections.
- Sustained thermals/lifecycle.

## Packaging failure modes fixed during bring-up

- APK must use `extractNativeLibs=true`/legacy JNI packaging for FCL's
  `nativeLibraryDir`; otherwise `UnsatisfiedLinkError` before rendering.
- The old 9,386-byte bridge JAR lacks `RenderTargetsMixin`; it leaves the
  scene-extent path inert.
- Constructor `@ModifyVariable` handlers must be `static` before `super()`.
