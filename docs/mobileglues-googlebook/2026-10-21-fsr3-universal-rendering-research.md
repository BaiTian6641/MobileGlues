# Universal compatibility research and rendering roadmap

Research date: 2026-10-21. This document describes source findings, the eight pinned MobileGlues FSR3 bridge adapters, and the work needed to broaden them. It is not a claim of universal Minecraft, shaderpack, GPU or temporal FSR3 support. Runtime acceptance must identify the exact bridge/core artifacts and game, loader, shader dependency, backend and hardware tuple.

The current approach is a version-specific Iris/Oculus connector into MobileGlues' existing native upscaler. Build compatibility, successful shader compilation, a loaded mod, a successful dispatch, correct frame contents and temporal image quality are different evidence levels.

## The rendering contract

Three dimensions must remain explicit: the scene render extent, Minecraft's main output extent, and the surface/swapchain extent. The scene may be smaller than the main output. The main output and surface often match, but launcher resolution settings and other rendering integrations can make them differ.

Iris documents the `final` program as writing the screen/backbuffer at Minecraft window resolution regardless of other buffer sizes or scaling factors. Reducing scene targets does not make the final pass scene-sized. Final coordinates, scene texture coordinates and presentation coordinates must be treated according to their actual contracts. [Iris final program reference](https://shaders.properties/current/reference/programs/final/)

A bridge cannot determine these roles from the currently bound framebuffer alone. It must know which resource represents scene input, which resource is the final output, the valid rectangle in each, and who owns their lifetime. A full-screen blit later in the pipeline is not evidence that an earlier size mismatch will be repaired.

## Eight exact adapters

The adapter matrix covers the following release artifacts. Their full dependency hashes and API descriptors are recorded in the [pinned compatibility lock](https://github.com/BaiTian6641/MobileGlues-FSR3-Bridge/blob/7be7089c69711a9ef206039ad33e48c39e35a234/compatibility-lock.json). Game Java versions below describe the target runtime; shared bridge classes are compiled to Java 17 bytecode.

| Minecraft | Loader | Pinned shader dependency | Distribution version ID | Game Java |
| --- | --- | --- | --- | --- |
| 1.18.2 | Fabric | Iris 1.6.11 | `ogIRhnAJ` | 17 |
| 1.18.2 | Forge | Oculus 1.6.4 | `UynWw2df` | 17 |
| 1.20.1 | Fabric | Iris 1.7.6+mc1.20.1 | `s5eFLITc` | 17 |
| 1.20.1 | Forge | Oculus 1.8.0 | `iQ1SwGc3` | 17 |
| 1.21.1 | Fabric | Iris 1.8.14-beta.1+mc1.21.1 | `bAo1Qhte` | 21 |
| 1.21.1 | NeoForge | Iris 1.8.14-beta.1+mc1.21.1 | `KduFYu4t` | 21 |
| 26.2 | Fabric | Iris 1.11.4+mc26.2 | `gxZWWnKH` | 25 |
| 26.2 | NeoForge | Iris 1.11.4+mc26.2 | `k55HdONq` | 25 |

The exact shader dependency SHA-256 values in that lock are:

| Profile | Shader dependency SHA-256 |
| --- | --- |
| `1.18.2-fabric` | `fa266e2016e8471354b876804936cbb63284be0eb7a15f3c73e34101d7ede474` |
| `1.18.2-forge` | `7731a22424f21f2637a6ad565c12d9b063bbefc4d516a292cb80d9e0ac9798ba` |
| `1.20.1-fabric` | `9eb15e563e0c9ae6eff15b7863f8432dd290cf9f724c7373e9b15b14aa829ed5` |
| `1.20.1-forge` | `0945df0cba0f62b3901dd80c3268e5311b770ece78c78037a45db12ac0425fef` |
| `1.21.1-fabric` | `0ceb694040b4628bf3fa02286cfe5f0db833195669ab07f8caa4001359763eff` |
| `1.21.1-neoforge` | `60d5f8bf52f25e9986440f4a4270a6bd986d9cff994510e1108ac1547063b314` |
| `26.2-fabric` | `f1f7ab57c974d193ba33aa285864a0ded949216f402116fceaf4dc7739b4dd7c` |
| `26.2-neoforge` | `5faeb646ff117bfc29e636baa9085f6fff6d048a328618749a991fd01c70019d` |

Hashes identify inspected input artifacts. They are not runtime acceptance results.

These adapters implement pinned scene allocation/resize hooks, matching scene depth, viewport suppliers, camera capture and a final-pass submission boundary. Packs without the required final pass remain unscaled. Android native helper builds cover ARM64 and x86-64; building an ABI does not qualify every device using that ABI.

There is no 1.17 adapter in this matrix, and no implied coverage for every intermediate Minecraft, Iris/Oculus, Forge or NeoForge release. The 1.21.1 NeoForge delivery lane remains distinct from legacy and experimental lanes. A shader dependency pin also does not select or qualify every possible companion rendering mod.

The existing published validation record separates build/host checks from its bounded runtime smoke. New visual or lifecycle results should be added with their own artifact identities rather than inferred from that earlier run. [Versioned validation record](https://github.com/BaiTian6641/MobileGlues-FSR3-Bridge/blob/e04d93bbfa49d63ff3332eeff8bfbd5f1506fc7a/docs/validation.md)

## API families from 1.17 through 26.2 and later

| Transition | Primary-source change | Adapter consequence |
| --- | --- | --- |
| 1.17 development, 21w10a | OpenGL 3.2 core replaced fixed function rendering with shaders. [Mojang 21w10a](https://www.minecraft.net/de-de/article/minecraft-snapshot-21w10a) | Establish a shader and presentation boundary; do not assume all later framebuffer APIs remain equivalent. |
| Pinned 1.18.2–1.21.1 artifacts | Direct inspection finds an explicit main-target `bindWrite(true)` or mapped equivalent near the end of all six legacy final-pass methods. | The existing post-return current-FBO assumption has a concrete prerequisite for these exact inputs. It is not a version-wide guarantee. |
| 1.21.2 | FrameGraphBuilder, FramePass and resource handles were introduced. [NeoForge 1.21.2 primer](https://docs.neoforged.net/primer/docs/1.21.2/) | Track resource lifetime and pass roles separately from a GL binding. No adapter for this family is currently in the matrix. |
| 1.21.5 | RenderTarget color/depth IDs became GpuTexture objects; framebufferId and bindWrite/unbindWrite disappeared. GpuDevice, CommandEncoder and RenderPass own rendering. [NeoForge 1.21.5 primer](https://docs.neoforged.net/primer/docs/1.21.5/) | Resolve the intended output resource through the new owner. Renaming a legacy hook is insufficient. |
| 1.21.6 | Render targets gained texture views, and render-pass attachments/presentation began using views with mip and lifetime semantics. [NeoForge 1.21.6 primer](https://docs.neoforged.net/primer/docs/1.21.6/) | Include view, mip and extent in the resource contract. |
| 1.21.6 development, 25w16a | Core/post-process uniforms moved into blocks; SamplerInfo describes output and input texture sizes. [Mojang 25w16a](https://www.minecraft.net/en-us/article/minecraft-snapshot-25w16a) | Replacing loose uniforms cannot be a general solution for modern vanilla rendering. Iris custom uniforms are a separate adapter surface. |
| 1.21.9 | OpenGL 3.3 became required. Vanilla full-screen passes changed to three vertices generated using gl_VertexID. [Mojang 1.21.9](https://www.minecraft.net/en-us/article/minecraft-java-edition-1-21-9) | Verify topology and shader inputs at the actual integration point. Iris' final draw has its own implementation. |
| 26.2 | The main target moved to GameRenderer.mainRenderTarget; window presentation uses GpuSurface, and render-pass closure submits work. [NeoForge 26.2 primer](https://docs.neoforged.net/primer/docs/26.2/) | Acquire authoritative main output identity and handle render-pass cleanup explicitly. |
| 26.2 backend choice | OpenGL is the default backend and Vulkan is experimental. Raw OpenGL integrations need a separate migration to operate through native Vulkan. [Fabric 26.2 guidance](https://www.fabricmc.net/2026/06/15/262.html) | The current bridge requires an OpenGL context. Minecraft's native Vulkan backend is unsupported. |
| Later releases and hardware | This investigation establishes no future API or hardware contract. | Audit exact artifacts and providers before adding coverage; no inherited 26.3+, X2E or future-driver qualification. |

Minecraft's minimum OpenGL version is not a list of every feature a shaderpack may use. Desktop OpenGL/GLSL semantic coverage in MobileGlues remains a separate requirement from the bridge's Minecraft API compatibility.

## Two concrete scaling failures and the bounded corrections

The source investigation identified distinct failures rather than a single universal scaling formula.

The corrections described here are implemented in the bridge lineage through
[0.2.2, commit 7be7089](https://github.com/BaiTian6641/MobileGlues-FSR3-Bridge/commit/7be7089c69711a9ef206039ad33e48c39e35a234).
They remain under runtime qualification. The latest native depth correction,
bridge ownership protocol and observed 0.2.1 failures are recorded separately in
[depth contract and runtime status](2026-10-21-fsr3-depth-contract-and-runtime-status.md).
A published source revision or successful host test does not establish device acceptance.

For a viewport-derived custom uniform, a shaderpack can cache reciprocal viewWidth/viewHeight during scene rendering and then consume that value with gl_FragCoord in its native final pass. Updating only the direct native uniforms leaves the derived value stale. The bounded correction refreshes viewport roots and eligible pure dependent cached nodes in their existing evaluation order when entering and leaving the final scope. It preserves cached stateful/unknown nodes instead of advancing smooth/random operations twice per frame. A mixed expression containing both viewport data and stateful operations remains outside this correction; arbitrary expression rewriting is not implemented.

The later Create/Oculus target retest with bridge 0.2.1 displayed Steadfast 0.8.0
terrain, scene and HUD across the frame. That is a bounded visual regression pass,
not proof for every shader expression or preset. The 26.2 retest with bridge
0.2.2 and renderer 2.0.5-dev still failed complete terrain/HUD rendering despite
successful loading and tracing. Exact artifact/source boundaries and capture
hashes are in the linked runtime status record.

For both pinned Iris 26.2 artifacts, final drawing uses the main color texture view through a render pass. Closing that pass binds framebuffer zero. Optional later swap copies bind scene-source framebuffers, and `renderFinalPass` returns without a main-target rebind. Therefore, inspecting the current draw FBO after return can find zero or a reduced scene target. Calling that reduced size native and dividing by the quality factor again creates a scale feedback error. The six pinned legacy artifacts explicitly rebind main output and do not have this particular missing-rebind condition.

The bounded 26.2 correction obtains Minecraft's current main color texture and reuses Iris' existing `colorHolder`. Before dispatch, it validates framebuffer completeness, attachment identity/type/mip and agreement between target, texture-object and GL dimensions. Submission is scoped and replays the saved framebuffer, texture and viewport state afterward. Actual state recovery through MobileGlues' viewport policy remains a runtime gate, particularly for arbitrary subrectangles. It allocates no additional GL resources. Exact fields and methods are checked during the adapter build.

Direct inspection of the pinned Minecraft 26.2 implementation also finds that GlSurface delegates presentation to a blit with equal source/destination rectangle dimensions, cropped to the smaller source/swapchain extent. It does not generally upscale a small source to fill the window. This is a finding about the inspected implementation, not a promise about every future presentation backend.

The inspected Minecraft JAR SHA-256 is `40896ee9f1e2bec3c934daac7e93d41e9e3d9c2f8ae0ca366d52ffbfd1afa290`; the Iris/Oculus artifact hashes are in the linked lock. A reproducible review can run `javap -p -c` on FinalPassRenderer, GlCommandEncoder, GlSurface and GlStateManager from those exact inputs. Host fault-injection tests verify rejection and cleanup paths; they do not substitute for running those paths through Minecraft and MobileGlues.

These corrections preserve the existing native-final contract. A broader solution must define a coherent virtual rendering space across window queries, targets, depth, matrices, uniforms, hand/HUD composition and presentation. Extending selective uniform replacement to every arbitrary expression is not that architecture.

## Upstream architectural comparison

Super Resolution release `0.8.3-alpha.5`, commit `2bc16b46b3c61e62d31c4ae0109f16694b7718d5`, explicitly added 26.2 Fabric/NeoForge for OpenGL and recorded Iris reload fixes. This is useful upstream design evidence, not Android or MobileGlues acceptance. [Upstream release](https://github.com/IReallyWantToSleep/superresolution/releases/tag/0.8.3-alpha.5)

Its ordinary mode keeps separate screen/render dimensions and scales window dimension queries during its rendering scope. It replaces the client main target with an owned reduced target, restores the original output at world-render completion, dispatches from explicit color/depth resources, and blits to the original output. Post-chain sizing, native hand rendering and lifecycle restoration receive separate handling. [WindowMixin](https://github.com/IReallyWantToSleep/superresolution/blob/2bc16b46b3c61e62d31c4ae0109f16694b7718d5/common/src/hack/java/io/homo/superresolution/common/mixin/core/WindowMixin.java), [RenderHandlerManager](https://github.com/IReallyWantToSleep/superresolution/blob/2bc16b46b3c61e62d31c4ae0109f16694b7718d5/common/src/main/java/io/homo/superresolution/common/minecraft/handler/RenderHandlerManager.java), [MinecraftRenderHandler](https://github.com/IReallyWantToSleep/superresolution/blob/2bc16b46b3c61e62d31c4ae0109f16694b7718d5/common/src/hack/java/io/homo/superresolution/common/minecraft/handler/MinecraftRenderHandler.java)

Its shader-cooperative mode describes color, depth, motion, exposure, jitter and output regions explicitly. That distinction is valuable: generic scene capture and a shader-cooperative temporal integration have different prerequisites. An ordinary shaderpack cannot be assumed to expose correct moving-object vectors or compatible temporal history. [IrisShaderCompatUpscaleDispatcher](https://github.com/IReallyWantToSleep/superresolution/blob/2bc16b46b3c61e62d31c4ae0109f16694b7718d5/common/src/shadercompat/java/io/homo/superresolution/shadercompat/IrisShaderCompatUpscaleDispatcher.java)

No upstream implementation is copied by this research. Adopting a broader target/window virtualization design would be a separate implementation with its own lifecycle and compatibility gates.

## Why full temporal FSR3 remains unqualified

The current bridge's default path resets native temporal history each frame.
The 26.2 reversed-sample protocol added in 0.2.2 also forces zero motion and a
history reset even when experimental history is requested. Camera reprojection
for the legacy forward-depth path remains an experimental opt-in and is
classified as camera-only motion. It does not establish correct vectors for independently moving objects, animated geometry, particles, the hand or every shaderpack's sky. Jitter ownership, actual projection/depth parameters, exposure and the boundary with the shaderpack's existing TAA are unresolved integration requirements. Dispatch success does not prove those inputs are coherent.

AMD's integration contract requires compatible color/depth dimensions, current-to-previous motion with defined scaling, correct depth conventions and controlled jitter. Reactive and transparency/composition masks address surfaces that color/depth/motion alone cannot fully describe; exposure must match the surrounding rendering pipeline. Those contracts must be measured and implemented at the chosen capture point before temporal history is qualified. [AMD FSR3 upscaler integration manual](https://gpuopen.com/manuals/fidelityfx_sdk/techniques/super-resolution-upscaler/)

This bridge path contains no frame-generation implementation. AMD separates upscaling and frame generation; frame generation adds presentation/swapchain integration, scheduling and UI composition requirements. Reconstructing a larger image with an upscaler is not evidence of generated frames or improved frame pacing. [AMD FSR3 integration overview](https://gpuopen.com/fidelityfx-super-resolution-3/)

## API-family roadmap and acceptance gates

| Work package | Required implementation/evidence | Completion boundary |
| --- | --- | --- |
| Qualify the eight pinned adapters | Verify actual loader transformation, scene/depth/output dimensions and full-frame content for each exact tuple. Preserve artifact hashes and original shader archives. | A profile becomes runtime-qualified only for its recorded tuple; build/host checks remain separate. |
| Stabilize legacy direct-GL integration | Exercise final/no-final packs, presets, shader reload, resize, world/dimension changes, save/quit, rejection fallback and GL state recovery. | No incorrect size inference, repeated scale application or corruption of HUD/hand output. |
| Introduce a coherent scene/output contract | Represent input/output identities, extents, valid rectangles, formats and generation/lifetime explicitly. Resolve cases where main output differs from EGL/swapchain. | No output role inferred from an arbitrary bound attachment; mismatches fail visibly with a preserved valid frame. |
| Add missing API families | Implement and verify distinct adapters for frame-graph, GpuTexture, texture-view/uniform-block and later presentation APIs as needed. Audit exact release bytecode and loader mappings. | Supporting two endpoints does not automatically cover every version between them. |
| Qualify temporal reconstruction | Establish jitter and motion ownership, moving-object handling, depth/projection/exposure conventions, mask policy, history reset and shaderpack TAA isolation. Compare stable and moving scenes with artifacts and timings recorded. | Temporal history is enabled by evidence, not merely by an option or successful dispatch. |
| Evaluate future backend/hardware work | Audit native Vulkan separately; record the actual provider and capabilities on each new GPU/driver, including X2E when available. | No native-Vulkan or future-hardware claim inherited from an OpenGL build. |

Qualification should include NativeAA and every scaled preset, distinct input/output rectangles, shader switching, resize, repeated world transitions, failure recovery and measurable frame time. A native guard requiring output color extent to equal the EGL surface is a bounded rejection policy; it does not by itself support launcher configurations with different main/surface sizes. Broader support requires an explicit negotiated output contract.

CI establishes compilation, lint, AST/package checks, pinned API verification and reproducibility where those jobs pass. Host fixtures establish selected control-flow and failure behavior. Provider probes establish the tested provider's behavior. Minecraft acceptance establishes the tested runtime tuple. None of these evidence levels should be relabelled as universal support.
