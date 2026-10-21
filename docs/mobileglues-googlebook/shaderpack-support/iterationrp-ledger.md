# iterationRP adoption ledger

Status: **investigation. No support claim.**
Target: original `iterationRP Alpha 0.8.31.zip`
(`334d0b0d…`). Private X1E ports are research evidence only.

| ID | Task | Status | Evidence / gate |
| --- | --- | --- | --- |
| ITRP-00 | Lock original archive, license, local path | DONE | `targets.md` |
| ITRP-01 | Static feature/API map against current backend | DONE | [static audit](audits/2026-10-20-iterationrp-static-audit.md) |
| ITRP-02 | Shared legal image-format table (G1) | PARTIAL | RG16F/RG32F lowering + host tests; target-aware attachments/units/stages open; `validation/2026-10-20-g1-rg16f-rg32f-lowering.md` |
| ITRP-03 | Core-only deterministic image units (G2) | PROOF GAP | core resolver exists; no original-pack no-bridge run |
| ITRP-04 | SSBO clear reachability (G3) | NOT A DEFAULT BLOCKER | 4 KiB SSBO fully written by SSBOUpdate_CS |
| ITRP-05 | Graphics-stage image rewriting (G4) | NOT RUN | `PixelData_VS`, `shadow.gsh` |
| ITRP-06 | Device S0/S1 on default profile | **S0 PASS; S1 FAIL** | original TAA: terrain fixed by baseline side; sky still black; `audits/2026-10-20-iterationrp-color-sky-analysis.md` |
| ITRP-07 | S2 effect matrix on default profile | NOT RUN | no missing effects |
| ITRP-08 | Advanced ACT/WSR/RTW profiles | NOT RUN | separate profile gates |
| ITRP-09 | Final/composite sampling and ping-pong identity audit | IN PROGRESS | periodic reset observed; `IterationRPFinalSamplingAudit` |
| ITRP-10 | Remaining image/sampler/barrier contract audit | IN PROGRESS | `IterationRPImageContractAudit` |
| ITRP-11 | Atmosphere LUT sampler3D consumption audit | IN PROGRESS | `IterationRPAtmosphereAudit` |
| ITRP-12 | Black-sky presentation/sampling audit | IN PROGRESS | day/clear still unlit albedo + black sky; final write/side leading; `IterationRPSkyAudit` |
| ITRP-13 | Missing light/shadow effect audit | IN PROGRESS | `IterationRPShadowAudit` |
| ITRP-14 | Sky/LUT/shadow runtime probe design | IN PROGRESS | `IterationRPSamplerProbeDesign` |

## Promotion rule

The [2026-10-20 detailed color/sky review](audits/2026-10-20-iterationrp-color-sky-analysis.md)
adds offline evidence for a terrain geometry-output lifetime hazard and a core
image-replay collision, while keeping their device relevance unproven. It also
corrects the earlier exposure exclusion: manual exposure still uses
`pixelData2D`. The first target observations remain depthtexS and per-pass
sampler/image/FBO identities. No row is promoted by these offline checks.

The private `iterationRP-X1E-port-0.8.31*.zip` results do not close any row.
A row passes only with the original archive hash, the exact core/bridge pair,
and the named gate. Advanced profile work is separate from the default-profile
claim.

## Primary risks

The default graph has 15 active illegal storage-image qualifier sites
(9x `rgba16`, 1x `rg16`, 5x `rg16f`). `img_pixelData2D` is `rg16f` in five
programs, including vertex `PixelData_VS`. `shadow.gsh` declares three
`uimage3D`s and performs `imageAtomicMax`.

The first two blockers are therefore **G1** (legal format/binding/backing) and
**G4** (stage-scoped graphics image rewriting). **G2** is a proof gap: the core
resolver exists, but there is no original-pack no-bridge unit log. **G3** is not
a default blocker because `SSBOUpdate_CS` writes the whole 4 KiB SSBO.

The private ports widen formats and rewrite subgroup/BackwardAnalysis/SH
semantics. They mask the original defects and are not correctness references.
The base-port hash record also conflicts:

- current ZIP: `5b061ae6ae8ef2c0d278ca9458f37747c6b73898e4572a91e0e00433977b78b4`
- earlier validation record: `0cdd9259acb75fc311a7fdc7d5d927448ccfcd325c9a2fd59430e1b167f72921`
- r2 ZIP: `46a0224b664fbbe56e22e240cff960f83373c0e864157e1da9af0dae9f50461e`

Do not add a second backend or rewrite the original pack to pass a gate.
