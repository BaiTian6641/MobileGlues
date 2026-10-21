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
| 26.2 FSR-disabled comparison | The preceding world was saved and quit before launching the control. USB ADB disconnected at 03:41 local time during control capture; no screenshot or usable result was obtained. | **INCOMPLETE; no rendering result.** A later uptime check confirmed a device restart; the reported boot reason was `reboot`. No reboot command was issued by the test operator. The cause is undetermined, and sign-in is required before continuing. |
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
these changes as **2.0.6-dev / 2006**. Builds and device qualification of that
artifact were pending at this documentation checkpoint. It must not inherit
the results recorded for 2.0.4 or 2.0.5.
