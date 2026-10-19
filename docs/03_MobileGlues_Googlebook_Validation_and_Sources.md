# MobileGlues for Googlebook validation and source register

Research date: 19 October 2026. This file defines what to test, what constitutes a pass, and which evidence informed the architecture and implementation plan.

Source inspection, downloaded-artifact inspection, calculated resource sizes, and proposed acceptance targets are identified separately. No Snapdragon device benchmark or qualified game run was performed for this plan. Staged game-process runs and provider probes exist on the HP X1E unit and are recorded in `mobileglues-googlebook/`; the supported 21.1.262 tuple is staged/working, but none of these results is acceptance or release qualification.

## 1. Initial runtime lock

These are selected upstream inputs for qualification. They are not an already-certified Googlebook distribution.

| Lane | Minecraft | Java major | Iris | Sodium | Status |
| --- | --- | --- | --- | --- | --- |
| Delivery (current required) | 1.21.1 | 21 (`java-runtime-delta`) | 1.8.14-beta.1+1.21.1-neoforge, Modrinth `KduFYu4t` | mc1.21.1-0.8.13-neoforge, Modrinth `uMOpc5uV` | Staged/working on the installed NeoForge 21.1.262 tuple (world load, render, save/quit). NeoForge 21.1.264 is an optional future patch, NOT RUN and not a blocker; effect fidelity, performance and lifecycle remain open |
| Delivery fallback | 1.21.1 | 21 | 1.8.12+1.21.1-neoforge, `t3ruzodq` | mc1.21.1-0.6.13-neoforge, `Pb3OXVqC` | Stable pair; never pair Iris 1.8.12 with Sodium 0.8.13 |
| Future/experimental | 26.3 | 25 | 1.11.7 Fabric, `vTN4NRGW` | 0.9.2 Fabric, `bAZQdGpg` | Not required for release; exact published dependency selection, unqualified |
| Legacy required | 1.20.1 | 17 | 1.7.6 | 0.5.13 candidate | Resolve metadata discrepancy and validate the pair |
| Optional engine comparison | 1.21.11 | 21 | 1.10.8 | 0.8.14 | Add if it isolates a regression; not an initial release multiplier |

The delivery lane is Minecraft 1.21.1 on NeoForge with Java 21 and LWJGL 3.3.3 GLFW; it is the lane ordinary shaderpack ZIPs must load in. The supported X1E target is the installed Minecraft 1.21.1 / NeoForge 21.1.262 / Iris 1.8.14-beta.1 / Sodium 0.8.13 tuple, which is staged/working through a world load, render and save/quit pass. NeoForge 21.1.264 is an optional future patch: it is not a delivery target and not a release blocker. No pack is effect-by-effect or release-qualified yet.

### Delivery tuple identities

```text
Minecraft 1.21.1 manifest
https://piston-meta.mojang.com/v1/packages/499680d659b7a0ccb2fa9a809c345c5baf2ace7d/1.21.1.json
client SHA-1: 30c73b1c5da787909b2f73340419fdf13b9def88
Java: java-runtime-delta, major 21; LWJGL 3.3.3 (GLFW path)

NeoForge 21.1.262 (supported X1E target, installed launcher instance neoforge-21.1.262)
Installer SHA-1/SHA-256: not recorded from the installed instance.

NeoForge 21.1.264 (optional future patch; not a delivery target and not a release blocker)
https://maven.neoforged.net/releases/net/neoforged/neoforge/21.1.264/neoforge-21.1.264-installer.jar
SHA-1:   f801601bafa7f5957295353930e2cb61dcfddc6c
SHA-256: d7c035dd5436a7c524dfbad6df191b0ef34179fd3edfee9dc99262af3b80239b

Iris 1.8.14-beta.1+1.21.1-neoforge (Modrinth KduFYu4t)
file:      iris-neoforge-1.8.14-beta.1+mc1.21.1.jar
size:      2540723
SHA-512:   33fddb430e16222902eac75b8b3d57c381b0f3cc729f3b56c4628df12ca108f23a0b11e2feb167923d6d14d1fc7e8ee1a977ec421b19d1951bdca74c4092b9c5

Sodium mc1.21.1-0.8.13-neoforge (Modrinth uMOpc5uV)
file:      sodium-neoforge-0.8.13+mc1.21.1.jar
size:      1292934
SHA-512:   4f537696af95411e9daf15795fb5fcbc49913d488a96fa912fa8f1d7db408710995e8a1461190f3cd0f8217bf9e35e9539176592428ed587ca2d7aa84450a390

Fallback pair: Iris 1.8.12+1.21.1-neoforge (t3ruzodq) with
Sodium mc1.21.1-0.6.13-neoforge (Pb3OXVqC)
```

The delivery Iris release is a beta. Its metadata points to Sodium 0.8.13; the stable fallback pairing must be recorded and tested as its own tuple rather than assumed interchangeable. Stage 0 must document which complete tuple it selects and why; do not hide that in an automatic updater.

Pin the NeoForge version, relevant NeoForge/support mods, launcher, Android JVM package, native modules, renderer/provider builds, resource packs and game settings as well. The lane rows do not constitute the whole runtime lock.

For the 1.20.1 machinery/train test, select a compatible Create Fabric build and its required rendering support mods explicitly. A Forge/Create/Oculus/Embeddium configuration is a separate lane. Iris/Fabric results do not certify it. Do the equivalent NeoForge-versioned selection for a delivery-lane Create test before claiming modded-geometry coverage there.

