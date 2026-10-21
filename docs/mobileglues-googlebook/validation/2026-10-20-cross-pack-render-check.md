# Cross-pack render check (X1E)

Device: HP Googlebook 14 `C1L63708PZ`, Adreno X1-85, ANGLE/Vulkan 1.4.295,
MobileGlues stable bridge, release core, 2880x1585.

| Pack | Result | Evidence |
|---|---|---|
| Complementary Reimagined 5.9.3 | Renders correctly | Blue sky, volumetric clouds, lit desert terrain; LOW ~39 FPS |
| BSL v10.1.8 | Renders, cloud-shadow caveat | Sky/terrain lighting present; HIGH ~39 FPS; cloud shadows appear oversized/blocky and sky clouds look flat |
| iterationRP Alpha 0.8.31 | Deferred | Final composite runs; flat sky/terrain remains pack-specific; no further probing |

The earlier bridge override that forced the final pass onto colortex0 main was
removed (commit `038852f`). Iris now chooses the final target itself. The
iterationRP diagnostic sampler/upload mixins and native probe were reverted.

Conclusion: the iterationRP failure is not a general MobileGlues sky/final-pass
failure. Complementary is the current clean reference. BSL needs a focused
cloud-shadow comparison before any wrapper change.

## Shadow comparison

Same scene/time, per-pack defaults:

- Complementary Reimagined 5.9.3 LOW: smooth, coherent soft shadowing on snow.
- BSL v10.1.8 HIGH: hard, blocky shadow patches; BSL's HIGH profile uses a
  2048 shadow map over 256 blocks.

The bridge's shadow/viewport mixins are trace-only, FSR3 submit is inactive,
and the Complementary reference uses the same Iris shadow targets. No
wrapper-wide shadow-pass failure is demonstrated. BSL's hard shadow look is
its pack-side shadow algorithm/settings, not a reason to change MobileGlues.

## BSL shadow follow-up

Deferred at user request. BSL's raw shadow map is non-empty and the live
`shadowtex0` state is comparison-enabled, `LEQUAL`, linear-filtered, with a
valid 2048x2048 depth texture. No shader compile error appears. The remaining
symptom is a player-relative/offset shadow; attempts to tune Iris shadow
distance and grid snapping were inconclusive or black.

A diagnostic bridge `glGetTexParameteriv` call crashed with a heap-typed
`IntBuffer`; the same call with a direct buffer is the valid LWJGL contract.
The crash was diagnostic-side, not a MobileGlues getter defect. All probes and
derived BSL packs were removed, and the device is back on Complementary with
the stable bridge/core.
