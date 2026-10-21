// MobileGlues - gl/image_uniform_write.h
// SPDX-License-Identifier: LGPL-2.1-only
#ifndef MOBILEGLUES_IMAGE_UNIFORM_WRITE_H
#define MOBILEGLUES_IMAGE_UNIFORM_WRITE_H

#include "shader_state.h"
#include <algorithm>

// Providers may reflect an array as one record or as individual elements.
// Normalize both shapes before accepting an array write from any element.
inline void mg_finalize_image_uniform_arrays(MgProgramObject& state) {
    std::unordered_map<std::string, GLint> last_locations;
    for (const auto& entry : state.image_uniform_array_remaining) {
        if (entry.second == 0) continue;
        const auto name = state.image_uniform_names.find(entry.first);
        if (name == state.image_uniform_names.end()) continue;
        auto& last = last_locations[name->second];
        last = std::max(last, entry.first);
    }
    for (auto& entry : state.image_uniform_array_remaining) {
        if (entry.second == 0) continue;
        const auto name = state.image_uniform_names.find(entry.first);
        if (name != state.image_uniform_names.end())
            entry.second = last_locations.at(name->second) - entry.first + 1;
    }
}

// ES 3.2 section 7.6 rejects every Uniform* write to an image. Desktop GL
// 4.6 section 7.6.1 permits 1i/1iv, so validate the entire write before updating
// the logical-to-canonical mapping. Excess active-array elements are ignored.
inline bool mg_update_image_uniforms(MgProgramObject& state, GLint location, GLsizei count,
                                     const GLint* values, GLint max_units, GLenum& error) {
    const auto first = state.image_uniform_units.find(location);
    if (first == state.image_uniform_units.end()) return false;
    error = GL_NO_ERROR;
    if (count < 0) {
        error = GL_INVALID_VALUE;
        return true;
    }
    if (count == 0) return true;
    const auto range = state.image_uniform_array_remaining.find(location);
    const GLsizei remaining = range == state.image_uniform_array_remaining.end() ? 0 : range->second;
    if (remaining == 0 && count > 1) {
        error = GL_INVALID_OPERATION;
        return true;
    }
    const GLsizei applied = remaining > 0 ? std::min(count, remaining) : 1;
    if (values == nullptr) {
        error = GL_INVALID_VALUE;
        return true;
    }
    for (GLsizei i = 0; i < applied; ++i) {
        if (values[i] < 0 || values[i] >= max_units) {
            error = GL_INVALID_VALUE;
            return true;
        }
    }
    for (GLsizei i = 0; i < applied; ++i) {
        const auto element = state.image_uniform_units.find(location + i);
        if (element != state.image_uniform_units.end())
            state.image_unit_remap[static_cast<GLuint>(values[i])] = element->second;
    }
    return true;
}

#endif // MOBILEGLUES_IMAGE_UNIFORM_WRITE_H
