# MobileGlues FSR3 research and implementation plan

Research checked: 20 October 2026 UTC. Target: native Android ARM64 on Snapdragon X1E-80-100 Googlebook, followed by separately qualified X2E devices. This plan extends the existing MobileGlues Googlebook work. Source inspection was performed; no Snapdragon execution, image-quality qualification, or device benchmark was performed for this plan.

## 1. Recommended product

Implement **FSR 3.1 temporal upscaling inside MobileGlues**, with a small bundled Minecraft/Iris integration that provides the frame information needed for reconstruction. First validate and repair the existing FSR1 path and establish real lower-resolution world rendering. Keep frame generation as a separate later project.

The product has three honest runtime states:

| Mode | Intended behavior | Availability condition |
| --- | --- | --- |
| Off | Existing rendering and normal pack AA, with no upscaling resources, extra copies, or jitter changes | Always available |
| FSR1 spatial upscaling | Corrected existing EASU and RCAS implementation operating on a genuinely lower-resolution scene | Qualified resolution/presentation path |
| FSR3.1 temporal upscaling | FSR3 reconstruction with valid temporal inputs, reduced scene resolution, and native-resolution UI | Qualified GPU backend plus compatible engine/pack profile |

A camera-motion-only prototype must be labeled experimental and cannot earn general moving-entity support. FSR1 must remain labeled FSR1. Successful shader compilation, a new toggle, or sharpened final output is not completion of FSR3.

The user should receive the adapter through the supported launcher/instance setup. They should not need to construct another rendering bridge, replace MobileGlues, edit shader ZIPs manually, or choose native libraries themselves. Internally, the adapter and pack-specific integration are necessary engineering components; the native upscaler remains owned by MobileGlues.

### Scope retained from the existing project

- Maintain Minecraft 26.3 and Minecraft 1.20.1 as separate qualification lanes. Reuse the existing runtime and launcher plan rather than rebuilding them for this feature.
- Continue using Minecraft's OpenGL route through MobileGlues. Vulkan beneath a GLES provider or inside an explicitly qualified upscaler is different from switching Minecraft to native Vulkan.
- Qualify Complementary r5.9.3 and retain r5.9.2 for regression. Verify exact release assets and options.
- Retain the earlier transparency/UI white-layer, ACT light-color, and WS Player Reflection problems as named regression cases. Reproduce them if still present; do not assume this audit establishes their cause.
- Preserve shader features and user settings. An FPS improvement obtained by disabling ACT, reflections, or other requested effects fails the feature-preservation gate.
- Use one coherent EGL/GLES provider and resource namespace. Provider changes require a fresh graphics session. A second API may access a resource only through a separately proven sharing/synchronization contract.

## 2. What the research establishes

### 2.1 MobileGlues already has FSR1

The audited core is `97558a69157b033b890958b1e49258c5b51a80c6`, dated 22 September 2026. MobileGlues implements desktop GL over host OpenGL ES, with desktop-GLSL-to-GLSL-ES conversion. Its source already has FSR1 resources, configuration, default-framebuffer redirection, presentation hooks, and context bookkeeping. The V1.3.0 release announced built-in FSR1. [R01], [R02], [R03], [R20]

The previous project lock distinguished plugin `f5105e4ddd1752cccdcc6421c402960d40ec3d9d`, its embedded core `f56af31d3e490d95808b3adc4ace20234249a37a`, and audited core main `97558a6...`. Treat those as provenance leads to reverify, not proof that the user's installed APK contains any particular revision.

### 2.2 Static findings in the existing FSR1 implementation

These findings come from reading the pinned source, not an on-device reproduction:

| Finding | Relevant source | Required action |
| --- | --- | --- |
| The custom fragment shader computes EASU into `color`, but outputs only `sharpenedColor`; RCAS still reads the original input texture | `gl/FSR1/FSRShaderSource.h` | Reproduce with diagnostic patterns; implement a correct EASU-to-RCAS dataflow and border handling |
| EASU output dimensions are derived from input texture size | Same file | Verify input extent, active region, and output extent independently |
| Surface width/height become render width/height, then the preset multiplies them to create target dimensions; the viewport wrapper forwards requested dimensions | `gl/FSR1/FSR1.cpp` | Prove which passes actually become cheaper; establish output-first size ownership |
| Render color allocation changes from RGBA8 at initialization to RGBA32F on recreation | Same file | Adopt an intentional format policy that survives resize and is supported for every required operation |
| The pass saves selected bindings but does not establish/restore every raster state it relies on; clear color changes and viewport ownership also require review | Same file | Add focused injected-pass state and lifecycle tests |

The render-resolution helper appears unused in the audited tree. These observations are sufficient to require a baseline gate; they do not establish the behavior of a different downstream build. Exact pinned source links are [R02] and [R03].

RGBA32F contains four times as many raw bytes per pixel as RGBA8. That arithmetic does not establish the actual bandwidth or frame-time impact on Adreno. Measure those costs.

