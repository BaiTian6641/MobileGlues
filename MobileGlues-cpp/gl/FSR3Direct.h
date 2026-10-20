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

// Drop the per-context resources when a context goes away. Called only after
// the last current reference has been released; if no context is current the
// GL objects are already gone and only the bookkeeping is erased.
void mg_fsr3_forget_context(unsigned long long ctx_id);
