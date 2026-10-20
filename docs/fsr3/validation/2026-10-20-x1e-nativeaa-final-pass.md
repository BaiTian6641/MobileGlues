# FSR3 NativeAA on a final-pass-only pack

Date: 2026-10-20
Device: HP Googlebook 14 `C1L63708PZ`, X1E80100 / Adreno X1-85,
ANGLE Vulkan 1.4.295, Android 17 API 37.
Pack: AuroraLumen private port, final-pass-only.
Installed pair: APK `538455b7…` (core `5733b7bb…`, motion-zero stable core),
bridge JAR `1f759cfe…`.
Setting: MobileGlues FSR3 On, quality **NativeAA**, RCAS sharpness 0.4.

## Observed

- User-operated night-world run: the duplicated translucent hand/light-shadow
  overlay is **gone**.
- The star layer is **world-locked** while rotating the camera; it no longer
  rotates with the viewpoint.
- No crash with the viewport-uniform mixin enabled.

## What fixed each symptom

- **Star layer:** `ViewportUniformsMixin` reports the scaled scene extent to
  world/composite programs. The final pass is explicitly scoped back to the
  native extent, so final/UI do not consume the scaled `viewWidth/viewHeight`.
- **Hand/shadow overlay:** this pack is final-pass-only, so scaling Iris
  `RenderTargets` changes hand/depth ownership without reducing its scene
  geometry. Scaling reproduced the overlay; NativeAA (scale 1.0 / no Iris
  target scaling) removed it.
- **Temporal reprojection:** with history off, motion input is zeroed
  (`uHasPrev=0`). Camera-only vectors are no longer fed for camera-attached
  hands or sky.

## Tradeoff

NativeAA does not reduce scene render resolution on this pack, so it cannot
recover the Balanced-mode frame-time gains. Measured FPS was not captured; the
user reported that disabling scene scaling reduced frame rate. Balanced/Performance
remain faster but reproduced the hand/light-shadow overlay in this pack and are
not the artifact-free default.

The correct performance path for final-pass-only packs is a separate
pre-final boundary that renders hands/sky/light/shadow with owned motion and
target extents. That is not implemented and must not be claimed as FSR3 scene
scaling.

## Limits

- Not a temporal FSR3 or hand-ownership qualification.
- Not validated on a gbuffer/deferred pack with the same bridge.
- No measured performance numbers in this record.
