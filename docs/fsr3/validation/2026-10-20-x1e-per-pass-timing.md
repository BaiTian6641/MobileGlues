# X1E FSR3 per-pass GPU timing

Status: **PASS for per-pass GPU cost measurement; flat synthetic frame, not game performance.**

- Device: HP Googlebook 14 `C1L63708PZ`, X1E80100 / Adreno X1-85.
- Provider: ANGLE Vulkan 1.4.295, GLES 3.2, Adreno driver 512.886.6.
- Extents: render 1920x1057, output 2880x1585 (Quality-style).
- Probe: `backend_probe_arm64 --render 1920x1057 --output 2880x1585`.
- Core: `build/android-arm64-release/libmobileglues.so` at R1/R6/R7/R9/R4-source head.
- Method: env-gated `MG_FSR3_TIMING=1`; `GL_EXT_disjoint_timer_query`
  `glQueryCounterEXT(GL_TIMESTAMP_EXT)` at each pass boundary, one `glFinish`
  before reading results. No per-pass serialization.
- Median per-pass ms over three dispatches:

| Pass | Median ms | Range |
| ---: | ---: | ---: |
| 0 PrepareInputs | 2.671 | 2.277–2.741 |
| 1 LumaPyramid (SPD) | 0.705 | 0.678–0.752 |
| 2 ShadingChangePyramid | 2.943 | 2.698–6.818 |
| 3 ShadingChange | 0.241 | 0.234–0.345 |
| 4 PrepareReactivity | 6.061 | 5.848–6.314 |
| 5 LumaInstability | 0.933 | 0.815–1.172 |
| 6 Accumulate | 4.347 | 3.838–4.965 |
| 7 RCAS | 0.799 | 0.788–0.917 |

- Per-dispatch total by row sums: ~17.3, ~18.3, ~17.3 ms.
- Raw: `2026-10-20-x1e-per-pass-timing.log` / `.json`.

Limits: flat synthetic color/depth/motion, no pack, no scene adapter, no
output sampling; timestamps measure GPU work but not game frame time. Do not
turn this into a benchmark claim. The cost magnitude explains why the
Quality/Performance game spot did not show a clear FPS win.
