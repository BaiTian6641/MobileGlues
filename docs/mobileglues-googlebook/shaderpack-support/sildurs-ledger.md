# Sildur's series adoption ledger

Status: **investigation. No support claim.**
Targets: Vibrant 2.02 Extreme, Enhanced Default 1.20 Fancy, Basic 2.7 Fancy.
All are All-Rights-Reserved local fixtures.

| ID | Task | Status | Evidence / gate |
| --- | --- | --- | --- |
| SILD-00 | Lock versions, licenses, hashes, local fixtures | DONE | `targets.md`; Modrinth SHA-512 verified |
| SILD-01 | Static source/feature/GLSL-version audit for all three | IN PROGRESS | delegated read-only audit |
| SILD-02 | Host translation/compile matrix for each top-level program | NOT RUN | needs host harness output, not device |
| SILD-03 | Device S0 import and truthful diagnostics, one pack at a time | NOT RUN | Vibrant first |
| SILD-04 | Device S1 pipeline route (world/sky/water/hand/HUD) | NOT RUN | 60 s route |
| SILD-05 | S2 named-effect matrix | NOT RUN | effects in `goals.md` |
| SILD-06 | S3 lifecycle/performance/thermal | NOT RUN | fixed profile, 20 min |
| SILD-07 | Enhanced Default and Basic regression after Vibrant fixes | NOT RUN | no Vibrant regression |

## Initial hypothesis

The source scan shows legacy OptiFine/Iris programs (`gl_FragData`,
`GL_ARB_shader_texture_lod`, `GL_EXT_gpu_shader4`) and no top-level
image/SSBO/compute declarations. The likely failure class is translator or
program-link compatibility, not a missing advanced GPU feature. This is a
hypothesis; the static audit must confirm it.

## Promotion rule

Vibrant cannot be called supported until S2; the series cannot be called
supported until all three targets have at least S2 and no Vibrant regression.
