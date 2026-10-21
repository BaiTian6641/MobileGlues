# Desktop GL continuation, universal FSR3 bridge and CI

Date: 2026-10-21. Baseline: `HANDOFF-desktop-gl-gaps.md` at `d1b97d3`.
Branch: `codex/desktop-gl-universal-fsr`. This is an implementation and evidence
record, not a declaration of full desktop OpenGL conformance or release acceptance.

The delivery lane remains Minecraft 1.21.1 / NeoForge 21.1.262 / Java 21 /
Iris 1.8.14-beta.1 (`KduFYu4t`) / Sodium 0.8.13 (`uMOpc5uV`). New bridge profiles
extend the development matrix without replacing this lane.

## Implemented changes and gap status

| Gap / scope | Implementation | Evidence and remaining boundary |
| --- | --- | --- |
| G0, late uniforms | Conservative source relocation with built-in type, array, initializer, directive, precision and comment dependency checks. Shader translation cache schema increased to 5. | Host regression and actual GLSL compiler repros pass. X1E isolated shader executes and reads back 7. This does not qualify the original Bliss default pack. |
| G3/G6, image formats | Shared legal GLES image-format table; legal signed formats and RGBA8_SNORM; RG32F null/immutable backing widened consistently to RGBA32F. Data-backed storage compatibility is checked before state mutation. | Host tests and targeted X1E image write/readbacks. Existing widening paths remain numerical deviations until quantization, clamping and overflow are qualified. |
| G3, unsupported formats | Invalid storage/image combinations fail explicitly. New r8/r16/r16f/r8i/r16i mappings are rejected rather than inventing precision semantics. PBO offset zero is treated as data. | Strict compiler, metadata/format regressions and sanitizer checks. No full narrow-format support claim. |
| G4, atomics | Added compute 2D/3D, fragment 2D and geometry 3D operation/readback probes, including all eight image atomic operations. | Compute and fragment pass. Geometry produces three invocations where one is expected; the same failure reproduces with system EGL/GLES below MobileGlues. |
| G5, subgroups | Added semantic readback for virtual 32-lane IDs/count, scalar/vec2/vec3/vec4 inclusive addition and quad swaps in two 128-invocation workgroups. | 5,120 checked values pass on X1E. This does not qualify SPD, XOR-partition duplicate suppression, arbitrary divergent control flow or hardware subgroup exposure. |
| G10, clip/cull | glslang resource limits follow the real provider extension, remaining zero when absent. Unsupported internal queries removed. | Host/build checks; no extension or capability is fabricated. |
| Core identity | Exported `mg_get_build_id()` and source fingerprint in normal `GL_RENDERER`. Fingerprint covers implementation inputs at CMake configure time, including dirty source. | Exact binary SHA and marker captured in wrapper results. A source fingerprint does not identify compiler options or prove which library FCL cached. The matching marker was subsequently confirmed in the delivery-lane game log. |
| FSR ownership | Release JNI context state and direct-session CPU ownership after context destruction without issuing GL calls into a replacement context; delete/reset motion resources during valid resize cleanup. | Real session API with recording host backend passes. Actual repeated GPU context loss and recreation remains a device gate. |

The review also localized illegal GLES image-uniform forwarding and the new
format validator's null-image binding regression. Their corrections preserve
desktop validation and logical image-unit state while using legal GLES bindings.
The final X1E run passes all 64 checks in `all`, including 18 error checks for
image/ordinary uniforms and the null-image unbind path. Array-element reflection
is normalized before validating multi-element writes. Ordinary uniform driver
errors are preserved in the application error queue.

G1 metadata producer coverage, G2 1D virtualization, G7 mutable image mip chains,
G8 texture barrier/view failure policy, G9 default tessellation levels, G11
multi-image stage capacity, G12 provider SSBO cap and G13 bindless/SSO remain open
or explicitly unsupported as recorded in `desktop-gl-gap-closure.md`. Existing
`colorimg8` special handling is retained and disclosed. No shaderpack archive was
rewritten and no provider limit was raised.

