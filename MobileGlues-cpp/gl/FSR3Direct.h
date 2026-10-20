// MobileGlues - gl/FSR3Direct.h
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header
#pragma once

#include <cstdint>

// Experimental native presentation path. It is called immediately before the
// real EGL swap when the MobileGlues config selects FSR3 Experimental. It has
// no engine adapter: the final display image is the color input and depth/motion
// are synthetic, so this is not a supported temporal-quality path.
void ApplyFSR3Present();

// Minecraft/Iris scene adapter helpers. They are no-ops unless the experimental
// FSR3 setting is enabled and the current draw target is smaller than the EGL
// surface (i.e. Iris RenderTargets was created at the selected scene extent).
bool FSR3AdjustViewport(int& x, int& y, int& width, int& height);
bool FSR3GetSurfaceSize(int& width, int& height);
void FSR3NativeViewport();

// True when the optional Minecraft/Iris bridge has presented in this frame.
// The swap-path diagnostic present must then stay out of the way: the bridge
// already ran before HUD composition, and running the direct path would process
// (and blur) the HUD as well. The flag is consumed by the next swap present.
void FSR3MarkBridgePresent();
bool FSR3BridgePresentedThisFrame();

// Drop the per-context resources when a context goes away. Called only after
// the last current reference has been released; if no context is current the
// GL objects are already gone and only the bookkeeping is erased.
void mg_fsr3_forget_context(unsigned long long ctx_id);
