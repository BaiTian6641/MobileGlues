# MobileGlues for Googlebook implementation plan

Research date: 19 October 2026. This roadmap implements the requirements in `01_MobileGlues_Googlebook_Architecture.md`. Acceptance methods and source evidence are in `03_MobileGlues_Googlebook_Validation_and_Sources.md`.

The critical path is: reproduce the runtime and graphics stack, establish truthful capabilities, repair shader and resource semantics, validate complete effects, then optimize and release. Current Minecraft bootstrap and the GLES feasibility decision must happen early.

## 1. Workstreams and ownership

Use four coordinated workstreams. A small team can combine roles, but the responsibilities remain distinct.

| Workstream | Responsibility | Typical artifacts |
| --- | --- | --- |
| Graphics core | GL state, capabilities, resource formats, draw/dispatch paths, synchronization | Core patches and semantic GPU tests |
| Shader compiler | GLSL lowering, linking, reflection, source maps, binding plans and caches | Compiler regression corpus and translated-program records |
| Android integration | Launcher module, Java/native runtime, SDL/GLFW, EGL/provider ownership, input and lifecycle | Release APKs and installation/lifecycle tests |
| Qualification | Pack/version lock, reference scenes, image and temporal comparison, performance, X1/X2 profiles | Evidence reports, compatibility matrix and release decision |

The integrator owns common interfaces, dependencies and release claims. Parallel agents should work on isolated branches/worktrees and disjoint files where practical. They must not independently change capability policy, object ownership, or serialization formats.

Keep the existing upstream repository layout initially. Add narrowly scoped components in the relevant directories. Introduce new abstractions only to solve an identified ownership, contract, or testability problem.

### Proposed repository support files

These are proposed deliverables, not claims about files already present upstream.

- `docs/product-requirements.md`
- `docs/architecture.md`
- `docs/execution-state.md`
- `docs/decisions/`
- `compat/runtime-lock.json`
- `compat/shaderpacks.json`
- `compat/device-profiles/`
- `compat/feature-contracts.json`
- `tests/gl-contract/`
- `tests/shader-corpus/`
- `tests/scenes/`
- `tools/probe/`
- `tools/capture/`
- `tools/compare/`
- `tools/qualification/`

Keep large captures, proprietary game assets, and pack ZIPs out of source control unless redistribution is permitted. Store reproducible acquisition metadata and hashes. Use retained test artifacts for failures and release evidence.

## 2. Stage map

| Stage | Outcome | Dependencies | Parallel work |
| --- | --- | --- | --- |
| S0 | Pinned, reproducible source and runtime baselines | None | Hardware inventory, pack inspection, launcher review |
| S1 | Real Googlebook startup and early provider feasibility decision | S0 | Legacy/current runtime bootstrap; GLES/Zink comparison; early X2 probes |
| S2 | Truthful GL contract and reliable function routing | S0; hardware part of S1 | Compiler groundwork and format microtests |
| S3 | Correct shader/program/reflection/caching model | S2 interfaces | Resource implementation from S4 |
| S4 | Correct render targets, formats, buffers and execution ordering | S2; S3 resource mapping | S3 compiler corpus |
| S5 | Standard unmodified shaderpack fidelity | S1–S4 | Desktop lifecycle polish and additional pack triage |
| S6 | Advanced lighting and world-space reflections | S3–S5; feasibility decision | Additional compute-oriented pack |
| S7 | Measured X1E optimization | Correct S5/S6 reference | Release UX work |
| S8 | Independent X2E and additional OEM qualification | Early S1 probes; core fixes | X1 regression and release packaging |
| S9 | Fresh-install product and release qualification | Required S5–S8 gates | Documentation and upstream submissions |

Publish useful previews at S5 with explicit standard-effect scope. Reserve the complete 1.0 claim for the required advanced features, current/legacy runtime lanes, desktop behavior, and qualified X1E/X2E device profiles.