Sources: [Minecraft 1.21.1 manifest](https://piston-meta.mojang.com/v1/packages/499680d659b7a0ccb2fa9a809c345c5baf2ace7d/1.21.1.json), [NeoForge 21.1.262](https://maven.neoforged.net/releases/net/neoforged/neoforge/21.1.262/neoforge-21.1.262-installer.jar), [NeoForge 21.1.264 (optional future patch)](https://maven.neoforged.net/releases/net/neoforged/neoforge/21.1.264/neoforge-21.1.264-installer.jar), [Iris 1.8.14-beta.1](https://modrinth.com/mod/iris/version/KduFYu4t), [Sodium 0.8.13](https://modrinth.com/mod/sodium/version/uMOpc5uV), [Iris 1.8.12](https://modrinth.com/mod/iris/version/t3ruzodq), [Sodium 0.6.13](https://modrinth.com/mod/sodium/version/Pb3OXVqC), [Iris 26.3 release](https://modrinth.com/mod/iris/version/vTN4NRGW), [Sodium 26.3 release](https://modrinth.com/mod/sodium/version/bAZQdGpg), [Mojang 26.3 manifest](https://piston-meta.mojang.com/v1/packages/11f1aefe473eefba0af62e3286c33b692807b2eb/26.3.json), publisher API records and JAR metadata inspected during this audit.

## 2. Shaderpack lock and qualification role

| Pack | Selected version | Publisher version ID | Initial role |
| --- | --- | --- | --- |
| Complementary Reimagined | r5.9.3 | `Bqen1mJX` | Required standard corpus; required ACT 192 and WSR 128 advanced corpus |
| Complementary Unbound | r5.9.3 | `B1kyfoUZ` | Required distinct visual style and standard corpus; advanced corpus as scheduled |
| Complementary Reimagined/Unbound | r5.9.2 | Resolve original regression assets in S0 | Regression against the prior source generation; carries the staged ACT/WSR evidence |
| BSL | 10.1.8 | `Y68yiql9` | Required independent standard pack; advanced option test |
| Photon | v1.4 | `BnGklH5y` | Independent compute-bearing pack |
| Rethinking Voxels | r0.1-beta9 | `cpD4esk9` | Advanced stress on a publisher-listed game version |

Photon v1.4 was published on 19 October 2026. Its newness is a reason to pin it and retain a prior-version comparison, not to assume stability. Rethinking Voxels' inspected version metadata does not list 26.3; start its tests on a lane its metadata explicitly supports.

### Original ZIP identities

The following SHA-256 values were calculated from downloaded publisher assets. Publisher SHA-512 checks also matched. Recheck acquisition metadata before using them in a distribution.

```text
ComplementaryReimagined_r5.9.3.zip
fed6c879e732cf7fc7bc6570ebb1def43235aa7e96f62859fbdbdbfa0b484b38

ComplementaryUnbound_r5.9.3.zip
98ac741dee294d2a3c82d153349148c3bb7fb688c30eb3b49c3b9ac37e968c5a

BSL_v10.1.8.zip
36b0a50ff7918bf10e9422c401d93779f9d0088a26acea975b435243f96930b5

photon_v1.4.zip
da0929b38f5fd99befaaa8cd141cbf60e1fad05b92d916d0eeeee7c79f86c0fd

rethinking-voxels_r0.1-beta9.zip
a8cd9d4338c7d3d1fffecc4525348a31ea3d219ef31382c51d5ddc3b4b0114e1
```

The inspected Complementary GitHub main commit `c09950df0650b27dac260c1b0f69afc11387aa6f` identifies r5.9.2, while the official ZIPs are r5.9.3. Comparing normalized source text found changes in three rendering files; the profile/resource declarations discussed here match. Pack identity must therefore use the downloaded asset hash.

## 3. Actual Complementary feature tiers

These settings were read from the original r5.9.3 assets and matched to the published source.

| Profile | ACT setting | ACT volume | WSR | Initial release role |
| --- | ---: | --- | --- | --- |
| High | 0 | Disabled | Off | Required standard fidelity |
| Very High | 192 | 192 × 96 × 192 | Off | Required advanced lighting |
| WSR 128 (custom) | 128 | 128 × 64 × 128 | On | Required advanced WSR target |
| Ultra | 256 | 256 × 128 × 256 | On | Optional/future X2E; NOT REQUIRED for the X1E release. `BLOCKED` on X1E: 202,899,456 B exceeds the 134,217,728 B storage-block limit |
| Complementary | 512 | 512 × 256 × 512 | On | Optional extended stress; not a release claim |

ACT means Advanced Color Tracing, the pack's voxel-based colored-lighting feature. WSR means world-space reflections. Test standard SSR separately from these two features. [Profile declarations](https://github.com/ComplementaryDevelopment/ComplementaryReimagined/blob/main/shaders/shaders.properties) [Option definitions](https://github.com/ComplementaryDevelopment/ComplementaryReimagined/blob/main/shaders/lang/en_US.lang)

### Hard requirements found in the shaders

- ACT's compute flood-fill declares an 8 × 8 × 8 local group, or 512 invocations.
- The shadow vertex path writes integer 3D images.
- ACT uses an R16UI voxel volume and two RGBA16F lighting volumes.
- WSR adds integer images and a `std430` storage buffer with packed unsigned data.
- Optional player reflections add another storage buffer and atomic operations.
- The pipeline uses formats beyond RGBA8/RGBA16F, including normalized signed and packed HDR formats.

The original compute dispatch requires each local-size dimension to support at least 8 and the total workgroup limit to support at least 512 invocations. A provider below those limits cannot execute it directly. Splitting or reducing workgroups needs a separate equivalence proof covering invocation identity, shared memory, barriers and ordering.

Sources: the hashed ZIP's `shaders/program/shadowcomp.glsl`, `shaders/program/shadow.glsl`, `shaders/lib/voxelization/SSBOs/`, and `shaders/lib/pipelineSettings.glsl`; corresponding [public source](https://github.com/ComplementaryDevelopment/ComplementaryReimagined/tree/c09950df0650b27dac260c1b0f69afc11387aa6f/shaders).

No geometry or tessellation shader files were found in these Complementary assets. Their absence here does not remove those features from the broader shaderpack inventory.

Logical `colortex` indices can be high without requiring that many simultaneous draw buffers. The inspected static directives include five-output passes, while Iris maps logical targets into per-pass outputs. Count active outputs after preprocessing and loader transformation; do not derive MRT requirements from the largest logical index.

### Ultra resource gate (optional/future X2E)

The required X1E advanced target is WSR 128: 128 × 64 × 128 images and a 50,855,936-byte SSBO, measured `PASS` on the X1E provider. The ACT 256/WSR 256 figures below are retained for future X2E sizing and are not an X1E release gate.

Calculated raw allocations for ACT 256 with WSR are:

| Resource group | Size |
| --- | ---: |
| ACT images | 144 MiB |
| WSR images including LOD | 8.0625 MiB |
| Single WSR SSBO | 193.5 MiB |
| Selected ACT/WSR resource subtotal | 345.5625 MiB |
| Ultra rain-puddle image | 0.015625 MiB |
| Listed resources including puddle image | 345.578125 MiB |

The single SSBO requests **202,899,456 bytes**, which is future X2E work and not an X1E release gate; the X1E provider reports a 134,217,728-byte limit. Check `GL_MAX_SHADER_STORAGE_BLOCK_SIZE` and real access across the whole range. Sufficient system RAM cannot compensate for a smaller accessible storage-block range. The listed resources exclude ordinary framebuffers, history, atlas/world data, other optional resources, driver overhead and Java memory. Ultra's `RAIN_PUDDLES=2` enables the listed 128 × 128 R8UI image. Optional player reflections additionally allocate a 5,376-byte SSBO and a 64 × 64 RGBA8 atlas; qualify that option separately. On the X1E ANGLE provider the enabled option uses vertex-stage atomics and an image store that lose the device; the wrapper must not silently rewrite the option, so it remains NOT RUN until that path is implemented. [Allocation declarations](https://github.com/ComplementaryDevelopment/ComplementaryReimagined/blob/main/shaders/shaders.properties)

The size calculation uses declared dimensions and bytes per texel; it is not a measurement of physical GPU allocation. Padding and backing-format expansion can increase actual use. Reducing the grid or disabling WSR produces a different profile.

## 4. Additional pack distinctions

| Pack mode | Source-derived requirement | Required test |
| --- | --- | --- |
| BSL 10.1.8 ordinary High | Conventional raster effects; optional multicolored lighting is disabled in default settings | Standard targets, shadows, TAA, PBR and postprocessing |
| BSL multicolored blocklight | Integer voxel image, floating lighting volumes and a compute pass | Independent lighting visibility and resource test |
| Photon v1.4 default skylight | Compute reduction, local size 256, RGBA16F image and shared memory | Default settings must execute the reduction correctly |
| Photon optional subgroup path | Subgroup arithmetic when enabled | Detect support and compare allowed implementation paths |
| Rethinking Voxels r0.1-beta9 | Declares custom images, SSBOs and compute as required | Run on a compatible game version with all required features active |

Do not label every non-Ultra pack as compute-free. Do not treat a pack requiring SSBOs as merely a GLSL-version conversion test.

Sources: original hashed assets; [BSL](https://modrinth.com/shader/bsl-shaders/version/Y68yiql9), [Photon](https://modrinth.com/shader/photon-shader/version/BnGklH5y), [Rethinking Voxels](https://modrinth.com/shader/rethinking-voxels/version/cpD4esk9).

## 5. Feature contract and test mapping

The release contract is a truthful desktop GL surface for ordinary shaderpacks: the core version MobileGlues actually implements plus individually earned extensions, covering GLSL translation, framebuffer/target routing, image/SSBO resource mapping, compute dispatch, and each advertised barrier/fence/visibility rule. An unimplemented feature stays a named compatibility deviation; it is not advertised as a capability. This is not a claim of full OpenGL 4.6 conformance, and a pack that silently disables an effect does not pass.

For each contract, record the pack/program/options that require it, desktop semantics, provider requirements, actual limits, exact emulation if any, expected cost, and proving tests.

| Contract | Main risk | Minimum proving test |
| --- | --- | --- |
| Source, compile, link and reflection | Lost source chunks, locations, bindings or shared program state | Multi-string source and interleaved program/link tests |
| Samplers and texture buffers | Wrong type, integer values, LOD, logical/physical units | Sample arrays and buffers with high logical indices and bounded active resources |
| MRT and per-target state | Color written to the wrong target or cleared/blended incorrectly | Sparse and permuted output lists, indexed clear/blend and readback |
| Formats and depth | Lossy conversion, wrong signedness, incompatible image access | Per-format round trips and sampled/rendered values |
| SSBOs | Wrong layout, binding/range or upper-bound access | `std430` structures, remapping, aliases, range boundaries and large-block sentinels |
| Images and atomics | Unsupported stage/format or lost writes | Vertex/fragment/compute writes, atomics and subsequent consumers |
| Compute | Wrong invocation count or resource preparation | Every local invocation contributes to a deterministic integer result |
| Barriers and sync | Stale data, illegal feedback or CPU/GPU races | Draw/dispatch transitions, indirect commands, fences and controlled feedback |
| Buffer storage/mapping | False coherent/persistent behavior | Explicit flush, coherent map when supported, fence-safe wraparound and CPU reads |
| Indirect/base-instance drawing | Incorrect instance IDs or index offsets | Nonzero base vertex/instance and equivalent direct/indirect outputs |
| Texture views and layers | Broken storage aliasing or wrong layer/format | Write through one view, read through another with valid semantics |
| Queries | Wrong timing type, units, availability or blocking | Pending/completed results, disjoint handling and independent wall-clock comparison |
| Context and lifecycle | Stale metadata, reused names, lost targets/history | Shared contexts, deletion/reuse, surface-only replacement and real context loss |

Depth-texture filter completeness, 64-bit timestamp queries, same-format D32F `glCopyImageSubData`, logical attachment invalidation after a draw-buffer shuffle, multisample framebuffer resolve, profile-bound GLSL cache identity, geometry and tessellation stages, and depth/stencil blit preflight are implemented and tested on the X1E tuple (`validation/2026-10-19-x1e-wrapper-depth-filter.json`, `...-timer-query.json`, `...-copy-image-depth.json`, `...-all-invalidate.json`, `...-blit-resolve.json`, `...-all-cachekey.json`, `...-geometry-stage.json`, `...-tessellation-stage.json`, `...-all-stages.json`, `...-blit-depth-match.json`, `...-copy-image-depth-mismatch.json`, `...-clear-tex-level.json`, `...-depth-sampler-compute.json`, `...-all-audit.json`; full suite 46/46). Geometry and tessellation use desktop `#version 400 core` and render correctly through glslang/SPIRV-Cross and ANGLE. `glPatchParameterfv` is a named deviation: ES has no default tessellation-level entry point, so valid calls report `GL_INVALID_OPERATION`; a TCS must write `gl_TessLevel*`. The depth guard now handles default-framebuffer and combined/separate stencil attachment tokens, covers `glCopyTexImage2D`, and is verified by matching and mismatched D24S8/D24 cases. The 3D image-store clear uses the requested level and share-group-owned programs, and compute dispatches run the depth-sampler guard; the sampler mutators are exported so the wrapper shadow actually sees them (`validation/2026-10-19-x1e-wrapper-all-audit.json`, 46/46). Mutable multi-level 3D image textures remain a named limitation. The cache key includes source, stage, ESSL/backend, texture-buffer and texture-query-LOD preprocessing, wrapper version including PATCH, and wrapper settings that change generated GLSL; host `cache_key_test` proves each input changes the key. `glTextureBarrier`/NV and `glTextureView` remain named deviations: ES has no legal feedback barrier equivalent, and the X1E provider exposes no `GL_EXT_texture_view`. The pinned Iris/Sodium builds do not call those entry points. Backing-format copy substitutions remain backlog.

Exact integer and API-state results should match exactly. Floating comparisons require an operation-specific tolerance, usually expressed as an absolute error plus a relative error term. Base tolerances on the specified representation and operation; do not hide a format substitution behind a broad full-frame threshold.

Run negative tests too. A rejected unsupported format or over-limit configuration should produce a meaningful failure. False success is a failed test.

Use relevant Khronos GL/GLES specifications and tests as contract references. Run portions of conformance suites where practical, while distinguishing a selected test pass from formal API conformance. [OpenGL registry](https://registry.khronos.org/OpenGL/index_gl.php) [Buffer storage contract](https://registry.khronos.org/OpenGL/extensions/ARB/ARB_buffer_storage.txt)

## 6. Device qualification report

The probe must execute inside the same native environment as the game. Include:

| Group | Required observations |
| --- | --- |
| Identity | OEM/model, SoC, OS build, SDK, ABI, page size, RAM |
| Provider | Requested and actual EGL/GLES implementation, strings/extensions, loaded binary identity |
| GL contract | Reported context version/profile, limits, functions and feature tests |
| Formats | Allocation, filtering, renderability, blending, image/atomic use, clearing/copy/readback |
| Vulkan | Device/driver IDs or UUIDs where exposed, extensions, features, limits, format support, queues and surface capabilities |
| Runtime | JVM, game manifest, launcher/native bridge, Iris/Sodium and renderer hashes |
| Behavior | Compilation, representative execution, surface lifecycle and recovery |
| Performance | Cold/warm times, frame-time distribution, memory and sustained conditions |

Use explicit states: `PASS`, `FAIL`, `BLOCKED`, `NOT RUN`, or `NOT APPLICABLE`. Record an explanation for the latter three.

The following illustrates the record shape; null values deliberately represent measurements not yet obtained.

```json
{
  "schema_version": 1,
  "qualification_status": "NOT RUN",
  "target": {
    "soc": "Snapdragon X1E-80-100",
    "model": "XPS Googlebook"
  },
  "observed": {
    "os_build_fingerprint": null,
    "sdk": null,
    "page_size": null,
    "gpu_driver_identity": null,
    "requested_provider": null,
    "actual_provider": null
  },
  "runtime_lock_sha256": null,
  "renderer_build_sha256": null,
  "capability_report_sha256": null,
  "test_results": [],
  "recommended_presets": []
}
```

A shipped profile must not contain unmeasured values for any field needed to match its qualification identity. Do not ship this example as a real device report.

## 7. Reference scene corpus

Use a versioned world and camera-path manifest. The desktop reference must run the same content and enabled effect set.

| Scene | Controlled content | Main failures revealed |
| --- | --- | --- |
| R01 Sun and shadows | Fences, foliage, thin geometry, glass and entities | Alpha handling, depth precision, shadow bias, per-target state |
| R02 Water and SSR | Reflective objects, underwater transition and camera movement | Depth reconstruction, reflection sampling and temporal reuse |
| R03 PBR wall | Known normal/roughness/material channels, close and oblique views | Signed channels, derivative/LOD/POM behavior |
| R04 ACT room | Multiple colored lights, stairs, doors and occluders | Voxel writes, flood fill, leakage beyond reference behavior and barriers |
| R05 WSR room | Reflectors with off-screen objects inside the pack's voxelized/reflection volume and supported geometry set; optional player model | WSR actually active; packed geometry and storage reads |
| R06 Temporal motion | Fast pans, disocclusion, teleport and dimension change | History corruption, reprojection and stale resources |
| R07 Weather and dimensions | Day/night, rain, Nether and End | Variant compilation and dimension-specific programs |
| R08 Desktop transitions | Resize, maximize, fullscreen, minimize, restore and display change | Surface dimensions, target ownership, focus/input and history |
| R09 Modded geometry | Pinned material/entity mods, then Create machines and trains on the pinned lane | Vertex formats, IDs, dynamic geometry and CPU/GPU attribution |
| R10 Optional LOD integration | Distant Horizons or Voxy in separate instances | Explicit loader/pack integration outside vanilla coverage |

Select a deterministic workload for each scene. Record seed, world hash, coordinates/path, resolution, render distance, simulation distance, shadow settings, texture/resource packs, exposure controls, weather/time and temporal warmup.

Use pre-generated chunks for steady rendering measurements. Measure chunk-generation/travel workloads separately instead of excluding their stalls after the run.

## 8. Visual acceptance procedure

1. Capture a known-good reference with the pinned game/loader/pack/options and controlled scene.
2. Verify the target's actual backend, original ZIP hash and effective options.
3. Compare selected intermediate targets to find the first incorrect pass.
4. Compare feature-specific image regions as well as the full frame.
5. Compare short sequences for temporal effects and lifecycle transitions.
6. Review any difference against repeated reference variance and pack-inherent limitations.
7. Retain a report with the exact options, source/driver identities and disposition.

A global image-similarity score cannot certify every effect. A missing reflection or colored-light system can occupy only a small part of the image. Require named feature tests and a clear signal that each requested effect executed.

Freeze numeric thresholds only after measuring repeated reference runs and acceptable cross-driver variance. For diagnostics with exact expected values, use exact checks immediately. For floating rendering, document the tolerance and its reason.

Pack-inherent SSR loss of off-screen objects is expected. WSR-specific scenes must reflect their intended off-screen geometry within the pack's supported volume and representation. Respect the pack's finite distance and geometry limits when deciding whether a difference is a translator defect.

Instrument pack-program compilation, execution, image/SSBO allocation and selected options. Disabled advanced passes cannot earn an advanced-feature pass.

## 9. Performance objectives and measurement

The numbers below are proposed engineering objectives for planning. No cited source establishes that X1E or X2E achieves them.

| Profile objective | Internal resolution | Initial goal | Qualification rule |
| --- | --- | --- | --- |
| Minimum playable standard experience | 1920 × 1200 | Sustained 30 FPS | Mandatory before a device receives a recommended standard configuration |
| Smooth standard experience | 1920 × 1200 | 60 FPS target | Use an explicitly selected standard preset; measure sustained frame times |
| Native-panel standard quality | 2560 × 1600 | 30 FPS target | Preserve the selected effects and disclose the measured preset |
| Advanced lighting (ACT 192) | Start at 1920 × 1200 | 30 FPS target | Keep the specified ACT setting enabled |
| Advanced WSR 128 | Start at 1920 × 1200 | Correctness first; establish practical frame rate | Required advanced WSR target; never claim an unmeasured 30/60 FPS result |
| Optional Ultra WSR 256 | Start at 1920 × 1200 | Future X2E only | Not an X1E release gate; `BLOCKED` on X1E by the provider storage-block limit |
| High-refresh lightweight play | Device-appropriate size | 90/120 FPS exploratory | Independent target for eligible vanilla/light profiles |

Use GPU work time and presentation intervals separately. For a 60 FPS target the frame period is approximately 16.67 ms; for 30 FPS it is approximately 33.33 ms. An average alone can hide substantial stutter.

Before certification, freeze per-profile thresholds for sustained presentation rate, p95/p99 frame intervals, long-frame frequency, and cold/warm compile times. Derive tolerances from a controlled baseline and the intended frame-pacing mode, then record them before evaluating the release candidate. Collecting measurements or documenting a missed target does not constitute a performance pass. The minimum 30 FPS standard gate is distinct from the preferred 60 FPS objective; a 60 FPS recommendation must meet its own budgets.

### Benchmark protocol

- Fix OS build, driver/provider, runtime, pack/options, window/internal resolution and workload.
- Record external power/battery condition, brightness, ambient conditions where practical, and background workload.
- Measure at least three comparable runs. Warm shaders, chunks and thermal behavior deliberately and describe the warmup.
- Measure a fixed steady-state route for at least ten minutes per performance candidate.
- Record median, p95 and p99 CPU/GPU or frame intervals, long-frame counts, presentation rate and peak/steady memory.
- Report cold shader compile and warm reload separately. Initial usability goals are under 60 seconds for a standard pack's first compilation and under 10 seconds for a warm reload, excluding downloads; revise only through a recorded product decision after measurement.
- Keep profiler overhead out of the final timing run. Use traces to diagnose, then repeat uninstrumented.
- Compare identical quality. A different internal resolution, shadow distance or feature toggle is a separate profile.

For perspective, 2560 × 1600 has about 1.78 times the pixels of 1920 × 1200. One RGBA16F image at the larger size contains 31.25 MiB of raw texels; eight such images would contain 250 MiB. These are arithmetic illustrations, not a claim about a particular pack's full render graph.

Measure JVM heap, native memory, compiler peaks and GPU resources together. Collect GPU memory-budget data when the driver exposes it; otherwise record the available allocator and process measurements with their limitations.

## 10. Stability and product gates

The following are proposed release tests. Adjust duration only through an explicit test-plan revision.

| Gate | Required exercise | Pass condition |
| --- | --- | --- |
| Fresh installation | Clean release install, normal sign-in, instance creation and original ZIP import | Supported scene reached with ordinary user actions |
| Reload | 50 pack/options reload cycles | No crash, accumulating leak, stale program or wrong effect |
| Desktop lifecycle | 50 mixed resize/focus/minimize/display transitions | Correct size/input, valid resource ownership and restored rendering |
| Sustained play | Two-hour standard session and one-hour advanced session | No crash/hang, persistent corruption or unbounded memory growth |
| Update | Working build to new build, interrupted update and rollback | Consistent component set and preserved worlds/options |
| Provider change | Restart into another qualified provider | Clean context/resource/cache separation |
| Memory pressure | Controlled low-memory conditions and relaunch | Clear failure/recovery; no corrupted saves or false-success rendering |
| Native packaging | Dependency and ELF/APK checks plus target page-size execution | All selected native components load and run |
| Unknown device/driver | Remove profile match or install a changed build | Conservative eligibility and truthful qualification status |

A process that still runs after a hidden rendering failure fails the test. A restored surface that loses valid shared resources unnecessarily also fails lifecycle correctness.

## 11. Source audit findings to reproduce

All entries below refer to core commit `97558a69157b033b890958b1e49258c5b51a80c6`. They are static source findings, not reproduced defects of the distributed APK.

| Finding | Source area | First proving test |
| --- | --- | --- |
| Error reporting always returns success; a fragment-sampler limit is multiplied | `gl/getter.cpp` | Deliberate GL errors and provider-versus-frontend limit comparison |
| Translated source uses a one-element array with the original count | `gl/shader.cpp` | Two or more source chunks |
| Latest-shader/global metadata participates in program linking | `gl/program.cpp`, shader records | Interleaved compilation and per-program output binding |
| Binding/LOD/derivative/effect substitutions exist in translation | `gl/glsl/glsl_for_es.cpp` | Exact compiler corpus with the substitutions identified |
| Some advertised storage/clear/view/barrier/query functions remain stubs | `gl/gl_stub.cpp`, `DSAWrapper.cpp` | Reachability and complete advertised-contract tests |
| Execution preparation does not cover all direct/indirect paths | `gl/gl_native.cpp`, `gl/drawing.cpp` | Resource remapping exercised through every execution entry point |
| Format and persistent-map policies change representations/flags | `gl/texture.cpp`, `gl/buffer.cpp`, settings | Format round trips and mapping visibility tests |
| Translation cache omits important semantic inputs | `gl/glsl/glsl_for_es.cpp`, cache code | Stage/policy/build collision and stale-cache tests |

Use this [pinned source root](https://github.com/MobileGL-Dev/MobileGlues/tree/97558a69157b033b890958b1e49258c5b51a80c6/MobileGlues-cpp) to resolve the paths.

Existing compute, image and memory-barrier forwarding, framebuffer shuffling, atomic cache persistence and output-validated multidraw benchmarking are valuable starting points. Validate and extend them instead of duplicating them.

### Source pins

```text
MobileGlues core main:
97558a69157b033b890958b1e49258c5b51a80c6

MobileGlues-plugin main:
f5105e4ddd1752cccdcc6421c402960d40ec3d9d

Core pinned by the inspected V2.0.0 plugin:
f56af31d3e490d95808b3adc4ace20234249a37a

Core glslang submodule:
f5f664dee8146676b04a332a7233959fc3ce9681

Core SPIRV-Cross submodule:
a0fba56c34a6700f1724bf9b751da5b488a3775c

Complementary public source main inspected:
c09950df0650b27dac260c1b0f69afc11387aa6f

Iris 26.3 branch inspected:
671a5c87747d33983d7ed659461a64f46dfcb1f0

Sodium development source inspected:
148151cf25c56f8b670986b348dcc589278c867e
```

Development branch commits are not automatically the source identities of released JARs. Resolve release provenance before distributing downstream builds.

## 12. Game artifact evidence

### 12.1 Delivery lane: Minecraft 1.21.1

The official 1.21.1 manifest selects Java 21 (`java-runtime-delta`) and LWJGL 3.3.3 with GLFW, OpenGL, OpenAL, STB, FreeType, jemalloc, JNA and tinyfd modules. Like the 26.3 manifest, it does not list Android-native classifiers, so the launcher must supply the Android window/EGL surface and the desktop-GL bridge. The client artifact is `30c73b1c5da787909b2f73340419fdf13b9def88`, installed through NeoForge 21.1.262, the supported X1E target. NeoForge 21.1.264 is an optional future patch only.

The supported tuple is the installed FCL Minecraft 1.21.1 / NeoForge 21.1.262 instance with Iris 1.8.14-beta.1 and Sodium 0.8.13. An early game-process start rendered the main menu while the installed MobileGlues 2.0.0 build rejected `#version 400` shaders and produced `iris_Normal` syntax errors. A workspace-built plugin then initialized the original Complementary packs at High without fallback, and a later whole-source-parser build loaded and rendered the world and saved/quit to title. These are staged/working results on the supported tuple; effect fidelity, performance and lifecycle gates remain open. NeoForge 21.1.264 is an optional future patch with no run and is not a blocker.

A later workspace-plugin run reached terrain and then crashed; FCL's uploaded log is `https://mclo.gs/KgWJ5oC`. The renderer log exposed every active image uniform mapped to core unit 0, which the whole-source image parser now fixes and the X1E wrapper suite covers. The same log contains an independent `SodiumServiceModLocator` duplicate-class `LinkageError` and repeated `Failed to create fence object`; isolate the Sodium/mod instance before attributing that crash to the renderer. Neither run qualifies the lane.

A subsequent whole-source-parser build loaded the same 1.21.1 / NeoForge 21.1.262 world, rendered the original pack, logged distinct image units 0–3 for the affected programs, and saved and quit to title. The earlier Sodium/fence failure did not reproduce. This is the staged/working reference pass for the supported 21.1.262 tuple; effect-by-effect fidelity, performance and lifecycle gates remain open. NeoForge 21.1.264 is an optional future patch and is not required. [Minecraft 1.21.1 manifest](https://piston-meta.mojang.com/v1/packages/499680d659b7a0ccb2fa9a809c345c5baf2ace7d/1.21.1.json) [NeoForge 21.1.262](https://maven.neoforged.net/releases/net/neoforged/neoforge/21.1.262/neoforge-21.1.262-installer.jar)

The delivery lane's Iris and Sodium binaries must each be audited for loader/LWJGL/provider restrictions. Sodium 0.8.13 has not been audited against this exact MobileGlues build; any Android adaptation is a maintained downstream build with its own version and hash.

### 12.2 Future lane: Minecraft 26.3 artifact evidence

This subsection is retained as future-lane evidence. It describes neither a required lane nor a delivery-lane blocker.

The 26.3 manifest selects Java 25 and LWJGL 3.4.3, including SDL, ShaderC, SPIRV-Cross, VMA and Vulkan modules. It does not list GLFW or Android-native classifiers.

The official client SHA-1 is `e877b6a07acd633fb3bb475002175cec036e7b87`. Inspection used `javap -c -p` on:

- `com.mojang.renderpearl.backend.opengl.GlBackend`
- `com.mojang.renderpearl.backend.opengl.GlPipelineRecompiler`
- `com.mojang.renderpearl.frontend.shaders.GlslCompiler`

The bytecode confirms that `GL.getFunctionProvider()` is cast to `org.lwjgl.system.SharedLibrary`, its `getPath()` is passed to SDL's loader, and SDL/LWJGL function addresses must agree. It also confirms an OpenGL shader path using ShaderC, SPIRV-Cross outputting GLSL 330, then ordinary GL source/compile calls. These observations concern that exact client artifact.

[Official manifest](https://piston-meta.mojang.com/v1/packages/11f1aefe473eefba0af62e3286c33b692807b2eb/26.3.json) [Official client artifact](https://piston-data.mojang.com/v1/objects/e877b6a07acd633fb3bb475002175cec036e7b87/client.jar)

Audited Iris 26.3 source still uses the OpenGL shader path and contains a guard against the native Vulkan mode. Audited Sodium development source contains Android/Pojav rejection checks. Resolve the exact selected release behavior, maintain any necessary compatibility adaptation visibly, and validate its semantics. Removing a guard or hiding an environment variable is not a qualification result.

## 13. Source register

### Platform and hardware

1. [Google Store XPS Googlebook specifications](https://store.google.com/us/product/xps_googlebook_specs?hl=en-US) — exact X1E-80-100 reference model.
2. [Googlebook launch overview](https://blog.google/products-and-platforms/devices/googlebook/first-look-googlebook/) — launch hardware and availability.
3. [Googlebook adaptive development](https://android-developers.googleblog.com/2026/09/adaptive-development-scale-app-googlebook.html) — Android desktop application foundation.
4. [Googlebook Linux Terminal preview](https://developer.android.com/blog/posts/googlebook-linux-terminal-preview-vm?hl=en) — current VM scope and acceleration roadmap.
5. [Android Vulkan and ANGLE guidance](https://developer.android.com/games/develop/vulkan/overview) — Android 17 system ANGLE preference and provider behavior.
6. [ANGLE Android setup](https://chromium.googlesource.com/angle/angle/+/main/doc/DevSetupAndroid.md) — Android builds and development-provider restrictions.
7. [Android 16 KB page sizes](https://developer.android.com/guide/practices/page-sizes) — native alignment and verification.
8. [Android 17 target behavior changes](https://developer.android.com/about/versions/17/behavior-changes-17) — readonly native dynamic loading and other target-SDK behavior.
9. [Android desktop windowing](https://developer.android.com/develop/adaptive-apps/guides/support-desktop-windowing) — desktop windows, insets and interaction.
10. [Android multi-window lifecycle](https://developer.android.com/develop/ui/views/layout/support-multi-window-mode) — lifecycle behavior in multiple windows.

### Graphics implementations and contracts

11. [MobileGlues core](https://github.com/MobileGL-Dev/MobileGlues) — project scope and implementation.
12. [MobileGlues V2.0.0](https://github.com/MobileGL-Dev/MobileGlues-release/releases/tag/V2.0.0) — released artifact, ANGLE integration and benchmarking.
13. [MobileGlues plugin](https://github.com/MobileGL-Dev/MobileGlues-plugin) — Android launcher/plugin packaging.
14. [MobileGL](https://github.com/MobileGL-Dev/MobileGL) — distinct in-development alternative.
15. [Zink requirements](https://docs.mesa3d.org/drivers/zink.html) — feature/format/limit requirements for a complete GL alternative.
16. [Mesa 26.2 release notes](https://docs.mesa3d.org/relnotes/26.2.0.html) — contemporary X2 device support work.
17. [Mesa 26.1 release notes](https://docs.mesa3d.org/relnotes/26.1.0.html) — A8xx/KGSL development evidence.
18. [OpenGL registry](https://registry.khronos.org/OpenGL/index_gl.php) — normative GL contracts.
19. [ARB buffer storage](https://registry.khronos.org/OpenGL/extensions/ARB/ARB_buffer_storage.txt) — persistent/coherent mapping contract.
20. [glslang](https://github.com/KhronosGroup/glslang) — compiler frontend and SPIR-V generation.
21. [SPIRV-Cross](https://github.com/KhronosGroup/SPIRV-Cross) — reflection and target-language generation.

### Game and loader

22. [Minecraft 1.21.1 manifest](https://piston-meta.mojang.com/v1/packages/499680d659b7a0ccb2fa9a809c345c5baf2ace7d/1.21.1.json) — authoritative delivery-lane runtime and dependency selection (Java 21, LWJGL 3.3.3).
23. [NeoForge 21.1.262 installer](https://maven.neoforged.net/releases/net/neoforged/neoforge/21.1.262/neoforge-21.1.262-installer.jar) — supported delivery-lane loader; the installed instance is the reference. [NeoForge 21.1.264](https://maven.neoforged.net/releases/net/neoforged/neoforge/21.1.264/neoforge-21.1.264-installer.jar) is an optional future patch, not a release gate.
24. [Iris 1.8.14-beta.1 NeoForge](https://modrinth.com/mod/iris/version/KduFYu4t) — selected delivery-lane release.
25. [Sodium 0.8.13 NeoForge](https://modrinth.com/mod/sodium/version/uMOpc5uV) — selected delivery-lane dependency.
26. [Iris 1.8.12 NeoForge](https://modrinth.com/mod/iris/version/t3ruzodq) and [Sodium 0.6.13 NeoForge](https://modrinth.com/mod/sodium/version/Pb3OXVqC) — stable fallback pair; never pair 1.8.12 with 0.8.13.
27. [Minecraft 26.2 release](https://www.minecraft.net/en-us/article/minecraft-java-edition-26-2) — experimental Vulkan and backend preferences (future lane).
28. [Minecraft 26.3 release](https://feedback.minecraft.net/hc/en-us/articles/48913133328013-Minecraft-Java-Edition-26-3) — shader compilation/interface changes (future lane).
29. [Minecraft 26.3 manifest](https://piston-meta.mojang.com/v1/packages/11f1aefe473eefba0af62e3286c33b692807b2eb/26.3.json) — future-lane runtime and dependency selection.
30. [Iris repository](https://github.com/IrisShaders/Iris) — pack loader, support boundaries and license notes.
31. [Iris 1.11.7](https://modrinth.com/mod/iris/version/vTN4NRGW) — future-lane release.
32. [Sodium repository](https://github.com/CaffeineMC/sodium) — translation-layer support policy and license.
33. [Sodium 0.9.2](https://modrinth.com/mod/sodium/version/bAZQdGpg) — future-lane dependency.
34. [Zalith Launcher 2](https://github.com/ZalithLauncher/ZalithLauncher2) — integration candidate to inspect and pin.

### Shaderpacks

30. [Complementary Reimagined r5.9.3](https://modrinth.com/shader/complementary-reimagined/version/Bqen1mJX).
31. [Complementary Unbound r5.9.3](https://modrinth.com/shader/complementary-unbound/version/B1kyfoUZ).
32. [Complementary source](https://github.com/ComplementaryDevelopment/ComplementaryReimagined).
33. [Complementary profiles/resources](https://github.com/ComplementaryDevelopment/ComplementaryReimagined/blob/main/shaders/shaders.properties).
34. [Complementary option definitions](https://github.com/ComplementaryDevelopment/ComplementaryReimagined/blob/main/shaders/lang/en_US.lang).
35. [BSL 10.1.8](https://modrinth.com/shader/bsl-shaders/version/Y68yiql9).
36. [Photon v1.4](https://modrinth.com/shader/photon-shader/version/BnGklH5y).
37. [Rethinking Voxels r0.1-beta9](https://modrinth.com/shader/rethinking-voxels/version/cpD4esk9).

The publisher APIs and downloaded artifacts resolve facts unavailable from a web page's summary. Retain their metadata and checksums in Stage 0; do not infer exact release source from a moving branch.

## 14. Distribution and support boundaries

Maintain a per-version component/license register. The audited Sodium development revision identifies PolyForm Shield; the inspected Iris source identifies LGPLv3 and an AGPLv3 dependency; the inspected MobileGlues source identifies LGPL-2.1. Verify the exact distributed versions and modifications. Shaderpacks have their own distribution terms. These are distinct obligations, so do not describe the entire combined distribution as one uniformly licensed fork.

Use official-source acquisition or user import for game and shaderpack assets. Preserve original pack contents and external settings. Review the exact bundled assets and licenses before publishing a combined package.

Keep support classifications explicit:

- **Certified:** the complete named device/runtime/pack/profile tuple passed.
- **Compatible but performance-limited:** fidelity passed, with measured performance outside the preferred target.
- **Experimental:** particular features or runtime combinations remain under test.
- **Unsupported:** a known blocker prevents the requested configuration.
- **Unqualified:** insufficient evidence; no pass/fail claim yet.

Optional/future profiles stay outside the release claim without hiding their measured status. A real provider limit on an optional profile is recorded as `BLOCKED` with an explicit optional/future-X2E scope; it is not converted to `NOT APPLICABLE`, and the profile is never reported as supported.

A version string cannot replace this matrix, and a capability probe cannot replace a whole-pack result.

## 15. Coding-agent review and handoff rules

The execution-state file should report the current commit, selected baseline, active task, last successful test, remaining failure, next action and required hardware evidence. Keep the full backlog in the roadmap; keep execution state short enough to read at every handoff.

Require an independent review for changes to capability claims, shader transforms, resource formats, synchronization, provider selection or release profiles. The reviewer should compare the code against a concrete contract and test, not only review style.

Use Git actively: preserve user changes, commit focused working increments, retain source pins, and avoid resetting history to hide failed approaches. Create or use the owner's intended remote only when identified; do not reuse another project's repository.

Agents may continue unblocked implementation while a hardware lane is blocked. They must keep lane-specific state. A missing X2 device, delivery-lane runtime failure, or unavailable GPU test cannot be converted into aggregate completion.

For each release claim, a reviewer should be able to answer:

1. Which exact configuration does this claim cover?
2. Which original pack and effective settings were used?
3. Which required features actually executed?
4. Where is the correctness and endurance evidence?
5. Which performance results were measured?
6. What changed since the preceding qualified release?

