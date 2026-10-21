# Googlebook and FSR branch integration audit

Audit date: 2026-10-21. The delivery branch is
`codex/desktop-gl-universal-fsr`. The native output-extent guard was reviewed at
`3b0e32dd5f299e1c6c67416b5d93b2913e0422ee`; branch comparisons below use the
same source history. Remote-tracking references were the coordinator's fetched
references. No branches were merged, reset, deleted, or force-pushed by this
audit. Existing user ignore rules were not edited.

The local `main`, backend/core work and desktop-GL handoff are already in the
delivery ancestry. Two unmerged branch labels contain work integrated by squash
commits. The FP16 branch contains a real, unqualified experiment that remains
separate from delivery. One omitted dispatch reproduction step is restored in
`tools/fsr3_spike/reproduce.sh` alongside this report.

## Reference inventory

| Reference | Audited tip | Result |
| --- | --- | --- |
| `main` | `d22b1878b6e8` | Ancestor of delivery; all seven commits ahead of remote `main` are included |
| `origin/main`, `upstream/main` | `97558a69157b` | Ancestors of delivery |
| `googlebook/fsr-backend-s2` | `355d797eeaf6` | Ancestor of delivery |
| `googlebook/fsr-core-s1` | `857fe6a5fa18` | Ancestor of delivery |
| `googlebook/x1e-stage1-3` | `d1b97d359f73` | Ancestor of delivery |
| `origin/googlebook/x1e-stage1-3` | `355d797eeaf6` | Older remote label; its contents are included |
| `googlebook/fsr-dispatch-s2` | `249c0bd81e7d` | Four commits are not ancestors or individual patch-ID matches; substantive work is squash-integrated by `8182399` |
| `googlebook/fsr-game-audit` | `27b37cdda3b4` | Not an ancestor or patch-ID match; the complete audit is squash-integrated by `3c5f98e` |
| Local and remote `googlebook/x1e-fp16-precision` | `64fc7ba30c68` | Four commits remain separate; source, probe and design differences described below |

The method combined `git merge-base --is-ancestor`, `git rev-list`,
`git cherry -v`, and per-file Git blob comparisons. A plus sign from
`git cherry` alone does not establish that a feature is missing: a squash that
also changes coordination documents has a different patch ID.

## Squash integration and the reproduction omission

The dispatch branch's harness, build/generation scripts, dispatch report, and
three validation JSON files are byte-identical in delivery. Its four original
commits are `0524d56`, `d77dd8c`, `ae2b4c3`, and `249c0bd`; delivery commit
`8182399a1d5b27dd5e2697a57ee6901c37333eb8` integrates those files and updates
the FSR execution records.

The one substantive omission is the top-level `reproduce.sh` block that builds
and runs two RCAS cases and four full-chain cases before building the Android
dispatch harness. The restored block retains that behavior and explicitly
passes the caller's `SPIKE` directory into shader generation. It does not invoke
the device runner or copy anything to a device.

The game audit file `docs/fsr3/audits/minecraft-iris-pass-input.md` is
byte-identical to `27b37cd`. Delivery commit
`3c5f98eca2291670173aa390d1c0fcb598a6d7df` includes it and the related decision,
execution-state and ledger updates. There is no missing audit text to merge.

## FP16 experiment retained separately