Track `S1-legacy`, `S1-current` and `S1-provider` separately. Aggregate S1 remains `PARTIAL/BLOCKED` while a required lane is blocked. Unaffected work may proceed through its explicitly satisfied lane dependencies; current-lane S5 and complete-product qualification require a passing `S1-current`.

## 3. S0 Freeze and reproduce

### Objective

Make every result attributable to an exact source, binary, driver, runtime and shaderpack configuration.

### Tasks

**S0.1 Record source identities.** Start from audited core `97558a69157b033b890958b1e49258c5b51a80c6` and inspect changes since that commit before choosing the implementation baseline. Independently reproduce the V2.0.0 plugin's pinned core `f56af31d3e490d95808b3adc4ace20234249a37a`. Keep release and development baselines separate.

**S0.2 Reproduce the build.** Pin NDK, CMake, compiler, Gradle/AGP, Java build tools, submodules and dependency archives. Build core and plugin from clean checkouts. Record build flags and native dependency inspection. Check whether the existing tests are host-side mocks or real GPU tests.

**S0.3 Select one launcher integration.** Inspect maintained Fold Craft Launcher and Zalith Launcher 2 integration paths because MobileGlues already interoperates with them. Select one primary integration through source review and a release-build prototype. Record the decision and exact commit. Reuse account handling, installation, instance management and update facilities; this project does not need a new launcher platform.

**S0.4 Freeze two runtime lanes.** Define legacy Minecraft 1.20.1/Fabric and current Minecraft 26.3/Fabric locks. Each lock includes the game manifest, Java runtime, LWJGL Java/native modules, SDL/GLFW adaptation, loader, Iris, Sodium, support mods and renderer identity. Follow dependency metadata rather than choosing each component's newest version independently.

**S0.5 Freeze original packs.** Acquire the selected official Complementary Reimagined/Unbound r5.9.3, r5.9.2 regression assets, BSL 10.1.8 and the selected additional pack through permitted channels. Record original asset hashes, provenance, options, licensing and supported game metadata.

**S0.6 Record baseline behavior.** Capture stock-release and development-main startup logs, actual backend selection, unmodified pack behavior, native loading errors and screenshots on available hardware. A failed startup is useful evidence if its exact step is known.

**S0.7 Audit release-specific mod restrictions.** Inspect the selected Iris/Sodium binaries and corresponding sources for launcher, LWJGL, native ABI and provider restrictions. Record the exact conditions and applicable license. Development HEAD findings are leads, not proof of the selected release's behavior. If an Android adaptation is needed, assign the S1 owner and preserve the underlying ABI/capability checks.

### Deliverables

A reproducible baseline build; runtime/pack lock files; an audit of upstream versus downstream changes; initial device reports; a failure list classified by layer.

### Exit gate

Another clean build environment can reproduce the chosen baseline source artifacts. All upstream runtime/native dependency requirements and selected pack assets are identified and hash-locked; a not-yet-built Android component has a pinned source/build specification and an owned porting gap. S0 records these gaps explicitly. Implemented, loadable Android Java 25 and SDL/LWJGL dependency closure is an S1 gate, so source reproducibility must not be labeled release-runtime readiness.

## 4. S1 Prove the Android runtime and provider envelope

### Objective

Establish that the required game paths can initialize in an ordinary Googlebook application and that the shader requirements fit an accessible renderer strategy.

### Tasks

**S1.1 Build the in-process probe.** Implement a small Android graphics probe and invoke the same probing code from the actual launcher/game environment. Compare results; a standalone test APK cannot establish which provider the game process loads.

**S1.2 Complete legacy startup.** Bring the pinned Android Java runtime, legacy LWJGL/GLFW bridge and MobileGlues to a deterministic vanilla scene, then the pinned Sodium/Iris baseline. Verify GL entry-point resolution and window size/input events.

