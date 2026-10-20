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
