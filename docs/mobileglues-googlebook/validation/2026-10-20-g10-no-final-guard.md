# G10 no-final pack guard

Date: 2026-10-20
Device: C1L63708PZ, X1E80100 / Adreno X1-85, ANGLE Vulkan 1.4.295.
Pack: original iterationRP Alpha 0.8.31
(`334d0b0d…`).
Installed core: `c741b74d…`; bridge: `cf29e0b8…` (G10 guard).

## Change

Iris's no-final path copies colortex0 into the native main target. A scaled
source cannot be consumed by that copy. The bridge now detects
`programSet.get(ProgramId.Final)` at pipeline-constructor HEAD, before
`RenderTargets`, and disables scaling/submission for packs without a final
pass.

## Result

**PASS.** With the diagnostic image bridge removed, iterationRP renders a
full native frame under the scaled FSR preset. The lower-left 960x528-only
frame is gone. Color/lighting remains wrong and is a separate image-contract
issue.

Screenshot: `/tmp/itrp-g10-full.png` at capture time; SHA-256 `bb65153312686d94a8deed3deffa0b08c5748019e08d4c09fbfddfe514d928d8`.

No support promotion: S1 still fails on color/lighting.
