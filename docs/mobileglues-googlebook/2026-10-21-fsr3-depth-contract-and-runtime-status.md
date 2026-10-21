# FSR3 depth contract, bridge ownership and runtime status

Date: 2026-10-21. This record separates observed game results from the new
corrections awaiting device acceptance. It does not qualify universal shaderpack
support, full temporal FSR3, frame generation or a performance gain.

The supported delivery lane remains Minecraft 1.21.1 / NeoForge 21.1.262 /
Java 21 / Iris 1.8.14-beta.1 / Sodium 0.8.13. The additional bridge adapters
extend the development matrix; their builds do not replace that delivery lane.
The [universal rendering research](2026-10-21-fsr3-universal-rendering-research.md)
records all eight exact shader dependency locks, the 1.17–26.2 API families,
primary sources and future gaps.

## Source and artifact boundary

| Component | Revision | Meaning |
| --- | --- | --- |
| Native semantic correction | [4d93e41f92d606e29dc16aab9005a05d680a0032](https://github.com/BaiTian6641/MobileGlues/commit/4d93e41f92d606e29dc16aab9005a05d680a0032) | Negotiates sampled-depth convention, normalizes reversed samples and validates spatial inputs. Source marker `src-4eb840b04c1d51e1`. |
| Android packaging | [eed6b4c34e30bd96325939acc363360dd7d23ebb](https://github.com/BaiTian6641/MobileGlues/commit/eed6b4c34e30bd96325939acc363360dd7d23ebb) | Versions this correction as `2.0.5-dev`, version code `2005`. Packaging is distinct from semantic implementation and runtime acceptance. |
| Standalone bridge | [7be7089c69711a9ef206039ad33e48c39e35a234](https://github.com/BaiTian6641/MobileGlues-FSR3-Bridge/commit/7be7089c69711a9ef206039ad33e48c39e35a234) | Bridge `0.2.2`: depth negotiation, generation/lifetime handling, independent snapshot attachments and bounded diagnostics. |

The matching `2.0.5-dev` diagnostic core and `0.2.2` bridge have now been loaded
on the 26.2 NeoForge target, but **failed complete world-rendering acceptance**.
Successful startup and trace collection do not close that failure. Local diagnostic APKs enable additional tracing;
production release builds keep diagnostics disabled. A source marker identifies
source inputs, not all build options or the installed binary. Runtime promotion
requires the exact APK, loaded core and bridge JAR hashes.

## Observed game results

| Runtime/artifacts | Recorded observation | Status and limit |
| --- | --- | --- |
| Minecraft 26.2 NeoForge, renderer `2.0.4`, bridge `0.2.1` | The previously tiny title/F3 presentation was corrected, but the world view still showed sky without the expected terrain. | **FAIL for complete world rendering.** Presentation improvement does not qualify world contents or depth correctness. |
| Create Delight Remake, Minecraft 1.20.1 Forge / Oculus 1.8.0, bridge `0.2.1` | The bridge loaded without the earlier “no valid ResourcePackInfo” metadata error. A Complementary screenshot showed the scene across the full frame. | **PARTIAL: loader and visual spot-check evidence.** This does not establish every effect, preset, lifecycle path or frame time. |
| Create Delight Remake, renderer `2.0.4` diagnostic / bridge `0.2.1`, Steadfast 0.8.0 / Oculus 1.8.0 | Full-frame scene, terrain, hand and HUD were visible. F3 confirmed the actual shaderpack/Oculus tuple and native marker `src-ea4affcdf1aeae4a`. | **PASS for this bounded visual regression; broader qualification remains PARTIAL.** A single F3 frame displayed 37 FPS; this is not a benchmark or speedup result. |
| Minecraft 26.2 NeoForge, renderer `2.0.5-dev` diagnostic / bridge `0.2.2` | The bridge loaded without an observed Mixin transformation error, native marker `src-4eb840b04c1d51e1` appeared, title/F3 filled the frame and the hand was restored. Expected terrain remained missing and HUD rectangles were incorrect. | **FAIL for complete rendering.** The depth changes do not establish that the terrain failure is fixed. |
| 26.2 FSR-disabled comparison | The preceding world was saved and quit before launching the control. USB ADB disconnected at 03:41 local time during control capture; no screenshot or usable result was obtained. | **INCOMPLETE; no rendering result.** A later uptime check confirmed a device restart; the reported boot reason was `reboot`. No reboot command was issued by the test operator. The cause is undetermined. The device is back at the lock screen and user unlock is pending before tests can continue. |
| Delivery 1.21.1 NeoForge and requested Fabric profile checks with the new pair | No new acceptance result is recorded for these profiles. | **PENDING device checks.** Earlier bridge-version smoke results are not transferred to `0.2.2`. |

These observations came from the Snapdragon X1E Android target using the system
ANGLE-over-Vulkan provider. Exact OS/provider identity, device identifiers,
screenshots and full logs remain in local evidence. Public reports preserve
artifact identity and acceptance scope without publishing private runtime paths,
world/account details or unrelated system logs. The table records the supplied
observations; it is not a new automated or independently repeated device run.

The Create test restored the original Oculus properties byte-for-byte and saved
and quit the world. No coordinate or world-location information from F3 is
published here. The screenshot hashes below bind the observations without
publishing private captures:

| Local evidence role | SHA-256 |
| --- | --- |
| Create / Steadfast world and HUD | `22aee3988ec2938a530fa0af09da18e484a560530768639b5c3a5df5c385029d` |
| Create / Steadfast F3 tuple and terrain | `4b466670de4498a99f15537066d89d06c664c49da87b9ca0eee0193183041e9f` |
| Create / Complementary visual spot-check | `5b3132831a653d68080fd0cfc1092806527e01738eebdb505704f61660e3fb93` |
| 26.2 / new depth pair, world failure | `68e957e26fff27e9f6d58ef62fec045380ed76f7ce411fe6fb1743bc4a5c42de` |
| 26.2 / new depth pair, F3 | `c399dc0f9838cdc6e9f5d4863aa8f0459b1b239f9782cad473cb262a997a58ea` |
| 26.2 / new depth pair, JSONL trace | `f1f95f11012b0c306d8de91369eb4d31d28b853f13ebc894ed2ec0f1398e4f6b` |

All 1,561 rows of that trace parse as JSON. It records scene extent 1694×932,
output/surface extent 2880×1585, reversed-sample negotiation and zero motion.
These facts establish diagnostics and configuration, **not successful native FSR
submission**. The trace does not prove the opaque-terrain draw path or native
submit result. A shadow-viewport issue identified from tracing is being handled
as a separate correction; it must not be folded into this artifact's result or
declared the cause of the missing terrain without a discriminating retest.

## Why sampled depth and captured projection differ

The pinned Minecraft 26.2 `GameRenderer.render` clears main depth to `0.0`.
Its world depth is reversed: near samples approach one and far samples approach
zero. Iris clears shadow depth separately to `1.0`; that shadow operation does
not define the world depth convention.

Before publishing `CapturedRenderingState.getGbufferProjection`, pinned Iris
26.2 applies `MatrixUtils.undoRevZ`. The captured matrix therefore describes
forward clip Z in `[-1,1]`, even though the sampled GPU depth remains reversed.
Reversing both would be incorrect. The bridge build checks the locked method
descriptors, relevant invocation/clear instructions and bytecode identities.
The exact inputs and implementation are recorded in the
[0.2.2 compatibility lock](https://github.com/BaiTian6641/MobileGlues-FSR3-Bridge/blob/7be7089c69711a9ef206039ad33e48c39e35a234/compatibility-lock.json)
and [versioned depth ownership record](https://github.com/BaiTian6641/MobileGlues-FSR3-Bridge/blob/7be7089c69711a9ef206039ad33e48c39e35a234/docs/depth-lifecycle.md).

The optional `nativeDepthConvention(int)` protocol describes sampled depth:

| Value | Sampled depth | Captured Iris projection | Native policy |
| --- | --- | --- | --- |
| `0` | Forward, near 0 / far 1 | Forward `[-1,1]` | Existing legacy behavior; default history reset remains. Experimental camera-only history is still unqualified. |
| `1` | Reversed, near 1 / far 0 | Forward `[-1,1]` | Normalize samples, force zero motion and reset history on every dispatch. |
| Other | Unspecified | Unspecified | Reject negotiation. |

The 26.2 adapter enables allocation/submission only when convention `1` is
accepted. A renderer lacking the optional symbol supports the legacy convention
only. It cannot silently interpret reversed samples as forward depth.

For convention `1`, the
[production input shader](https://github.com/BaiTian6641/MobileGlues/blob/4d93e41f92d606e29dc16aab9005a05d680a0032/MobileGlues-cpp/upscaling/fsr3_input_shader.h)
writes `1 - sampledDepth` into forward R32F depth. Native code derives near/far/FOV
from the captured perspective matrix and preserves a recognized infinite-far
declaration. It rejects unsupported matrix shapes, insufficient/mismatched
scene-depth dimensions and conversion failure. It also isolates and restores
texture-unit-zero's sampler binding. Conversion failure cannot fall through with
raw reversed samples and auxiliary resources describing different extents.

Matrix shape alone cannot distinguish every forward `[0,1]` matrix from a
forward `[-1,1]` matrix. The pinned Iris provenance establishes the range;
structural validation is not an independent proof of it. Custom, oblique,
orthographic or otherwise unsupported projections remain outside this correction.

The pinned AMD implementation selects shader permutations for inverted depth;
infinite far instead changes reconstruction constants. MobileGlues still rejects
raw inverted input, while accepting forward infinite-far input only with history
reset and no motion. This uses existing infinite reconstruction constants and
does not substitute an invented finite far plane.
[Pinned AMD implementation](https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/blob/c6efa6bf7f2027b3ec94f28578bb5965eabb9e55/sdk/src/components/fsr3upscaler/ffx_fsr3upscaler.cpp),
[AMD depth configuration contract](https://gpuopen.com/manuals/fidelityfx_sdk/techniques/super-resolution-upscaler/#depth-buffer-configurations).

## Bridge 0.2.2 texture ownership

Iris `RenderTargets.resizeIfNeeded` replaces its cached `currentDepthTexture`
only when the incoming generation changes. A replacement object or size change
alone does not guarantee that cached identity changes. Dynamic world/composite
and center-depth suppliers subsequently read the cached object. Closing the old
texture early can leave these consumers with a stale GL ID.

The bridge now tracks both Minecraft's source generation and the selected depth
object identity. Constructor and resize hooks use the same generated sequence.
Replaced bridge textures and clear framebuffers remain alive until Iris returns
normally, including when resize returns `false`: that boolean describes a size
change, not whether depth adoption succeeded. A throwing resize retains both
generations. A retry uses a fresh generation to repeat attachment updates.
This is not rollback of the entire Iris resize; a partial color-texture resize
can still require pipeline recreation. Teardown releases outstanding resources.
Minecraft's original main texture and views remain externally owned.

The same Iris generation loop visits `noHandDestFb` and
`noTranslucentsDestFb`, which must keep independent snapshot depth attachments.
Without intervention, it attaches the current scene depth to those destinations
as well. The 26.2 redirect covers the three exact `addDepthAttachment` call
sites. Only those two receiver identities select their current snapshot texture;
other receivers keep the original supplied depth argument. Snapshot fields have
already been published at each relevant call. This preserves Iris' call count
without an extra per-frame attachment operation or ownership transfer.
[Generated snapshot redirect](https://github.com/BaiTian6641/MobileGlues-FSR3-Bridge/blob/7be7089c69711a9ef206039ad33e48c39e35a234/templates/GpuDepthAttachments.java.in).

The 26.2 bridge clear uses `0.0`, while legacy profiles keep `1.0`. Diagnostics
emit texture identities as JSON numbers or null, including `GpuTexture` objects.
An unavailable optional diagnostic API is reported once rather than every frame.
These corrections improve observability; a valid trace is not evidence that its
rendered contents are correct.

## Host, provider and build evidence

- Repository host semantic tests passed. The new
  [depth-contract test](https://github.com/BaiTian6641/MobileGlues/blob/4d93e41f92d606e29dc16aab9005a05d680a0032/MobileGlues-cpp/tests/fsr3_depth_contract_test.cpp)
  also passed strict warnings, AddressSanitizer and UndefinedBehaviorSanitizer.
  It checks finite/infinite reconstruction, invalid inputs, convention values
  and history/motion restrictions.
- The [host GPU test](https://github.com/BaiTian6641/MobileGlues/blob/4d93e41f92d606e29dc16aab9005a05d680a0032/MobileGlues-cpp/tests/fsr3_depth_gpu_test.cpp)
  executes the production conversion shader. Forward/reversed inputs passed five
  samples including both endpoints, with all motion components zero. Provider:
  Mesa llvmpipe, LLVM 20.1.8, OpenGL ES 3.2 Mesa 26.0.0-devel. This is host-provider
  evidence, not Android or Minecraft acceptance.
- Scoped Clang syntax/AST, analyzer and lint checks passed across nine release
  translation units without increasing the existing diagnostic baseline. JNI
  was rechecked in release and diagnostic configurations. An existing adjacent
  `jint` warning in diagnostic `nativeTrace` remains separately recorded.
- ARM64 and x86_64 release cores built with diagnostics disabled. A separate
  ARM64 diagnostic core and `2.0.5-dev` tracing APK passed configuration/resource,
  ABI, 16 KB ELF LOAD alignment, signature and stripped-native-input checks.
- Bridge host tests execute pinned Iris resize/getter methods with GPU ownership
  probes. They reproduce stale identity and partial attachment failure, exercise
  retry and false-return adoption, and verify deferred cleanup without closing
  Minecraft's depth. A focused test reproduces snapshot attachment overwrite and
  exercises the generated redirect handler. It does not establish a live Mixin
  transformation. A real `GlTexture` metadata case verifies numeric JSON output.

Production Android CI is tracked by
[run 39568101759](https://github.com/BaiTian6641/MobileGlues/actions/runs/39568101759).
It completed successfully at exact commit `eed6b4c34e30bd96325939acc363360dd7d23ebb`.
The independently downloaded
[artifact 12332205098](https://github.com/BaiTian6641/MobileGlues/actions/runs/39568101759/artifacts/12332205098)
matched GitHub's archive digest
`04e3cb3daa6c1774d46e69e5e10dad7ce0a7f18a6f001900e93c5c5b4846451d`.
Both APKs passed independent version/FSR-settings, ABI, marker and 16 KB ELF/ZIP
alignment checks. Both native ABIs expose the depth convention method and omit
diagnostic `nativeTrace`; debug/release contain identical core payloads. Commit,
plugin pin and patch/test provenance were checked separately because provenance
library hashes describe unstripped inputs.

The CI release APK is unsigned by design, SHA-256
`c7b670851e85a4f381a13198cc5a20eb4df4c45f49bb12579bac4101f584ce7a`.
A separate local debug-signed qualification derivative has SHA-256
`1fba69520bc0d102355f4feab8ea37147b42d6b2a7352478886a941df484c7f9`;
its original ZIP-entry bytes and alignment were verified. It is not a
production-signed public release and is distinct from the diagnostic APK used
for tracing. CI success does not override the recorded physical rendering failure.

Bridge [run 39568889269](https://github.com/BaiTian6641/MobileGlues-FSR3-Bridge/actions/runs/39568889269)
passed all eight profiles at `7be7089c69711a9ef206039ad33e48c39e35a234`.
All eight downloaded JARs independently matched the local verified artifacts;
hosted verification reports also matched their local counterparts. Their bridge
source fingerprint is
`56c7e2ae17b5c1842db8cab5b07871a686fcd85c5023b1ccd4c9f7946290bbde`.
This establishes the checked builds and artifact identity, not eight runtime passes.

## Remaining device acceptance

The `2.0.5-dev` / `0.2.2` 26.2 result remains a rendering failure despite successful
loading and trace collection. The device owner should append a separate
hash-bound result for each subsequent tested tuple, retaining the
actual OS/provider details locally and a sanitized public summary. Required
observations include:

1. The loaded native marker, APK/core/JAR hashes, loader/Iris/Sodium versions,
   shaderpack hash and selected preset.
2. Scene color/depth, main output and surface dimensions; depth convention and
   decoded near/far/FOV; successful negotiation and conversion or exact rejection.
3. Correct full-frame terrain and final output, hand/HUD composition and depth
   snapshots, followed by shader reload, preset/resize transitions and save/quit.
4. Complete the interrupted FSR-disabled comparison and separate regressions for
   Create/Complementary, Steadfast and the requested 1.21.1/26.2 Fabric and NeoForge
   profiles. Prior `0.2.1` visual results do not qualify `0.2.2`; untested rows stay pending.

The reversed-sample path is deliberately spatial: zero motion and a history reset
every frame. Converting reversed depth to forward floating point also gives up
the reversed-Z far-distance precision advantage. Full inverted permutations,
temporal history, jitter ownership, moving-object vectors, mask policy,
shaderpack TAA interaction and frame generation remain separate work. No result
above qualifies every shaderpack, all eight runtime profiles, native Vulkan,
Minecraft 1.17, unimplemented intermediate releases or future X2E hardware.

## Subsequent viewport correction checkpoint

The bounded trace exposed a separate GL-state violation: 108 nonempty shadow
roles used a 2880×1585 first-draw viewport with 2048×2048 shadow attachments.
Binding framebuffer zero could implicitly reset the viewport, including when
Minecraft closed an intermediate render pass. Commit
[`c3cf54c`](https://github.com/BaiTian6641/MobileGlues/commit/c3cf54c) removes viewport
changes from framebuffer binding. Explicit viewport handling and the bridge's
final-output reset remain. The old framebuffer implementation fails 31 new
regression checks; the corrected implementation passes, including sanitizer
checks and the repository host suite. Integration must still prove explicit
scene viewport boundaries across the adapters. This correction is not evidence
that the missing terrain is fixed.

FCL's pinned game-log callback discards chunks containing certain compiler-error
strings before both display and file writing. Commit
[`89c4270`](https://github.com/BaiTian6641/MobileGlues/commit/89c4270) copies the
existing bounded backend shader failure message to Android's log and gives
link/validate diagnostics the same searchable marker. It adds no GL query or
error consumption and does not change status reporting. Actual native-function
harnesses verified failure/success behavior and unchanged query counts.
[Pinned FCL callback](https://github.com/FCL-Team/FoldCraftLauncher/blob/4c2818646aa5bd8b7b9ff272d2be2a8991477a6e/FCL/src/main/java/com/tungsten/fcl/control/GameMenu.java#L944).

Packaging commit
[`38f644b`](https://github.com/BaiTian6641/MobileGlues/commit/38f644b) identifies
these changes as **2.0.6-dev / 2006**. Android
[CI run 39572357304](https://github.com/BaiTian6641/MobileGlues/actions/runs/39572357304)
succeeded at exact source revision
[`ea25b84f8832a347be6eb6758edb0bdd9e62dbdb`](https://github.com/BaiTian6641/MobileGlues/commit/ea25b84f8832a347be6eb6758edb0bdd9e62dbdb).
This is a build result; device qualification remains pending. It does not inherit
the results recorded for 2.0.4 or 2.0.5.


## Subsequent buffer and diagnostic checkpoint

The general buffer mapping correction
[`ed85188`](https://github.com/BaiTian6641/MobileGlues/commit/ed851883ef8234af989943541fdf7a01f7954126)
preserves caller storage/map flags and explicit flush ranges. The previous
policy suppressed flushes without requesting coherent mapping access. The
[buffer investigation and verification record](2026-10-21-buffer-mapping-and-rendering-checkpoint.md)
links the Khronos contract, exact Sodium comparison, actual-translation-unit
regression and real software-provider readbacks. All four physical X1E **below-wrapper** provider cases subsequently passed.
The subsequent full-library run passed three visibility cases but failed strict
invalid-flag rejection, so its overall result remains FAIL. Terrain causality
remains unproven; this is not a shaderpack-specific fix.

Bridge **0.2.3** at
[`2f0e4faf57489c51418bfa9fe534d63f7623a769`](https://github.com/BaiTian6641/MobileGlues-FSR3-Bridge/commit/2f0e4faf57489c51418bfa9fe534d63f7623a769)
connects the previously unused phase diagnostic to the pinned Iris `setPhase`
return and records bounded terrain roles and post-submit status. It changes
diagnostic coverage, not rendering behavior. Its
[CI run 39573273898](https://github.com/BaiTian6641/MobileGlues-FSR3-Bridge/actions/runs/39573273898)
passed all eight profiles. Independently downloaded JARs and hosted reports
matched the verified local outputs. Source fingerprint:
`cebd99ef5c24ec0c2158163a866a1ce6325165be11b9627dfc0c0642a6f49fa6`.
No new runtime acceptance is inferred from those checks.

| Bridge 0.2.3 profile | Verified JAR SHA-256 |
| --- | --- |
| 1.18.2-fabric | `162a797efdf4b7920e114c8667a747a1f8cc00ef465218b0950f1524bef5b9c5` |
| 1.18.2-forge | `69d4deeacaed63edc4e89d91c12730f22b21bcd49eec06a4c1699a86fc8ef6a6` |
| 1.20.1-fabric | `5c1044b02d188541e44be2f75576f1c27af4df20dca977b916cd8226b68dbbd8` |
| 1.20.1-forge | `3b7d7d28c11cc8e56b874a08d256ba4d5cf4fdd95ef87c408bd9bd1be36ae3e7` |
| 1.21.1-fabric | `7cb07b7a4ba2e341a6dd42537ac1b697cef1660c0896309b1f2a171cde94baea` |
| 1.21.1-neoforge | `f255d6dfb797235b8fae8c5ebadd91505d10df8d1c2f44706497f2f223331778` |
| 26.2-fabric | `49622ef07508ec3aa3cdb852768baf0adb4e3db43f4255295539749839f25296` |
| 26.2-neoforge | `1cdf909bdfbb38818a196dc477000f203c9fdd3cb447b6ff6dc7f992108c3d88` |

## 2.0.7 viewport and buffer checkpoint

The further explicit-viewport correction is committed at
[`e3ab06c9b61bb97c52bc01816698f86852251d0b`](https://github.com/BaiTian6641/MobileGlues/commit/e3ab06c9b61bb97c52bc01816698f86852251d0b).
Previously, any native-sized or oversized attachment could cause an arbitrary
viewport to expand to the full surface. The same helper serves `glScissor`, so
that rule also overwrote scissor rectangles and atlas regions. The corrected
planner scales only a positive, zero-origin, complete-surface rectangle aimed
at a smaller target. Offset, partial, native-sized, oversized and already-scaled
rectangles preserve the caller's coordinates and dimensions. The explicit
final-presentation reset remains.

The actual pre-fix `FSR3Direct.cpp` failed five regression cases: native partial
viewport, native small scissor, atlas partial/full and offset full-extent request.
The corrected actual translation unit passed the same recording-provider test,
ASan/UBSan and Clang syntax/AST checks. It verifies texture-unit/binding recovery,
current-surface resizing, retained scene conversion, invalid/disabled cases and
the explicit native reset. The test calls the shared adapter; public viewport
and scissor call sites were separately inspected. This remains host semantic
evidence, not live Minecraft qualification.

The integration audit covered all eight exact final-renderer artifacts from the
[versioned compatibility lock](https://github.com/BaiTian6641/MobileGlues-FSR3-Bridge/blob/2f0e4faf57489c51418bfa9fe534d63f7623a769/compatibility-lock.json):

| Pinned adapter family | Reviewed final-presentation boundary |
| --- | --- |
| 1.18.2 Fabric/Forge, 1.20.1 Fabric/Forge, 1.21.1 Fabric/NeoForge | The shared bridge final-pass entry performs an explicit native reset; these six legacy final renderers also rebind the main target before returning. The general attachment-size expansion rule is unnecessary for those inspected paths. |
| 26.2 Fabric/NeoForge | Final drawing uses the native main target through Mojang's render pass. Closing it can bind framebuffer zero, and subsequent swap copies can bind a scene source. The shared entry reset plus authoritative `colorHolder` submission-target validation handles that boundary; no arbitrary attachment is inferred to be final output. |

These bytecode findings support removing the heuristic for the pinned paths.
They do not qualify uninspected releases, live Mixin execution or physical
shader effects. The buffer and viewport semantic changes remain separate
commits. Packaging
[`ce8b6c613bc21cc7b1c828cbae0b612993430fb4`](https://github.com/BaiTian6641/MobileGlues/commit/ce8b6c613bc21cc7b1c828cbae0b612993430fb4)
identifies their new delivery candidate as **2.0.7-dev / 2007**. Local native
builds passed for production ARM64, production x86_64 and diagnostic ARM64. The
diagnostic APK was built, verified and installed with no Minecraft process
running; installation is not rendering acceptance. The CI artifacts were
subsequently downloaded and independently verified as recorded below.

The refreshed nine-translation-unit static audit reports **25 existing
diagnostics and zero new diagnostics**, against an allowance of 26. Its
`FSR3Direct.cpp` count decreased from seven to six; this is not a warning-free
whole-repository claim. Separately, the actual Android ARM64 compiler invocation
parsed definitions of `glMapBufferRange`, `glBufferStorage` and
`glFlushMappedBufferRange` successfully. The complete host semantic suite passed
after both corrections.


| Item | Current evidence |
| --- | --- |
| Final viewport semantic revision | `e3ab06c9b61bb97c52bc01816698f86852251d0b`; independent source/host review passed. |
| 2.0.7 packaging revision and native marker | `ce8b6c613bc21cc7b1c828cbae0b612993430fb4`; local marker `src-c338d26c9afb945b`. |
| Local production ARM64 core | SHA-256 `251e9efa71c99891cfcef50b40ccc5ec8980772b0e66902d6590a7dd7e962a21`; used by the full-library readback: three visibility cases PASS, invalid-flags FAIL. |
| Local diagnostic APK | SHA-256 `480e7ba5174aecb2cc6431d64043b0d2ba5417e1b4ec176b9257a3e1a0317a3e`; verified and installed; new Minecraft rendering acceptance PENDING. |
| Production CI APK and native hashes | Independently downloaded and verified; exact packaged payload hashes are recorded below, separately from the local unstripped probe core and diagnostic APK. |
| 2.0.7 CI result | [Run 39574465836](https://github.com/BaiTian6641/MobileGlues/actions/runs/39574465836) succeeded at exact `ce8b6c613bc21cc7b1c828cbae0b612993430fb4`; [artifact 12334357473](https://github.com/BaiTian6641/MobileGlues/actions/runs/39574465836/artifacts/12334357473) independently verified. |
| X1E provider buffer probe and full-wrapper regression | Four below-wrapper provider cases PASS. Corrected GLES3 full-library run: three visibility cases PASS, invalid-flags case FAIL; overall FAIL. Original EGL setup failure retained separately. |
| 26.2 FSR-disabled comparison | INCOMPLETE: uncontrolled restart, cause unknown; no comparison result. |
| 26.2 terrain/HUD and delivery/Fabric profile retests | PENDING device unlock and separate hash-bound runs. |

The verified 2.0.7 CI archive contains version 2007, FSR settings, both ABI
payloads with `src-c338d26c9afb945b`, and the required depth-convention export.
The production payloads exclude the diagnostic trace export. ELF load segments
and APK ZIP alignment passed the 16 KB checks. These archive/build checks do not
change the physical full-library failure recorded above.

| Verified 2.0.7 artifact | SHA-256 |
| --- | --- |
| CI debug APK | `d9889f9fa9cf09148fdce09b98529dd74e06f9defbbfe4768a7bb72b3c7bd244` |
| CI unsigned release APK | `e1f5429bed251adfba14336c64bed638434581cffe9c5c13dd7e0dcc840ff69f` |
| Local signed derivative of CI release payload | `5de10d7971b91bb3b4d938e0c7d2bebd794c3c09c80bc50a2767e48b9530d114` |
| Packaged ARM64 core | `171c9237f68361c7aa87791cf60cb5d2b4de14d81b4548a326aee9484b858175` |
| Packaged x86_64 core | `7aa5922d041448278419c7800e72f82bf0643c8e182598d0cedea7637acadf39` |

The local signed derivative preserves all original APK entry bytes and is a
qualification artifact, not a published production signing identity. Packaged
native libraries are stripped payloads; their hashes are not equated to either
CI's unstripped inputs or the separately built local probe core.

The preceding game had been saved and exited before the planned FSR-disabled
control. The observed restart was not an operator-issued reboot. Neither its
cause nor the control's rendering outcome can be inferred from the interruption.
The existing 2.0.5/0.2.2 failure remains the latest recorded 26.2 rendering result.

For the pending FSR-disabled control, bridge 0.2.3 starts the frame diagnostic at
`beginLevelRendering` independently of FSR availability. Its terrain phase roles
can therefore discriminate that control. `FINAL_DRAW` and `SUBMIT_RESULT` are
not expected when FSR is unavailable, because final-pass entry returns early;
their absence alone is not a control failure. This is a reviewed trace contract,
not a completed FSR-disabled rendering comparison.


## 2.0.8 full-library pass and remaining delivery gates

The bounded buffer-error correction is committed at
[`95499319f3081e51ffc4da1267b0c78eb76f8432`](https://github.com/BaiTian6641/MobileGlues/commit/95499319f3081e51ffc4da1267b0c78eb76f8432),
the reusable full-library test tooling at
[`c02565ef034fc798686271f053a5e25c67832472`](https://github.com/BaiTian6641/MobileGlues/commit/c02565ef034fc798686271f053a5e25c67832472),
and **2.0.8-dev / 2008** packaging at
[`ab4d31db0aa849cb5bbd3446cee91d60a71ef793`](https://github.com/BaiTian6641/MobileGlues/commit/ab4d31db0aa849cb5bbd3446cee91d60a71ef793).
Independent source/security/readability review found no blocker; 187 actual
buffer/getter checks passed under ASan/UBSan. The completed bounded old-source
comparison failed 38 assertions, while the corrected source failed none.
The [buffer record](2026-10-21-buffer-mapping-and-rendering-checkpoint.md)
explains the comparison scope, AST/lint baseline and separate GPU evidence.

| Gate | Current evidence |
| --- | --- |
| Source and host semantic correction | Committed and independently reviewed; 187 focused checks and full host suite PASS. |
| Five-case host provider/actual buffer-plus-getter adapter | Both PASS; previous buffer source passes three visibility cases but fails both error cases through the same adapter. |
| Physical five-case provider probe | PASS below MobileGlues; probe `38631387e7dab2df8b71fa5c4b90dc507663c551ada56dd3df2358c425163704`, result `6bcd22714f94e26fe80837df0c897f1c025831307326a9a39f9bbbc1d590a264`. Same recorded Android/ANGLE/Adreno provider; exit 0 and empty stderr. |
| 2.0.8 local production ARM64 core | SHA-256 `966fef021838e35f58c7889fff7c4105b8ada3379c0e9c87c4484de33d277912`; marker `src-beaba336ea4452da`. |
| 2.0.8 local diagnostic APK | SHA-256 `bcb891f932e7b0e3bb8ee53668f672ce77ebf288d17671858dd5f579b8c1f467`; installed and package version 2008 confirmed with no Minecraft process running. Installation is not a game-rendering pass. |
| 2.0.8 production-build CI | [Run 39578250534](https://github.com/BaiTian6641/MobileGlues/actions/runs/39578250534) **SUCCESS** at exact source `ab4d31db0aa849cb5bbd3446cee91d60a71ef793`; downloaded [artifact 12335463401](https://github.com/BaiTian6641/MobileGlues/actions/runs/39578250534/artifacts/12335463401) independently verified. APK and native payload hashes are recorded below. |
| 2.0.8 physical full-library five-case run | **PASS all five**, exit 0 and empty stderr. Probe `27cd1c9ddc987e472472c5200bbcc035f741f3ee3d8359d9f55dcc25d9ac2105`; result `edb91b4779e1896617386b7bcca87e6324acf290c327a91231d67379f2495ed0`. Exact core/marker above and same recorded Android/ANGLE/Adreno provider. The 2.0.7 failure remains unchanged. |
| Exact CI ARM64 payload physical five-case run | **PASS all five**, exit 0. Extracted packaged core `ad68056127073cd540d148ad73a8063b6cec44d68256d109940247b3a526af94` matches the runner and pushed binary hashes; same expected marker, probe, config, OS and provider. The separate runner evidence hash is recorded below. |
| Minecraft 26.2, delivery 1.21.1 NeoForge and requested Fabric retests | PENDING device unlock and separate profile-specific runs. |

The downloaded CI artifact contains debug and unsigned release APKs with version
`2.0.8-dev` / `2008`, FSR controls, both ARM64 and x86_64 cores carrying
`src-beaba336ea4452da`, the depth-convention JNI export and no diagnostic trace
export. Native ELF load segments and APK ZIP alignment passed the 16 KB checks.
The artifact archive digest is
`daa75b9e08c2100d808a61558738101bf7ddffb5d06e3507fca055ea47e14fb2`.

| Verified 2.0.8 CI/distribution artifact | SHA-256 |
| --- | --- |
| CI debug APK | `4a8e6ee2f23c694fd611bcf51ea82e0b12d78c9d6fa07080a4d683c98e317ba4` |
| CI unsigned release APK | `adf93d0d7e1d7f151a322a702e1e6950b68e0af29767eaf99e252ed7e7ab0fef` |
| Locally signed derivative of the CI release APK | `b00de0016354cb9b26256be248e25fbf14a6b2da8d0009023f35dded5d6aa2c5` |
| Packaged stripped ARM64 core | `ad68056127073cd540d148ad73a8063b6cec44d68256d109940247b3a526af94` |
| Packaged stripped x86_64 core | `cb99ef0f38ed8268a80506cc1951ecfe6b7be2a58c6529aa341fbb6d39489bda` |
| CI unstripped ARM64 input | `3804966747710f26a419850275607aa82bfd09f5b111c04acd09339ebaba47e5` |
| CI unstripped x86_64 input | `37bc44ba8a2f13463ac32aadd77965d3cef4560de6bddab045a6466f6f76d9da` |

The locally signed derivative verifies against the existing local diagnostic
certificate; all original ZIP entries are unchanged and the original unsigned
APK is preserved. It is a qualification package, not a published production
signing identity. The CI debug certificate differs from that local certificate.
The local distribution bundle at `build/distribution/2026-10-21-fsr3-refinement/`
retains these artifacts, verification records and physical probe evidence.

The CI provenance binds the unstripped inputs to their separately hashed
stripped APK payloads. It does not make either CI core byte-identical to the
local unstripped core `966fef…277912` used by the first five-case physical run.
After archive verification, a separate physical run loaded the exact ARM64
core extracted from the CI APK and passed all five cases. Independent review
matched the extracted bytes to the runner's core hash and the pushed binary
hash. The result JSON matches the earlier run byte-for-byte because all its
reported fields match; its SHA-256 remains
`edb91b4779e1896617386b7bcca87e6324acf290c327a91231d67379f2495ed0`.
The distinct runner evidence binds the CI core and has SHA-256
`92c5d1eeca1f6157e0df754b8577fe0783798e203d934e37a1030d80459b5bbd`.
The independent verification appendix has SHA-256
`b897c8395d3938b05dbd6aae3af57358d45e562d83e69aa95c9154a3081a9231`.
Both runs remain preserved; their matching result JSON does not merge their
binary provenance.

This qualifies the exact CI ARM64 payload for the five tested buffer contracts.
It does not qualify its full APK lifecycle, x86_64 runtime or Minecraft rendering.
The installed diagnostic APK is another distinct artifact and remains an
installation result until game tests run.

The latest device-owner check still found the device locked. Once the user
unlocks it, qualification should verify the selected profile and exact native
marker before entering a world. The FSR-disabled control should capture actual
terrain phase state, followed by a comparable enabled run of the same pack and
settings, with each world saved/exited normally. Preserve original pack bytes
and restore backed-up settings. The pending runtime matrix explicitly includes
1.20.1 Create Delight Remake Forge and an isolated Fabric profile, 1.21.1 NeoForge delivery and Fabric,
and 26.2 NeoForge and Fabric. The
[current handoff](HANDOFF-desktop-gl-gaps.md#current-continuation-checkpoint)
records each next step. 1.18.2 Forge/Fabric remain build/static evidence until
separately tested; eight successful adapter builds do not make their physical
rendering results interchangeable.


A separate bridge review is correcting depth-generation handling in the six
legacy adapter profiles when a quality change replaces the managed depth
texture or transitions to NativeAA. Its revised source/artifacts and
qualification are still pending; 0.2.3 remains the last verified CI artifact
set recorded here. The issue has not been established as the cause of 26.2's
missing terrain. The native five-case pass does not qualify that bridge fix or
remove its lifecycle testing requirement.
