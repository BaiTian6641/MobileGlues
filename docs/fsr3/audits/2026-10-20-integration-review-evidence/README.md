# Review reproduction evidence

This harness links the actual, unmodified FSR3 backend translation unit into a
standalone host executable. It initializes the backend against a real software
GLES provider and inspects its state and sampler assignments. It does not load
the whole MobileGlues wrapper, use the Android bridge, or qualify image quality.

Run from the repository root at `source_reconciled_through` in `manifest.json`.
The backend files are hash-verified identical at the initial and final reviewed
source revisions; the later bridge corrections do not invalidate this trace.

```sh
g++ -O1 -std=c++20 -I MobileGlues-cpp \
  docs/fsr3/audits/2026-10-20-integration-review-evidence/backend_review.cpp \
  MobileGlues-cpp/upscaling/fsr3/fsr3_backend.cpp \
  -lEGL -lGLESv2 -o /tmp/mg-fsr3-backend-review

LIBGL_ALWAYS_SOFTWARE=1 timeout 50s /tmp/mg-fsr3-backend-review
```

Requires a host Mesa EGL/GLES 3.2 implementation, development headers/libraries,
and a C++20 compiler. Shader compilation may take several seconds.

The checked-in run used llvmpipe, Linux 7.0.0-38-generic and Mesa
26.0.0-devel. It is not target-device evidence. Raw output is
`backend_review.log`; source and evidence hashes are in `manifest.json`.
`host-suite.log` is the final rerun of `sh MobileGlues-cpp/tests/run.sh` after
the concurrent source updates. All 11 executables passed. It does not exercise
the complete Java/JNI rendering path.

Interpretation:

- `initialize=PASS` confirms allocation/compilation, while the next line shows
  application program/texture/UBO bindings changed to zero.
- At pass 4, sampler type `0x8dd2` is `GL_UNSIGNED_INT_SAMPLER_2D`.
  Its assigned unit is 0, shared with the motion sampler, and format `0x881a`
  is `GL_RGBA16F`. The reconstructed-depth resource should be `GL_R32UI`.
- `dispatch_result=PASS` reports the backend's return value. It does not
  negate either defect. This harness performs no pixel readback.
- GL object names can differ on a new run. The binding relationships and
  formats are the relevant observations.

The instrumentation runs `glValidateProgram` and queries active uniforms before
dispatch; it leaves texture selection and sampler assignments unchanged. The
provider reported validation success even with the incorrect sampler binding.
