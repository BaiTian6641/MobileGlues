# FSR3 production backend and ABI - X1E probe

Status: **in-tree backend + public ABI implemented; LDR probe PASS on X1E. Not yet
integrated with Minecraft/Iris, not performance-qualified.**

This is the first production-shaped slice after the S2 dispatch gate. It is
deliberately narrower than the frozen contract: it accepts the LDR path and
backend-owned output, rejects unqualified variants, and has no CPU readback in
the production path.

## Files

| File | Purpose |
| --- | --- |
| `MobileGlues-cpp/upscaling/mg_fsr_abi.h` | Frozen ABI v1 declarations and layout checks |
| `MobileGlues-cpp/upscaling/mg_fsr.cpp` | Session/context validation, capability query, output resource, error reporting |
| `MobileGlues-cpp/upscaling/fsr3/fsr3_backend.h/.cpp` | GLES resources, 8-pass schedule, history, state guard, barriers |
| `MobileGlues-cpp/upscaling/fsr3/fsr3_shaders.h` | Adapted ESSL generated from pinned AMD source (MIT) |
| `tools/fsr3_spike/backend_probe/` | Production-API probe; readback is test-only |

The in-tree backend uses the same pinned AMD shader set and substitutions as the
S2 dispatch harness. It does not include AMD host code, frame generation,
optical flow, or the Vulkan interop path.

## Implemented behavior

- Opens one backend per session/context and allocates render/output/history
  resources at the requested extents.
- Executes PrepareInputs, LumaPyramid, ShadingChangePyramid, ShadingChange,
  PrepareReactivity, LumaInstability, Accumulate and RCAS.
- Applies frame parity to accumulation, internal-upscaled color, luma history
  and current/previous luma, matching the SDK's odd/even selection.
- Clears reconstructed depth, SPD atomic counter, locks, shading change and
  aliasable intermediates; clears history on reset/first frame.
- Computes depth-to-view parameters, motion scale (pixel or UV), jitter
  cancellation, delta time, pre-exposure ratio, camera/FOV and SPD constants.
- Saves/restores program, active texture, all texture/sampler bindings, image
  bindings, indexed UBO bindings, FBO/viewport/scissor and raster state around
  the internal work. A reusable clear FBO avoids per-frame FBO allocation.
- Rejects HDR, application-owned output, non-full active rectangles, tiny
  render extents and unsupported resource namespaces with explicit errors.
  These are honest availability limits until the adapter profiles exist.
- No `glFinish`, no CPU readback, and no device-wide wait in the production
  dispatch. `glMemoryBarrier(GL_ALL_BARRIER_BITS)` follows each pass; the
  eventual backend should narrow this barrier set after validation.

## Target probe

`tools/fsr3_spike/backend_probe` loads the built `libmobileglues.so`, creates a
GLES 3.2 pbuffer through the library's EGL frontend, creates a 160x90 flat
RGBA8 color, R32F depth and RG16F zero motion, then calls:

1. `mg_fsr_query_caps`
2. `mg_fsr_create_session`
3. `mg_fsr_dispatch` frame 1 with reset
4. `mg_fsr_get_output` + FBO readback (test only)
5. frame 2 without reset (history active) and frame 3 with reset
6. `mg_fsr_destroy_session`

Result on `C1L63708PZ`, ANGLE/Adreno X1-85:

```json
{"probe":"fsr3-backend","renderer":"loaded","checks":[
 {"name":"context","status":"PASS"},
 {"name":"caps","status":"PASS"},
 {"name":"dispatch1","status":"PASS"},
 {"name":"output","status":"PASS"},
 {"name":"output_fbo","status":"PASS"},
 {"name":"flat_output","status":"PASS"},
 {"name":"gl_error","status":"PASS"},
 {"name":"dispatch2_history","status":"PASS"},
 {"name":"flat_output2","status":"PASS"},
 {"name":"dispatch3_reset","status":"PASS"}
],"gl_renderer":"Adreno (TM)  X1-85 | ANGLE | Vulkan 1.4.295 | OpenGL ES 3.2"}
```

The flat output must reproduce the input color; the second-frame and reset
checks exercise history/reset state without claiming image-quality equivalence.

### Evidence

| File | SHA-256 |
| --- | --- |
| `docs/fsr3/validation/2026-10-20-x1e-backend-probe.json` | `5c11f1be6f651f5e2f9fd5a7f4c1a062490357576176f1021406cf067fd2262f` |
| `docs/fsr3/validation/2026-10-20-x1e-backend-probe.log` | `b3e1167e79d84e4740d8d4d3124a0afa45180880d098cede4d91ebbfbf33ba09` |

Build identity at the probe:

- `build/android-arm64-release/libmobileglues.so`
  SHA-256 `3ee4c807d9fbe09f00accebc542877a75be2a978090e5ce36a3af70e630c970c`

## Not qualified

- Minecraft/Iris adapter, `composite6` replacement, scene-wide jitter and real
  motion/depth inputs (S4).
- Full-frame real content, moving objects, disocclusion, trails and temporal IQ.
- Multi-frame sequences beyond the three-frame probe; no reference clip.
- GPU timing, CPU submission cost, power/thermal behavior, bandwidth from the
  RGBA16F/RGBA8 substitutions.
- HDR, application-owned output, non-full active rectangles and tiny renders.
- Context loss/recreation; the session currently expects the caller to destroy
  it while its context is current.

The next gate is the adapter vertical slice, then a real camera-motion clip and
matched native/bilinear/FSR1/FSR3 measurements.
