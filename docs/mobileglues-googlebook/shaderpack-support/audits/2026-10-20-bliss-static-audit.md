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

## 2026-10-20 source trace: feature usage

Archive SHA-256 `f41db92acc585fe9ffe0cc124d6ecaea27cef9ded91129ebc2a8e60226947b2c`.
No pack edits.

### Program shape

| Feature | Evidence |
|---|---|
| Legacy graphics | 226 files `#version 120` |
| Compatibility graphics | 12 files `#version 330 compatibility` |
| Compute | 6 files `#version 430 compatibility` (`setup.csh`, `shadowcomp.csh` per dimension) |
| Compute workgroups | `setup.csh` 8x8x1; `shadowcomp.csh` 8x8x8 (512 invocations) |
| Shared memory | `shadowcomp.csh`: `shared vec4 lpvSharedData[10*10*10]`, `shared uint voxelSharedData[10*10*10]`, `barrier()` |
| No SSBO | no `std140`/`std430` buffer declarations |
| No subgroup/geometry/tessellation | source scan empty |
| Legacy builtins | 28 files use `ftransform`; 8 use `shadow2D` |

### LPV is optional, not the default path

`lib/settings.glsl:746` has `//#define LPV_ENABLED`. `shaders.properties`
disables `setup` and `shadowcomp` when LPV is off. The default Bliss path is
therefore legacy graphics only; image/compute failures are not the first S0
blocker.

### Optional LPV resources

| Resource | Format | Dimensions | Use |
|---|---|---:|---|
| `imgBlockData` | RG32UI, 1D | 2048 | imageLoad/imageStore in setup and shadowcomp |
| `imgVoxelMask` | R16UI, 3D | 64/128/256 cubed | imageStore from `shadow.vsh`; imageLoad in shadowcomp |
| `imgLpv1`, `imgLpv2` | RGBA8, 3D | 64/128/256 cubed | ping-pong LPV image load/store |

Declared with explicit formats (`layout(r16ui)`, `layout(rgba8)`,
`layout(rg32ui)`) in `lib/voxel_common.glsl`, `lib/lpv_buffer.glsl`, and
`lib/lpv_blocks.glsl`.

### MobileGlues support mapping

| Bliss need | Current MobileGlues status | Consequence |
|---|---|---|
| Compute 8x8x8, 512 invocations | supported; 512-invocation probe passes | not a blocker |
| 20 KiB shared + barrier | supported within the tested 32 KiB shared budget | not a blocker |
| 3D RGBA8 image | allowed by `mg_is_image_storage_format`; immutable 3D path exists | likely usable, not device-tested with Bliss |
| 3D R16UI image | allocation name recognized, but **R8UI/R16UI lowering is not implemented**; binding is refused | LPV blocker |
| Vertex `imageStore` on R16UI 3D | vertex image store passes for tested formats; R16UI path inherits the missing lowering | LPV blocker |
| 1D RG32UI image | RG32UI is absent from the image-storage allowlist and `glTexImage1D` is not implemented | LPV blocker |
| `BLOCK_EMISSION_ATTRIBUTE` | optional Iris feature; no local BlocK-emission qualification | shadow-entity fidelity risk |

No image/SSBO/compute feature is a reason for the default `deferred.fsh`
failure. That failure is a source-order issue: Iris emits `uniform ivec2
eyeBrightness;` after a function, which desktop GLSL accepts but ANGLE rejects.
A bridge hoist of 16/22 late uniforms runs, but the driver still reports the same
`0:948 'uniform'` error; the exact offending source has not yet been isolated.

### Format risks for the graphics path

Bliss requests `RGBA16`, `RGBA16F`, `R11F_G11F_B10F`, and `RGBA8` color
targets. `RGBA16F` and `R11F_G11F_B10F` are recorded passes; `RGBA16` is not
listed as a qualified attachment format and needs a device check once the
compile ordering failure is resolved.

No support level is promoted by this audit.
