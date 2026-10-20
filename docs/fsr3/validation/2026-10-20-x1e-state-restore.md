# X1E state-preservation probe

Status: **PASS for first-use backend state preservation.** Resize/context/off-path
coverage remains incomplete.

- Device: HP Googlebook 14 `C1L63708PZ`, X1E80100 / Adreno X1-85.
- Provider: ANGLE Vulkan 1.4.295 / GLES 3.2.
- Probe: `backend_probe_arm64`, public FSR ABI, initialized through the core EGL frontend.
- Before dispatch the caller bound:
  - a color texture on unit 0;
  - a non-default generic UBO;
  - a non-default draw FBO;
  - viewport `{1,2,17,19}`.
- After `mg_fsr_dispatch` (which initializes all backend resources and runs eight passes):

```text
state_texture PASS
state_ubo PASS
state_fbo PASS
state_viewport PASS
dispatch1 PASS
dispatch2_history PASS
dispatch3_reset PASS
```

This verifies the R6 wrapper-level `SessionStateGuard` on X1E. It does not cover
an actual resize transition, two contexts, or the FSR3-off path; those remain
open.
