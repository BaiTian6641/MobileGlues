# Sildur's Vibrant S0 device evidence

Date: 2026-10-20
Device: C1L63708PZ, X1E80100 / Adreno X1-85, ANGLE Vulkan 1.4.295, GLES 3.2.
Pack: `Sildurs-Vibrant-2.02-Extreme.zip`
SHA-256 `824504bc937f9570277c0a809f5059ae4a5fe70c413cf4459b6ccff4dfed7ebe`.
Installed core/bridge: `c741b74d…` / `92aa55cf…`; FSR3 bypassed.
Iris profile: Custom (+0 changes).

## Result

**S0 PASS (provisional S1).** Iris logged
`Using shaderpack: Sildurs-Vibrant-2.02-Extreme.zip`; a world frame
rendered with the pack active and F3 showed 140 FPS, GPU 100%, no fallback or
flat-color frame. No shader compile/link errors were present in the captured
log.

This does not close S1/S2: the full fixed route, named-effect matrix, resize,
reload, and lifecycle gates remain NOT RUN.

Evidence:

- screenshot: `/home/polar/MobileGlues-fixtures/Sildurs/evidence-2026-10-20-vibrant-s0.png`
  SHA-256 `ade686676d6204310ffbda0684ba8f128ee4052ddb7e8e19d1a92df02ca13dc4`
  (correct after capture; see fixture manifest)
- log: `/home/polar/MobileGlues-fixtures/Sildurs/evidence-2026-10-20-vibrant-s0.log`

The screenshot is evidence only and is not committed; Sildur's is
All-Rights-Reserved.