**S1.3 Complete current startup.** Establish Android-compatible Java 25 and matching LWJGL 3.4.3 JNI modules for 26.3. Audit SDL, OpenGL, ShaderC, SPIRV-Cross, VMA/Vulkan, OpenAL, STB, FreeType, jemalloc and core LWJGL dependencies. Include the launcher's actual AWT/font/Cacio requirements, JVM flags, native loading and crash handling in a minimal runtime smoke test. Pin LWJGL compiler JNI modules independently from MobileGlues' compiler dependencies; similar names do not establish ABI compatibility. Do not substitute Linux ARM64/glibc binaries for Android libraries.

**S1.4 Unify the SDL/LWJGL GL loader.** Ensure `GL.getFunctionProvider()` satisfies Minecraft 26.3's `org.lwjgl.system.SharedLibrary` cast and exposes the actual loadable MobileGlues frontend through `getPath()`. SDL must load that same frontend, and both must resolve the same `glGetError` address. Verify transformed functions, not just this one address. Qualify Iris against Minecraft's OpenGL backend and capture the actual path after startup. A Vulkan implementation below the GL frontend does not imply that Iris works with Minecraft's native Vulkan backend.

**S1.5 Test production providers.** Probe the default system provider, the Android 17 system ANGLE preference when available, and a coherent app-local ANGLE build when release-loadable. Record requested and actual choices. Run each in a fresh graphics process.

Test the real launcher's native closure in a non-debug release build on the reference device's actual page size and target-SDK loading rules. Verify read-only native loading where required, then add 16 KB emulator/device coverage as components become available. Repeat these early smoke tests for both runtime lanes; S9 expands the final packaging matrix.

**S1.6 Compare requirements against limits.** Parse the actual preprocessed pack programs and reflected active resources. Measure sampler/image/UBO/SSBO limits, required formats, atomics, render targets, compute limits and relevant synchronization features. Test Complementary's 512-invocation compute group, vertex-stage integer image writes, and Ultra's 202,899,456-byte single SSBO early. Validate access across the complete buffer range. Build a gap table with direct support, exact emulation, blocker and cost.

Test the locked Iris release's effective feature checks, preprocessor state and pass activation as well as raw backend support. The inspected development branch requires OpenGL 4.4 or both SSBO and buffer-storage extensions for its SSBO flag; reverify the selected release. Earn the complete truthful contract it needs. A compiled pack with silently disabled ACT, WSR or compute passes is a failed advanced-feature test.

**S1.7 Run the bounded alternative experiment.** In parallel, evaluate a pinned Zink build on the vendor Vulkan driver using its published requirements. Attempt Turnip only if the exact Android/kernel/WSI integration is available. Collect load/present results and selected hard-feature tests. End the initial experiment with a decision after approximately two engineering weeks; do not expand it into an unbounded renderer rewrite.

**S1.8 Probe X2 early.** As soon as an exact X2E Googlebook is available, run this stage there. Record limitations independently. Implementation can continue without that hardware, but certification cannot.

**S1.9 Implement any required downstream mod adaptation.** Resolve the S0.7 findings with a narrowly gated, maintained build under the exact release's license. Assign its own version, source revision and hash, retain accurate platform/provider diagnostics, and accept support responsibility for the qualified Android configuration. Hiding launcher environment variables or paths alone does not establish compatibility. Validate the adaptation against the complete runtime lock.

### Architecture decision

| Evidence | Action |
| --- | --- |
| A production GLES route can express all required semantics | Continue the focused MobileGlues path |
| Missing behavior has a bounded exact emulation | Specify the emulation, performance risk and proving tests |
| Essential behavior exceeds all accessible GLES routes | Keep the feature blocked and select a separately qualified complete GL route through an architecture decision |
| Alternate stack fails Android loading or surface integration | Record the specific failure and continue unaffected work; do not turn root/debug setup into the product requirement |

Selecting an alternate complete GL route requires a revised component lock, named implementation owner, renewed launcher/ABI/provider qualification, and rerunning the applicable S2–S9 gates on that route. Rebaseline the effort; the bounded experiment establishes feasibility only.

### Exit gate

