# FSR3 history and motion experiment

Status: **design only. History stays off in the installed stable profile.**

## Why the stable profile is history-off

The bridge currently processes the post-final image. The pack supplies no
engine velocity buffer and no global TAA jitter. Camera-only depth
reprojection is valid for static world geometry but not for the hand, sky,
particles, entities or transparent effects. The confirmed overlay was a
scene-color/scene-depth ownership bug; it does not establish temporal
correctness.

## Current pack finding

AuroraLumen `MOBILE_WSR` has `DETAIL_QUALITY=2`; with `TAA_DEFINE=-1` this
auto-defines `TAA`, and `composite6` runs the pack TAA. PBR/WSR reflection
history is also enabled. These are temporal owners independent of FSR3. Disable
pack TAA (`TAA_DEFINE=0`) before attributing ghosting to FSR3 or enabling FSR3
history.

## Required inputs before enabling

1. **Scene jitter owner.** Apply the exact projection jitter before drawing
   and pass it to FSR3; UI/shadow projections stay unjittered. Do not reuse
   per-vertex jitter.
2. **Motion contract.** Camera reprojection for static world plus explicit
   zero/reactive handling for sky, hand, particles and unknown geometry.
3. **Reactive mask.** Mark hand/sky/disocclusion pixels so history is rejected
   instead of producing translucent copies. A mask is not a substitute for
   missing motion.
4. **Depth domain.** Use the managed scene depth, its active rectangle and
   documented depth convention. No native/main-depth rectangle mixing.
5. **Final boundary.** FSR3 consumes the pre-UI image and the final/UI pass
   remains native; requested/effective temporal mode and reset reason are
   recorded.

## Experiment ladder

### T0 — backend and mask unit checks

- Synthetic motion/reactive patterns: sky zero, hand near/far, camera pan.
- Verify reset/history parity and no stale history after resize/preset change.

### T1 — camera-only history, no jitter

- Enable history only by explicit experimental switch.
- Reactive mask: near-depth/hand and far/sky pixels reject history.
- Test camera pan, hand swing, transparent held item, menus.
- Gate: no hand copy, no sky warp, no unbounded ghost. If it fails, do not
  proceed.

### T2 — global jitter

- Add a single scene projection jitter owner and pass the exact offset.
- Keep HUD/shadow projections out.
- Compare static detail, flicker, disocclusion and latency.

### T3 — motion coverage

- Add previous transforms for independently moving geometry only when the
  producer exists. Do not advertise entity/particle motion from camera-only
  reprojection.

## Enablement and fallback

- Default: history off, motion zero, as installed.
- Experimental: a process-restart-safe opt-in, not only a cached environment
  lookup. Record requested/effective mode and reset reason.
- On invariant failure: reset and present the history-off spatial result; do
  not present mixed resources.
- Never call this temporal FSR3 qualification until T1/T2/T3 gates pass.

## Performance measurement

Run the trace-disabled release candidate with the same shaders, preset, scene
and thermal state. Report frame intervals and variance, not an FPS screenshot.
Separate temporal-resolve cost from scene-resolution savings.