### 2.3 Select a reproducible FSR3 source generation

AMD separates upscaling and frame generation in FSR3.1. Its public FSR3 page now names algorithm version 3.1.5. Current SDK main belongs to the newer SDK 2 generation and lists Vulkan as unsupported in its known-issues table. A suitable initial source candidate is **FidelityFX SDK v1.1.4, containing FSR3.1.4**, at commit `c6efa6bf7f2027b3ec94f28578bb5965eabb9e55`. [R04], [R05], [R06]

Pin this candidate and its relevant dependencies after a source/build check. Do not vendor an unspecified latest branch or confuse SDK version 1.1.4 with algorithm version 3.1.4. Review later FSR3 fixes individually once the baseline works; an upgrade requires renewed shader/reference testing.

### 2.4 Android support requires a port and qualification

The documented FSR3 shader route uses Vulkan-oriented GLSL 4.50 and extensions, not a supplied GLES backend. Android's Vulkan availability and ANGLE support do not establish FSR3 compatibility on a particular Adreno driver. [R07], [R12]

The main research choice is a custom GLES compute implementation versus explicit Vulkan interoperation. The default recommendation is the GLES implementation because it keeps the game and upscaler within the existing provider and GPU resource system. Its feasibility must be established early.

### 2.5 Minecraft already provides useful integration precedents

The Super Resolution mod has FSR implementations and documents engine/shaderpack integration, but its README targets Windows/Linux x64 and explicitly excludes Android. It is a reference, not an Android dependency. Its shaderpack guide requires coordinated scaling, resource identification, jitter, and reconstruction placement. [R09], [R10]

Complementary issue #317 contains a firsthand report of severe FSR3 entity ghosting with approximate camera/depth reprojection. A later comment describes an `at_velocity` implementation; the accompanying guide limits that facility to Minecraft 26.1 and newer. This supports separate modern and legacy motion-data work, not a claim that all current motion cases are solved. [R10], [R11]

## 3. Performance model and realistic targets

Upscaling saves time only when the cost avoided by lower-resolution rendering exceeds the cost of input preparation, reconstruction, additional copies, and synchronization.

For planning, use:

`new GPU time ≈ fixed GPU work + pixel-dependent work × scale² + motion/mask preparation + FSR dispatch + copies/synchronization`

Here scale is render width divided by output width. This is an approximation: cache behavior, occupancy, tile memory, scheduling and bandwidth can make scaling nonlinear. Overall frame rate can remain limited by CPU work or presentation even when GPU work falls.

At a hypothetical 2560 × 1600 output, the approximate render sizes are:

| Mode | Per-dimension divisor | Approximate render size | Relative scene pixels |
| --- | ---: | ---: | ---: |
| Native AA | 1.0 | 2560 × 1600 | 100% |
| Quality | 1.5 | 1707 × 1067 | 44.4% |
| Balanced | 1.7 | 1506 × 941 | 34.6% |
| Performance | 2.0 | 1280 × 800 | 25% |

Ratios follow AMD's naming; sizes and pixel percentages are calculated examples, not measurements. Use the selected SDK helper and explicit integer/active-region rules for actual sizes. Start qualification with Quality and Balanced. Native AA is a reconstruction-quality option, not a resolution-based performance claim. [R07]

**Illustrative calculation:** suppose a native GPU frame consists of 4 ms fixed work and 12 ms pixel work. Quality scaling gives roughly `4 + 12/2.25 = 9.33 ms` before integration cost. If preparation and reconstruction add an assumed 2 ms, the result is about 11.33 ms instead of 16 ms. This is approximately 1.41× GPU throughput, not 2.25× total FPS. If the CPU needs 20 ms per frame, the game can remain near 50 FPS. None of these numbers was measured on the user's device.

ACT includes voxel/floodfill work, and WSR can involve geometry and non-screen-sized data. Lower scene resolution cannot be assumed to shrink all of that work. Keep a separate CPU-heavy Create scene to identify simulation/submission limits. Complementary's own option descriptions establish the effect categories; the performance implication is engineering reasoning. [R14]

### Proposed release thresholds

These are project acceptance targets to calibrate against baseline variance, not predicted hardware performance:

- A supported Quality profile should aim for at least **20% lower median real-frame time** in a fixed GPU-limited route, with acceptable reconstruction quality.
- Require improvement beyond repeated-run variance and report tail times. Use a provisional limit of no more than 5% p95/p99 degradation; investigate a failure rather than averaging it away.
- Off mode should add no rendering passes or GPU allocations. Its observed frame-time difference should be within baseline noise; use 1% as an initial investigation threshold, not a claim of measurement precision.
- Compare FSR3 with native rendering and with same-input-resolution bilinear and FSR1. FSR3 need not be faster than FSR1: its additional work should purchase useful image quality.
- Validate sustained runs and feature correctness before selecting defaults. An unsupported combination uses a qualified fallback and explains its actual mode.

## 4. Architecture and responsibilities

