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
