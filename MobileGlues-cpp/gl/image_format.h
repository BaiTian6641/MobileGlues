// MobileGlues - gl/image_format.h
// SPDX-License-Identifier: LGPL-2.1-only
#ifndef MOBILEGLUES_IMAGE_FORMAT_H
#define MOBILEGLUES_IMAGE_FORMAT_H

#include <GL/gl.h>
#include <string_view>

// ESSL 3.20 section 4.4.7 and ES 3.2 table 8.27. The first thirteen
// entries are native image formats; subsequent entries retain the existing
// compatibility widenings. A legal backing alone does not prove equivalent
// store conversion, missing-channel values, sampling, or framebuffer behavior.
struct MgImageFormat {
    GLenum logical;
    const char* desktop_qualifier;
    GLenum backing;
    const char* essl_qualifier;
    unsigned logical_bits;
};

inline constexpr MgImageFormat mg_image_formats[] = {
    {GL_RGBA32F, "rgba32f", GL_RGBA32F, "rgba32f", 128},
    {GL_RGBA16F, "rgba16f", GL_RGBA16F, "rgba16f", 64},
    {GL_R32F, "r32f", GL_R32F, "r32f", 32},
    {GL_RGBA32UI, "rgba32ui", GL_RGBA32UI, "rgba32ui", 128},
    {GL_RGBA16UI, "rgba16ui", GL_RGBA16UI, "rgba16ui", 64},
    {GL_RGBA8UI, "rgba8ui", GL_RGBA8UI, "rgba8ui", 32},
    {GL_R32UI, "r32ui", GL_R32UI, "r32ui", 32},
    {GL_RGBA32I, "rgba32i", GL_RGBA32I, "rgba32i", 128},
    {GL_RGBA16I, "rgba16i", GL_RGBA16I, "rgba16i", 64},
    {GL_RGBA8I, "rgba8i", GL_RGBA8I, "rgba8i", 32},
    {GL_R32I, "r32i", GL_R32I, "r32i", 32},
    {GL_RGBA8, "rgba8", GL_RGBA8, "rgba8", 32},
    {GL_RGBA8_SNORM, "rgba8_snorm", GL_RGBA8_SNORM, "rgba8_snorm", 32},
    {GL_R8UI, "r8ui", GL_R32UI, "r32ui", 8},
    {GL_R16UI, "r16ui", GL_R32UI, "r32ui", 16},
    {GL_RG32UI, "rg32ui", GL_RGBA32UI, "rgba32ui", 64},
    {GL_RG16F, "rg16f", GL_RGBA16F, "rgba16f", 32},
    {GL_RG32F, "rg32f", GL_RGBA32F, "rgba32f", 64},
    {GL_RGBA16, "rgba16", GL_RGBA32F, "rgba32f", 64},
    {GL_RG16, "rg16", GL_RGBA32F, "rgba32f", 32},
    // Do not enable these by merely substituting r32f/r32i: that loses
    // normalized clamp/quantization, half rounding, or narrow integer range.
    {GL_R8, "r8", GL_NONE, nullptr, 8},
    {GL_R16, "r16", GL_NONE, nullptr, 16},
    {GL_R16F, "r16f", GL_NONE, nullptr, 16},
    {GL_R8I, "r8i", GL_NONE, nullptr, 8},
    {GL_R16I, "r16i", GL_NONE, nullptr, 16},
};

inline const MgImageFormat* mg_find_image_format(GLenum format) {
    for (const auto& entry : mg_image_formats) {
        if (entry.logical == format) return &entry;
    }
    return nullptr;
}

inline const MgImageFormat* mg_find_image_format(std::string_view qualifier) {
    for (const auto& entry : mg_image_formats) {
        if (qualifier == entry.desktop_qualifier) return &entry;
    }
    return nullptr;
}

inline bool mg_is_native_image_format(GLenum format) {
    const auto* entry = mg_find_image_format(format);
    return entry != nullptr && entry->backing == format;
}

// Desktop GL 4.6 table 8.26 accepts these enums even for a texture-zero unbind.
// Being a valid API argument does not imply that we implement its image storage.
inline bool mg_is_desktop_image_format(GLenum format) {
    if (mg_find_image_format(format) != nullptr) return true;
    switch (format) {
    case GL_R11F_G11F_B10F: case GL_RGB10_A2UI:
    case GL_RG16UI: case GL_RG8UI: case GL_RG32I: case GL_RG16I: case GL_RG8I:
    case GL_RGB10_A2: case GL_RG8:
    case GL_RGBA16_SNORM: case GL_RG16_SNORM: case GL_RG8_SNORM:
    case GL_R16_SNORM: case GL_R8_SNORM:
        return true;
    default:
        return false;
    }
}

inline unsigned mg_image_format_bits(GLenum format) {
    if (const auto* entry = mg_find_image_format(format)) return entry->logical_bits;
    // Non-image storage formats can still participate in a legal by-size view.
    switch (format) {
    case GL_RG8: case GL_RG8I: case GL_RG8UI: case GL_RG8_SNORM: return 16;
    case GL_RG16I: case GL_RG16UI: case GL_RG16_SNORM: return 32;
    case GL_RG32I: case GL_RGBA16_SNORM: return 64;
    case GL_R8_SNORM: return 8;
    case GL_R16_SNORM: return 16;
    case GL_RGB10_A2: case GL_RGB10_A2UI: case GL_R11F_G11F_B10F:
    case GL_RGB9_E5: case GL_SRGB8_ALPHA8: return 32;
    default: return 0;
    }
}

// Allocation policy for image-compatible null/immutable storage. Unmapped
// formats remain ordinary texture formats and cannot acquire a guessed image
// representation. Data-bearing allocations must not use this substitution.
inline GLenum mg_lower_image_storage_format(GLenum format) {
    const auto* entry = mg_find_image_format(format);
    return entry != nullptr && entry->backing != GL_NONE ? entry->backing : format;
}

#endif