Both `S1-legacy` and `S1-current` must render controlled vanilla and pinned Iris/Sodium scenes on real X1E hardware through their release-loadable native closure. An identified, owned bootstrap blocker enables planning and unaffected work, but leaves that lane `BLOCKED` and aggregate S1 `PARTIAL/BLOCKED`. It cannot satisfy current-lane S5 or complete-product release.

The initial GLES/alternative decision and capability reports exist. No stage assumes that a GPU family name guarantees a required format, feature, or GL version.

## 5. S2 Repair dispatch and the public GL contract

### Objective

Make the layer report and execute a trustworthy API contract before evaluating shaderpack success.

### Tasks

**S2.1 Restore errors and statuses.** Implement correct per-context frontend error state and preserve relevant backend failures. Diagnostic logging must not consume the application's errors accidentally. Remove success overrides from qualification and certified operation for shader compile, link, validation and FBO completeness.

**S2.2 Build one capability registry.** Derive strings, integer limits, extension arrays, context/profile negotiation and feature checks from one record. Clamp logical limits to implemented capacity and physical simultaneous limits. Keep procedure exposure and advertised support consistent. Verify the selected Iris release's effective feature flags and actual advanced-pass activation; do not advertise a higher core version just to bypass its feature checks.

**S2.3 Generate contract coverage.** Inventory calls reached by the selected game/mod/pack corpus and compare them with forwarding, implementations and stubs. Separately test every contract advertised through a core version or extension, including calls not reached by the corpus.

**S2.4 Verify symbol ownership.** Test lookup through `eglGetProcAddress`, legacy GLX-style lookup where used, `dlsym`, LWJGL and SDL. Confirm the intended wrappers handle functions requiring translation. Distinguish application execution paths from helper backend calls.

**S2.5 Preserve state ownership.** Extend existing context IDs, thread-local current context, framebuffer/VAO tables and deferred destruction. Put shader/program metadata in share groups; keep ordinary binding state in contexts. Add synchronization only where genuinely shared state requires it.

**S2.6 Correct immediately reachable stubs.** Prioritize required depth state, buffer clears, indexed multi-bind, storage block binding, queries and relevant texture operations. Decide direct forwarding or exact emulation per function. Do not attempt all historical/vendor stubs indiscriminately.

### Required tests

Invalid enums/values and error recovery; inconsistent version/extension queries; missing entry points; zero/deleted objects; context switching/sharing; wrapper-address routing; shader/link/FBO failures; real timer-query behavior and unavailable results.

### Exit gate

The support matrix is generated from the registry. No mandatory test obtains success from a stub or suppressed failure. Reported versions and limits match the implemented contract.

## 6. S3 Make shader translation and linking deterministic

### Objective

Translate ordinary pack programs without losing the information needed to render their effects.

### Tasks

**S3.1 Fix source assembly.** Handle multiple source chunks, explicit lengths, negative lengths, empty source, invalid inputs and embedded terminators according to the GL contract. Submit the actual translated pointer-array length.

**S3.2 Replace latest-shader state with object records.** Track attached shader sets, pre-link fragment output requests, link generations, interface reflection and deletion. Preserve correct behavior for interleaved compilation, multiple programs, sharing and relinking. Integrate program binaries and direct program uniform calls if advertised.

**S3.3 Preserve diagnostics.** Retain original source and a source map through preprocessing and lowering. Associate logs with pack path, program, stage, active options and compiler pass. Preserve line information rather than removing it to make logs shorter.

**S3.4 Formalize semantic passes.** Extend existing glslang/SPIRV-Cross usage. Preserve profile/version, explicit locations and bindings, stage interfaces, interpolation, integer signedness, array shape, sampler type, `std140/std430` layout and memory qualifiers. Validate legacy built-ins only where required by the loader-transformed input.

**S3.5 Isolate approximations.** Inventory existing derivative, LOD, sampler-buffer and effect-specific substitutions. Keep an exact reference path. A compatibility rule must have a precise trigger and a named behavioral deviation; visual replacements cannot certify the original effect.