The branch base is `355d797eeaf63d1ddfc8d2199671b061d9c6462f`; its retained tip
is `64fc7ba30c684523f1dac67d606579b0585f4e1b`. The exact branch remains available
at [the pinned experiment commit](https://github.com/BaiTian6641/MobileGlues/tree/64fc7ba30c684523f1dac67d606579b0585f4e1b).

Its four commits contain:

- `aad1b64`: an ALU timing probe, Android build script and precision design note.
- `420ce4a`: removal of the generated probe binary and an ignore rule.
- `3d99b6d`: precision-preserving ESSL output changes, explicit SPIRV-Cross highp
  defaults, cache-schema change and updated design notes.
- `64fc7ba`: removal of unrelated ignore changes from the experiment.

The probe source/build script and `fp16-precision-path.md` are absent from the
delivery tree. The ESSL precision change is also absent. Cache schema 5 is
already present independently; its matching value does not prove integration
of the precision policy. The existing performance-model report retains the
earlier FP16 timing discussion, but not the branch's added Radeon comparison.

Merging the renderer change as a correctness fix is not justified. Desktop
GLSL precision qualifiers have no effect on storage or arithmetic precision;
the specification states that they have "no semantic meaning".
[GLSL 4.60, section 4.7.2](https://registry.khronos.org/OpenGL/specs/gl/GLSLangSpec.4.60.html#precision-qualifiers)
Giving those qualifiers ESSL `mediump` behavior can change the numerical result
and therefore requires an explicit, separately qualified optimization policy.
The architecture baseline requires a correct reference before speculative
shader arithmetic changes.

The branch itself labels the work a design/probe gate and leaves G2 through G4
open. Its small timing probe is insufficient as a delivery gate: it does not
check EGL setup, returns success after shader compile/link failures, records no
provider/OS identity and performs no numerical readback. Its constants near one
can round away at half precision, so its timing does not by itself establish
equivalent useful work. No committed shaderpack A/B or focused precision
semantic tests close these gaps.

An isolated archival patch was prepared from the base-to-tip delta, excluding
the user's `.gitignore`; it was not applied to renderer source. Its SHA-256 is
`7d8d88f63f3953a2ae3bccb232a05530646fcdf29fa9932510d67a487efd1d9f`.
The pinned branch retains the complete original history, including its ignore
rule. A future bounded task should separate desktop GLSL and authored ESSL
semantics, add an opt-in policy/cache identity, validate numerical output, then
run visual and performance comparisons before promoting the experiment.

## Worktree residue

Branch ancestry does not include uncommitted files. Read-only inspection found
the core, dispatch, game-audit and stable worktrees clean. The backend worktree
contains 17 untracked audit/tool files: 15 are byte-identical to delivery, while
delivery's backend report has later qualification text and its Android probe
build has the required static C++ runtime flag.

The detached depth-port worktree retains staged edits. Its depth-filter
implementation, header and test are byte-identical to delivery and were
integrated by `609ff8e`; surrounding integration files have since evolved.
This audit did not discard those staged files or assume that a dirty worktree
can be removed merely because its main implementation is present.

## Validation boundary

The restored reproducer passes `bash -n`. Its dispatch harness builds on the
host, and the exact two RCAS/four chain command variants all return success with
valid JSON on the host GLES provider. They use the existing generated shaders.
The provider identifies as Mesa OpenGL ES 3.2, Mesa 26.0.0-devel, llvmpipe
(LLVM 20.1.8, 256 bits). This is software-rendered host evidence.
The complete SDK regeneration/top-level reproducer was not rerun. No new device
execution or performance claim follows from these host checks.

The separately reviewed native extent guard passes the focused viewport test
with strict warnings and AddressSanitizer/UndefinedBehaviorSanitizer. The scoped
native audit parses nine Android translation units with 25 observed diagnostics
and zero new diagnostics against its 26-diagnostic baseline. Its rejection path restores framebuffer,
viewport, scissor and color-mask state before returning, and it rejects a resize
mismatch before allocating or dispatching. This does not qualify every launcher
resolution policy: a main target deliberately smaller than the EGL output is
rejected by the current bridge contract. At this audit checkpoint, the native-target
branch that expanded partial viewports remained separate review debt; the pure
viewport helper test did not cover that production branch.

That debt was subsequently corrected by
[`e3ab06c`](https://github.com/BaiTian6641/MobileGlues/commit/e3ab06c9b61bb97c52bc01816698f86852251d0b).
The new regression links the actual `FSR3Direct.cpp`: the pre-fix translation
unit fails five cases, while the corrected source passes the same checks and
ASan/UBSan. It preserves partial/offset viewport and scissor rectangles, including
atlas targets. The later nine-translation-unit static audit reports 25 existing
diagnostics and zero new diagnostics. These remain host/static results; see the
[latest runtime and artifact record](2026-10-21-fsr3-depth-contract-and-runtime-status.md)
for the separate 2.0.7 failure, 2.0.8 full-library pass and remaining artifact/game gates.

Later delivery commits preserve the separation of semantic fixes and tooling:
[`9549931`](https://github.com/BaiTian6641/MobileGlues/commit/95499319f3081e51ffc4da1267b0c78eb76f8432)
corrects three buffer entry points' application errors;
[`c02565e`](https://github.com/BaiTian6641/MobileGlues/commit/c02565ef034fc798686271f053a5e25c67832472)
adds the durable full-library probe and five-case suite; and
[`ab4d31d`](https://github.com/BaiTian6641/MobileGlues/commit/ab4d31db0aa849cb5bbd3446cee91d60a71ef793)
versions the candidate as 2.0.8-dev. Independent review and 187 focused
ASan/UBSan checks passed. The old-source bounded comparison failed 38 assertions;
the physical system provider and corrected 2.0.8 full library each passed all
five expanded cases in separate runs. Native [CI 39578250534](https://github.com/BaiTian6641/MobileGlues/actions/runs/39578250534)
subsequently succeeded at exact `ab4d31d`, and artifact `12335463401` was
independently verified. The runtime record distinguishes its stripped payloads
from the local unstripped core used physically. A subsequent separate run also
passed all five cases using the exact extracted CI ARM64 payload, with its own
core-bound evidence. Game rendering and the pending
bridge lifecycle correction remain unqualified. These updates do not
promote the separate FP16 experiment or close the broader effect/performance
and lifecycle gates.