## Target-device evidence

New hardware probes ran on the requested Snapdragon X Elite X1E-80-100
Android target using ARM64 and ANGLE over Vulkan. Exact device serial, OS build,
driver identity and paths remain in local raw evidence, retained under
`build/qualification/2026-10-21-private-evidence/`. Public summaries redact those
fields while preserving artifact hashes, readbacks, errors and evidence levels. The x86_64 APK is a build lane, not X1E device evidence.

`tools/ci/run_wrapper_probe.py` writes hash-bound JSON, logs, exit codes, device
identity and raw provider/core identity. It uses a unique `[device temporary storage]`
directory and never edits launcher data. A failed check returns failure.
The geometry control explicitly dispatches both EGL and GLES through the system
libraries. It observes invocation counter 3 and final add value 15 instead of
1 and 5, including when MobileGlues is absent from the context creation path.
The failure is retained; geometry atomics are not promoted to supported.

Targeted image readbacks include r8ui=17, r16ui=17, rg32ui=(17,23), and
rg32f=(0.5,0.25). These values demonstrate the exercised storage/bind/write/read
path, not edge-value equivalence for all widened formats. The late-uniform probe
returns 7 without a GL error when run independently.

Redacted public JSON summaries are versioned in
[`validation/2026-10-21-desktop-gl/`](validation/2026-10-21-desktop-gl/).
The ARM64 core SHA-256 is
`d515ca6a334d29783ff742e75d069249d76461357cabacc77b3ac3bfd5035f18`,
source marker `src-167f4da0e295031b`. The harness SHA-256 is
`315938c827b7e012c1bdc3630138902ba2803965f39cab8d3bade44b01c6a5c9`.
`all.json` has 64 successful checks; `advanced_images.json` and
`provider_geometry_atomics_native.json` intentionally retain exit code 1 for the
geometry failure. Do not summarize the entire advanced suite as passing.

No installed game process is qualified by a `[device temporary storage]` wrapper run.
FCL was launched after installing the final debug APK. At 01:10:15 its game log
reported `OpenGL Renderer: Adreno (TM) X1-85 | ANGLE | Vulkan [Vulkan revision retained locally] | OpenGL ES 3.2
| MG src-167f4da0e295031b` (spacing retained in the evidence excerpt). The title
screen showed Minecraft 1.21.1 / NeoForge 21.1.262 with Complementary Reimagined
r5.9.3 selected. This confirms the new core reached the actual game process;
no renderer cache deletion or launcher data reset was needed. Existing Iris
custom-uniform warnings remain; title-screen rendering does not qualify all
pack effects or world performance.

## Standalone bridge