The adapter submits explicit scene inputs to the in-tree upscaling subsystem. The selected backend reconstructs the display-sized scene, after which the adapter and pack finish the image and compose the HUD. Existing EGL code retains presentation ownership. Context and surface lifecycle events invalidate or rebuild the appropriate histories. The exact post-processing order belongs to each qualified pack profile.

| Component | Ownership |
| --- | --- |
| MobileGlues upscaling subsystem | Mode selection, validation, resources, backend dispatch, timing, native ABI, state restoration and failure reporting |
| Existing GLES provider | Actual texture/image operations, compute execution, barriers and EGL objects |
| Minecraft/Iris adapter | True scene resolution, temporal frame data, render-stage boundaries, current resource identity and native-resolution HUD |
| Pack profile | Exact integration point, color-space and exposure conventions, jitter/final-AA coordination, masks and pack-version compatibility |
| Launcher integration | Matched binaries/mods, settings, instance lock, safe restart and diagnostic export |
| Independent verifier | Reference sequences, hardware runs, correctness/performance results and release decisions |

### In-tree implementation locations

Extend existing modules before inventing duplicates. The following new paths are suggestions, to be adapted to the repository's established layout:

- `MobileGlues-cpp/upscaling/`: controller, frame contract, capability checks, history and backend interfaces.
- `MobileGlues-cpp/upscaling/fsr3/`: selected host algorithm, GLES backend and generated shader variants.
- `MobileGlues-cpp/gl/FSR1/`: retain and correct the current spatial implementation; migrate only reusable pieces that have proven ownership.
- `integrations/minecraft/`: version-specific adapter modules and versioned pack integration metadata.
- `tests/upscaling/` and `bench/upscaling/`: synthetic/reference harness and device measurements.
- `docs/fsr3/`: decisions, task ledger, execution state and evidence manifests.

One integrator owns changes to shared interfaces and core build files. Do not scatter temporal reconstruction across unrelated GL entry points.

### Existing hooks to inspect and retain

| Existing path | Why it matters |
| --- | --- |
| `egl/egl.cpp` | `presentSurface`, swap-with-damage handling, final presentation and surface lifecycle |
| `egl/context.cpp` | Current FSR context bind/forget hooks; basis for explicit temporal ownership |
| `gl/framebuffer.cpp` | Default draw-FBO redirection, read scopes and blits |
| `gl/getter.cpp` | Application-visible state must remain consistent with logical GL objects |
| `gl/texture.cpp` | Texture copies, default-buffer reads and driver binding-shadow validity |
| `gl/drawing.cpp` | Existing compute and memory-barrier forwarding |
| `gl/glsl/glsl_for_es.cpp` | Existing compiler/lowering infrastructure |
| `config/settings.cpp` and `.h` | Existing FSR configuration and actual provider/capability filtering |
| `CMakeLists.txt` and plugin build | Native dependency/build integration and delivery provenance |

The swap hook is suitable for presentation and lifecycle work. Temporal reconstruction generally needs to run earlier, at an explicit scene stage, before native-resolution HUD composition. Cover ordinary swaps and both damage-swap paths. [R13]

## 5. Frame contract before parallel coding

Freeze an initial ABI/schema before backend and game agents implement against it. The names below are proposed project concepts, not existing MobileGlues or Khronos APIs. AMD's dispatch structure supplies the algorithm fields; the additional identity/lifetime fields are project design. [R08]

| Contract group | Required information and invariant |
| --- | --- |
| ABI | Version and structure size; reject incompatible producers explicitly |
| Identity | Frame ID, graphics-session ID, context/share-group identity, surface/view identity and resource generation |
| Resources | Color, depth, motion and output views with explicit logical-versus-backend handle namespace, mip/layer, format and lifetime |
| Dimensions | Allocated extents, active rectangles, render size, output size and origin conventions |
| Color | Linear/perceptual encoding, HDR/LDR status, exposure policy and valid positive pre-exposure |
| Depth | Clip/depth range, reversed/infinite-depth flags and sampleable scene depth |
| Motion | Current-to-previous direction, units, signs, origin, jitter inclusion and confidence/coverage classification |
| Temporal | Actual jitter, elapsed frame time, reset flag/reason and history continuity |
| Camera | Near/far conventions, vertical FOV in radians and coordinate scale |
| Quality inputs | Exposure, reactive and transparency/composition inputs or explicitly documented defaults |
| Scheduling | Producer completion, accepted render-stage ID, consumer lifetime and completion semantics |
| Presentation | Output target and explicit native-resolution UI ownership |

Backend-private intermediates remain owned by the backend. Do not require the game to manufacture FSR scratch buffers unless the selected API actually calls for shared resource ownership.

### Interface behavior

Use an in-process versioned native interface, resolved from the same MobileGlues library the game is using. A small JNI integration is acceptable where needed. Do not expose invented standard GL extensions or silently interpret unrelated texture names as semantic inputs.

