// MobileGlues - gl/FSR1/FSR1.h
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header
#pragma once

#include <cstdlib>
#include <cstring>
#include <vector>

#ifndef __APPLE__
#include <malloc.h>
#endif

#ifdef __ANDROID__
#include <android/log.h>
#endif

#include "../../gles/gles.h"
#include "../../gles/loader.h"
#include "../../includes.h"
#include "../framebuffer.h"
#include "../glsl/glsl_for_es.h"
#include "../log.h"
#include "../mg.h"
#include <GL/gl.h>

namespace FSR1_Context {
    extern GLuint g_renderFBO;
    extern GLuint g_renderTexture;
    extern GLuint g_depthStencilRBO;
    extern GLuint g_quadVAO;
    extern GLuint g_quadVBO;
    extern GLuint g_fsrProgram;
    // Uniform locations of g_fsrProgram, resolved when it is linked and valid for
    // as long as it lives. -1 for a name the linker dropped, which glUniform*
    // ignores.
    extern GLint g_inputTexLoc;
    extern GLint g_passLoc;
    extern GLint g_inputSizeLoc;
    extern GLint g_outputSizeLoc;

    // EASU writes here at target extent; RCAS reads it and writes g_targetTexture.
    // Both are the same size and format so the second pass is a 1:1 sampling.
    extern GLuint g_easuFBO;
    extern GLuint g_easuTexture;

    extern GLuint g_targetFBO;
    extern GLuint g_targetTexture;

    extern GLuint g_currentDrawFBO;
    extern GLint g_viewport[4];
    // Output-first: g_output* is the EGL surface and g_target* is exactly it.
    // g_render* is derived from g_output* with the preset scale.
    extern GLsizei g_outputWidth;
    extern GLsizei g_outputHeight;
    extern GLsizei g_targetWidth;
    extern GLsizei g_targetHeight;
    extern GLsizei g_renderWidth;
    extern GLsizei g_renderHeight;
    // "The render target holds scene content not yet presented." Set by
    // gl/framebuffer.cpp when it redirects a draw bind of framebuffer 0 to
    // g_renderFBO, cleared after a successful upscale and after a resize rebuild
    // (the new texture is unwritten). ApplyFSR does not sample the render texture
    // while it is false; the surface is presented as drawn instead. Per context,
    // like the objects it describes.
    extern bool g_dirty;

    extern bool g_resolutionChanged;
    extern GLsizei g_pendingWidth;
    extern GLsizei g_pendingHeight;
} // namespace FSR1_Context

extern bool fsrInitialized;

// Swap the FSR1 objects when the current context changes.
//
// Every name above is a GL object owned by the context that created it, and
// gl/framebuffer.cpp redirects framebuffer 0 to g_renderFBO -- in a second
// context that name refers to nothing, or to somebody else's object. The values
// are saved and reloaded rather than reached through a pointer because they are
// declared extern and read from several translation units.
void mg_fsr1_bind_context(unsigned long long ctx_id);
void ApplyFSR();
void InitFSRResources();
void CheckResolutionChange(EGLDisplay display, EGLSurface surface);
void OnResize(int width, int height);

extern "C"
{
    GLAPI void glViewport(GLint x, GLint y, GLsizei w, GLsizei h);
}