Repository: [BaiTian6641/MobileGlues-FSR3-Bridge](https://github.com/BaiTian6641/MobileGlues-FSR3-Bridge),
created private, then made public with explicit user approval; local checkout `[workspace]/MobileGlues-FSR3-Bridge`.
Initial reviewed source commit: `84c1e5a`. Its own README, lockfiles and validation
record are authoritative for profile dependencies and build commands. The older
in-tree bridge remains a historical snapshot with a migration pointer.

| Minecraft | Loaders | Pinned shader dependency |
| --- | --- | --- |
| 26.2 | Fabric, NeoForge | Iris 1.11.4 |
| 1.21.1 | Fabric, NeoForge | Iris 1.8.14-beta.1 |
| 1.20.1 | Fabric, Forge | Iris 1.7.6 / Oculus 1.8.0 |
| 1.18.2 | Fabric, Forge | Iris 1.6.11 / Oculus 1.6.4 |

Each of the eight JARs has its own verified descriptors, invocation hooks and
loader metadata, plus ARM64 and x86_64 native helpers. Java output targets 17;
the actual 26.2 API inspection/compiler uses JDK 25. The 26.2 adapter performs
real scene scaling through Mojang GpuTexture ownership APIs and requires OpenGL.
Vulkan is rejected before scaling. The six older adapters use their exact Iris
or Oculus depth-allocation API, including legacy three-argument constructors.

All eight pass local build, strict Java/native compilation, real classfile hook
inspection, host contracts, packaging isolation, native checksum validation and
repeated-build byte comparison. JNI host tests exercise real `dlopen` behavior
for an unloaded core, repeated incomplete ABI attempts and later successful
binding. They are explicitly host fixtures, not simulated device acceptance.

Security fixes include loaded-core-only `RTLD_NOLOAD`, complete ABI validation
before publishing pointers, bounded checksummed private native extraction,
coherent color/depth allocation, failure-path GL state restoration, bounded
diagnostics and rate-limited errors. Camera/depth input is not evidence of
moving-object motion, frame generation, anti-ghosting quality or a speedup.
The new JARs remain development artifacts until each actual loader/game run.

The hosted 1.21.1 NeoForge artifact was subsequently smoke-tested on this X1E:
commit `287fff5`, JAR SHA-256
`db37b5f138f8271cf5fc90735a724a365b0f72d0a045350212eae2433439842b`.
NeoForge recognized version 0.2.0, and the real debug log records all seven bridge
mixins applying. The existing 1.21.1 world rendered the original Complementary
Reimagined r5.9.3 pack at the existing LOW profile (+2 user options). F3 showed
the new core marker. Save and Quit completed with all chunks and dimensions
saved, followed by a clean exit to FCL. No blocks, inventory, pack options or
renderer settings were changed; the normal world clock advanced during the run.

This is a loader/Mixin/world/save-quit smoke pass only. It does not establish
native submit success, exact scaled input extents, each FSR preset, image quality,
shader reload, context loss or a performance gain. The other seven profiles
remain build/host verified. The previously installed bridge was backed up and
restored byte-for-byte after testing; the new debug renderer APK remains installed.
The test temporarily used the old mod filename, while loader metadata and SHA
identified version 0.2.0. `game-smoke.json` records hashes, and
`game-smoke-excerpt.log` contains the relevant loader/renderer/save evidence.
Local screenshots and full logs are in `build/qualification/2026-10-21-runtime/`.

## APK packaging, automated checks and distribution

The Android workflow builds this core for ARM64 and x86_64 using NDK r28c, then
uses a disposable clean upstream launcher-plugin checkout pinned to
`f5105e4ddd1752cccdcc6421c402960d40ec3d9d`. It retains extracted native-library
packaging and generates version `2.0.2-dev`: a debug-signed test APK and an
unsigned release APK. The dirty maintained `[workspace]/MobileGlues-plugin`
checkout is untouched. No production signing key or automatic release is added.

The adaptation pins Gradle's SHA-256 and R8 8.13.19 for Kotlin 2.3 metadata.
It corrects three stale upstream test expectations to the existing native/decoder
ANGLE fallback. All 51 plugin unit tests pass. Android lint has zero errors and
47 existing warnings. Both APKs pass ARM64/x86_64 ELF machine checks and all
packaged native libraries have LOAD alignment of at least 16,384 bytes.

The separate bridge workflow builds all eight profiles, checks host/JNI contracts
and Java ASTs, rebuilds to verify byte identity and uploads JARs with reports.
Both repositories pin third-party action commits, use read-only permissions,
disable persisted checkout credentials and retain build artifacts for 14 days.
The first hosted attempts exposed two environment issues: a removed Android SDK
`tools` package and setup-java's rejection of Temurin's four-part version.
The fixes select `platform-tools` and install the exact checksummed official
Temurin archive. Failures are recorded rather than described as completed builds.

See [independent review](2026-10-21-review.md) for security, AST, lint, ownership
and comment findings. Scoped native analysis retains 26 reviewed baseline
diagnostics; this is not a repository-wide zero-warning claim. Java AST checks
reject broad Throwable and empty catches. Manual review covers ownership,
exception behavior and comments beyond scanner counts.

## Research and next gates

- Khronos [ESSL 3.20](https://registry.khronos.org/OpenGL/specs/es/3.2/GLSL_ES_Specification_3.20.pdf)
  and [ES 3.2](https://registry.khronos.org/OpenGL/specs/es/3.2/es_spec_3.2.pdf)
  define legal image qualifiers/bindings and image uniform restrictions. Valid
  desktop declarations require a storage and numerical contract, not just text replacement.
- [OES_shader_image_atomic](https://registry.khronos.org/OpenGL/extensions/OES/OES_shader_image_atomic.txt)
  motivates the stage-specific atomic probes. An extension string does not
  substitute for the observed geometry readback.
- [Fabric's 26.2 changes](https://fabricmc.net/2026/06/15/262.html) explain the
  new rendering API and experimental Vulkan path. Actual release classfiles and
  Mojang GPU API descriptors are pinned and verified in the bridge repository.
- Qualcomm's [X2 Elite product brief](https://www.qualcomm.com/content/dam/qcomm-martech/dm-assets/documents/Snapdragon-X2-Elite-Product-Brief.pdf)
  describes the newer GPU/API family. It cannot establish a future Googlebook's
  Android ANGLE/GLES features, limits or correctness. X2E onboarding starts with
  below-wrapper probes on the actual OS/driver, followed by these same readbacks.
- Android's [16 KB guidance](https://developer.android.com/guide/practices/page-sizes)
  and [Kotlin/R8 compatibility table](https://developer.android.com/build/kotlin-support)
  informed the artifact checks and R8 pin. ELF alignment alone does not qualify
  runtime behavior on a 16 KB-page device.

Next acceptance work is: exercise the new 1.21.1 NeoForge JAR with required
mixins through world rendering, compare original pack effects and
FSR presets, then test resize, focus/context recovery, shader reload, world
transitions, save/quit and sustained performance/thermals. Qualify each extra
loader tuple separately. Preserve original shader archives, worlds and settings.

## Hosted artifact verification

The renderer [run 39553731285](https://github.com/BaiTian6641/MobileGlues/actions/runs/39553731285)
at final code commit `f5b2c5081bacac6487c191a3f8f8ae2e41b9f425` passed and uploaded
[both APKs](https://github.com/BaiTian6641/MobileGlues/actions/runs/39553731285/artifacts/12325675038).
The downloaded debug APK SHA-256 is
`532c38229a6b5bf45effed9515bdf9e414530b83da1a051392453597a24b6deb`;
the unsigned release SHA-256 is
`7505cef33d3c0d8595fefcfd80f9c39369f9817258939c284eb218ee8c6aead2`.
Both contain the source marker independently recomputed from 115 exact-commit
source inputs. Downloaded checksums, both ABI ELF payloads, 16 KB alignment,
debug signature and unsigned-release state were checked. The report distinguishes
CMake input hashes from Gradle-stripped APK payload hashes. See
`validation/2026-10-21-desktop-gl/apk-ci-verification.json`.

The bridge [run 39553378205, attempt 2](https://github.com/BaiTian6641/MobileGlues-FSR3-Bridge/actions/runs/39553378205)
at commit `287fff5e6e72c6cf94b082f13f81cb7250d012ec` passed all eight jobs and
uploaded all eight artifacts. The first attempt passed builds/checks but hit the
private-repository storage quota. After the user's explicit public-visibility
approval, the same commit passed uploads. Downloaded JARs and build reports are
byte-identical to the exact-commit local rebuild; class isolation, native hashes,
ELF ABIs/alignment and ZIP integrity were rechecked on the downloaded files.
See `validation/2026-10-21-desktop-gl/bridge-ci-verification.json`.

Local final APK SHA-256 values:

- Debug: `2be4dd1f419537505a29e6f1aa0ef5349319e0202b86174c36ce4955cde6ea20`.
- Unsigned release: `a51aa28eb05c2b75c7b8e4359111ab3095870b5b4fe631cfe01f04dbe91e6352`.

The debug APK was installed with package-manager replace, preserving data.
Build reports and APK validation are in `build/distribution/`; hosted debug
signatures and binaries need not be byte-identical to local builds.
