# iterationRP original-pack device bring-up

Date: 2026-10-19 21:07 local  
Device: HP Googlebook 14 (`C1L63708PZ`), X1E80100, Adreno X1-85.  
Provider: ANGLE 2.1 / Vulkan 1.4.295 / ES 3.2 through MobileGlues 2.0.0.  
Launcher: FCL 1.3.4.0, Minecraft 1.21.1, NeoForge 21.1.262, Iris 1.8.14-beta.1,
Sodium 0.8.13.

Pack: original `references/iterationRP Alpha 0.8.31.zip`
(SHA-256 `334d0b0dadb5e2b927b992f2b93596c4cc1a518305c1e2a9b175c754a4d946e8`),
unmodified. No secondary bridge mod is installed or required.

## Result

The original pack applies, the shader pipeline initializes, and the world renders
with shader water/lighting; no shaderpack failure message is shown. This is the
first device bring-up of the unmodified pack, not a performance or effect
qualification.

## Compatible core changes used

- 16-bit image legalization: only desktop unorm `rgba16`/`rg16` are rewritten
  (`rgba16` -> `rgba32f`, except `colorimg8` -> `rgba16f` for its RGBA16F
  backing; `rg16` -> `rgba32f`). Legal `rgba16f`/`rg16f` are untouched; ANGLE
  rejects `rg32f`.
- Null 2D/3D `RGBA16`/`RG16` image storage becomes `RGBA32F`; image bind format
  follows the actual texture backing.
- KHR-only BackwardAnalysis (`subgroupShuffleXor` loop, after Iris removes the
  NVIDIA branch) is rewritten to the same shared partition table as the NV form.
- Subgroup inclusive-add/quad/SPD lowering and the reserved-identifier and
  const-transpose pre-passes remain active.
- Direct ESSL sources receive the image legalization; eager compile-failure and
  image-bind error logs are bounded.

## Evidence

- Host tests: `sh MobileGlues-cpp/tests/run.sh` PASS.
- Host sweep: all 70 original compute shaders compile after the wrapper
  transforms (`glslangValidator`).
- GPU probe: 58/58 PASS on the X1E provider.
- Standalone core SHA-256: `b34da74940abbb2c716f3f7814cd399f8a3abb6501a325ab9404afebb65a7236`.
- Device screenshot shows the rendered world with water shading; no pack-error
  chat line.

## Not yet qualified

- Visual comparison against a desktop GL reference for each effect/profile.
- ACT/WSR/compute profile effects, optional FSR2/SR/DOF paths.
- Performance, thermals, long-run stability, X2E.
- Any non-default shaderpack. Support claims remain limited to this bring-up.