The interface needs operations equivalent to capability query, session creation, frame dispatch, history invalidation and destruction. Pass resource handles with explicit namespace and validate that they belong to the active graphics session. Preserve ABI errors and backend failure reasons.

Start with a synchronous render-thread submission contract within one context. GPU commands may still execute asynchronously. Add cross-context submission only when there is a measured need and a complete ownership/synchronization design.

### Motion tests that precede image tuning

For a declared current-to-previous pixel-space convention, an object that moves two render pixels right has an x motion of minus two at its current location. Encode that case in a synthetic fixture, then test camera translation, rotation, depth discontinuities and orientation flips. If the adapter emits UV units, convert using declared dimensions and the selected SDK scale parameters.

Camera matrices and depth can reconstruct static-scene motion. Independently moving entities need previous transforms/positions; animated geometry needs matching previous deformation information or a conservative quality policy. Test hands, particles, foliage, vehicles and Create contraptions separately. An entity translation attribute alone does not establish correct deformation or reflection motion.

## 6. Resolution, color, transparency and temporal ownership

### True resolution control

The adapter owns separate render and display dimensions. Resize scene targets and the appropriate screen-space passes coherently. Keep shadow maps, voxel volumes, exposure textures, lookup tables and other fixed-size resources under their own policies.

Do not globally intercept every texture allocation or viewport to shrink it. Do not lie about the Android surface size. For each expensive pass, record its allocated extent, active viewport, input/output region and uniform conventions. Revalidate shaderpack ping-pong target identity after each swap.

A launcher rendering a smaller Android buffer can be a separate compatibility mode. It does not by itself establish native-resolution UI or the explicit FSR3 input contract. Do not allow launcher scaling and scene scaling to multiply unintentionally.

### Correct spatial baseline

Validate existing FSR1 against AMD's EASU/RCAS behavior. A straightforward first correct implementation may use separate passes; fuse only after equivalence and benefit are demonstrated. Its normal placement is after tone mapping with the appropriate perceptual color input. Do not use FSR1's placement and color assumptions as an automatic template for FSR3. [R16]

### Temporal pipeline placement

For the first controlled HDR profile, prefer scene rendering and required effect preparation, then temporal reconstruction, then the appropriate display-stage finishing and HUD. If a pack needs a different order, document and test it explicitly. A validated LDR profile is possible when its algorithm flags and exposure/color handling agree.

FSR3 must replace the final image's TAA resolve and use the jitter actually applied to the scene. Audit Complementary's final TAA, per-effect denoisers/history, sample-count policy and sharpening separately. Preserve useful effect temporal filtering when its coordinates and history remain valid. A global TAA-off macro plus an appended upscale pass is not a sufficient integration. [R07], [R14]

### Transparency and composition

Keep UI text, chat, inventory and menus at display resolution. Test translucent world materials, underwater transitions, clouds, particles, emissives and reflections with known backgrounds. Declare premultiplied/straight alpha and output-surface alpha policy instead of forcing arbitrary conventions.

Reactive and transparency/composition inputs are optional at the algorithm interface, but a supported pack still needs a tested policy for rapidly changing or poorly reprojected pixels. Do not set every pixel fully reactive to conceal missing motion; that sacrifices temporal reconstruction and still needs honest quality labeling.

### State and lifecycle

Establish and restore every state the injected pass changes or depends on: program, VAO, texture/sampler units, read/draw FBO, viewport/scissor, blend functions/equations/enables, cull/depth/stencil state, color masks, clear state and relevant raster modes. Compute backends also account for image and indexed-buffer bindings.

Use trusted state shadows where available; repair or invalidate shadows after internal raw GLES calls. Avoid a blanket set of expensive driver queries every frame. State preservation must be demonstrated with adversarial caller state, not inferred from a visually normal scene.

Histories are per rendering stream, not global. Handle surface-only recreation, true context loss, resize, shader reload, world/dimension change, teleport, camera cut, perspective/FOV change, algorithm switch and resource generation changes. Reset or rebuild according to the pinned API's requirements. At first, use conservative history resets for render-scale changes; optimize dynamic-resolution continuity later.

For minimized/zero-sized surfaces, do not dispatch against invalid dimensions. Retire in-flight resources safely. Normal frame execution should avoid resource allocation, shader compilation, CPU readback and global GPU waits.

### Truthful internal checks

The audited frontend can override failed shader compile and program link status when application error ignoring is enabled. Internal upscaler compilation, linking, framebuffer completeness and capability probes must inspect truthful backend results regardless of those compatibility settings. If an internal pass fails, disable that mode and coordinate a valid fallback with the adapter; never execute it because the frontend reported apparent success. Inspect the actual downstream equivalents of the pinned shader/program query implementations before changing them. [R22], [R23]

### Scene texture mip bias