**S3.6 Implement resource reflection and plans.** Build per-program logical/physical resource mappings for storage blocks, images, samplers and UBOs where translation changes layout. Resolve active resource counts and aliases. Do not equate high logical binding numbers with a need for equally many active slots.

**S3.7 Make caches reproducible and bounded.** Version the serialization. Include all compilation and semantic inputs in translation keys; keep driver binaries separately keyed. Limit allocation, validate record lengths and integrity, preserve atomic writes, and protect concurrent operations. Record cache hit/miss reasons.

**S3.8 Qualify background compilation.** Start with a correct serialized compilation path. Introduce worker compilation only after compiler and EGL sharing rules are tested. Bound workers and memory; cache success must be indistinguishable from a cold compile.

### Required tests

Multi-stage interface mismatch; explicit and implicit locations; fragment-output binding before/after attachment; arrays and blocks; shader object name reuse; program relink; compute-only programs; shared compilation contexts; cold/warm equivalence; corrupted and stale caches.

### Exit gate

The compiler corpus compiles or rejects deterministically with correct logs. Runtime reflection matches the frontend contract. Original and cached program behavior agree. No required effect depends on an untracked text substitution.

## 7. S4 Complete resource, format and execution semantics

### Objective

Ensure that correctly translated shaders see the correct resources and data.

### Tasks

**S4.1 Build the format/usage matrix.** Test required normalized, signed/unsigned integer, floating, packed HDR, sRGB, depth and depth-stencil formats. Cover allocate/upload/sample/filter/render/blend/image/atomic/clear/copy/readback operations separately. Add array, cube, 3D, layer, mip and multisample cases where used.

**S4.2 Preserve framebuffer routing.** Extend the existing attachment-shuffle implementation and tests. Include sparse/nonidentity draw lists, indexed clears/blends, subsequent identity routing, readback, layer attachments, depth-only targets, resize and attachment-query consistency.

**S4.3 Implement deterministic clearing.** Complete buffer data/subdata clears and texture subimage clears reached by the corpus. Preserve format/type/range semantics and emulation state. Test zero and nonzero values, partial ranges and untouched regions.

**S4.4 Implement SSBO indirection.** Use share-group program-block mappings plus per-context indexed buffer/range bindings. Apply the physical resource plan at execution and return logical bindings to reflection. Preserve aliasing and link invalidation. Track generations for shared program mappings and context bindings so a mapping change in another context invalidates materialized state even without a local `glUseProgram`. Reject resource plans that exceed measured stage or combined limits.

**S4.5 Cover every execution entry point.** Apply preparation to direct, indexed, instanced, base-vertex/base-instance, indirect, multidraw, indirect-count and both compute-dispatch forms. The current code has direct forwarding paths outside `prepareForDraw`; changing only that function is insufficient. Register aliases and resolver-returned implementation functions.

**S4.6 Protect helper state.** Internal clear/copy/multidraw emulation may run draws or compute. Save and restore its bindings and state without recursively rematerializing application resources over helper resources.

**S4.7 Implement visibility and ordering.** Test SSBO/image writes followed by shader reads, texture fetches, vertex/index reads, indirect commands and CPU reads. Preserve application barriers and add correct ordering for renderer-generated operations. Test both direct and indirect dispatch.

**S4.8 Resolve texture feedback.** Distinguish normal ping-pong rendering from simultaneous sampling and framebuffer writes to related storage. Implement texture barriers only with a verified equivalent or a restricted exact emulation. A memory barrier or `glFinish` does not automatically make an otherwise illegal ES feedback pattern correct.

**S4.9 Qualify buffer mapping.** Test storage/map flags, explicit flush ranges, coherent behavior when advertised, fences, CPU/GPU visibility and resource lifetime. A staging mirror cannot observe arbitrary writes through a persistent pointer without a real update mechanism. Replace broad compatibility defaults with tested, scoped policies.

