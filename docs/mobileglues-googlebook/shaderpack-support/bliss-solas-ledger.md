# Bliss and Solas candidate ledger

Status: **static audits complete; device gates NOT RUN.**
These are candidate additions to the shaderpack support program.

| ID | Task | Status | Evidence / gate |
| --- | --- | --- | --- |
| BLS-00 | Pin Bliss 2.1.2 hash/license/fixture | DONE | `targets.md` |
| BLS-01 | Static Bliss audit | DONE | `audits/2026-10-20-bliss-static-audit.md` |
| BLS-02 | Host format/varying and Iris-transform probes | NOT RUN | RGBA16 mixed FBO, varying capacity, core chain |
| BLS-03 | Device S0/S1 default profile | **FAIL** | `deferred.fsh: ERROR: 0:948: uniform syntax error`; `validation/2026-10-20-bliss-s0-fail.md` |
| BLS-04 | Optional LPV lane | NOT RUN | 1-D image blocker |
| SOL-00 | Pin Solas 3.0 hash/license/fixture | DONE | `targets.md` |
| SOL-01 | Static Solas audit | DONE | `audits/2026-10-20-solas-static-audit.md` |
| SOL-02 | Host shadowcomp/shadow image probes | NOT RUN | distinct units, vertex stage |
| SOL-03 | Device S0/S1 default profile | NOT RUN | default includes VX compute/images |
| SOL-04 | End/PBR/DH/advanced lanes | NOT RUN | separate gates |

Neither pack is promoted to a committed support target by this ledger. Bliss
default may be graphics-only; Solas default is predicted to need G1/G2/G4.