Assign texture LOD selection to the S4 integration work. Apply the pinned FSR version's recommended scene-sampler bias through the adapter or qualified pack profile, accounting for existing pack bias and anisotropic filtering. Do not globally alter shadow, lookup, volume, post-processing or UI samplers. Validate distant textures, foliage, fences and motion for both lost detail and shimmering. AMD's integration checklist includes mip-bias setup; the scoping and validation policy here are project requirements. [R15]

## 7. Backend decision and bounded feasibility work

### Preferred GLES compute route

Implement the FSR host/backend functions needed for resource creation, bindings, dispatch, barriers and pipeline variants. Port the selected shader sources using existing compiler infrastructure and explicitly maintained backend adaptations. A GLSL version replacement is insufficient.

Probe the actual provider for compute support, limits, needed samplers/images, image load/store formats, integer atomics, texture operations, precision and shader permutations. Test operations, not only extension strings. Preserve an FP32 correctness reference; enable FP16 only after a supported and measured path is established. [R07]

Generated internal shaders may use the provider's raw GLES dispatch, provided they preserve MobileGlues state and resource mappings. Do not accidentally translate an already generated GLES shader through a second incompatible frontend.

### Vulkan reference and conditional Android alternative

Build a small pinned Vulkan reference harness to produce comparable outputs from the same recorded synthetic frame inputs. The reference can run on a desktop workstation; it does not qualify an Android binary.

Only investigate a production Vulkan upscaler if the GLES route fails a concrete capability/performance gate. Before game integration, prove color/depth/motion/mask/output exchange with the actual provider. AHardwareBuffer import requires supported allocation and format combinations; native fences require correct ownership. A GL object name is not a Vulkan image, and ANGLE being implemented on Vulkan does not automatically expose its internal resources. [R17], [R18]

If the resource needs conversion or copying, include that cost. Exclude CPU readback/re-upload from a performance path. Do not adopt repeated `glFinish` or queue/device-wide waits as the normal handoff. If the interoperation cannot be made correct and useful, record a backend blocker rather than initiating an unbounded renderer rewrite.

### Feasibility exit criteria

After a bounded spike, produce one of these decisions with evidence:

1. GLES candidate executes all required reference passes correctly and has a credible measured cost: continue.
2. GLES fails a documented requirement and the Android Vulkan candidate proves the entire resource/sync chain: select it through a recorded architecture decision.
3. Neither route is demonstrated: retain the useful spatial milestone, keep FSR3 unavailable, and assign the exact remaining blocker. Continue independent integration/tests; do not report the overall FSR3 task complete.

## 8. Cooperating agent team

Use one coordinating agent and three active workers. Five roles are useful, but they do not require five simultaneous writers.

| Role | Ownership | Required evidence |
| --- | --- | --- |
| Coordinator and integrator | Scope, baseline locks, shared contract, task ledger, merge order and final status | Reproducible integrated build and gate decisions |
| Core/state worker | Existing FSR1 corrections, resolution plumbing, state, FBO/readback and lifecycle | Focused fixtures plus disabled-path regression evidence |
| FSR backend worker | AMD source pin, reference harness, GLES port, capability/performance probes | Reference-sequence comparison and per-pass GPU costs |
| Minecraft/Iris worker | Both game lanes, scene producer, motion/jitter/depth, HUD and pack profiles | Debug input captures and moving-scene results |
| Independent validation worker | Adversarial scenes, benchmark protocol, hardware runs and release audit | Independent pass/fail reports tied to exact binaries |

Rotate the core worker into a new independent validation assignment after its foundation changes are reviewed, or spawn a fresh reviewer when a slot opens. The author does not solely approve its own critical changes. Keep the target device reserved per benchmark so concurrent runs do not contaminate results.

### Git and ownership rules

- Inspect repository instructions, branch, status, remotes and submodules before edits. Preserve user changes.
- Give each worker an isolated worktree and task branch from the agreed base. Do not share a writable checkout.
- Assign file ownership. Only the coordinator changes the common ABI and merges shared files; workers request changes through a small patch/proposal.
- Prefer narrow commits with purpose and proving evidence. Do not combine translator rewrites, cache redesign, dependency upgrades and FSR work in one commit.
- Integrate completed vertical slices and run their required regression gates. Build success alone never advances a graphics correctness gate.
- Keep a continuation record with exact HEAD, worktree/branch, stage, pass/fail/not-run results, failed hypotheses and next executable task.
- After two failed attempts at the same hypothesis, change the experiment or isolate a smaller repro. Do not repeatedly regenerate similar patches.

### Worker handoff record

Every completed task records: base and resulting commit, owned files, change purpose, commands actually run, output artifact locations, expected versus observed behavior, unresolved risks and the next dependency. Use `PASS`, `FAIL`, `BLOCKED`, `NOT RUN` and `NOT APPLICABLE` accurately. A mock capability report cannot stand in for target hardware.

## 9. Stages and acceptance gates

