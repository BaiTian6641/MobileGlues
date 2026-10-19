# Googlebook port documentation

Research date: 19 October 2026. Target: native Android ARM64 on Snapdragon X1E and X2E
Googlebooks. Device execution, performance qualification and retail X2E qualification remain
work to perform.

This directory separates maintained requirements from captured evidence. A support claim is
valid only when it names the exact device, runtime, renderer, provider and shaderpack tuple and
points to retained evidence.

The delivery lane is Minecraft 1.21.1 on NeoForge 21.1.262 with Java 21, Iris
1.8.14-beta.1 NeoForge and Sodium 0.8.13 NeoForge. NeoForge 21.1.264 is an optional future
patch, not a delivery target or blocker. Ordinary shaderpack ZIPs must import, select their own
preset and render their enabled effects in that lane. Standard desktop OpenGL and
ordinary shaderpack behavior are the primary goal; performance is secondary. Minecraft 26.3 /
Java 25 is future/experimental; Minecraft 1.20.1 / Java 17 Fabric is legacy. No delivery-lane
tuple is qualified yet.

The required advanced X1E coverage is Very High with ACT 192 plus the custom WSR 128 profile
(ACT 128, world-space reflections on). Ultra ACT 256/WSR 256 is optional/future X2E work: it is
`BLOCKED` on X1E by the provider's 134,217,728-byte shader-storage-block limit and is never
claimed as supported. ACT 512 is an optional stress result.

Capability coverage means a truthful desktop GL contract for ordinary packs: a core version
MobileGlues actually implements plus individually earned extensions, covering GLSL translation,
framebuffer/target routing, image/SSBO resource mapping, compute and barrier/visibility
semantics. Unimplemented features remain named compatibility deviations rather than claims; this
is not a promise of full OpenGL 4.6 conformance.

## Authority order

1. `01_MobileGlues_Googlebook_Architecture.md` defines product scope, architecture, provider
   strategy, the OpenGL contract and the release definition.
2. `02_MobileGlues_Googlebook_Implementation_Plan.md` defines stages, tasks, dependencies and
   exit gates.
3. `03_MobileGlues_Googlebook_Validation_and_Sources.md` defines the lock proposal, test
   corpus, acceptance methods, source register and support boundaries.
4. `mobileglues-googlebook/` contains execution records and retained evidence.
5. `../compat/` contains machine-readable runtime, launcher, shaderpack and device-profile
   locks.

If a measured fact in an evidence artifact contradicts the specification, correct the
specification and retain the evidence. Never relabel or delete evidence to make a gate pass.

## Document map

| Path | Role | State |
| --- | --- | --- |
| [01_MobileGlues_Googlebook_Architecture.md](01_MobileGlues_Googlebook_Architecture.md) | Product and architecture requirements | Authoritative |
| [02_MobileGlues_Googlebook_Implementation_Plan.md](02_MobileGlues_Googlebook_Implementation_Plan.md) | S0-S9 execution plan and gates | Authoritative |
| [03_MobileGlues_Googlebook_Validation_and_Sources.md](03_MobileGlues_Googlebook_Validation_and_Sources.md) | Locks, acceptance methods and sources | Authoritative for method |
| [mobileglues-googlebook/](mobileglues-googlebook/) | Execution state, decisions, feature matrix and validation artifacts | Evidence only |
| [../compat/](../compat/) | Runtime, launcher, shaderpack and device-profile locks | Proposed until qualified |
| [MobileGlues_Googlebook_X1E_Optimization_Plan.md](MobileGlues_Googlebook_X1E_Optimization_Plan.md) | Historical X1E plan | Superseded |

## Stage scheme

The maintained plan uses `S0`-`S9`. Runtime-lane results stay separate:

- `S1-legacy`: Minecraft 1.20.1 / Java 17 / Fabric / Iris / Sodium.
- `S1-current`: the delivery lane, Minecraft 1.21.1 / Java 21 / NeoForge 21.1.262 / Iris
  1.8.14-beta.1 NeoForge / Sodium 0.8.13 NeoForge. The installed 21.1.262 tuple is staged/working
  (world load, render, save/quit); effect fidelity, performance and lifecycle remain open.
  NeoForge 21.1.264 is an optional future patch, not a gate.
- `S1-provider`: below-wrapper and in-process EGL/GLES provider evidence.

Minecraft 26.3 / Java 25 / LWJGL 3.4.3 on Fabric is a future/experimental lane, not the
current requirement; `S1-legacy` remains the 1.20.1 Fabric lane.

Aggregate S1 is `PARTIAL/BLOCKED` while a required lane has no passing real-device startup.
Older execution records use `Stage 0`-`Stage 8`; the approximate mapping is in
`mobileglues-googlebook/README.md`. Do not convert old evidence to a new stage result without
checking its exact lane, provider and pack tuple.

## Status vocabulary

Device and test results use exactly `PASS`, `FAIL`, `BLOCKED`, `NOT RUN` or
`NOT APPLICABLE`, with a reason for the latter three. `NOT APPLICABLE` is reserved for
requirements that genuinely do not apply; a measured provider limit on an optional profile stays
`BLOCKED` with an explicit optional/future scope so the limit is not hidden.

Support classifications are:

- **Certified**: the complete named device/runtime/pack/profile tuple passed.
- **Compatible but performance-limited**: fidelity passed, measured performance is outside the
  preferred target.
- **Experimental**: particular features or runtime combinations remain under test.
- **Unsupported**: a known blocker prevents the requested configuration.
- **Unqualified**: insufficient evidence; no pass or fail claim.

A compile, a screenshot or an average frame rate alone does not establish support. A pack with
an intended effect disabled is a failed test for that effect.

## Lock and evidence rules

- Original shaderpack ZIPs stay byte-identical. Only hashes, acquisition metadata and test
  results belong in Git; archives stay in `/home/polar/MobileGlues-fixtures/`.
- The connected X1E unit is an HP Googlebook 14 (`quartz`, 2880 x 1800). The XPS Googlebook
  (2560 x 1600) is the official reference specification, not an identity for the HP unit.
- Provider selection is recorded as observed, not requested. A provider change starts a fresh
  graphics process and a new cache namespace.
- Performance objectives remain planning targets until measured on the named tuple.
