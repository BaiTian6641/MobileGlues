# FSR3 execution records

Continuation records for built-in FSR3.1 temporal upscaling in the MobileGlues Googlebook
distribution.

| File | Purpose |
| --- | --- |
| `frame-contract.md` | Frozen ABI v1 between the Minecraft/Iris adapter and MobileGlues |
| `task-ledger.md` | Gates, ownership, status and blockers |
| `execution-state.md` | Exact repository/device provenance and current next action |
| `decisions.md` | Architecture decisions with status and evidence |
| `audits/` | Worker source/runtime audit handoffs; `stable-profile.md` records the temporary history-off policy |
| `validation/` | Device and host evidence (created when runs exist) |

The immediate next phase is
[scaled preset overlay correction](scaled-presets-prompts.md), starting with
Experiment A and the confirmed NativeAA baseline. Its
[diagnostic contract](design/scaled-presets-diagnostic-contract.md) specifies
draw-time traces and separate gates for Quality, Balanced, Performance and
Ultra Performance. Keep history off and motion zero during this phase.

For the broader temporal integration roadmap, use the
[handoff](agent-handoff.md) and paste the
[main implementation prompt](agent-prompts.md#main-implementation-prompt).
The [frame-generation rollout](design/framegen-experimental-rollout.md)
records the present-path and CPU-assistance gates; it is not a capability claim.

The [follow-up review](audits/2026-10-20-followup-review.md) addresses the
reported menu/HUD scaling and world flicker, with shaders required.
[Implementation notes](implementation-notes.md) include code examples and
acceptance checks; [primary references](references.md) records the web
research. These are supporting instructions, not another execution ledger
or a claim that the integration is complete.

The [20 October integration review](audits/2026-10-20-integration-review.md)
assesses both renderer-only and hybrid bridge paths, reconciled through source
revision `51b3eaa` and records at `87e289d`, with focused host reproductions of
resource-binding and initialization-state defects.

The research baseline is `docs/MobileGlues_FSR3_Research_and_Implementation_Plan.md`. Do not
create a second plan or a second status table. Update these files instead.