| Stage | Concrete work | Parallelism | Gate |
| --- | --- | --- | --- |
| S0 Baseline and provenance | Identify installed/source builds, lock runtime lanes, inventory current FSR1, capture existing regressions, identify CPU/GPU limits | Core audit, backend source audit and Minecraft pass audit | Reproducible baseline and scoped source/device findings |
| S1 Resolution and spatial correctness | Prove reduced scene work, correct EASU/RCAS, intentional formats, state and readback, native HUD in adapter path | Core foundation while adapter establishes scaling | Input/output sizes correct; no crop/white layer; disabled path preserved |
| S2 Contract and feasibility | Freeze ABI; synthetic input corpus; Vulkan reference; GLES capability and full-pass spike | Backend and game workers use the same contract; reviewer writes adversarial cases | Correct pass outputs and a recorded backend decision |
| S3 Native temporal backend | Implement selected in-tree backend, resource reuse, barriers, error paths and lifecycle | Backend work alongside engine motion integration | Reference sequence match within justified tolerances and measured full cost |
| S4 First complete game path | Controlled pack/vanilla scene, real motion categories, jitter, depth/exposure, masks, scene mip bias and native UI | Adapter worker plus independent input/output validation | Useful temporal result with honest coverage; camera-only remains experimental |
| S5 Complementary qualification | r5.9.3 profile, r5.9.2 regression, final TAA versus effect histories, ACT and WSR/player reflection cases | One lane first; second lane then repeats its own gates | Moving-scene correctness with the same requested feature settings |
| S6 Performance and delivery | Tune measured bottlenecks, sustained tests, release native/plugin packaging, rollback and diagnostics | QA owns device; other workers fix isolated failures | Quality/performance/lifecycle gates pass on each advertised tuple |

S2's source/harness work can start during S1. S3 and S4 overlap only after the interface is frozen. S5 waits for a correct controlled path. General shaderpack coverage follows successful named profiles.

### Suggested first execution wave

1. Coordinator reads current project documents and establishes source/runtime/device lock plus file ownership.
2. Core worker reproduces FSR1 dataflow and size findings using small test scenes; it does not begin a general state rewrite.
3. Backend worker builds the selected FSR source manifest and compiles the hardest required shader/format operations in a minimal harness.
4. Minecraft worker maps actual world render targets, TAA/jitter ownership, HUD boundary and available motion data in both lanes.
5. Coordinator reviews findings and freezes the frame contract. Only then assign connected implementation tasks.

### Effort planning

For an experienced graphics engineer supervising agents with reliable device access, budget roughly 1–2 weeks for baseline/spatial foundations, 1–2 weeks for backend feasibility, several additional weeks for a usable temporal backend and first game path, and further weeks for Complementary motion, lifecycle and sustained qualification. A credible planning envelope is approximately **8–16 weeks or longer** for a robust first supported configuration. This is an engineering estimate, not a promised schedule. Legacy motion support and Android interoperability can dominate it.

Agents can parallelize reading, isolated implementation and evidence review. They cannot eliminate serial graphics debugging or substitute generated reports for hardware measurements. If a future execution environment lacks the device, complete host work and provide an exact device-run package while marking device gates not run.

## 10. Validation and measurement protocol

### Independent reference cases

| Case | Failure it targets |
| --- | --- |
| One-pixel patterns, slanted lines and image borders | Incorrect EASU/RCAS path, coordinates, clipping and sampling |
| Stationary camera with a moving object | Missing object motion concealed by camera-only reconstruction |
| Moving camera with static terrain | Sign, scale, jitter and matrix conventions |
| Foliage, fences, distant thin geometry | Subpixel stability and animation handling |
| Water, glass, rain, particles and emissive changes | Reactive policy, alpha/color errors and history trails |
| Hands, moving player, animals and Create trains/contraptions | Distinct motion/deformation paths |
| SSR and WSR with off-screen geometry | Actual preservation of the intended reflection feature |
| ACT colored-light room with occluders | Lighting correctness and fixed-work attribution |
| Inventory, chat, translucent menus and tooltips | Native UI sharpness, alpha and composition order |
| Resize, fullscreen, minimize/restore and shader reload | Resource generation, history and surface ownership |
| Teleport, dimension change and perspective/FOV switch | Discontinuous camera/history reset |
| Two contexts/surfaces and resource deletion/reuse | State isolation, stale names and history cross-contamination |

Capture debug views of input color, depth, motion, masks, jitter, active rectangles and reset events before tuning reconstruction. Keep exact expected values for synthetic state/integer tests and justified per-operation tolerances for float comparisons. Temporal quality requires clips and region-specific inspection; one static screenshot or one global similarity score is insufficient.

### Benchmark matrix

Run the same world, camera route, settings and output size with:

1. Native scene resolution and the pack's normal final AA.
2. Reduced scene resolution and bilinear output, as a control.
3. The same reduced scene resolution and verified FSR1.
4. The same reduced scene resolution and FSR3.1, with the intended final-AA replacement.

Record any changed sample count or effect scheduling caused by pack integration so it cannot masquerade as an algorithm-only speedup. Keep the visual effects themselves equivalent.