**S4.10 Cover draw parameters and texture views.** Validate index offsets, base vertex, base instance, draw ID and indirect-command interpretation. Implement views only when aliasing, layers, format compatibility and subsequent writes remain correct. These features are gated by actual use and any advertised contract.

### Exit gate

Required GPU microtests pass on at least one production X1E provider. Every required pack format has an operation-specific result. No passing test depends on blanket synchronization, lossy packed-data substitution, or skipped resource preparation.

## 8. S5 Qualify standard shaderpack fidelity

### Objective

Make normal official shaderpack ZIPs work with their intended selected effects.

### Tasks

**S5.1 Freeze reference scenes.** Capture the same game, pack, options, world, camera route, resource pack, time/weather and temporal warmup on a known-good desktop GL reference.

**S5.2 Qualify Complementary separately.** Test Reimagined and Unbound as distinct assets. Test r5.9.3 for the main release and r5.9.2 for regression. Record High and other selected profiles exactly; do not equate High with Ultra or advanced WSR.

**S5.3 Inspect intermediate results.** Capture color targets, depth, normals/material data, shadow targets, history and relevant debug outputs when the loader permits. Locate the first incorrect pass before making a whole-pack workaround.

**S5.4 Test complete effects.** Include sunlight and shadows, water and underwater views, SSR, PBR normal/specular data where enabled, bloom, exposure, clouds/volumetrics, temporal antialiasing and dimension changes.

**S5.5 Validate BSL and another implementation.** Use a second pack with a different pass structure to expose assumptions specific to Complementary. Add one compact diagnostic pack whose effect outputs can be tested numerically.

**S5.6 Test both runtime lanes.** Reuse scenes where possible but keep references specific to each game version. Do not compare captures made under materially different game/loader behavior as though only MobileGlues changed.

### Exit gate

All required standard profiles pass compile, resource and visual checks, temporal sequences, repeated reload, and minimum endurance on the reference device. Every known visual discrepancy has a disposition. This permits a clearly labeled standard-shader preview release.

## 9. S6 Implement advanced lighting and world-space reflections

### Objective

Meet the advanced-feature portion of the full-support requirement.

### Tasks

**S6.1 Derive requirements from actual release ZIPs.** Resolve feature macros, Iris feature flags, geometry/compute stages, buffer sizes, image formats, clear schedule, atomic operations and update dependencies for each advanced option.

**S6.2 Qualify advanced colored lighting independently.** Require the original Very High profile with ACT 192 and WSR off, followed by Ultra's ACT 256. Test moving colored emitters, placement/removal of lights, occlusion, camera movement, chunk load/unload, dimension changes and temporal convergence. Verify actual storage updates and light contribution.

**S6.3 Qualify WSR independently.** Require the original Ultra profile with ACT 256 and WSR on. Test off-screen geometry, reflection occlusion, reflective surfaces moving into view and scene updates. Verify that WSR is enabled in effective options and runtime programs. The named Complementary profile with ACT 512/WSR on is an additional optional stress result, not an initial 1.0 requirement.

**S6.4 Audit memory at every step.** Compare declared SSBO sizes with provider limits and account for all image/buffer allocations and compiler peaks. Reject integer overflow or over-limit bindings cleanly. Do not relabel a smaller voxel grid as the requested grid.

**S6.5 Qualify an independent compute-oriented pack.** Use a pinned release selected in the validation matrix. Derive its actual requirements; do not assume the same resources as Complementary.

**S6.6 Resolve genuine ceilings through the S1 decision.** If a necessary limit or semantic cannot be implemented through the selected GLES provider, activate the documented complete-GL alternative track. A failed advanced profile remains a release blocker for that claim. Continue unaffected standard work.

### Exit gate

The specified advanced lighting and WSR profiles pass effect-specific image/temporal tests and endurance. Numeric allocations, options and actual dispatched/drawn programs are recorded. The enabled effect survives reload, movement and scene updates.

## 10. S7 Optimize X1E from measured bottlenecks

### Objective

Reduce frame times, stalls, memory pressure and energy use while preserving the qualified output.

