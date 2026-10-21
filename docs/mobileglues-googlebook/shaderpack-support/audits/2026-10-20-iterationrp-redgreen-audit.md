# iterationRP red/green and black-sky audit

Read-only source audit of the original 0.8.31 archive; no device changes.

Symptoms: terrain is flat red/green/orange, sky black; geometry silhouettes are
present; no compile/link errors. Manual exposure does not fix it. The 512x128x33
atmosphere LUT is uploaded with LINEAR/MAX_LEVEL=0 and legal RGBA16F backing.

## Ranked causes

1. **`depthtexS` contract.** The pack's `R32UI` image gates the sky/solid branch.
   `Composite_Copy_CS` writes it only for solid pixels; `Image_Clear_Screen_CS`
   (composite73_b) writes 1.0 for sky at frame end. If that dispatch or image
   write is lost, sky is treated as solid and the deferred path samples the
   wrong branch. Medium-high; wrapper compute/image dispatch.
2. **Final/deferred sampler resolves to the packed G-buffer.** `colortex2`
   stores encoded normals in RG and is black for sky, matching the observed
   red/green terrain plus black sky. A stale sampler bind or FBO attachment
   alias would produce this. Medium; wrapper sampler/FBO.
3. **Atmosphere LUT layout/content or draw-time unit.** Upload and parameters
   are confirmed; only sampler binding or pack interpretation remains. Medium.
4. RGBA16/RG16 attachment widening and `colortex6` feedback: low/exonerated on
   this provider.

## Bounded probes

1. Read `depthtex0`, `depthtexS`, `colortex0`, `colortex1`, `colortex2`,
   `colortex6`, and `colortex8` at one terrain and one sky pixel after frame 2.
2. Snapshot sampler/image units for composite, composite73_b, composite25,
   composite80 and composite85 with bound texture IDs and internal formats.
3. GPU-read the atmosphere LUT at representative texels and compare to the CPU
   half-float decode.
4. Confirm the composite80 colortex6 attachment and sampler texture are
   different flip sides.

No support claim.
