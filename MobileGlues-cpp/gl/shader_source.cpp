// MobileGlues - gl/shader_source.cpp
// SPDX-License-Identifier: LGPL-2.1-only
#include "shader_source.h"

#include <cstring>
#include <limits>

bool mg_collect_shader_source(GLsizei count, const GLchar* const* strings, const GLint* lengths,
                              std::string* out, GLenum* error) {
    if (error) *error = GL_NO_ERROR;
    if (count < 0 || (count > 0 && strings == nullptr)) {
        if (error) *error = GL_INVALID_VALUE;
        return false;
    }

    std::string collected;
    size_t total = 0;
    for (GLsizei i = 0; i < count; ++i) {
        const GLchar* string = strings[i];
        if (string == nullptr) {
            if (error) *error = GL_INVALID_VALUE;
            return false;
        }
        if (lengths && lengths[i] >= 0) {
            total += static_cast<size_t>(lengths[i]);
        } else {
            total += strlen(string);
        }
    }
    collected.reserve(total);

    for (GLsizei i = 0; i < count; ++i) {
        const GLchar* string = strings[i];
        if (lengths && lengths[i] >= 0) {
            collected.append(string, static_cast<size_t>(lengths[i]));
        } else {
            collected.append(string);
        }
    }

    *out = std::move(collected);
    return true;
}
