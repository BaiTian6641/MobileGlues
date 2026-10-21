# iterationRP Alpha 0.8.31 static adoption audit

Date: 2026-10-20. Read-only source/port audit; no device operations.
Original: `/home/polar/MobileGlues/references/iterationRP Alpha 0.8.31.zip`
SHA-256 `334d0b0dadb5e2b927b992f2b93596c4cc1a518305c1e2a9b175c754a4d946e8`.

## Verdict

The original archive is not adoptable as-is on X1E/ANGLE. The default program
graph contains 15 active illegal storage-image qualifier sites: 9x `rgba16`,
1x `rg16`, 5x `rg16f`. Custom image metadata also requests `RG16F`, `RGBA16`,
and `RG16`. The first hard blockers are G1 (format legalization) and G4
(stage-scoped vertex/geometry image rewriting). G2 is a proof gap, not yet a
demonstrated failure. G3 is not a default-profile blocker.

## Ranked blockers

| # | Requirement / programs | Evidence | G-ID | Smallest correct fix | Gate |
| ---: | --- | --- | --- | --- | --- |
| 1 | `rg16f` image `img_pixelData2D` in CausticsTex_CS, Exposure_CS, PixelData_VS, SH_TRACING_CS, SPD_HandEmission_CS | `shaders.properties:328`; caps `rg16f=no`; `image_metadata.cpp`; `compat_image.cpp` leaves rg16f untouched | G1 | Legal-format table covers rg16f/rg32f; write-only uses formatless + writeonly with actual backing; add RG16F to storage allowlist and bind mapping | Programs compile/link; image bind succeeds; S1 |
| 2 | `rgba16`/`rg16` images and unorm16 attachments | `Composite_Copy_CS` 4x, `Bloom_CS`, `BloomCS_SIG` 3x, `VoxelData_Copy_CS`; `BuildingWarp` `rg16`; colortex1-5 `RGBA16`, colortex12 `RG16` | G1 | Target-aware shared table; promote only image-bound unorm16; remove name hacks; no bridge-specific qualifiers | Original bytes compile/render; FBO completeness recorded |
| 3 | Vertex image `PixelData_VS`; geometry `shadow.gsh` 3x uimage3D + imageAtomicMax | `composite80.vsh`; `Shadow.glsl`; D7 `ERROR:1:1` risk | G4 | Pass stage into image rewriting; add layout/format only where legal; formatless write-only for VS/GS when needed; explicit multi-image probes | PixelData_VS and shadow.gsh compile/link; distinct units; atomics read back |
| 4 | Multi-image determinism | Composite_Copy_CS 6 images; shadow.gsh 3; SPD 2; concurrent compute | G2 | Prove core resolver with bridge removed; no unit-0 fallback | Distinct units in original pack, no bridge |
| 5 | Lifecycle state / 32-name cap | fallback map persists; unit 0 after 32 names | G6 | Clear maps on load/reload/context loss; fail loudly | A->B->A switch and reload |
| 6 | Custom uniforms | pack-specific smooth biome/eye uniforms; G5 names are other-pack-specific | G5 | Qualify tuple or document suppression; no ZIP edit | Log audit or NOT TESTED |
| 7 | SSBO clear | 4 KiB `bufferObject.0`; SSBOUpdate_CS writes all fields | G3 | Shared reachable zero/clear path; not iterationRP-specific | WSR/original first-frame contents |
| 8 | Advanced options | FSR2/SR/DLSS, DOF, waveform, line images, custom resolution, larger voxel/IRC/RTW | G7/G8 | Separate profile matrix; no code until reference | S2/S3 per option |

## Default vs advanced

Default is defined by `Settings.glsl` defaults and `program.*.enabled`, not
Iris `profile.*`. Default includes voxel 192, IRC 128, RTW 1024, skybox 64,
TAA, motion blur, bloom, rough specular. DOF, FSR2, SR/DLSS, waveform, line
images, custom resolution, DH/Voxy and larger volumes are advanced lanes.

## Private ports are not evidence

The ports widen image/attachment formats (`RG16F`->`RGBA16F`,
`RGBA16`->`RGBA32F`), change subgroup code, and change BackwardAnalysis/SH
semantics. They mask G1/G4 and are not behaviorally equivalent to the
original. They must remain `PRIVATE EVIDENCE ONLY`.

Base-port hashes conflict in the record: current ZIP
`5b061ae6ae8ef2c0d278ca9458f37747c6b73898e4572a91e0e00433977b78b4` versus
the earlier validation record
`0cdd9259acb75fc311a7fdc7d5d927448ccfcd325c9a2fd59430e1b167f72921`.
The r2 ZIP is `46a0224b664fbbe56e22e240cff960f83373c0e864157e1da9af0dae9f50461e`.
No derived result can close an original row.

## First experiments

1. G1 host format-table and original-source transform sweep: add rg16f/rg32f,
   target-aware rgba16/rg16, formatless write-only; compile the original
   affected programs as ESSL 320. Gate: no invalid qualifiers, truthful errors.
2. G2/G4 host stage/unit probes: vertex 1-2 images, GS 3x uimage3D with
   atomics, fragment legal rgba16f, compute 6 images. Gate: distinct units, no
   `ERROR:1:1`, atomics visible.
3. Device S0/S1 on original bytes, no bridge, default profile. Gate: import,
   no illegal qualifiers, 60-second pipeline route, then S2.

Do not start device work before the host G1/G4/G2 compile gates.
