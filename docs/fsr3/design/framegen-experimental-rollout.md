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

## Explicit non-goals

- No claim that increasing the FSR3 spatial preset to Quality/Balanced/
  Performance/Ultra provides frame generation.
- No FSR3 FG insertion inside the current FSR3 final-image bridge.
- No use on unsupported Vulkan/ANGLE profiles; NativeAA/native remains the
  fallback.
- No public distribution of a modified shaderpack or FSR3 FG binary before
  license review.

## Relationship to the current work

The current scaled-preset work fixes scene/hand color-depth ownership, clears
the bridge-owned scene depth, and scopes final-program viewport uniforms to
display resolution. Those are prerequisites for any later frame-generation
boundary, but they do not implement it.
