# FSR3 architecture decisions

## D-FSR-001: Use AMD FidelityFX SDK v1.1.4 / FSR3.1.4 as the reference source

Status: proposed in S2, pending backend source verification.

Pin commit `c6efa6bf7f2027b3ec94f28578bb5965eabb9e55`. Do not vendor SDK main or mix in a later
SDK generation. FSR3 upscaling is separable from frame generation; frame generation is outside
this milestone. The backend worker must verify licenses and the actual upscaler files before
integration.

## D-FSR-002: Installed renderer identity is `8ac7036`, not the integration checkout

Status: accepted from S0 evidence.

Plugin 2.0.0 embeds a core built from `/home/polar/MobileGlues-plugin/MobileGlues` at `8ac7036`.
The integration checkout is `355d797`, which contains later compatibility fixes. All device
results must name the exact APK/core hash; a pass on one binary does not certify the other.
Do not reset the user's checkout or overwrite plugin work.

## D-FSR-003: FSR1 is output-first and has a real EASU→RCAS chain

Status: accepted design; implementation under review.

- The EGL surface is the output extent.
- `target_width/height == output_width/height`.
- `render_width/height` is derived from the selected preset and output extent.
- EASU writes an output-sized intermediate; RCAS reads that intermediate and writes the final
  output. No pass may silently discard EASU.
- One intentional color format policy survives resize. RGBA8 is the conservative first choice.
- The injected pass saves and restores every state it changes or depends on.
- Off mode allocates no FSR resources and preserves normal AA/jitter.

This is an FSR1 correction and S1 foundation, not an FSR3 implementation.

## D-FSR-004: Prefer same-provider GLES compute, retain Vulkan only as a bounded fallback

Status: proposed, gated on S2 evidence.

The upscaler stays inside MobileGlues. A GLES compute backend is preferred because it uses the
existing EGL/GLES resource namespace. A Vulkan production alternative requires proof of the
complete external-memory, format, lifetime, fence, and synchronization chain; ANGLE's internal
Vulkan use is not resource sharing. A pinned Vulkan reference harness may be used for
comparison without qualifying an Android binary.

## D-FSR-005: No hidden success or effect suppression

Status: accepted.

Internal shader/program/FBO/capability checks use truthful backend results regardless of the
application `ignoreError` setting. A failed FSR3 mode falls back to a coherent available mode;
it is not reported as FSR3 success. FPS improvements obtained by disabling ACT, WSR, player
reflections, or other requested effects fail the feature-preservation gate.

## D-FSR-006: Native-resolution HUD and explicit jitter ownership

Status: accepted.

FSR3 reconstructs the scene before HUD/UI composition. UI, chat, inventory, and menus remain at
display resolution. Exactly one component applies camera jitter; the adapter must report the
actual jitter to the backend. Replacing the final TAA resolve does not authorize globally
disabling effect histories or denoisers; each is audited separately.

## D-FSR-007: Keep original shaderpack archives byte-identical

Status: accepted.

Qualification profiles are versioned integration metadata/transforms. A transformed pack is a
distinct configuration, not evidence of universal unmodified-pack support. Complementary r5.9.3
is the qualification target; r5.9.2 remains the regression lane.

## D-FSR-008: FSR3 runs at the pack reconstruction boundary, not the EGL swap

Status: accepted from the S2 game audit.

For Complementary Reimagined r5.9.3/r5.9.2, final TAA is the pack's `composite6` pass. A
swap-time upscale in `egl.cpp` is too late: Iris has already produced a native-resolution
image and the HUD may be composed. The first supported profile must replace the `composite6`
temporal resolve with MobileGlues reconstruction, then let `composite7`/`final` and the native
HUD finish the image. An appended post-final upscale is not FSR3 integration.

An HDR boundary after `composite1`, before bloom/tonemap, is a possible higher-quality profile
but needs a display-resolution HDR target and separate validation. It is not the first profile.

## D-FSR-009: The adapter must own a scene-wide jitter; pack jitter is not sufficient

Status: accepted from the S2 game audit.

Complementary's `TAAJitter` is applied inside selected vertex shaders only (terrain, water,
clouds and similar); entities, hands and several textured/effect programs are not jittered.
`gbufferProjection` is not jittered. Therefore the pack's 8-phase jitter cannot be reported as
the FSR3 scene jitter. FSR3 requires one jitter value applied to all raster (and ray) geometry
and the same value reported in the dispatch. Until the adapter establishes that ownership,
FSR3 movement support is camera-only and experimental.

## D-FSR-010: The GLES port needs explicit format and binding adaptations

Status: accepted as S2 spike direction; device runtime not proven.

