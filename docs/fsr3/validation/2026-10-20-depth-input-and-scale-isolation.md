# FSR3 depth input and scaled-scene isolation

Date: 2026-10-20
Device: HP Googlebook 14 `C1L63708PZ`, X1E80100, Adreno X1-85,
Android 17 / API 37, ANGLE Vulkan 1.4.295, ES 3.2.
Pack: original `AuroraLumen-Mobile-v2.zip` (`6e8e1942…`), profile Custom (+4),
FSR3 Experimental / Balanced, scale 1.7, surface 2880×1585.

## Defect found in the post-final bridge

Iris `FinalPassRenderer.colorHolder` has only a color attachment. The previous
`nativeSubmit` required a depth attachment on the current final FBO, so the
post-final submit returned before dispatch. The visible 1.7× scene was Iris
scaling only; FSR3 was not running.

Fix: the bridge now supplies Iris' managed scaled scene depth
(`Fsr3Bridge.sceneDepthTextureId()`) through
`NativeBridge.setSceneDepth()` / `FSR3SetBridgeDepthTexture()`. The native
submit converts that depth to the scene-sized FSR3 input instead of requiring
final-FBO depth.

Installed evidence build:

- APK `42b249b4…`
- Core `7e30202f…`
- Bridge `06338d3d…`
- Trace: `AuroraLumen-Mobile-v2.zip`, `history_env_present=false`,
  `motion=zero`, scale 1.7, scene depth 1694×932.
- Bridge log: `FSR3 submit success` (one-time marker).
- Core submit: 1694×932 scene, 2880×1585 output.

This proves the FSR3 backend now dispatches. It does not prove the current
post-final boundary improves image quality.

## Scaled-scene isolation

Diagnostic property `debug.mgfsr3.bypass=1` skips native FSR3 dispatch while
leaving Iris scene scaling active (`RenderTargetsMixin` unchanged).

| Scaling | FSR3 dispatch | Pack TAA | Far terrain drag | Water flicker |
|---|---|---|---|---|
| 1.7× | on | on | present | present |
| 1.7× | **off** | on | present | present |
| 1.7× | **off** | **off** | present | present |
| 1.0× (FSR3 setting off) | off | on | absent | absent |

Conclusion: the reported drag/flicker is caused by rendering the scene at
1.7× and presenting it without a reconstruction that consumes the scaled
scene. FSR3 dispatch itself and pack TAA are not the cause. Turning FSR3 off
earlier also restored scale 1.0, which is why that comparison looked clean.

The current post-final FSR3 path receives the **already native final image**,
downsamples it to 1694×932, and upsamples again. It cannot recover the scaled
scene detail and does not remove the artifact.

## Required next architecture

A pre-final boundary is required, not a larger RCAS or a different filter:

1. final effects write to a scene-sized target, with scene `viewWidth/height`;
2. FSR3 consumes that scaled final color and the managed scene depth;
3. FSR3 writes native once; HUD draws native afterward.

That removes the double resample. It still needs a real temporal contract
(jitter, camera/scene motion, history) before FSR3 can suppress low-resolution
temporal artifacts; the current zero-motion, history-reset mode is spatial
only.

## Current state

- FSR3 dispatch is enabled normally; `debug.mgfsr3.bypass=0`.
- Original pack active; user modified TAA during isolation.
- The 1.7× scaled-scene artifact remains a known limitation.
- No Iris source or original shader archive was modified.
