// MobileGlues - gl/glsl/compat_image.h
// SPDX-License-Identifier: LGPL-2.1-only
#ifndef MOBILEGLUES_GLSL_COMPAT_IMAGE_H
#define MOBILEGLUES_GLSL_COMPAT_IMAGE_H

#include <string>

// Lowers desktop unorm16 storage-image qualifiers (layout(rgba16),
// layout(rg16)) for ESSL 3.10.
//
// The same shader must be able to address either an RGBA32F (null-allocated
// custom image / colortex) or an RGBA16F (Bloom alt output) backing. A fixed
// format qualifier cannot do both, so every image that is only written becomes
// formatless plus `writeonly`; the image-unit format then comes from the
// texture's actual storage at glBindImageTexture time (see
// mg_image_unit_format). The format token is removed from the layout while
// every other layout item, notably `binding`, is preserved.
//
// Reading an image keeps a format qualifier: ES 3.10 forbids formatless images
// with load/atomic access. Such declarations fall back to rgba32f/rg32f, which
// matches the RGBA32F/RG32F backing that null-allocated unorm16 images get.
// That is a limitation: a read/write rgba16 image over an RGBA16F backing
// cannot be represented by the strict ES qualifier set.
//
// Whole-token matching is required: rgba16f/rgba16ui must not become
// rgba32ff/rgba32fui.
std::string mg_lower_legacy_image_formats(const std::string& glsl);

#endif
