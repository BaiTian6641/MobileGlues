# Shaderpack support program

Status: **planning and investigation. No new pack is supported yet.**

This program tracks adoption of original shaderpack archives on the X1E
Googlebook tuple. The fixed support boundary is **(MobileGlues core, Iris
bridge, original archive bytes)**. Pack edits, private derivatives, and hidden
fallbacks are evidence only and never qualify an original pack.

## Priorities

| Priority | Pack | Why |
| ---: | --- | --- |
| P0 | Sildur's Vibrant Shaders 2.02 Extreme | Expected simplest original-pack lane; no compute/image dependencies found in the initial scan. |
| P0b | Sildur's Enhanced Default 1.20 Fancy | Shares the Sildur legacy GLSL path; regression/coverage for the same fixes. |
| P0c | Sildur's Basic 2.7 Fancy | Lightweight control case; must not regress while fixing Vibrant. |
| P1 | iterationRP Alpha 0.8.31 | Hardest lane: storage images, subgroup assumptions, SSBO/WSR/ACT, private ports. |

Do not start iterationRP format work before the shared format/unit contract in
`../backend-support-fixing-plan.md` is implemented. Sildur work can run in
parallel at the host-translation and import-gate level.

## Documents

| File | Purpose |
| --- | --- |
| [goals.md](goals.md) | Support levels, claim rules, non-goals, exit criteria |
| [targets.md](targets.md) | Exact archives, versions, licenses, hashes, and provenance |
| [sildurs-ledger.md](sildurs-ledger.md) | Sildur task ledger and gates |
| [iterationrp-ledger.md](iterationrp-ledger.md) | iterationRP task ledger and gates |
| [gate-definitions.md](gate-definitions.md) | Reusable S0-S3 evidence and acceptance rules |
| [../backend-support-fixing-plan.md](../backend-support-fixing-plan.md) | Shared backend fix order (G1-G8) |

## Reporting rule

Every result names:

- installed core hash;
- installed bridge hash;
- pack name, version, and SHA-256;
- Minecraft/NeoForge/Iris/ANGLE tuple;
- profile and options;
- observed result, screenshot/log reference, and gate status.

No result is promoted from host compile to device support, or from a private
derivative to original-pack support.
