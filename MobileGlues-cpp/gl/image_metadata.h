// MobileGlues - gl/image_metadata.h
// SPDX-License-Identifier: LGPL-2.1-only
#ifndef MOBILEGLUES_IMAGE_METADATA_H
#define MOBILEGLUES_IMAGE_METADATA_H

#include <GL/gl.h>
#include <string>
#include <unordered_map>

// Desktop GLSL allows formatless image uniforms; ESSL 3.10 does not. The
// desktop image's real format is owned by the loader (Iris), which normally
// supplies it only through glBindImageTexture after linking -- too late for the
// ESSL compiler. This bridge lets the loader register the actual format for the
// source it is about to submit:
//
//   mg_image_metadata_begin();
//   mg_image_metadata_add("voxel_img", GL_R16UI);
//   glShaderSource(shader, ...);       // consumes the pending map
//
// The source rewrite and backing-format choice happen inside glShaderSource.
// No format is guessed from a variable name.
#ifdef __cplusplus
extern "C" {
#endif
__attribute__((visibility("default"))) void mg_image_metadata_begin();
__attribute__((visibility("default"))) void mg_image_metadata_add(const char* name, GLenum format);
__attribute__((visibility("default"))) void mg_image_metadata_end();
#ifdef __cplusplus
}

// Consumes the pending registration for one shader source. The returned map is
// empty when the loader supplied nothing.
std::unordered_map<std::string, GLenum> mg_image_metadata_take();

// The ESSL qualifier and legal image backing for the logical format. GL_NONE /
// nullptr means no implemented representation. Existing widening entries do
// not imply equivalent numeric store conversion; see gl/image_format.h.
GLenum mg_image_backing_format(GLenum logical_format);
const char* mg_image_qualifier(GLenum logical_format);

// Resolve a legal ES image-unit format against the actual texture storage.
// Returns GL_NONE if the representation is unmapped or size-incompatible.
// texture_internal_format == 0 requests only the logical mapping.
GLenum mg_image_unit_format(GLenum logical_format, GLenum texture_internal_format);

// A zero texture has no storage to convert. Accept desktop-valid format enums
// while supplying a legal, unused GLES format. Invalid enums return GL_NONE.
GLenum mg_image_unbind_format(GLenum logical_format);

// Rewrites formatless image declarations for which metadata exists. The
// function is deliberately a no-op for declarations without metadata: an
// unknown image format is a loader-contract failure, not a guess.
void mg_normalize_image_qualifiers(std::string& source, const std::unordered_map<std::string, GLenum>& formats);

#endif // __cplusplus
#endif // MOBILEGLUES_IMAGE_METADATA_H
