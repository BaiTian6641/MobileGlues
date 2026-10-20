# FSR3 Frame Generation on Googlebook/X1E — experimental rollout plan

Status: **design only. Not implemented and not supported by the current
MobileGlues/ANGLE path.** This document is a plan, not a capability claim.

## Constraint

The current product path is a GLES/EGL wrapper at the Minecraft/FCL boundary.
FSR3 Frame Generation is not a GLSL post-process that can be inserted into the
existing final-image bridge. It requires:

- ownership of the native swapchain/present path;
- per-frame scene color/depth/motion/transparency/reactive inputs;
- optical-flow or equivalent frame interpolation;
- HUD/UI separation and a policy for menu, hand and particle ghosting;
- latency and frame-pacing control;
- async compute/synchronization and present timing on the Android compositor.

ANGLE exposes no FSR3 frame-generation extension and does not expose its
Vulkan device/swapchain for external semaphore signalling. A GLES-only
MobileGlues build therefore cannot honestly provide FSR3 FG by adding a shader
or a swap hook.

## Rollout gates

### P0 — Feasibility and licensing

- Pin the FSR3 FG SDK/version separately from the upscaler; review its license,
  redistribution terms and Android/Vulkan requirements.
- Probe the X1E tuple: Vulkan device/queue families, swapchain/present timing,
  external memory/semaphore capability, 120 Hz display path, and whether ANGLE
  Vulkan shares a device that can be extended.
- Kill criterion: no owned present path or no legal redistribution path.

### P1 — Vulkan presentation ownership

- Establish either a native Vulkan renderer or a documented ANGLE/Vulkan
  interop boundary that owns scene and UI composition.
- Separate scene color/depth/motion from HUD/UI; define a stable frame token,
  resize and return-to-title behavior.
- Keep the current GLES MobileGlues renderer as a fallback; no silent path
  switching.

### P2 — Experimental frame generation

- Integrate FSR3 FG behind an explicit experimental switch, default off.
- Provide interpolation inputs: color, depth, motion, reactive/transparency,
  UI mask, camera/projection generation.
- Present generated frames with measured frame pacing and latency; do not
  count presented frames as simulation FPS.
- HUD/menu policy: native UI, reprojected UI or generation disabled; each must
  be qualified separately. No hidden hand duplication or cleared lighting.
- Fallback: switch back to the ungenerated native path on any late failure and
  invalidate the frame/resource graph coherently.

### P3 — Qualification

- X1E at 2880×1800/1585, 120 Hz display; test 30/60/120 paced targets.
- Empty/opaque/translucent hand, menus, inventory, resize, shader reload,
  world exit/re-enter, thermal soak, suspend/resume.
- Artifact checklist: UI ghosting, hand/shadow trails, star/sky warping,
  disocclusion noise, reflection/WSR interaction.
- Report frame intervals and input-to-photons latency, not only presented FPS.
- Licensing and redistribution record.

## CPU-assisted motion and pacing

CPU work is allowed, but it does not change the official-API constraint above
and it must not be described as FSR3 Frame Generation unless the FSR3 FG
component actually owns interpolation and presentation.

Useful CPU responsibilities, in order of expected value:

1. **Frame pacing and deadline control.** Decide whether the next present has a
   usable interpolation result; drop a generated frame rather than blocking the
   render thread. Keep a queue depth of one and reset on resize, shader reload,
   teleport, menu entry, or late readback.
2. **Low-resolution motion seeds and confidence.** GPU-downsample depth/color to
   a bounded working extent, read it back through a ring of pixel-pack buffers,
   and let a worker compute camera reprojection, disocclusion confidence, and a
   coarse optical-flow seed. The GPU interpolation pass consumes the upload; the
   CPU never warps 2880x1585 pixels per output frame.
3. **UI/HUD exclusion masks.** Derive conservative rectangles or a coverage mask
   from the known HUD/menu layers so generated frames do not smear text, the
   hotbar, inventory, or hand overlays. Native HUD composition remains the
   fallback.
4. **Validation and telemetry.** Compare CPU reprojection against the existing
   camera-motion shader, timestamp readback/publish/present, and expose missed
   deadlines. These measurements decide whether CPU assistance is worthwhile at
   all on X1E.

The worker must be optional and bounded: one worker, one in-flight frame,
asynchronous readback, and no render-thread `glFinish`/buffer map wait. A
deadline miss presents the current native frame. CPU motion vectors feed the
interpolation/warp stage; they do not make an official FSR3 FG swapchain exist.

### CPU-assisted gate

- Host: deterministic reprojection/confidence fixtures, same camera/depth input
  as the GPU path, with error bounds and disocclusion masks.
- X1E: readback latency, worker CPU time, render-thread stall time, and package
  power measured at the intended render and output extents.
- Device: no generated frame on first frame/reset/menu; no HUD ghosting; no
  hand/shadow duplication; generated-frame rejection works under load.
- Labeling: if the FSR3 FG present path is not present, call this an
  experimental CPU-assisted interpolation path, not FSR3 FG.

## Explicit non-goals

- No claim that increasing the FSR3 spatial preset to Quality/Balanced/
  Performance/Ultra provides frame generation.
- No FSR3 FG insertion inside the current FSR3 final-image bridge.
- No claim that a CPU reprojection/optical-flow seed is official FSR3 Frame
  Generation; official FG still requires its present/interpolation chain.
- No use on unsupported Vulkan/ANGLE profiles; NativeAA/native remains the
  fallback.
- No public distribution of a modified shaderpack or FSR3 FG binary before
  license review.

## Relationship to the current work

The current scaled-preset work fixes scene/hand color-depth ownership, clears
the bridge-owned scene depth, and scopes final-program viewport uniforms to
display resolution. Those are prerequisites for any later frame-generation
boundary, but they do not implement it. CPU motion/pacing can be prototyped
independently, but it is not a substitute for presentation ownership.
