// MobileGlues - gl/image_bindings.h
// SPDX-License-Identifier: LGPL-2.1-only
#ifndef MOBILEGLUES_IMAGE_BINDINGS_H
#define MOBILEGLUES_IMAGE_BINDINGS_H

#include <GL/gl.h>
#include <string>
#include <unordered_map>

// The desktop/image uniforms Iris emits are formatless and rely on glUniform1i
// after linking. ANGLE's Vulkan descriptor setup aliases formatless image units
// on this driver, so the wrapper assigns a stable fallback unit per image name
// and makes both the shader default and the application's bind calls agree.
// Explicit bindings from the pack or the Iris bridge always win.
// Assigns layout(binding = N) to image declarations. Unique explicit bindings
// are preserved; repeated bindings and missing bindings use a fallback map
// scoped to the shader's share group. Existing format qualifiers are
// preserved; no format is guessed. `observed_units` receives name -> unit for
// link-time reflection.
void mg_bind_image_uniforms(std::string& source, unsigned long long share_group,
                            std::unordered_map<std::string, GLuint>* observed_units);

// Reflects the program's active image uniforms and stores location -> unit.
void mg_reflect_image_uniforms(GLuint program);

// Detects different active image names sharing one physical unit and rewrites
// the attached shader sources to distinct units, then relinks the program.
void mg_resolve_duplicate_image_bindings(GLuint program);

// Converts an application image unit to the wrapper's canonical unit for the
// program that is currently in use. Returns the input when no mapping exists.
GLuint mg_remap_image_unit(GLuint unit);

// Handles desktop 1i/1iv image writes using logical image binding state.
// Returns false for ordinary uniforms, which must still reach the ES driver.
// A handled invalid write raises the desktop error without changing mappings.
bool mg_image_uniform_write_for_program(GLuint program, GLint location, GLsizei count, const GLint* values);

bool mg_is_image_uniform_type(GLenum type);

#endif // MOBILEGLUES_IMAGE_BINDINGS_H