### Order of work

1. **Startup and compilation.** Remove redundant preprocessing and reflection, improve cache reuse, bound compilation workers and expose useful progress. Compare cold and warm runs separately.
2. **CPU submission.** Measure wrapper overhead, redundant state validation, JNI boundaries and draw submission. Cache only state with correct context/share-group invalidation.
3. **Multidraw.** Retain existing correctness checks and benchmark statistics. Add representative Sodium and machinery/train workloads, then validate the selected strategy over sustained sessions.
4. **Buffer streaming.** Improve ring allocation, orphaning/subdata strategy and fence reuse only after mapping semantics pass.
5. **GPU bandwidth.** Reduce unnecessary intermediate copies, clears and render-target lifetime. Preserve pack pass ordering, history and visible precision.
6. **Adreno compiler behavior.** Use real profiling to investigate register pressure, spills, varying usage, divergence and cache behavior. Keep any shader change semantics-preserving. Do not blanket-enable fast math or lower precision.
7. **Presentation.** Align pacing with the selected 30/60/high-refresh goal and the actual display. Measure queueing and input response as well as FPS.
8. **Thermals and memory.** Test sustained operation on battery and external power with fixed brightness and controlled background work. Apply ordinary user-visible quality profiles where useful.

### Optimization acceptance

Every change must pass the relevant correctness corpus and compare against the previous build using identical options. Report median/p95/p99 frame time, uncertainty across repeats, peak memory and thermal conditions.

Do not retain a microbenchmark gain that causes material regression in the required real scenes. Revert or scope the policy when results differ by driver or workload.

### Exit gate

At least one minimum playable standard configuration on every device advertised as recommended must meet the chosen performance objective in document 03. A missed objective is a performance failure for that profile; documenting it does not turn it into a pass. Advanced profiles receive separate measured ratings and limitations, and correctness alone does not make them the out-of-box recommendation. The release does not present goals as achieved benchmarks.

## 11. S8 Qualify X2E and additional devices

### Objective

Extend support through evidence rather than inherited tuning.

### Tasks

**S8.1 Inventory the exact model.** Identify SoC/GPU variant, memory configuration, OEM firmware, Android build, actual driver and provider. Re-run the complete probe.

**S8.2 Re-run semantic tests.** Cover formats, compilation, reflection, synchronization, mapped buffers and hard-feature limits. A newer GPU can introduce new driver failures.

**S8.3 Re-run the required pack corpus.** Include standard, advanced lighting and WSR profiles under both runtime lanes. Keep any runtime incompatibility separate from graphics differences.

**S8.4 Recalibrate policies.** Measure provider and multidraw choices, compilation workers, memory budgets and presentation modes. Derive X2 recommendations from sustained results.

**S8.5 Test update transitions.** Move between supported driver/OS versions where practical and verify cache invalidation, profile rematching and recovery.

### Exit gate

Each supported X2E/OEM configuration has a complete result record. If no appropriate retail X2E Googlebook is available, mark qualification pending and release X1E support under that exact scope. Do not declare the full two-generation target complete.

## 12. S9 Ship a dependable desktop product

### Tasks

**S9.1 Complete desktop behavior.** Test caption/inset handling, DPI, resize, fullscreen, maximize/minimize, external display, focus loss, pointer capture, key release, trackpad, keyboard layouts and IME. Handle surface replacement separately from actual context loss. Notify Minecraft/Iris to rebuild their own size-dependent targets and histories, update bridge-owned resources, and preserve valid shared objects when the context survives.

**S9.2 Complete ordinary installation.** Validate a signed release APK on a fresh user profile with ordinary permissions. The user should reach a supported shader scene through installation, sign-in, instance selection and ZIP import.

**S9.3 Finalize automatic policy.** Match qualified fingerprints first. For unknown combinations, run bounded startup probes and choose a conservative eligible provider. Do not use a menu screenshot as evidence for advanced pack eligibility.

