# X1E native GUI/hand overlay fix

Status: **PASS for native GUI and observed hand alignment.** FSR3 effect setting
was not revalidated from the screenshot/log; do not use this as temporal-quality
evidence.

## Change

The core now restores the native viewport when a native-sized draw target is
bound. Previously `FSR3AdjustViewport` returned early whenever the viewport was
already scaled, so Minecraft's main/GUI FBO retained the 1694x932 scene viewport
and placed the pause menu in the lower-left. FBO 0 also resets to the native
extent. The experimental direct post-HUD path is disabled once the bridge is
present.

## Observed

- Pause menu centered at native window size.
- HUD/hotbar native.
- First-person hand aligned with the displayed world in the captured frame.
- Screenshot: `/home/polar/MobileGlues-fixtures/2026-10-20-fsr3-ui-hand-aligned.png`.
- No obvious overlay crop in the captured pause/world frame.

## Limits

- The viewport-uniform JAR (`ViewportUniformsMixin`, 14,129 bytes) was **not**
  installed for this capture; it remains a separate test build.
- Depth ownership/screen uniforms and pre-final reconstruction boundary are
  still open. Hand alignment is visual at this pack/view, not a depth-coordinate
  proof.
- Performance/temporal quality is not measured here.