The ANGLE provider rejects image load/store for `r8`, `r16f` and `rg16f`; FSR3.1.4 uses all
of these internally. The port therefore maps internal storage to legal ESSL formats
(`r32f`, `rgba16f`, `rgba8`, `rgba16f`, `r32ui`, `rgba32f`), with documented bandwidth and
precision consequences. The Vulkan GLSL uses separate texture/sampler objects and
`GL_EXT_samplerless_texture_functions`/`GL_EXT_shader_image_load_formatted`, neither exposed by
this ANGLE/GLES provider; generated ESSL must use combined samplers and explicit image formats.
SPD's `FFX_SPD_NO_WAVE_OPERATIONS` LDS fallback avoids the unavailable subgroup-quad path.
These are source-level port requirements, not evidence that the full algorithm runs on the
device.

## D-FSR-011: GLES compute is the selected production backend route

Status: accepted after S2 dispatch evidence.

On X1E, the adapted FSR3.1.4 ESSL executed through ANGLE/Adreno: RCAS matched a
CPU model byte-exactly on 56,604/56,604 and 228,404/228,404 interior pixels, and
the full eight-pass chain passed flat-frame invariants and produced output/state
hashes identical to the host Mesa run at 2x and 1.5x. This selects the GLES
route for the production backend. It does not qualify the product: the chain
harness uses test-only readback, full barriers and `glFinish`; multi-frame
history/reset, ABI wiring, persistent resource reuse, real Minecraft inputs and
performance remain unproven. A Vulkan interoperation experiment is not opened
unless a concrete GLES production requirement fails.

## D-FSR-012: The first production backend is LDR and backend-output owned

Status: accepted for the S3 slice; narrower than ABI v1.

The in-tree GLES backend accepts LDR color and returns its own RGBA8 output;
the adapter reads it through `mg_fsr_get_output`. Application-owned output, HDR
color and non-full active rectangles are rejected with explicit errors until a
qualified profile exists. This avoids claiming support for color/exposure and
composition paths that have not been validated. The ABI structures already
carry those fields; enabling them is an additive backend capability, not a
contract change.

## D-FSR-013: The optional Iris connector submits the current main framebuffer

Status: experimental; performance FAIL at native scene size.

The bridge mixin runs at the end of Iris' `renderFinalPass`, where the main
framebuffer is current and before vanilla HUD composition. A tiny JNI helper
reuses the MobileGlues core already mapped by the renderer plugin, avoiding a
second EGL/GL state. The native path resolves the current color/depth texture
attachments and runs the same in-tree backend. It does not reduce scene
resolution, so the 8-pass cost is a net loss at 2880x1585 (13 FPS versus 26 FPS).
The config switch is off by default.

## D-FSR-014: Low-resolution Minecraft scene work is still missing

Status: blocker for the requested performance goal.

Neither the native presentation path nor this bridge changes Iris/Minecraft
render-target allocation, viewports, depth extents or screen-size uniforms. The
current FSR3 execution is therefore a reconstruction/quality experiment only.
A true low-resolution path needs a versioned scene-scale hook that keeps HUD at
display resolution and scales all scene-dependent targets/viewports/depth
coherently. Do not claim FSR3 performance support before that hook and its
measurements exist.


## D-FSR-015: FSR3 quality presets are reconstruction extents, not scene scale

Status: accepted for the quality-control slice.

`fsr3Quality` maps NativeAA/Quality/Balanced/Performance/Ultra Performance to
1.0/1.5/1.7/2.0/3.0x output-to-render ratios. The native path and the optional
Iris bridge downsample the final LDR image into the selected extent and
reconstruct to native output. This lets the quality slider be tested and keeps
the backend output-first, but it deliberately does not change application
render-target allocation or viewports. The 13-versus-26 FPS native-scene result
therefore remains a performance failure and D-FSR-014 remains the blocker.


## D-FSR-016: The first scene adapter is Iris-specific and viewport-aware

Status: experimental; implemented, device NOT RUN.

The optional Minecraft/Iris adapter scales `RenderTargets` at construction and
resize, then the core viewport/scissor wrappers scale a native-sized viewport
only when the currently bound draw FBO has a smaller color attachment. The
final pass resets to native and the swap-path direct present is suppressed for
250 ms after a bridge submit, preventing a second FSR pass over the HUD. This is
a bounded adapter, not a general renderer-scale feature. The ABI and backend
remain core-owned. Screen uniforms, non-color passes, and independent motion are
still unverified; do not advance S4 on compile evidence.


## D-FSR-017: Integration review is a gate, not a performance pass

Status: accepted 2026-10-20.

The independent integration review found concrete backend and boundary defects.
R1 (unsigned reconstructed-depth sampler/filter), R6 initialization state,
R7 camera continuity and R9 damage-swap routing were fixed and host/device
smoke-tested. R2 (final-image reconstruction boundary), R3 (scene depth and
screen-unit grid), R4 (float camera precision), R5 (jitter/depth/TAA
ownership), R8 (ABI field/capability validation) and R10 (steady-state cost)
remain open. The first full-frame game run proves rendering/HUD/camera-motion
plumbing only; it is not a matched performance or image-quality qualification.
