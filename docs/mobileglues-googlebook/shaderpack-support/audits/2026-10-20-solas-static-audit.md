# Solas 3.0 static adoption audit

SHA-256 `70761ed8a7b540c4df60e6775100415fa0de0f24f57c6182d09eab6323c4c78c`.
Read-only source audit; no device run. All-Rights-Reserved local fixture.

## Verdict

Solas is **not graphics-only by default**. `VX_SUPPORT` enables custom 3D images
and a `shadowcomp` compute pass. Without correct image metadata the pack is
predicted to fail at S0. S0/S1 are NOT RUN; no support claim.

## Shape

- `#version 130` graphics wrappers; `#version 430 compatibility` shadow and
  `shadowcomp`.
- One compute body: 24x12x24 groups, 512 invocations.
- Three custom images: `voxel_img` R8UI 192x96x192; `floodfill_img` and
  `floodfill_img_copy` RGBA16F 192x96x192.
- No SSBO, geometry, tessellation, subgroups, image atomics, or image reads.
- No active colortex format directives; final format block is not applied by
  the audited Iris tree.

## First risks

| # | Evidence | Class | G-ID | Gate |
| ---: | --- | --- | --- | --- |
| 1 | Formatless custom image declarations; core metadata producer missing | wrapper+bridge | G1+G4 | S0 |
| 2 | `voxel_img` in vertex `shadow.vsh`; imageStore in vertex | stage/image rewrite | G4 | S0 |
| 3 | `writeonly uniform image3D floodfill_img, floodfill_img_copy;` one line, two declarators | unit determinism | G2 | S0 |
| 4 | `endFlashIntensity` / `endFlashPosition` standard uniforms absent on tuple; zero can NaN End | tuple/metadata | G5 | End lane |
| 5 | Lifecycle maps persist; 32-name cap | wrapper+bridge | G6 | S3 |

## First probe order

1. Parse defaults: VX on, volume 192, three image entries, no SSBO.
2. Host `shadowcomp.csh`: two distinct image declarations, RGBA16F, units 11/22.
3. Host `shadow.vsh`: vertex R8UI image without `ERROR: 1:1`.
4. Host graphics programs: terrain, deferred, composite, final.
5. Device S0/S1 default Overworld. End, PBR, DH, advanced volumes separate.

No support claim follows from this audit.
