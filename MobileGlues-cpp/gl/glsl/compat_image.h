// MobileGlues - gl/glsl/compat_image.h
// SPDX-License-Identifier: LGPL-2.1-only
#ifndef MOBILEGLUES_GLSL_COMPAT_IMAGE_H
#define MOBILEGLUES_GLSL_COMPAT_IMAGE_H

#include <string>

// Rewrites explicit desktop image format qualifiers to the shared legal ESSL
// backing table, preserving binding/access qualifiers and comments. Unmapped
// formats produce a named #error. The existing widening paths are compatibility
// deviations, not a claim of desktop-equivalent store conversion. In particular
// unorm16 clamp/quantization and narrow integer overflow are not emulated.
// The historical colorimg8 -> rgba16f exception remains until render-target
// metadata can identify the actual backing independently of the image name.
std::string mg_lower_legacy_image_formats(const std::string& glsl);

#endif
