# iterationRP adoption ledger

Status: **investigation. No support claim.**
Target: original `iterationRP Alpha 0.8.31.zip`
(`334d0b0d…`). Private X1E ports are research evidence only.

| ID | Task | Status | Evidence / gate |
| --- | --- | --- | --- |
| ITRP-00 | Lock original archive, license, local path | DONE | `targets.md` |
| ITRP-01 | Static feature/API map against current backend | IN PROGRESS | delegated audit; existing iterationRP docs |
| ITRP-02 | Shared legal image-format table (G1) | BLOCKED ON BACKEND | `../backend-support-fixing-plan.md` G1 |
| ITRP-03 | Core-only deterministic image units (G2) | NOT RUN | no bridge dependency |
| ITRP-04 | SSBO clear reachability (G3) | NOT RUN | original archive, no R8 fallback |
| ITRP-05 | Graphics-stage image rewriting (G4) | NOT RUN | vertex image programs |
| ITRP-06 | Device S0/S1 on default profile | NOT RUN | after G1-G4 |
| ITRP-07 | S2 effect matrix on default profile | NOT RUN | no missing effects |
| ITRP-08 | Advanced ACT/WSR/RTW profiles | NOT RUN | separate profile gates |

## Promotion rule

The private `iterationRP-X1E-port-0.8.31*.zip` results do not close any row.
A row passes only with the original archive hash, the exact core/bridge pair,
and the named gate. Advanced profile work is separate from the default-profile
claim.

## Primary risk

The pack requires custom images and SSBO, and its shader source assumes
desktop storage-image formats/subgroup behavior. Current known blockers are
tracked in `../backend-support-fixing-plan.md`; do not add a second backend or
rewrite the pack to make the gate pass.
