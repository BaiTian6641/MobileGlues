# Primary references for the FSR3 implementation handoff

Checked on **20 October 2026**. These supplement the references in the
[research baseline](../MobileGlues_FSR3_Research_and_Implementation_Plan.md).
They support the [implementation notes](implementation-notes.md); they do not
establish that the current MobileGlues port or game bridge meets the described
requirements.

Prefer the pinned source revisions below over a moving `main`/`dev` branch.
The project baseline is AMD FidelityFX SDK **v1.1.4**, commit
`c6efa6bf7f2027b3ec94f28578bb5965eabb9e55`, containing the FSR3.1.4 upscaler.
The audited Iris source revision is
`eb7afb99f747cc8ed5ee4072119539035d33cefd`. Verify the actual distributed
Iris JAR and its method descriptors separately.

## AMD algorithm and implementation

| ID | Primary source | Use and limitation |
| --- | --- | --- |
| W01 | [FSR3.1.4 upscaler integration guide](https://gpuopen.com/manuals/fidelityfx_sdk/techniques/super-resolution-upscaler/) | Overall temporal integration, inputs and pipeline placement. It describes supported upstream API paths; it is not an Android GLES support guarantee. |
| W02 | [FfxFsr3UpscalerDispatchDescription](https://gpuopen.com/manuals/fidelityfx_sdk/reference_documentation/structs/ffx_fsr3_upscaler_dispatch_description/) | Field meanings and units for resources, motion scale, jitter, time, projection and reset. Translate MobileGlues fields explicitly rather than relying on similar names. |
| W03 | [FidelityFX SDK v1.1.4 release](https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/releases/tag/v1.1.4) | Reproducible upstream baseline. A newer version existing is not a reason to silently change this integration. |
| W04 | [Pinned FSR3 upscaler host implementation](https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/blob/c6efa6bf7f2027b3ec94f28578bb5965eabb9e55/sdk/src/components/fsr3upscaler/ffx_fsr3upscaler.cpp) | Compare pass scheduling, constants, depth conversion, jitter helpers, time conversion and reset behavior against the GLES port. |
| W05 | [Pinned GLSL resource callbacks](https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/blob/c6efa6bf7f2027b3ec94f28578bb5965eabb9e55/sdk/include/FidelityFX/gpu/fsr3upscaler/ffx_fsr3upscaler_callbacks_glsl.h) | Verify resource types, motion-load scaling and conditional jitter cancellation. Source compatibility does not establish GLES driver correctness. |
| W06 | [Pinned SDK license](https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/blob/c6efa6bf7f2027b3ec94f28578bb5965eabb9e55/LICENSE.txt) | Retain required upstream notices with vendored/generated derivatives. Keep project-specific changes auditable. |

## Graphics state and timing

| ID | Primary source | Use and limitation |
| --- | --- | --- |
| W07 | [Khronos EXT_disjoint_timer_query](https://registry.khronos.org/OpenGL/extensions/EXT/EXT_disjoint_timer_query.txt) | Extension/counter support, result availability and invalidation of disjoint intervals. Underpins nonblocking measurement; a nonnull function pointer alone is insufficient. |
| W08 | [OpenGL ES 3.2 specification](https://registry.khronos.org/OpenGL/specs/es/3.2/es_spec_3.2.pdf) | Pixel-transfer buffers, sampler state, texture completeness, framebuffer/blit state and compute visibility. Use the real provider's advertised limits/extensions rather than claiming unimplemented wrapper features. |
| W09 | [Android GPU Inspector system profiler](https://developer.android.com/agi/sys-trace/system-profiler) | Whole-system tracing to distinguish CPU scheduling, GPU work and presentation. Capture support and counter availability still need checking on quartz/ANGLE. |

## Iris and Minecraft integration examples

| ID | Primary source | Use and limitation |
| --- | --- | --- |
| W10 | [Pinned Iris ViewportUniforms.java](https://github.com/IrisShaders/Iris/blob/eb7afb99f747cc8ed5ee4072119539035d33cefd/common/src/main/java/net/irisshaders/iris/uniforms/ViewportUniforms.java) | Establishes where viewport-related uniforms get their dimensions in the audited source. Helps identify the mismatch when only Iris render targets are resized. |
| W11 | [Pinned Iris FinalPassRenderer.java](https://github.com/IrisShaders/Iris/blob/eb7afb99f747cc8ed5ee4072119539035d33cefd/common/src/main/java/net/irisshaders/iris/pipeline/FinalPassRenderer.java) | Confirms final-pass responsibilities and why an after-return callback sees a finished frame. The next bridge must negotiate an earlier profile-specific resource handoff. |
| W12 | [Pinned Iris RenderTargets.java](https://github.com/IrisShaders/Iris/blob/eb7afb99f747cc8ed5ee4072119539035d33cefd/common/src/main/java/net/irisshaders/iris/targets/RenderTargets.java) | Ownership, resizing and external-depth dependencies. Width/height interception alone does not scale every consumer. |
| W13 | [Iris shader uniform reference](https://shaders.properties/current/reference/uniforms/overview/) | Names and general semantics of current shader interfaces. This is moving documentation; verify availability and meaning in the pinned 1.21.1 lane. |
| W14 | [SuperResolution mod upstream documentation](https://github.com/IReallyWantToSleep/superresolution/blob/dev/docs/README_EN.md) | Example of a Minecraft upscaling integration and its platform constraints. Its documented Android exclusion means this is design research, not a drop-in MobileGlues dependency. |
| W15 | [SuperResolution shaderpack interface, version 3](https://sr.187j3x1-114514.org/docs/Shaderpack-Interface-documentation/Version-3/) | Explicit input/output, motion/jitter and trigger concepts. The documented newer-lane velocity support must not be presented as an existing Iris 1.8.14/MC1.21.1 feature. |

## JNI and Android packaging

| ID | Primary source | Use and limitation |
| --- | --- | --- |
| W16 | [Java 21 JNI function specification](https://docs.oracle.com/en/java/javase/21/docs/specs/jni/functions.html) | Typed array access and JNI contracts. Supports the need to pair or negotiate the changed double-position camera signature before calls. |
| W17 | [Android 16 KB page-size support](https://developer.android.com/guide/practices/page-sizes) | Native ELF and packaging compatibility. Verify core and helper separately; retain the launcher's tested native-extraction behavior and distinguish the target's actual 4 KB runtime page size. |

## Local evidence used alongside web research

These are repository/source observations, not claims borrowed from the sites:

- [Follow-up review](audits/2026-10-20-followup-review.md) and its
  [host reproductions](audits/2026-10-20-followup-evidence/README.md).
- [First integration review](audits/2026-10-20-integration-review.md) and
  [review-fix evidence](audits/2026-10-20-review-fix-evidence.md).
- [Minecraft/Iris pass-input audit](audits/minecraft-iris-pass-input.md), for
  Complementary boundaries, motion/jitter gaps and pack effect dependencies.
- [Scene adapter game run](validation/2026-10-20-scene-adapter-quality.md),
  [camera re-arm run](validation/2026-10-20-x1e-r7-camera.md),
  [state-restoration probe](validation/2026-10-20-x1e-state-restore.md) and
  [timing record](validation/2026-10-20-x1e-per-pass-timing.md).

The proposed frame ownership, bridge handshake, resource graph, profile
strategy and acceptance sequence are engineering recommendations derived from
these sources and the reviewed code. They are not upstream promises of
arbitrary shaderpack compatibility or measured X1E performance.
