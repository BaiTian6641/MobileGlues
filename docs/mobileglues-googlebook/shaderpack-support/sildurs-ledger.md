# Sildur's series adoption ledger

Status: **investigation. No support claim.**
Targets: Vibrant 2.02 Extreme, Enhanced Default 1.20 Fancy, Basic 2.7 Fancy.
All are All-Rights-Reserved local fixtures.

| ID | Task | Status | Evidence / gate |
| --- | --- | --- | --- |
| SILD-00 | Lock versions, licenses, hashes, local fixtures | DONE | `targets.md`; Modrinth SHA-512 verified |
| SILD-01 | Static source/feature/GLSL-version audit for all three | DONE | [static audit](audits/2026-10-20-sildur-static-audit.md) |
| SILD-02A | Host FBO/format probe: RGB8, RGB10_A2, RGB16F, R11F, RGBA8 | NOT RUN | prerequisite for S0/S1 |
| SILD-02 | Host translation/compile matrix for each top-level program | NOT RUN | needs host harness output, not device |
| SILD-03 | Device S0 import and truthful diagnostics, one pack at a time | NOT RUN | Vibrant first; Basic is the fallback control |
| SILD-04 | Device S1 pipeline route (world/sky/water/hand/HUD) | NOT RUN | 60 s route |
| SILD-05 | S2 named-effect matrix | NOT RUN | effects in `goals.md` |
| SILD-06 | S3 lifecycle/performance/thermal | NOT RUN | fixed profile, 20 min |
| SILD-07 | Enhanced Default and Basic regression after Vibrant fixes | NOT RUN | no Vibrant regression |

## Audit conclusion

All top-level programs are `#version 120` vertex/fragment pairs with no
compute, geometry, tessellation, image, SSBO, atomic, or subgroup dependency.
Iris 1.8.x normalizes them to `#version 330 core` before MobileGlues, so raw
legacy tokens are not the target-path blocker.

The first risks are render-target formats:

- Vibrant requests `RGB8` for colortex1/3, plus RGB16F and R11F formats.
- Enhanced requests `RGB10_A2`.
- Basic is all RGBA8 and is the lowest-risk control.

RGB8 has no X1E color-renderability evidence; RGB8_SNORM already failed FBO
completeness. Host F0 must settle RGB8/RGB10_A2 before device import.

`SILD-02` probes start with Vibrant `gbuffers_textured`, `composite1`, then
`deferred`, using the actual Iris-preprocessed/patched source. Raw-120 compiles
are diagnostic only. No support claim follows from this audit.

## Promotion rule

Vibrant cannot be called supported until S2; the series cannot be called
supported until all three targets have at least S2 and no Vibrant regression.
