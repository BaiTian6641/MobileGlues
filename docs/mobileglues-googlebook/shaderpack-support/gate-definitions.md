# Shaderpack support gate definitions

## Evidence format

Every gate records: core hash, bridge hash, pack SHA-256, tuple, profile,
options, game log, screenshot/trace, and exact pass/fail reason.

## S0 - Import

- Iris lists the ZIP and selects it.
- No launcher crash, no "fallback shader", no unknown pack format.
- Compile/link errors are surfaced truthfully.
- F3 shows the pack name and profile.

## S1 - Pipeline

- World, sky, terrain, entities, translucent water, hand, and HUD render.
- No GL error storm; no black screen or flat-color frame.
- No FSR/upscaler dependence.
- One continuous 60-second route without context loss.

## S2 - Named effects

Use a fixed scene/time/weather/camera route. For each named effect:

- present and visibly distinct from disabled;
- no duplicate hand/shadow/light overlay;
- correct world-locking;
- no unexplained sky/terrain color shift;
- desktop/OptiFine/Iris reference capture where available.

Effect rows are independent; a failure in one effect does not promote or
demote another without evidence.

## S3 - Product

- Resize, pack reload A->B->A, world exit/re-enter, suspend/resume.
- Sustained 20-minute run; F3 frame time, p95, power and thermal state.
- Only the measured profile is claimed.
- No effect disabled to obtain the FPS result.

## Status vocabulary

`NOT RUN`, `BLOCKED`, `FAIL`, `PARTIAL`, `PASS`, `PRIVATE EVIDENCE ONLY`.
Do not use "works", "supported", or "optimized" outside a matching gate.
