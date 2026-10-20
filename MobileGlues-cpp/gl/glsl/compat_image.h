// MobileGlues - gl/glsl/compat_image.h
// SPDX-License-Identifier: LGPL-2.1-only
#ifndef MOBILEGLUES_GLSL_COMPAT_IMAGE_H
#define MOBILEGLUES_GLSL_COMPAT_IMAGE_H

#include <string>

// Lower desktop-only unorm16 storage-image qualifiers (layout(rgba16),
// layout(rg16)) to ES 3.2 legal rgba32f/rg32f. Whole-qualifier matching is
// required: rgba16f/rgba16ui must not become rgba32ff.
std::string mg_lower_legacy_image_formats(const std::string& glsl);

#endif
