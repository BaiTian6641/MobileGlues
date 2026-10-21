# Shaderpack support program

Status: **device probes in progress. No pack is promoted above provisional S1;
S2/S3 remain NOT RUN.**

Current evidence:

- Sildur Vibrant: provisional S1 (world route, no compile errors).
- Sildur Enhanced Default: provisional S0; S1 not captured.
- Sildur Basic: provisional S1.
- iterationRP original: S0 PASS, S1 FAIL (red/green/orange terrain and black sky).
  The [G10 no-final guard](../validation/2026-10-20-g10-no-final-guard.md) has a
  recorded full-frame pass; remaining color/sky analysis is linked below.
- Bliss candidate: S0 FAIL (`deferred.fsh` uniform syntax error).
- Solas candidate: provisional S0; washed frame, S1 unverified.

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
| Candidate | Bliss 2.1.2 | Modrinth request; `#version 430 compatibility` compute/shadowcomp plus legacy graphics. |
| Candidate | Solas 3.0 | Modrinth request; `#version 430 compatibility` shadow compute plus `#version 130` graphics. |

Sildur Vibrant S0 is recorded; S1/S2 remain open. Bliss and Solas are not
committed support targets until their static audits and device gates complete.

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
| [audits/2026-10-20-sildur-static-audit.md](audits/2026-10-20-sildur-static-audit.md) | Sildur legacy/format/FBO findings and first probes |
| [audits/2026-10-20-iterationrp-static-audit.md](audits/2026-10-20-iterationrp-static-audit.md) | iterationRP format/stage/program findings |
| [audits/2026-10-20-iterationrp-color-sky-analysis.md](audits/2026-10-20-iterationrp-color-sky-analysis.md) | stronger red/green, black-sky, GS and replay analysis |
| [audits/2026-10-20-iterationrp-color-sky-analysis.md](audits/2026-10-20-iterationrp-color-sky-analysis.md) | Current color/sky source review, offline evidence, corrected exclusions and next diagnostic boundaries |
| [audits/2026-10-20-bliss-static-audit.md](audits/2026-10-20-bliss-static-audit.md) | Bliss default/LPV risks |
| [audits/2026-10-20-solas-static-audit.md](audits/2026-10-20-solas-static-audit.md) | Solas image/compute risks |
| [bliss-solas-ledger.md](bliss-solas-ledger.md) | Candidate Bliss/Solas gates |
| [../backend-support-fixing-plan.md](../backend-support-fixing-plan.md) | Shared backend fix order (G1-G9) |

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
