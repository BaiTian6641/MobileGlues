# iterationRP original S0/S1 device evidence

Date: 2026-10-20
Pack: original `iterationRP Alpha 0.8.31.zip`
SHA-256 `334d0b0dadb5e2b927b992f2b93596c4cc1a518305c1e2a9b175c754a4d946e8`.
Installed core: `179afa32…` (G1 RG16F/RG32F partial); bridge `92aa55cf…`.
Profile/options: pack defaults; FSR3 bridge scaling active, dispatch bypassed.

## Result

**S0 PASS; S1 FAIL.** Iris logged `Using shaderpack: iterationRP Alpha
0.8.31.zip` and produced no shader compile/link error. The first rendered
frame is split: only a 960x528 scene-sized region appears in the lower-left;
the rest is sky. This is a viewport/target ownership failure, not a
translation failure.

Screenshot: `/home/polar/MobileGlues-fixtures/iterationRP-original-s0-split.png`
SHA-256 `cef494971c3f5158bc4e11169209554b2642fc16d283655b30564ad3deb00766`. Log: `.../iterationRP-original-s0-split.log`.

## Next isolation

Set FSR3 to NativeAA (scale 1.0) and reload the same original pack. If the
full frame returns, the failure is the FSR bridge scaling/final-viewport path.
If the split remains, it is the pack's own custom render-resolution/viewport
path or an image-unit mismatch. Do not edit the pack.
