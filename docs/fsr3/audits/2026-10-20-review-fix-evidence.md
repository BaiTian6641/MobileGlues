# Integration review: fixed findings evidence

Status: R1, R6, R7 and R9 source fixes applied; host/device smoke PASS; full
integration still **NOT QUALIFIED**.

## R1 — unsigned reconstructed-depth sampler

Host reproduction after the fix:

```text
sampler type=0x8dd2 unit=2 texture=24 format=0x8236
SPIRV_Cross_Combinedr_reconstructed_previous_nearest_depth...
dispatch_result=PASS error= passes=8 remaining_gl_error=0x0
```

The unsigned sampler now has its own unit and the R32UI reconstructed-depth
texture; integer textures use NEAREST filtering. Raw:
`2026-10-20-integration-review-evidence/backend_review_after_fix.log`.

## R6 — initialization/resize state

`SessionStateGuard` now wraps backend initialize/resize, restoring program,
active texture/unit 0, sampler, read/draw FBO, viewport, UBO, pixel-unpack
buffer and unpack row/alignment/skip state. X1E state probe:
texture/UBO/FBO/viewport all PASS after first-use initialization and after a
resize dispatch; see `validation/2026-10-20-x1e-state-restore.md`. Two contexts
and off-path remain open.

## R7 — camera continuity

The time-gap/teleport decision is now preserved in `keep_previous` instead of
being overwritten to true in the same capture call. No separate device run yet.

## R9 — damage swaps

`presentSurfaceWithDamage` now treats FSR3 as an upscaling route, so KHR/EXT
damage swaps fall back to the full-swap presenter and run `ApplyFSR3Present`
rather than bypass it. Host build PASS.

## Device probe after backend fixes

`device_probe_after_fix.json`: all ten reset/history/reset checks PASS on X1E
after R1/R6/R7. This is backend API evidence, not full game quality.

## Still open from the review

R2 (final-image reconstruction boundary), R3 (scene depth/screen-unit grid),
R5 (jitter/depth parameters/TAA), R8 (ABI fields/capability validation) and
R10 (steady-state cost) remain open. R4 is fixed in source (double JNI
positions); its coordinated APK+JAR package and device run are pending.
Do not qualify performance from the current path.
