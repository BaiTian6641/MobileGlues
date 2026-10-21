# iterationRP visual root-cause audit

Read-only analysis of the original 0.8.31 archive after S0 import.
Evidence: `iterationRP-original-s0-split.{png,log}`,
`iterationRP-original-nativeaa.{png,log}`.

## Two independent symptoms

### A. Scaled frame occupies only 960x528 lower-left

**High confidence: bridge/no-final copy contract.**

- 960 = 2880/3, 528 = round(1585/3), i.e. Ultra Performance scale.
- iterationRP has no `final.fsh`/`final.vsh`; `composite80`/`composite85`
  perform presentation.
- Iris 1.8.14 takes the no-final `FinalPassRenderer` path:
  `baseline.bindAsReadBuffer()` then `copyTexSubImage2D(mainColor, ..., baseWidth,
  baseHeight)`.
- The read source is the bridge-scaled colortex0; the destination is native.
  No upscale occurs. The 960x528 source lands 1:1 at the GL lower-left.
- FSR off/NativeAA is full-frame, confirming the content is present.

Fix candidates, in order:

1. Do not scale Iris targets when the pack has no final pass (force coherent
   NativeAA for that pack/profile).
2. Handle the no-final copy as a scaled blit/upscale in the bridge/core.
3. Keep FSR scaling off for iterationRP until either is implemented.

Do not attribute this to iterationRP shader math.

### B. Brown/black lighting at scale 1

**Medium-high risk: custom-image contents/scalars.**

| Rank | Candidate | Evidence |
| ---: | --- | --- |
| 1 | `img_pixelData2D` (RG16F, clear=false) undefined exposure/eye-brightness | `Final_FS.glsl` exposure multiply; `VolumetricFog.glsl`; writers `PixelData_VS`, `Exposure_CS`, `SPD_HandEmission_CS`, `SH_TRACING_CS`, `CausticsTex_CS` |
| 2 | Atmosphere 3D LUT sample black/bad | `atmoLut3D` 512x128x33 RGBA16F; `PrecomputedAtmosphere.glsl`, `Sky_Overworld_FS.glsl`; data size is exact |
| 3 | Other clear=false custom images (skyBox2D, IRC, voxel) uninitialized | `shaders.properties` image list; compute writers |
| 4 | Flat albedo/LOD/atlas path | `Terrain_GS/FS`, anisotropic LOD |
| 5 | colortex feedback/format mismatch | colortex1-5 RGBA16, colortex12 RG16; colortex6 sampled while written |

The flat foreground is ~98.7% one RGB value, consistent with a constant
multiplier or uninitialized image term, not a correctly textured surface.

## Next probes

1. P1 host: reproduce the no-final 960x528 -> native copy and log final-pass FBO,
   viewport, and copy rectangle.
2. P2 device: query `pixelData2D`, `skyBox2D`, IRC, voxel image dimensions,
   formats, units, and bounded texel values after 60+ frames.
3. P3 device: use pack options (manual exposure, sunlight-leak fix, disable sky)
   and verify the atmosphere LUT against its CPU half-float data.

No support claim. No pack edit.

## Image-format lane closed

The diagnostic `glBindImageTexture` log on the original pack shows:

- `RG16F` logical -> `RGBA16F` bind/internal (texture 135), the G1 widening;
- `RG16`/`RGBA16` logical -> `RGBA32F` bind/internal;
- `R32F`, `RGBA8`, `R16UI`-family backings as expected.

No image-format or unit rejection was logged. Therefore the brown/black frame
is not explained by an illegal image qualifier or a wrong backing format. The
remaining causes are contents/initialization (especially `pixelData2D` and the
atmosphere LUT), custom-image clear, or a pack-side post/lighting issue.

## Manual-exposure observation

With `MANUAL_EXPOSURE` on and `SUNLIGHT_LEAK_FIX` off, the frame remains flat
orange with a black sky. This exonerates the `pixelData2D` exposure scalar as
the primary cause and weakens the gbuffer lightmap/sun-mask branch. The next
leading candidates are the atmosphere 3D LUT/custom-texture upload and the
remaining clear=false custom images.
