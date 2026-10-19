// MobileGlues - gl/shader_source.h
// SPDX-License-Identifier: LGPL-2.1-only
#ifndef MOBILEGLUES_SHADER_SOURCE_H
#define MOBILEGLUES_SHADER_SOURCE_H

#include <GL/gl.h>
#include <string>

// Concatenate the application's GLSL source array exactly as glShaderSource
// specifies. `lengths` may be null; a non-negative entry gives the exact byte
// count, and a negative entry means NUL-terminated. Embedded NUL bytes are
// preserved when the caller supplied an explicit length.
//
// Returns false and sets `error` to GL_INVALID_VALUE for a negative count, a
// null string pointer, or a negative-length entry treated as NUL-terminated
// whose pointer is null. `out` is only modified on success.
bool mg_collect_shader_source(GLsizei count, const GLchar* const* strings, const GLint* lengths,
                              std::string* out, GLenum* error);

#endif // MOBILEGLUES_SHADER_SOURCE_H