For each run, record CPU render/submission and tick time where available; GPU scene, motion/mask, FSR, copy and finishing costs; real frame interval distribution; memory and allocation counts; output quality; power mode and thermal state. Use provider timer queries only when actually supported and valid. Avoid fetching unfinished results synchronously; handle disjoint/invalid timing. Use Perfetto or available supported profiling tools for pipeline stalls.

Use pre-generated chunks for steady rendering routes and separate chunk-generation/travel tests. Warm shaders and histories consistently. Take at least three comparable runs, alternate baseline/candidate ordering where practical, and preserve raw frame-time data. Include a sustained 20–30 minute route or an equivalent measured thermal steady state. Report all methods and failures.

### Release qualification

A supported tuple identifies device/SoC, OS build, actual provider/driver, launcher/native runtime, core/plugin/adapter hashes, Minecraft/loader/Iris/Sodium versions, shaderpack ZIP hash, integration profile and effective settings.

X2E qualification is independent. Do not inherit an X1E pass from a shared vendor name. Unknown devices and packs receive honest availability status and only a verified fallback.

The shipped setting should show the effective mode and resolution, with concise explanations such as missing compatible temporal profile. On a mode failure, restore the fallback's AA/jitter and resource contract coherently; do not keep invalid histories or partly disabled pack AA. Save a support report and keep the last working component set.

## 11. Frame generation later

The requested first deliverable is upscaling for lower real-frame cost. FSR3 frame generation introduces separate frame interpolation, presentation/pacing, buffering and UI-composition work. AMD's integration guide treats these as additional components. [R15]

Only open that project after temporal upscaling has useful sustained performance. It needs Android surface/presentation ownership, input-to-display latency, generated-versus-rendered frame reporting, VRR/pacing and lifecycle qualification. Do not count interpolated FPS as success for this upscaling plan, or imply that frame generation speeds up Minecraft simulation.

## 12. Source and reuse policy

Keep the selected AMD source and its notices intact and record every adaptation. MobileGlues source identifies LGPL-2.1-only; AMD FSR source is supplied under MIT in the relevant legacy components. Super Resolution advertises GPL-3.0 for its mod and LGPL-3.0 for native libraries. Review the actual files and licenses before copying code across these components; do not paste reference-mod code into the MobileGlues core without a compatible reuse decision. [R01], [R04], [R09]

Prefer learning interface requirements from references and implementing the required Android integration in the project's own suitable module. Check shaderpack permissions before distributing modified pack assets. Keep original user ZIPs intact and make any implementation-side profile transformations versioned, reviewable and visible in diagnostics. A profile requiring a transformed pack is a distinct tested configuration, not evidence of universal unmodified-pack temporal support.

## 13. Primary source register

Sources were checked for this research. Repository branch pages and release listings can change; execution must pin exact revisions and assets. Static MobileGlues findings were verified in a read-only git checkout because several pinned source pages were not available through the text retriever.

