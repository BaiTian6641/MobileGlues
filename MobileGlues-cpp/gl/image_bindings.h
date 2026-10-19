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

// Converts the value passed to glUniform1i for an image uniform to the
// canonical unit, records the application's unit for bind remapping, and
// returns true when the location is an image uniform.
bool mg_image_uniform_value(GLint location, GLint value, GLint* out_value);

// Same as mg_image_uniform_value for an explicitly selected program.
// glProgramUniform* must never resolve locations against the current program.
// Array elements registered by mg_reflect_image_uniforms are looked up by
// their own location, so a 1iv write can be remapped element by element.
bool mg_image_uniform_value_for_program(GLuint program, GLint location, GLint value, GLint* out_value);

bool mg_is_image_uniform_type(GLenum type);

#endif // MOBILEGLUES_IMAGE_BINDINGS_H
