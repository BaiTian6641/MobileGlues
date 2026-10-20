# FSR3 bridge synchronization fixes

Status: **installed and fast-motion spot checked; one residual far-terrain ghost
remains under investigation.**

Installed: APK `b4d8e751…`, core `28ca8677…`, bridge `1b229c26…`.
Settings: Balanced, shaders ON, history absent, motion zero, RCAS 0.4.

## Fixed

1. **Scissor/color-mask leak.** `FramebufferGuard` saved FBOs/viewport but not
   scissor or color mask. Both FSR3 blits inherited the caller's scissor, which
   could leave a one-frame black edge. Commit `b930af8` saves, disables, and
   restores both.
2. **Read-FBO clobbering.** Scene-depth FBO creation and clearing bound
   `GL_FRAMEBUFFER`, changing both read and draw bindings, then restored only
   the draw FBO. Commit `f6b4f68` restores draw and read FBOs separately.

The final trace now reports final scissor disabled, full viewport
`[0,0,2880,1585]`, and fast-pan captures no longer show the black side edge.

## Remaining far-terrain ghost

FSR3 history remains off (`history_env_present=false`, motion zero). The pack
itself enables TAA: `DETAIL_QUALITY=2` auto-defines `TAA` when `TAA_DEFINE=-1`,
and `composite6` runs `/lib/antialiasing/taa.glsl`. `MOBILE_WSR` also enables
PBR/WSR reflection history. The residual far-terrain lag is therefore not
evidence of FSR3 history.

Isolation step: set `TAA_DEFINE=0` in the private pack profile and repeat the
same fast pan. If the lag disappears, pack TAA is the owner. Only after that
should an FSR3 temporal experiment (T1) be attempted, with one temporal owner.