- **R01 MobileGlues overview and licenses:** [Open source](https://github.com/MobileGL-Dev/MobileGlues)
- **R02 Existing FSR1 host implementation at audited revision:** [Open source](https://github.com/MobileGL-Dev/MobileGlues/blob/97558a69157b033b890958b1e49258c5b51a80c6/MobileGlues-cpp/gl/FSR1/FSR1.cpp)
- **R03 Existing FSR1 shader at audited revision:** [Open source](https://github.com/MobileGL-Dev/MobileGlues/blob/97558a69157b033b890958b1e49258c5b51a80c6/MobileGlues-cpp/gl/FSR1/FSRShaderSource.h)
- **R04 AMD FSR3 product and version information:** [Open source](https://gpuopen.com/fidelityfx-super-resolution-3/)
- **R05 FidelityFX SDK v1.1.4 release:** [Open source](https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/releases/tag/v1.1.4)
- **R06 Current SDK main and its Vulkan known issue:** [Open source](https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK)
- **R07 AMD FSR3.1 upscaler integration manual:** [Open source](https://gpuopen.com/manuals/fidelityfx_sdk/techniques/super-resolution-upscaler/)
- **R08 AMD upscaler dispatch structure:** [Open source](https://gpuopen.com/manuals/fidelityfx_sdk/reference_documentation/structs/ffx_fsr3_upscaler_dispatch_description/)
- **R09 Super Resolution mod requirements and licenses:** [Open source](https://github.com/IReallyWantToSleep/superresolution/blob/dev/docs/README_EN.md)
- **R10 Super Resolution shaderpack integration guide version 3:** [Open source](https://sr.187j3x1-114514.org/docs/Shaderpack-Interface-documentation/Version-3/)
- **R11 Complementary super-resolution support discussion:** [Open source](https://github.com/ComplementaryDevelopment/ComplementaryReimagined/issues/317)
- **R12 Android Vulkan and ANGLE overview:** [Open source](https://developer.android.com/games/develop/vulkan/overview)
- **R13 Existing EGL presentation implementation:** [Open source](https://github.com/MobileGL-Dev/MobileGlues/blob/97558a69157b033b890958b1e49258c5b51a80c6/MobileGlues-cpp/egl/egl.cpp)
- **R14 Complementary settings source:** [Open source](https://github.com/ComplementaryDevelopment/ComplementaryReimagined/blob/main/shaders/lang/en_US.lang)
- **R15 AMD combined upscaling and frame-generation integration:** [Open source](https://gpuopen.com/manuals/fidelityfx_sdk/techniques/super-resolution-interpolation/)
- **R16 AMD FSR1 integration information:** [Open source](https://gpuopen.com/fidelityfx-superresolution/)
- **R17 Vulkan Android hardware-buffer interoperability:** [Open source](https://docs.vulkan.org/refpages/latest/refpages/source/VK_ANDROID_external_memory_android_hardware_buffer.html)
- **R18 EGL Android native-fence synchronization:** [Open source](https://registry.khronos.org/EGL/extensions/ANDROID/EGL_ANDROID_native_fence_sync.txt)
- **R19 Complementary official changelog:** [Open source](https://www.complementary.dev/changelogs/)
- **R20 MobileGlues release history including FSR1 introduction:** [Open source](https://github.com/MobileGL-Dev/MobileGlues-release/releases)
- **R21 Modern Super Resolution OpenGL reference release:** [Open source](https://modrinth.com/mod/superresolution/version/26.3-0.9.2-alpha.1%2Bgl-neo)
- **R22 MobileGlues shader status handling at audited revision:** [Open source](https://github.com/MobileGL-Dev/MobileGlues/blob/97558a69157b033b890958b1e49258c5b51a80c6/MobileGlues-cpp/gl/shader.cpp)
- **R23 MobileGlues program status handling at audited revision:** [Open source](https://github.com/MobileGL-Dev/MobileGlues/blob/97558a69157b033b890958b1e49258c5b51a80c6/MobileGlues-cpp/gl/program.cpp)

Existing project architecture and validation documents were read for continuity. Their runtime locks and hardware assumptions remain inputs to reverify, not new device results. Use the companion agent-prompts file to start and continue execution without losing scope or evidence.

[R01]: https://github.com/MobileGL-Dev/MobileGlues
[R02]: https://github.com/MobileGL-Dev/MobileGlues/blob/97558a69157b033b890958b1e49258c5b51a80c6/MobileGlues-cpp/gl/FSR1/FSR1.cpp
[R03]: https://github.com/MobileGL-Dev/MobileGlues/blob/97558a69157b033b890958b1e49258c5b51a80c6/MobileGlues-cpp/gl/FSR1/FSRShaderSource.h
[R04]: https://gpuopen.com/fidelityfx-super-resolution-3/
[R05]: https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/releases/tag/v1.1.4
[R06]: https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK
[R07]: https://gpuopen.com/manuals/fidelityfx_sdk/techniques/super-resolution-upscaler/
[R08]: https://gpuopen.com/manuals/fidelityfx_sdk/reference_documentation/structs/ffx_fsr3_upscaler_dispatch_description/
[R09]: https://github.com/IReallyWantToSleep/superresolution/blob/dev/docs/README_EN.md
[R10]: https://sr.187j3x1-114514.org/docs/Shaderpack-Interface-documentation/Version-3/
[R11]: https://github.com/ComplementaryDevelopment/ComplementaryReimagined/issues/317
[R12]: https://developer.android.com/games/develop/vulkan/overview
[R13]: https://github.com/MobileGL-Dev/MobileGlues/blob/97558a69157b033b890958b1e49258c5b51a80c6/MobileGlues-cpp/egl/egl.cpp
[R14]: https://github.com/ComplementaryDevelopment/ComplementaryReimagined/blob/main/shaders/lang/en_US.lang
[R15]: https://gpuopen.com/manuals/fidelityfx_sdk/techniques/super-resolution-interpolation/
[R16]: https://gpuopen.com/fidelityfx-superresolution/
[R17]: https://docs.vulkan.org/refpages/latest/refpages/source/VK_ANDROID_external_memory_android_hardware_buffer.html
[R18]: https://registry.khronos.org/EGL/extensions/ANDROID/EGL_ANDROID_native_fence_sync.txt
[R19]: https://www.complementary.dev/changelogs/
[R20]: https://github.com/MobileGL-Dev/MobileGlues-release/releases
[R21]: https://modrinth.com/mod/superresolution/version/26.3-0.9.2-alpha.1%2Bgl-neo
[R22]: https://github.com/MobileGL-Dev/MobileGlues/blob/97558a69157b033b890958b1e49258c5b51a80c6/MobileGlues-cpp/gl/shader.cpp
[R23]: https://github.com/MobileGL-Dev/MobileGlues/blob/97558a69157b033b890958b1e49258c5b51a80c6/MobileGlues-cpp/gl/program.cpp
