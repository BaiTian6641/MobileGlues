# Distribution locks

Versioned, machine-readable inputs for the Googlebook distribution. These files describe the
intended component tuples and their qualification state. They are not installed-artifact
inventories and they do not create support claims.

| File | Contents |
| --- | --- |
| `runtime-lock.json` | Runtime lanes, Java/loader/Iris/Sodium pins, renderer lineage, provider policy. |
| `launcher-lock.json` | Primary launcher integration and the unsupported candidate. |
| `shaderpacks.json` | Original pack identities, profiles, resource requirements and qualification state. |
| `device-profiles/` | Per-model observed identity, provider report, hard gates and evidence references. |

## Lanes

| Lane | Tuple | State |
| --- | --- | --- |
| `current` | Minecraft 1.21.1 / Java 21 `java-runtime-delta` / NeoForge 21.1.262 / Iris 1.8.14-beta.1 / Sodium 0.8.13, all NeoForge | Delivery lane, `STAGED / WORKING` on 21.1.262 (world load, render, save/quit); effect, performance and lifecycle gates remain open. NeoForge 21.1.264 is an optional future patch, not a blocker |
| `current` stable fallback | Iris 1.8.12 (`t3ruzodq`) + Sodium 0.6.13 (`Pb3OXVqC`), NeoForge 1.21.1 | `NOT RUN`. Iris 1.8.12 declares Sodium 0.6.13; Sodium 0.8.13 is not its declared pairing |
| `future` | Minecraft 26.3 / Java 25 / Fabric / Iris 1.11.7 / Sodium 0.9.2 | `NOT RUN`; experimental, not a delivery lane |
| `legacy` | Minecraft 1.20.1 / Java 17 / Fabric / Iris 1.7.6 / Sodium 0.5.12-beta.2 | `NOT RUN` |
| `optional_comparison` | Minecraft 1.21.11 / Java 21 / Iris 1.10.8 / Sodium 0.8.14 | `NOT RUN` |

## Rules

- Every tuple defaults to `UNQUALIFIED` until the named device/runtime/pack/profile has retained
  evidence.
- A lane `PARTIAL` status means some required sub-gates remain open. Do not promote a pack to a
  release claim from a staged scene, and do not treat an optional future patch as a blocker.
  Optional/future profiles stay outside the claim while their measured status is still recorded.
- Test statuses are `PASS`, `FAIL`, `BLOCKED`, `NOT RUN` and `NOT APPLICABLE`. Evidence records
  also use `STAGED` for a bounded scene that is not the full effect matrix, `PARTIAL` for a mix of
  passed and open sub-gates, `OBSERVED` for recorded identity, and `REPRODUCED` for a rebuilt
  source/artifact. `UNQUALIFIED` is a support classification, not a test result.
- Hashes identify publisher artifacts. Original shaderpack ZIPs stay byte-identical; only hashes
  and acquisition metadata belong in Git.
- A provider change starts a fresh graphics process and a new cache namespace. Record the
  observed provider, not only the requested one.
- Update locks in a focused commit with provenance and bump `updated`. Do not let an automatic
  updater move a pinned lane independently.
- The connected reference unit is an HP Googlebook 14. The XPS Googlebook specification is a
  separate model record until an exact XPS unit is measured.
