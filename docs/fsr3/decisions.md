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
this ANGE/GLES provider; generated ESSL must use combined samplers and explicit image formats.
SPD's `FFX_SPD_NO_WAVE_OPERATIONS` LDS fallback avoids the unavailable subgroup-quad path.
These are source-level port requirements, not evidence that the full algorithm runs on the
device.
