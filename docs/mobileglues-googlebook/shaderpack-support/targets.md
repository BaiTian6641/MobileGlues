# Shaderpack support targets

Updated: 2026-10-20. All archives are local fixtures for testing only.
No ZIP is committed to Git.

## Sildur's series (P0/P0b/P0c)

| Field | Vibrant | Enhanced Default | Basic |
| --- | --- | --- | --- |
| Modrinth project | `z8EjLYqN` / `sildurs-vibrant-shaders` | `2jvH1Rcl` / `sildurs-enhanced-default-shaders` | `N1BIOvQX` / `sildurs-basic-shaders` |
| Version | 2.02 Extreme | 1.20 Fancy | 2.7 Fancy |
| Version ID | `J4RlzpKF` | `va1R00mK` | `fgt6eoLP` |
| SHA-256 | `824504bc937f9570277c0a809f5059ae4a5fe70c413cf4459b6ccff4dfed7ebe` | `b30353b5dd831cb5ab060ee96e1ac762c571bb4de5d729d329463dff0f2f1235` | `984b22d5b3d657b3c064ec0476bb67e903fced4536e894e4e28daf3e03ac7c74` |
| Modrinth SHA-512 verified | yes | yes | yes |
| License | All-Rights-Reserved | All-Rights-Reserved | All-Rights-Reserved |
| Local fixture | `/home/polar/MobileGlues-fixtures/Sildurs/Sildurs-Vibrant-2.02-Extreme.zip` | `.../Sildurs-Enhanced-Default-1.20-Fancy.zip` | `.../Sildurs-Basic-2.7-Fancy.zip` |
| Read-only extract | `/tmp/sildurs/Sildurs-Vibrant-2.02-Extreme` | `/tmp/sildurs/Sildurs-Enhanced-Default-1.20-Fancy` | `/tmp/sildurs/Sildurs-Basic-2.7-Fancy` |

Initial source scan: legacy OptiFine/Iris-style shaders with `gl_FragData`,
`GL_ARB_shader_texture_lod`, `GL_EXT_gpu_shader4`, no image/SSBO/compute
declarations in the top-level programs. Likely blockers are translator/profile
compatibility, not new GPU features. Confirm by static compile and device import.

## iterationRP (P1)

| Field | Value |
| --- | --- |
| Original archive | `/home/polar/MobileGlues/references/iterationRP Alpha 0.8.31.zip` |
| SHA-256 | `334d0b0dadb5e2b927b992f2b93596c4cc1a518305c1e2a9b175c754a4d946e8` |
| License | Copyright 2025 Tahnass, All Rights Reserved; private modification allowed, redistribution not |
| Required Iris features | `CUSTOM_IMAGES`, `SSBO` |
| Known local derived fixtures | `iterationRP-X1E-port-0.8.31.zip` `5b061ae6…`; `-r2.zip` `46a0224b…` |
| Derived status | Research-only, not a support target |

The private ports are evidence for the size of the work, not a qualification
path. The original archive is the target.

## Source of truth

- Sildur metadata: Modrinth API project/version endpoints, captured 2026-10-20.
- iterationRP metadata: local archive `LICENSE.md`, `shaders.properties`, and
  `docs/mobileglues-googlebook/validation/2026-10-19-iterationrp-*.md`.
- Target tuple and device rules: root `AGENTS.md`.
