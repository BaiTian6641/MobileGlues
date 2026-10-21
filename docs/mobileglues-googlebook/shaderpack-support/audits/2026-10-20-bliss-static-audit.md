# Bliss 2.1.2 static adoption audit

SHA-256 `f41db92acc585fe9ffe0cc124d6ecaea27cef9ded91129ebc2a8e60226947b2c`.
Read-only source audit; no device run. All-Rights-Reserved local fixture.

## Verdict

Default Bliss is **graphics-only**. LPV is disabled by default; its compute and
custom-image programs are optional. There are no SSBOs, geometry, tessellation,
or subgroups. Default import is therefore plausible, but S0/S1 are NOT RUN.

## First risks

| Scope | Evidence | Class | Gate |
| --- | --- | --- | --- |
| RGBA16 colortex attachments | `dimensions/composite5.fsh:4-21`; Iris keeps block comments and parses const directives line-wise; colortex1/10/11/14 = RGBA16 | provider/FBO format (G1 candidate) | FBO probe then S0 |
| Varying/link capacity | `all_translucent.vsh:16-70` has 17 varyings; query `MAX_VARYING_VECTORS` | compile/link capacity | S0 |
| Custom uniforms / 1.21.1 IDs | health, weather, biome, lightning; `block.properties` uses post-1.21.1 IDs | tuple/metadata (G5) | S0 log / S2 |
| Optional LPV 1-D image | `lib/lpv_blocks.glsl:8-12` `uimage1D imgBlockData`; Iris calls `texImage1D`; core leaves 1-D storage unimplemented | core/provider, G1/G4 | optional LPV only |
| Optional LPV formats/units/vertex | `voxel_common.glsl` R16UI, `shadowcomp.csh` four unbound images, `voxel_write.glsl` vertex imageStore | G1/G2/G4 | optional LPV only |

LPV's 1-D image is a structural blocker for colored floodfill; it does not
block default import. DH and other option lanes are separate and NOT RUN.

## First probe order

1. Static preprocessed defaults: LPV off, no required features, active
   format table, custom uniforms.
2. Provider `MAX_VARYING_VECTORS` and mixed RGBA16/RGBA16F/R11F FBO probe.
3. Host Iris-transform compile: skybasic, textured, terrain, all_translucent,
   shadow, deferred, composite chain, final.
4. Device S0, then 60-second S1. LPV only afterward.

No support claim follows from this audit.
