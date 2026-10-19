# Architecture and Baseline Decisions

## D1. Implementation base is core `97558a6`, not the plugin's recorded `f56af31`

- Evidence: the audited plugin commit `f5105e4` records core submodule `f56af31`; local core `main` is
  `97558a6`, whose history contains `f56af31` plus `0f1e10b` (multidraw grow-only staging resize).
- Upstream `MobileGL-Dev/MobileGlues` `main` is still `97558a6` at lock time, so there is no newer audited
  core snapshot to adopt. The plugin's recorded core gitlink is older than the audit baseline.
- Decision: implement on `97558a6`. Do not silently rewrite the plugin gitlink; when the plugin is integrated,
  update its submodule in a separate child→parent reviewed change and record both identities.
- Consequence: plugin-side build metadata must be requalified against the newer core; the displayed 2.0.0
  version is not an identity.

## D2. NDK r28c for the target baseline, keeping the Gradle pin explicit

- Evidence: the audited Gradle configuration pins NDK `27.3.13750724`; Android 16 KB page compatibility
  requires NDK r28+ for simpler alignment, and the plan requires auditing the complete native chain.
- Decision: `28.2.13676358 (r28c)` for the reproducible baseline build, with
  `-DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON`, `arm64-v8a`, `android-26`, `c++_static`.
- Evidence of effect: all PT_LOAD segments report `Align 0x4000`.
- Consequence: the Gradle `ndkVersion` remains a toolchain change to make deliberately when the plugin build
  is cut over; it is not mixed into semantic fixes.

## D3. Game tuple: modern lane first, regression lane retained

- Modern lane: Minecraft 26.1.2 + Fabric + Iris `1.11.3+26.1-fabric` + Sodium `0.9.1`. Evidence: Modrinth
  release metadata; Iris 1.11.3 requires Sodium version id `vf7UgZpC` (`mc26.1.2-0.9.1-fabric`). Do not
  independently pair it with Sodium 0.9.2 (that is Iris 1.11.4's declared pairing).
- Regression lane: Minecraft 1.20.1 + Iris `1.7.6+1.20.1` + Sodium `0.5.12`, retained because the plan
  calls for a separately pinned old lane and because newer Minecraft depth/sampler behavior differs.
- Launcher, Android ARM64 JVM, and LWJGL are **unknown**: no launcher checkout or JRE exists in this
  workspace. They must be resolved together before an installation can be qualified.
- Consequence: shader-fixture and compiler work can proceed; installation/device gates cannot.

## D4. No target device: keep hardware gates pending

- `adb devices` shows only x86_64 Android emulators (API 36/37, 4 KB pages). They are not X1E hardware and do
  not share the Adreno driver.
- Decision: use host builds/tests for semantic repair evidence; optionally use the emulator only for
  Android-packaging/loader smoke tests, labelled as such. No Googlebook rendering, provider, performance, or
  installation claim is made from either.

## D5. Keep the existing compiler stack and repair contracts in place

- The core already uses glslang + SPIR-V + SPIRV-Cross, compute/image/barrier forwarding, MRT remapping,
  translated-source caching, and multiple draw paths.
- Decision: extend those mechanisms; no Vulkan rewrite, no vendor impersonation, no global version inflation,
  no forced feature macros. Advanced-feasibility probing stays bounded and evidence-driven.
- Consequence: the first implementation work is the audited source/lifecycle contract (Stage 1), not a new
  backend.

## D6. Preserve original shader archives

- Fixtures are acquired from official Modrinth URLs and stored outside the repository; only hashes and
  acquisition metadata are committed. No loader/translator edits are made to the archives.
- If a fixture is unavailable at test time, the row stays `not tested`; a synthetic shader is labelled
  synthetic and cannot close a pack gate.
