# iterationRP private X1E port

Date: 2026-10-19  
Status: **source-level port built; device import/validation pending.**

The original archive remains byte-identical at
`references/iterationRP Alpha 0.8.31.zip`
(SHA-256 `334d0b0dadb5e2b927b992f2b93596c4cc1a518305c1e2a9b175c754a4d946e8`).

The derived pack is a **private fixture** because the pack license permits private
modification but requires the author's permission to redistribute modified
versions. It is stored outside Git at:

- `/home/polar/MobileGlues-fixtures/iterationRP-X1E-port-0.8.31/`
- `/home/polar/MobileGlues-fixtures/iterationRP-X1E-port-0.8.31.zip`
- ZIP SHA-256 `0cdd9259acb75fc311a7fdc7d5d927448ccfcd325c9a2fd59430e1b167f72921`
- directory manifest SHA-256 `0a297e5a0b6a04b43415e39b1bf75bc4d81bfc449754ef288673a3e288611214`

## Transformations

- `rg16f` → `rgba16f` (exact half values), `rg16` → `rgba32f` and
  `voxelData3D` RGBA16 → RGBA32F (exact unorm16 round-trip). Colortex1–4 packed
  payloads use RGBA32F, not RGBA16F, because half cannot represent arbitrary
  `k/65535` values.
- SPD quad swaps replaced with a shared-memory 256-invocation XOR reduction;
  mip 3/4/5 restructured so barriers stay in uniform control flow.
- Exposure/SH/BuildingWarp subgroup scans replaced with workgroup inclusive scans.
- BackwardAnalysis partition logic replaced with a shared open-addressed
  partition table and atomics.
- No `GL_KHR_shader_subgroup_*`, `GL_NV_shader_subgroup_partitioned`,
  `gl_Subgroup*`, or vendor guards remain.

## Gate

The pack has not been imported or run on X1E after the port. Validate shader
translation/compilation, gbuffer copies, RTW, exposure/SH, memory/bandwidth, and
the pre-existing non-default FSR2/SR/DOF paths separately. Do not claim support
until the device run passes.

The staged device copy for import is
`/sdcard/Download/iterationRP-X1E-port-0.8.31.zip`.
