# FSR3 depth input, dispatch, and scaled-scene findings

Date: 2026-10-20
Device: HP Googlebook 14 `C1L63708PZ`, X1E80100, Adreno X1-85,
Android 17 / API 37, ANGLE Vulkan 1.4.295, ES 3.2.
Pack: original `AuroraLumen-Mobile-v2.zip` (`6e8e1942…`), profile Custom (+4).

## Post-final bridge was not dispatching

Iris `FinalPassRenderer.colorHolder` has only a color attachment. The bridge's
`nativeSubmit` required a depth attachment on the current final FBO, so it
returned before dispatch. The visible scaled scene was Iris scaling only.

Fix: `NativeBridge.setSceneDepth(Fsr3Bridge.sceneDepthTextureId())` supplies
the managed scene depth; `nativeSubmit` uses the bridge texture directly. The
bridge now logs `FSR3 submit success`, and the core logs a 1920×1057 scene /
2880×1585 output for the Quality preset.

## Actual bypass and history results

`debug.mgfsr3.bypass=1` was compiled and verified (`FSR3 bridge: dispatch
bypassed by debug.mgfsr3.bypass`); an earlier attempt only added the include
and did not gate dispatch, so its result is discarded.

| Configuration | Result |
|---|---|
| 1.5×/1.7× scene, FSR3 post-final dispatch | motion-dependent terrain drag, water flicker; lower FPS |
| Same scale, dispatch bypassed | visibly better; small FPS increase |
| Same scale, bypassed, pack TAA off | terrain/water artifact persists |
| Dispatch on, `debug.mgfsr3.history=1` | FPS drops; ghosting scales with movement speed |

The post-final FSR3 path consumes an already-native final image, downsamples
it to the scene extent, and resamples back. It adds cost and artifacts rather
than reconstruction. History without jitter is not a fix; Iris exposes
model-view/projection/position but no TAA jitter state.

## Pre-final boundary attempt

A pre-final prototype redirected Iris' final color holder to a scene-sized
GL_RGBA8 target, ran the final shader at scene extent, then submitted FSR3 and
restored the native main target before HUD.

It reached the redirect (`prefinal redirect 1920x1057 color=123`) but native
submit returned `-1` and the world rendered flat blue. The prototype was
reverted in source; the stable install remains the post-final boundary with
dispatch bypassed for usable quality.

## Current state

- Installed APK: `MobileGlues-bypass2.apk` (`7e8a85a3…`), core `c741b74d…`.
- Installed bridge: `92aa55cf…` (post-final, depth input, dispatch switch).
- `debug.mgfsr3.bypass=1`, `debug.mgfsr3.history=0`.
- FSR3 quality setting controls scene scale independently; bypass leaves the
  scaled scene and native final path.
- No Iris source or original shader archive was modified.

The pre-final boundary needs a real color-target handoff and native submit
debugging before it can be enabled; the current post-final dispatch should
remain bypassed.
