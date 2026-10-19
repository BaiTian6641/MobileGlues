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

## Rules

- Every tuple defaults to `UNQUALIFIED` until the named device/runtime/pack/profile has retained
  evidence.
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