**S9.4 Deliver understandable diagnostics.** Report the failed program/feature and relevant component versions. Export a concise redacted bundle. Preserve complete technical details in support artifacts without filling ordinary game screens with them.

**S9.5 Make updates recoverable.** Validate atomic component-set updates, cancellation, insufficient storage, bad archive/hash, failed native load and bad renderer startup. Keep last-known-working components without overwriting user worlds or pack options.

**S9.6 Validate native packaging.** Test ELF/APK alignment and actual 4 KB/16 KB environments as applicable, readonly native loading for the target SDK, debug versus release differences, dependencies and symbols.

**S9.7 Run release qualification.** Execute the mandatory matrix on hardware, produce performance tables, review license/redistribution obligations, generate checksums and provenance, and publish exact support boundaries.

**S9.8 Preserve upstream maintainability.** Keep generic correctness fixes separate from device profiles and packaging. Submit suitable fixes upstream when the project owner authorizes publication.

### Exit gate

The signed artifact and the exact packaged component set pass fresh-install, upgrade, rollback, shaderpack, desktop lifecycle and endurance gates. Every support claim maps to retained evidence.

## 13. CI and test tiers

| Tier | When | Purpose |
| --- | --- | --- |
| Host checks | Every relevant change | Compiler tests, state-model tests, serialization validation, build and static analysis |
| Native probe tests | Every graphics change on trusted hardware runners | API/format/synchronization behavior through the actual provider |
| Pack smoke tests | Every integrated candidate | Startup, compile/link, representative targets and quick effect checks |
| Full image/temporal corpus | Nightly or before promotion | Visual and behavioral regressions |
| Sustained performance | Before policy/performance promotion | Repeated frame-time, thermal and memory comparison |
| Full product qualification | Release candidate | Fresh install, updates, desktop lifecycle, all required hardware profiles |

Use host tests and emulators for appropriate logic, ABI and packaging work. They cannot certify the retail graphics driver.

Keep signing keys separate from untrusted pull-request execution. Physical device runners should execute only approved test artifacts with controlled access. Do not upload account tokens, private worlds, or proprietary assets in public CI logs.

A gate that lacks hardware evidence is `NOT RUN` or `BLOCKED`, never `PASS`.

## 14. Delivery sequence and planning effort

A practical sequence is a probe/diagnostic release, a standard-shader preview, an advanced-feature beta, an X2-qualified beta, then the complete product release.

For a small experienced team, reserve roughly 2–4 weeks for initial discovery and bootstrap feasibility, 3–6 months for a credible standard-shader product, and 6–12 months or more for broad advanced-feature and two-generation qualification. These are planning estimates, not measured forecasts. The S1 outcome can change them substantially, especially if Java 25/SDL adaptation or a complete alternate GL stack requires major work.

A single coding agent still needs real hardware results and independent review. Parallel agents can reduce research and implementation time; they cannot infer driver correctness or substitute for device access.

Re-estimate after S1 and after the first complete standard/advanced pack passes. Track remaining work by failed contracts and release gates, not by the number of files changed.

## 15. Per-task execution rule

For every implementation ticket, record the requirement, relevant source/contract, proposed change, exact test, expected result, dependencies and completion evidence.

Use this loop:

1. Read the current execution state and relevant implementation.
2. Reproduce the failure or identify the exact unimplemented contract.
3. Add the smallest meaningful regression that distinguishes the bad behavior.
4. Implement a focused correction.
5. Run targeted tests, then the required integration gate.
6. Review output and state restoration independently.
7. Commit the change with evidence and update execution state.
8. Move to the next unblocked requirement.

After two failed fixes of the same symptom, stop changing unrelated switches. Reduce the failure, identify the first incorrect pass or state transition, and record a specific hypothesis. Keep independent work moving.

Never satisfy a gate by editing expected results to match a defect, hiding an enabled effect, increasing an advertised limit, suppressing a failure, or deleting the failing test. Changes to acceptance criteria require an explicit product decision and must preserve the user's goal.

