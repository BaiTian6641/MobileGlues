# FSR3 execution records

Continuation records for built-in FSR3.1 temporal upscaling in the MobileGlues Googlebook
distribution.

| File | Purpose |
| --- | --- |
| `frame-contract.md` | Frozen ABI v1 between the Minecraft/Iris adapter and MobileGlues |
| `task-ledger.md` | Gates, ownership, status and blockers |
| `execution-state.md` | Exact repository/device provenance and current next action |
| `decisions.md` | Architecture decisions with status and evidence |
| `audits/` | Worker source/runtime audit handoffs |
| `validation/` | Device and host evidence (created when runs exist) |

The [20 October integration review](audits/2026-10-20-integration-review.md)
assesses both renderer-only and hybrid bridge paths, reconciled through source
revision `51b3eaa` and records at `87e289d`, with focused host reproductions of
resource-binding and initialization-state defects.

The research baseline is `docs/MobileGlues_FSR3_Research_and_Implementation_Plan.md`. Do not
create a second plan or a second status table. Update these files instead.
