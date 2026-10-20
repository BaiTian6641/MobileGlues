// MobileGlues - gl/FSR1/FSR1.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header
#include "FSR1.h"
#include "../FSR3Direct.h"
#include "FSR1Plan.h"
#include <mutex>
#include <ska/flat_hash_map.hpp>
#include "FSRShaderSource.h"
#include "../../config/settings.h"
#include "../enable.h"

#define DEBUG 0

using namespace FSR1Plan;

// One colour policy for every texture this file owns. RGBA8 matches the common
// EGL surface, is colour-renderable and filterable everywhere GLES 3 is, and --
// unlike the old RGBA8-at-init/RGBA32F-on-recreate pair -- does not change
// precision or bandwidth when the surface is resized. The plan header names the
// same choice for the host tests.
static constexpr GLenum kColorInternalFormat = GL_RGBA8;
static constexpr GLenum kColorFormat = GL_RGBA;
static constexpr GLenum kColorType = GL_UNSIGNED_BYTE;
static_assert(static_cast<int>(ColorFormat::RGBA8) == 0, "the plan's RGBA8 tag is the only policy here");

// Saves the GL state the bodies in this file overwrite and puts it back.
//
// Answered from this layer's own tracking, at no driver cost:
//   - the current program. gl/program.cpp already treats gl_state->current_program
//     as the truth -- it drops a glUseProgram that repeats it -- and program names
//     are not renamed on the way to GLES, so the tracked value is the driver's.
//   - the draw framebuffer. gl/framebuffer.cpp writes gl_state->current_draw_fbo
//     with the name it hands the driver, the redirect of framebuffer 0 to the FSR1
//     render target already resolved, and it is the only file that binds a draw
//     framebuffer through GLES other than this one. It also keeps that field off
//     deleted names, through its own glDeleteFramebuffers -- with one exception,
//     RecreateFSRTargets, which deletes the render FBO behind its back and so
//     republishes the replacement -- to both trackers -- through
//     mg_framebuffer_republish_draw_binding.
//   - the enable state of GL_BLEND, GL_DEPTH_TEST, GL_STENCIL_TEST, GL_CULL_FACE,
//     GL_SCISSOR_TEST and GL_RASTERIZER_DISCARD. gl/enable.cpp owns that table and
//     keeps it per context. This guard changes those bits through the frontend
//     glEnable/glDisable, never through GLES directly, precisely so a nested guard
//     reads the value the outer one established and not the application's older
//     request. The table is left describing the driver at every point.
//
// The program and draw-framebuffer entries above are the exception to nesting:
// they read a shadow that the bodies deliberately do not write (they go straight
// to GLES so the application's tracked value survives). The only nested guard in
// this file is InitFullscreenQuad's, and it asks only for VAO and array buffer,
// which are driver-queried. A future guard nested inside a body that changes the
// program or the draw binding must query the driver instead of trusting those
// two entries, or update the shadow and restore it.
//
// Asked of the driver, because nothing in the tree can answer:
//   - the vertex array. What this layer tracks is the application's name, the
//     mapping to the driver's lives in gl/buffer.cpp and is not exported, and the
//     tracked name outlives glDeleteVertexArrays -- restoring from it could hand
//     GLES a name it never generated.
//   - the active unit and unit 0's GL_TEXTURE_2D binding. gl/texture.h's driver
//     shadow declines to answer while FSR1 is enabled, which is exactly when this
//     runs. The active unit has to come from the driver in any case: these guards
//     nest, the moves below go straight to GLES and so never reach that shadow, and
//     an inner guard reading it would restore the outer guard's unit and leave the
//     body running on a unit it never asked for.
//   - the read framebuffer and the renderbuffer binding, which nothing tracks.
//   - unit 0's sampler binding (GL_SAMPLER_BINDING), which overrides the FSR1
//     textures' filtering, wrap and compare state during the passes.
//   - viewport, scissor box, blend functions and equations, depth function and
//     write mask, cull mode, colour mask and clear colour. No shadow exists for
//     these; they are read once per presented frame, which is the cost of leaving
//     the application's state exactly as it was.
//
// The texture entry is unit 0, not whichever unit happened to be active. Unit 0 is
// the unit ApplyFSR samples from, so it is the binding that has to be preserved;
// saving the active unit's instead left the FSR1 textures on unit 0 once per
// presented frame with nothing anywhere to put the application's texture back.
struct GLStateGuard {
    uint32_t saved;
    GLint prevProgram = 0;
    GLint prevVAO = 0;
    GLint prevArrayBuffer = 0;
    GLint prevActiveTexture = GL_TEXTURE0;
    GLint prevTexture = 0;
    GLint prevSampler = 0;
    GLint prevReadFBO = 0;
    GLint prevDrawFBO = 0;
    GLint prevRenderbuffer = 0;
    GLint prevViewport[4] = {0, 0, 0, 0};
    GLint prevScissorBox[4] = {0, 0, 0, 0};
    GLint prevBlendSrcRGB = GL_ONE;
    GLint prevBlendDstRGB = GL_ZERO;
    GLint prevBlendSrcAlpha = GL_ONE;
    GLint prevBlendDstAlpha = GL_ZERO;
    GLint prevBlendEquationRGB = GL_FUNC_ADD;
    GLint prevBlendEquationAlpha = GL_FUNC_ADD;
    GLint prevDepthFunc = GL_LESS;
    GLint prevCullMode = GL_BACK;
    GLboolean prevBlend = GL_FALSE;
    GLboolean prevDepthTest = GL_FALSE;
    GLboolean prevStencilTest = GL_FALSE;
    GLboolean prevCullFace = GL_FALSE;
    GLboolean prevScissorTest = GL_FALSE;
    GLboolean prevRasterizerDiscard = GL_FALSE;
    GLboolean prevDepthMask = GL_TRUE;
    GLboolean prevColorMask[4] = {GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE};
    GLfloat prevClearColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};

    explicit GLStateGuard(uint32_t bits) : saved(bits) {
        if (saved & STATE_PROGRAM) prevProgram = static_cast<GLint>(gl_state->current_program);
        if (saved & STATE_VAO) GLES.glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &prevVAO);
        if (saved & STATE_ARRAY_BUFFER) GLES.glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &prevArrayBuffer);
        if (saved & (STATE_TEXTURE | STATE_SAMPLER)) {
            GLES.glGetIntegerv(GL_ACTIVE_TEXTURE, &prevActiveTexture);
            GLES.glActiveTexture(GL_TEXTURE0);
            if (saved & STATE_TEXTURE) GLES.glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTexture);
            // GL_SAMPLER_BINDING is per texture unit, so this is asked with unit 0
            // active: unit 0 is the unit both passes sample from.
            if (saved & STATE_SAMPLER) GLES.glGetIntegerv(GL_SAMPLER_BINDING, &prevSampler);
        }
        if (saved & STATE_FRAMEBUFFER) {
            GLES.glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prevReadFBO);
            prevDrawFBO = static_cast<GLint>(gl_state->current_draw_fbo);
        }
        if (saved & STATE_RENDERBUFFER) GLES.glGetIntegerv(GL_RENDERBUFFER_BINDING, &prevRenderbuffer);
        if (saved & STATE_VIEWPORT) GLES.glGetIntegerv(GL_VIEWPORT, prevViewport);
        if (saved & STATE_SCISSOR) {
            GLES.glGetIntegerv(GL_SCISSOR_BOX, prevScissorBox);
            prevScissorTest = mg_enable_get(GL_SCISSOR_TEST, 0);
        }
        if (saved & STATE_BLEND_ENABLE) prevBlend = mg_enable_get(GL_BLEND, 0);
        if (saved & STATE_BLEND_FUNCS) {
            GLES.glGetIntegerv(GL_BLEND_SRC_RGB, &prevBlendSrcRGB);
            GLES.glGetIntegerv(GL_BLEND_DST_RGB, &prevBlendDstRGB);
            GLES.glGetIntegerv(GL_BLEND_SRC_ALPHA, &prevBlendSrcAlpha);
            GLES.glGetIntegerv(GL_BLEND_DST_ALPHA, &prevBlendDstAlpha);
        }
        if (saved & STATE_BLEND_EQUATION) {
            GLES.glGetIntegerv(GL_BLEND_EQUATION_RGB, &prevBlendEquationRGB);
            GLES.glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &prevBlendEquationAlpha);
        }
        if (saved & STATE_DEPTH) {
            prevDepthTest = mg_enable_get(GL_DEPTH_TEST, 0);
            GLES.glGetBooleanv(GL_DEPTH_WRITEMASK, &prevDepthMask);
            GLES.glGetIntegerv(GL_DEPTH_FUNC, &prevDepthFunc);
        }
        if (saved & STATE_STENCIL) prevStencilTest = mg_enable_get(GL_STENCIL_TEST, 0);
        if (saved & STATE_CULL) {
            prevCullFace = mg_enable_get(GL_CULL_FACE, 0);
            GLES.glGetIntegerv(GL_CULL_FACE_MODE, &prevCullMode);
        }
        if (saved & STATE_RASTERIZER_DISCARD) prevRasterizerDiscard = mg_enable_get(GL_RASTERIZER_DISCARD, 0);
        if (saved & STATE_COLOR_MASK) GLES.glGetBooleanv(GL_COLOR_WRITEMASK, prevColorMask);
        if (saved & STATE_CLEAR_COLOR) GLES.glGetFloatv(GL_COLOR_CLEAR_VALUE, prevClearColor);
    }

    // Follow a framebuffer this guard saved through a delete-and-recreate.
    //
    // A saved name that the body then deletes cannot be restored: GL rejects it and
    // leaves the binding wherever the body happened to put it. RecreateFSRTargets is
    // the only body here that deletes framebuffers, and the render FBO is the name
    // gl/framebuffer.cpp redirects a bind of framebuffer 0 to -- which is the case
    // this whole path exists for -- so the guard is told where the replacement went
    // instead of being left to restore a dead name. Passing 0 is deliberate: it is
    // the real surface, which is where the application draws when the rebuild
    // failed.
    void framebuffer_recreated(GLuint from, GLuint to) {
        if (!(saved & STATE_FRAMEBUFFER) || from == 0 || from == to) return;
        if (prevReadFBO == static_cast<GLint>(from)) prevReadFBO = static_cast<GLint>(to);
        if (prevDrawFBO == static_cast<GLint>(from)) prevDrawFBO = static_cast<GLint>(to);
    }

    ~GLStateGuard() {
        if (saved & STATE_PROGRAM) GLES.glUseProgram(prevProgram);
        if (saved & STATE_VAO) GLES.glBindVertexArray(prevVAO);
        if (saved & STATE_ARRAY_BUFFER) GLES.glBindBuffer(GL_ARRAY_BUFFER, prevArrayBuffer);
        if (saved & (STATE_TEXTURE | STATE_SAMPLER)) {
            // Unit 0 is current for the guard's lifetime, but say so anyway: a body
            // is free to move the active unit as long as this line puts it back.
            GLES.glActiveTexture(GL_TEXTURE0);
            if (saved & STATE_TEXTURE) GLES.glBindTexture(GL_TEXTURE_2D, prevTexture);
            // Driver-side, like the texture binding above: the sampler binding was
            // read from the driver, and putting that same name back leaves the
            // application's shadow and the driver agreeing again. Sampler 0 is
            // bound for the passes, so a sampler object the application left on
            // unit 0 cannot override the FSR1 textures' filtering or compare mode.
            if (saved & STATE_SAMPLER) GLES.glBindSampler(0, prevSampler);
            GLES.glActiveTexture(prevActiveTexture);
        }
        if (saved & STATE_RENDERBUFFER) GLES.glBindRenderbuffer(GL_RENDERBUFFER, prevRenderbuffer);
        if (saved & STATE_FRAMEBUFFER) {
            GLES.glBindFramebuffer(GL_READ_FRAMEBUFFER, prevReadFBO);
            GLES.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, prevDrawFBO);
        }
        // Through the frontend, so gl/enable.cpp's table follows the driver. The
        // guard changed these bits through the same entry points.
        if (saved & STATE_BLEND_ENABLE) {
            if (prevBlend)
                glEnable(GL_BLEND);
            else
                glDisable(GL_BLEND);
        }
        if (saved & STATE_DEPTH) {
            if (prevDepthTest)
                glEnable(GL_DEPTH_TEST);
            else
                glDisable(GL_DEPTH_TEST);
            GLES.glDepthMask(prevDepthMask);
            GLES.glDepthFunc(prevDepthFunc);
        }
        if (saved & STATE_STENCIL) {
            if (prevStencilTest)
                glEnable(GL_STENCIL_TEST);
            else
                glDisable(GL_STENCIL_TEST);
        }
        if (saved & STATE_CULL) {
            if (prevCullFace)
                glEnable(GL_CULL_FACE);
            else
                glDisable(GL_CULL_FACE);
            GLES.glCullFace(prevCullMode);
        }
        if (saved & STATE_SCISSOR) {
            if (prevScissorTest)
                glEnable(GL_SCISSOR_TEST);
            else
                glDisable(GL_SCISSOR_TEST);
            GLES.glScissor(prevScissorBox[0], prevScissorBox[1], prevScissorBox[2], prevScissorBox[3]);
        }
        if (saved & STATE_RASTERIZER_DISCARD) {
            if (prevRasterizerDiscard)
                glEnable(GL_RASTERIZER_DISCARD);
            else
                glDisable(GL_RASTERIZER_DISCARD);
        }
        if (saved & STATE_BLEND_FUNCS)
            GLES.glBlendFuncSeparate(prevBlendSrcRGB, prevBlendDstRGB, prevBlendSrcAlpha, prevBlendDstAlpha);
        if (saved & STATE_BLEND_EQUATION)
            GLES.glBlendEquationSeparate(prevBlendEquationRGB, prevBlendEquationAlpha);
        if (saved & STATE_COLOR_MASK)
            GLES.glColorMask(prevColorMask[0], prevColorMask[1], prevColorMask[2], prevColorMask[3]);
        if (saved & STATE_CLEAR_COLOR)
            GLES.glClearColor(prevClearColor[0], prevClearColor[1], prevClearColor[2], prevClearColor[3]);
        if (saved & STATE_VIEWPORT) GLES.glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);
    }
};

namespace FSR1_Context {
    GLuint g_renderFBO = 0;
    GLuint g_renderTexture = 0;
    GLuint g_depthStencilRBO = 0;
    GLuint g_quadVAO = 0;
    GLuint g_quadVBO = 0;
    GLuint g_fsrProgram = 0;

    // Resolved once, when g_fsrProgram is linked. A uniform location is fixed for
    // the life of a program object and this one is never relinked, so asking for it
    // again is a driver-side name lookup per presented frame for an answer that
    // cannot have changed. -1 is what glGetUniformLocation returns for a name the
    // linker dropped, and glUniform* ignores it, so an unresolved location needs no
    // separate "not found" state.
    GLint g_inputTexLoc = -1;
    GLint g_passLoc = -1;
    GLint g_inputSizeLoc = -1;
    GLint g_outputSizeLoc = -1;

    GLuint g_easuFBO = 0;
    GLuint g_easuTexture = 0;

    GLuint g_targetFBO = 0;
    GLuint g_targetTexture = 0;

    GLuint g_currentDrawFBO = 0;
    GLint g_viewport[4] = {0};
    GLsizei g_outputWidth = 0;
    GLsizei g_outputHeight = 0;
    // g_target* is always the output extent; g_render* is derived from it.
    GLsizei g_targetWidth = 0;
    GLsizei g_targetHeight = 0;
    GLsizei g_renderWidth = 0;
    GLsizei g_renderHeight = 0;
    bool g_dirty = false;

    bool g_resolutionChanged = false;
    GLsizei g_pendingWidth = 0;
    GLsizei g_pendingHeight = 0;

    // Per-context init bookkeeping. g_initializing stops the frontend glCreateShader
    // calls made while compiling the FSR program from re-entering this file.
    bool g_initializing = false;
    bool g_initFailed = false;
} // namespace FSR1_Context

static bool QueryOutputSize(EGLDisplay display, EGLSurface surface, GLsizei* width, GLsizei* height) {
    LOAD_EGL(eglQuerySurface);
    if (display == EGL_NO_DISPLAY || surface == EGL_NO_SURFACE) {
        display = eglGetCurrentDisplay();
        surface = eglGetCurrentSurface(EGL_DRAW);
    }
    if (display == EGL_NO_DISPLAY || surface == EGL_NO_SURFACE || egl_eglQuerySurface == nullptr) return false;

    EGLint surfaceWidth = 0;
    EGLint surfaceHeight = 0;
    if (!egl_eglQuerySurface(display, surface, EGL_WIDTH, &surfaceWidth) ||
        !egl_eglQuerySurface(display, surface, EGL_HEIGHT, &surfaceHeight)) {
        return false;
    }
    if (surfaceWidth <= 0 || surfaceHeight <= 0) return false;
    *width = surfaceWidth;
    *height = surfaceHeight;
    return true;
}

static void ApplyPlanSizes(const Plan& plan) {
    FSR1_Context::g_outputWidth = plan.output.width;
    FSR1_Context::g_outputHeight = plan.output.height;
    FSR1_Context::g_targetWidth = plan.target.width;
    FSR1_Context::g_targetHeight = plan.target.height;
    FSR1_Context::g_renderWidth = plan.render.width;
    FSR1_Context::g_renderHeight = plan.render.height;
}

GLuint CompileFSRShader() {
    GLuint program = glCreateProgram();

    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    LOG_D("Vertex shader source:\n%s", FSR_VSSource);
    glShaderSource(vs, 1, &FSR_VSSource, nullptr);
    glCompileShader(vs);

    GLint status;
    glGetShaderiv(vs, GL_COMPILE_STATUS, &status);
    if (!status) {
        char log[512];
        glGetShaderInfoLog(vs, 512, nullptr, log);
        // LOG_F is a debug-only line. A failed FSR pass must not look like a
        // working one in a release log, so this one is forced.
        LOG_W_FORCE("FSR1: vertex shader compile failed: %s", log);
        glDeleteShader(vs);
        glDeleteProgram(program);
        return 0;
    }

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    LOG_D("Fragment shader source:\n%s", FSR_FSSource);
    glShaderSource(fs, 1, &FSR_FSSource, nullptr);
    glCompileShader(fs);

    glGetShaderiv(fs, GL_COMPILE_STATUS, &status);
    if (!status) {
        char log[512];
        glGetShaderInfoLog(fs, 512, nullptr, log);
        LOG_W_FORCE("FSR1: fragment shader compile failed: %s", log);
        glDeleteShader(vs);
        glDeleteShader(fs);
        glDeleteProgram(program);
        return 0;
    }

    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (!status) {
        char log[512];
        glGetProgramInfoLog(program, 512, nullptr, log);
        LOG_W_FORCE("FSR1: program link failed: %s", log);
        glDeleteShader(vs);
        glDeleteShader(fs);
        glDeleteProgram(program);
        return 0;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);

    return program;
}

static bool InitFullscreenQuad() {
    GLStateGuard state(STATE_VAO | STATE_ARRAY_BUFFER);
    const float quadVertices[] = {-1.0f, 1.0f, 0.0f, 1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f, 0.0f,

                                  -1.0f, 1.0f, 0.0f, 1.0f, 1.0f,  -1.0f, 1.0f, 0.0f, 1.0f, 1.0f,  1.0f, 1.0f};

    GLES.glGenVertexArrays(1, &FSR1_Context::g_quadVAO);
    GLES.glGenBuffers(1, &FSR1_Context::g_quadVBO);
    if (FSR1_Context::g_quadVAO == 0 || FSR1_Context::g_quadVBO == 0) {
        LOG_W_FORCE("FSR1: could not allocate the fullscreen quad (VAO=%u VBO=%u)", FSR1_Context::g_quadVAO,
                    FSR1_Context::g_quadVBO);
        return false;
    }

    GLES.glBindVertexArray(FSR1_Context::g_quadVAO);
    GLES.glBindBuffer(GL_ARRAY_BUFFER, FSR1_Context::g_quadVBO);

    GLES.glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

    GLES.glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    GLES.glEnableVertexAttribArray(0);

    GLES.glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    GLES.glEnableVertexAttribArray(1);

    GLES.glBindBuffer(GL_ARRAY_BUFFER, 0);
    GLES.glBindVertexArray(0);
    return true;
}

static void SetColorTextureParameters() {
    GLES.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    GLES.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    GLES.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    GLES.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    GLES.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
}

static bool CreateColorTexture(GLsizei width, GLsizei height, const char* label, GLuint* texture) {
    *texture = 0;
    if (width <= 0 || height <= 0) {
        LOG_W_FORCE("FSR1: refusing to allocate %s at %dx%d", label, width, height);
        return false;
    }
    GLES.glGenTextures(1, texture);
    if (*texture == 0) {
        LOG_W_FORCE("FSR1: glGenTextures failed for %s", label);
        return false;
    }
    GLES.glBindTexture(GL_TEXTURE_2D, *texture);
    GLES.glTexImage2D(GL_TEXTURE_2D, 0, kColorInternalFormat, width, height, 0, kColorFormat, kColorType, nullptr);
    SetColorTextureParameters();
    return true;
}

static bool CheckFramebufferComplete(const char* label) {
    const GLenum status = GLES.glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        LOG_W_FORCE("FSR1: %s framebuffer is incomplete (status 0x%04x)", label, status);
        return false;
    }
    return true;
}

// Colour-only attachment used by the EASU and RCAS destinations. Both are the
// same extent and format, so the second pass samples one texel per output pixel.
static bool CreateColorTarget(GLsizei width, GLsizei height, const char* label, GLuint* texture, GLuint* framebuffer) {
    *framebuffer = 0;
    if (!CreateColorTexture(width, height, label, texture)) return false;

    GLES.glGenFramebuffers(1, framebuffer);
    if (*framebuffer == 0) {
        LOG_W_FORCE("FSR1: glGenFramebuffers failed for %s", label);
        return false;
    }
    GLES.glBindFramebuffer(GL_FRAMEBUFFER, *framebuffer);
    GLES.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, *texture, 0);
    return CheckFramebufferComplete(label);
}

// The scene target the application draws into. It keeps a depth-stencil
// renderbuffer because the application believes it is drawing to the window.
static bool CreateRenderTarget(GLsizei width, GLsizei height) {
    if (!CreateColorTexture(width, height, "render colour", &FSR1_Context::g_renderTexture)) return false;

    GLES.glGenRenderbuffers(1, &FSR1_Context::g_depthStencilRBO);
    if (FSR1_Context::g_depthStencilRBO == 0) {
        LOG_W_FORCE("FSR1: glGenRenderbuffers failed for the render depth-stencil");
        return false;
    }
    GLES.glBindRenderbuffer(GL_RENDERBUFFER, FSR1_Context::g_depthStencilRBO);
    GLES.glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);

    GLES.glGenFramebuffers(1, &FSR1_Context::g_renderFBO);
    if (FSR1_Context::g_renderFBO == 0) {
        LOG_W_FORCE("FSR1: glGenFramebuffers failed for the render target");
        return false;
    }
    GLES.glBindFramebuffer(GL_FRAMEBUFFER, FSR1_Context::g_renderFBO);
    GLES.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, FSR1_Context::g_renderTexture, 0);
    GLES.glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                                   FSR1_Context::g_depthStencilRBO);
    return CheckFramebufferComplete("render");
}

static bool CreateFSRTargets() {
    // Order matters only for cleanup: everything is deleted together on failure.
    return CreateRenderTarget(FSR1_Context::g_renderWidth, FSR1_Context::g_renderHeight) &&
           CreateColorTarget(FSR1_Context::g_targetWidth, FSR1_Context::g_targetHeight, "easu output",
                             &FSR1_Context::g_easuTexture, &FSR1_Context::g_easuFBO) &&
           CreateColorTarget(FSR1_Context::g_targetWidth, FSR1_Context::g_targetHeight, "final output",
                             &FSR1_Context::g_targetTexture, &FSR1_Context::g_targetFBO);
}

static void DestroyTexturesAndFramebuffers() {
    GLuint framebuffers[3] = {FSR1_Context::g_renderFBO, FSR1_Context::g_easuFBO, FSR1_Context::g_targetFBO};
    GLuint textures[3] = {FSR1_Context::g_renderTexture, FSR1_Context::g_easuTexture, FSR1_Context::g_targetTexture};
    GLuint renderbuffers[1] = {FSR1_Context::g_depthStencilRBO};

    GLES.glDeleteFramebuffers(3, framebuffers);
    GLES.glDeleteTextures(3, textures);
    GLES.glDeleteRenderbuffers(1, renderbuffers);

    FSR1_Context::g_renderFBO = 0;
    FSR1_Context::g_renderTexture = 0;
    FSR1_Context::g_easuFBO = 0;
    FSR1_Context::g_easuTexture = 0;
    FSR1_Context::g_targetFBO = 0;
    FSR1_Context::g_targetTexture = 0;
    FSR1_Context::g_depthStencilRBO = 0;
}

static void DestroyProgramAndQuad() {
    if (FSR1_Context::g_fsrProgram != 0) glDeleteProgram(FSR1_Context::g_fsrProgram);
    if (FSR1_Context::g_quadVBO != 0) glDeleteBuffers(1, &FSR1_Context::g_quadVBO);
    if (FSR1_Context::g_quadVAO != 0) glDeleteVertexArrays(1, &FSR1_Context::g_quadVAO);
    FSR1_Context::g_fsrProgram = 0;
    FSR1_Context::g_quadVBO = 0;
    FSR1_Context::g_quadVAO = 0;
    FSR1_Context::g_inputTexLoc = -1;
    FSR1_Context::g_passLoc = -1;
    FSR1_Context::g_inputSizeLoc = -1;
    FSR1_Context::g_outputSizeLoc = -1;
}

bool fsrInitialized = false;

// Build everything from the current plan sizes. Called only with no live FSR
// objects: a failure here is final for the context, and the caller drops back to
// drawing straight to the surface rather than leaving a half-built redirect.
static bool BuildFSRResources() {
    FSR1_Context::g_fsrProgram = CompileFSRShader();
    if (FSR1_Context::g_fsrProgram == 0) return false;

    FSR1_Context::g_inputTexLoc = glGetUniformLocation(FSR1_Context::g_fsrProgram, "uInputTex");
    FSR1_Context::g_passLoc = glGetUniformLocation(FSR1_Context::g_fsrProgram, "uPass");
    FSR1_Context::g_inputSizeLoc = glGetUniformLocation(FSR1_Context::g_fsrProgram, "uInputSize");
    FSR1_Context::g_outputSizeLoc = glGetUniformLocation(FSR1_Context::g_fsrProgram, "uOutputSize");

    // Every uniform but the sampler is read from a known location or dropped by
    // the linker. A dropped name would silently produce the wrong image, so the
    // one that selects the pass is required. (-1 is ignored by glUniform*, which
    // is fine for the sampler when the linker folds it out, but not for uPass.)
    if (FSR1_Context::g_passLoc < 0 || FSR1_Context::g_inputSizeLoc < 0 || FSR1_Context::g_outputSizeLoc < 0) {
        LOG_W_FORCE("FSR1: shader is missing a required uniform (pass=%d input=%d output=%d)", FSR1_Context::g_passLoc,
                    FSR1_Context::g_inputSizeLoc, FSR1_Context::g_outputSizeLoc);
        return false;
    }

    // GLES.glUseProgram and not this layer's own: the frontend one writes
    // gl_state->current_program, and the guard around this function restores the
    // driver from that same field. Going through the frontend here would leave the
    // tracked program saying 0 while the driver holds the application's, and
    // gl/program.cpp then drops the application's next glUseProgram(0) as
    // redundant.
    GLES.glUseProgram(FSR1_Context::g_fsrProgram);
    GLES.glUniform1i(FSR1_Context::g_inputTexLoc, 0);
    GLES.glUseProgram(0);

    if (!InitFullscreenQuad()) return false;
    return CreateFSRTargets();
}

void InitFSRResources() {
    if (fsrInitialized || FSR1_Context::g_initializing || FSR1_Context::g_initFailed) return;
    FSR1_Context::g_initializing = true;

    GLStateGuard state(kResourceCreationState);

    GLsizei width = 0;
    GLsizei height = 0;
    if (!QueryOutputSize(EGL_NO_DISPLAY, EGL_NO_SURFACE, &width, &height)) {
        // No surface yet. Do not invent an extent: the first frame after a swap
        // retries from CheckResolutionChange, which is also where a surface that
        // changed size is picked up. Until then the application draws straight to
        // the surface, which is the correct image at native resolution.
        FSR1_Context::g_initializing = false;
        return;
    }

    ApplyPlanSizes(MakePlan(global_settings.fsr1_setting, width, height));

    const bool built = BuildFSRResources();
    if (!built) {
        DestroyTexturesAndFramebuffers();
        DestroyProgramAndQuad();
        FSR1_Context::g_initFailed = true;
        FSR1_Context::g_dirty = false;
        FSR1_Context::g_initializing = false;
        LOG_W_FORCE("FSR1: resource creation failed; the spatial path is disabled for this context");
        return;
    }

    fsrInitialized = true;
    FSR1_Context::g_dirty = false;
    FSR1_Context::g_initializing = false;

    // The guard restores the application's bindings and viewport, so there is
    // deliberately no "start with the render target current" bind here: it would
    // be undone immediately, and a render target nobody has drawn into is not a
    // frame. The redirect starts at the application's next draw bind of
    // framebuffer 0, which also sets g_dirty so ApplyFSR knows the application
    // draws into the render texture from then on. Until then the surface is
    // presented as drawn.
    LOG_I("FSR1: resources built for output %dx%d, target %dx%d, render %dx%d; upscaling starts on the next "
          "redirect bind of framebuffer 0",
          FSR1_Context::g_outputWidth, FSR1_Context::g_outputHeight, FSR1_Context::g_targetWidth,
          FSR1_Context::g_targetHeight, FSR1_Context::g_renderWidth, FSR1_Context::g_renderHeight);
}

// Rebuild the three render targets at the current plan sizes. Returns false when
// the new set could not be completed; the caller then disables the redirect so
// the application immediately goes back to drawing on the surface. The failure is
// never reported as a working upscale.
static bool RecreateFSRTargets() {
    GLStateGuard state(kRecreateState);
    const GLuint oldRenderFBO = FSR1_Context::g_renderFBO;
    const GLuint oldEasuFBO = FSR1_Context::g_easuFBO;
    const GLuint oldTargetFBO = FSR1_Context::g_targetFBO;
    // True while the application's framebuffer 0 was the thing being drawn to,
    // i.e. the tracked draw binding is the redirect: gl/framebuffer.cpp names the
    // render FBO there instead of 0. Only then does deleting the render FBO leave
    // a tracked name that has to be republished.
    const bool redirected = oldRenderFBO != 0 && gl_state->current_draw_fbo == oldRenderFBO;

    DestroyTexturesAndFramebuffers();

    if (!CreateFSRTargets()) {
        DestroyTexturesAndFramebuffers();
        // The old render FBO is gone, so both trackers have to name the surface
        // again -- gl_state->current_draw_fbo because the guard restores from it,
        // and gl/framebuffer.cpp's per-context draw binding because every later
        // glDrawBuffers/attachment update indexes through it. Updating only one of
        // them is how the two would disagree about the name the driver holds.
        if (redirected) mg_framebuffer_republish_draw_binding(0);
        state.framebuffer_recreated(oldRenderFBO, 0);
        state.framebuffer_recreated(oldEasuFBO, 0);
        state.framebuffer_recreated(oldTargetFBO, 0);
        FSR1_Context::g_dirty = false;
        GLES.glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return false;
    }

    // The tracked draw binding names the render FBO for as long as the application
    // is drawing to framebuffer 0, because gl/framebuffer.cpp redirects that bind
    // and records the name it handed the driver. That name was deleted above, and
    // nothing in gl/framebuffer.cpp can see it happen -- its glDeleteFramebuffers
    // hook, the one that rebinds 0, is not the path taken here. Left alone, both
    // trackers would keep naming a dead framebuffer and every GLStateGuard from
    // here on would try to restore it.
    //
    // Name 0 is excluded, and it is reachable: this runs once a frame from the swap
    // as soon as FSR1 is switched on, while InitFSRResources waits for the first
    // shader. A tracked 0 there is the real surface, not a redirect, and moving it
    // onto the render FBO would diverge from the driver in the other direction --
    // the guard below restores what it saved, which is 0.
    if (redirected) mg_framebuffer_republish_draw_binding(FSR1_Context::g_renderFBO);
    state.framebuffer_recreated(oldRenderFBO, FSR1_Context::g_renderFBO);
    state.framebuffer_recreated(oldEasuFBO, FSR1_Context::g_easuFBO);
    state.framebuffer_recreated(oldTargetFBO, FSR1_Context::g_targetFBO);

    // Priming deliberately survives the rebuild. The new render texture has not
    // been drawn into yet, but the driver's draw binding is restored to it by
    // the guard (or republished above), so the application's next frame -- which
    // it draws before presenting, whether or not it binds framebuffer 0 again --
    // fills it. There is no reliable "nothing was drawn" signal, and holding a
    // present on "no bind" would throw away that valid frame, so the next
    // presented frame is upscaled. An unprimed path stays unprimed and waits for
    // its first redirect bind.

    LOG_D("FSR1 resources recreated: output %dx%d, render %dx%d", FSR1_Context::g_outputWidth,
          FSR1_Context::g_outputHeight, FSR1_Context::g_renderWidth, FSR1_Context::g_renderHeight);
    return true;
}

std::vector<std::pair<GLsizei, GLsizei>> g_viewportStack;

void ApplyFSR() {
    if (!fsrInitialized) {
        // The shader-compile hook runs before a surface is necessarily current;
        // retry here, where one certainly is. A latched failure makes this cheap.
        InitFSRResources();
        if (!fsrInitialized) {
            static bool warned = false;
            if (!warned) {
                warned = true;
                LOG_W_FORCE("FSR1: asked to present but resources are unavailable; presenting the surface as drawn");
            }
            return;
        }
    }

    // Priming is persistent: once the application has bound framebuffer 0 for
    // drawing, gl/framebuffer.cpp redirects every later draw of it to
    // g_renderFBO, and an application that binds only once keeps rendering into
    // the target without another bind. So ApplyFSR upscales every present after
    // that point, including the first frame after a rebuild -- the application
    // has drawn into the replacement before it presents.
    if (DecidePresent(FSR1_Context::g_dirty) == PresentDecision::Wait) {
        static bool warned = false;
        if (!warned) {
            warned = true;
            LOG_W_FORCE("FSR1: waiting for the application to bind framebuffer 0; presenting the surface as drawn "
                        "and not claiming an upscale for this frame");
        }
        return;
    }

    GLStateGuard state(kApplyFSRState);

    // Deterministic pass state. Every bit below is restored by the guard.
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_RASTERIZER_DISCARD);
    GLES.glDepthMask(GL_FALSE);
    GLES.glBlendFuncSeparate(GL_ONE, GL_ZERO, GL_ONE, GL_ZERO);
    GLES.glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
    GLES.glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    GLES.glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    GLES.glUseProgram(FSR1_Context::g_fsrProgram);
    GLES.glActiveTexture(GL_TEXTURE0);
    // A sampler object bound on unit 0 replaces the FSR1 textures' filter, wrap
    // and compare state. The guard saved it and this pass must see the texture
    // parameters the textures were created with, so unit 0 gets sampler 0.
    GLES.glBindSampler(0, 0);
    GLES.glBindVertexArray(FSR1_Context::g_quadVAO);

    // EASU: render extent -> intermediate at output extent. gl_FragCoord is the
    // output pixel, which is what FsrEasuF's ip parameter wants.
    GLES.glBindFramebuffer(GL_FRAMEBUFFER, FSR1_Context::g_easuFBO);
    GLES.glViewport(0, 0, FSR1_Context::g_targetWidth, FSR1_Context::g_targetHeight);
    GLES.glBindTexture(GL_TEXTURE_2D, FSR1_Context::g_renderTexture);
    GLES.glUniform1i(FSR1_Context::g_passLoc, static_cast<GLint>(Pass::Easu));
    GLES.glUniform2f(FSR1_Context::g_inputSizeLoc, static_cast<GLfloat>(FSR1_Context::g_renderWidth),
                     static_cast<GLfloat>(FSR1_Context::g_renderHeight));
    GLES.glUniform2f(FSR1_Context::g_outputSizeLoc, static_cast<GLfloat>(FSR1_Context::g_targetWidth),
                     static_cast<GLfloat>(FSR1_Context::g_targetHeight));
    GLES.glDrawArrays(GL_TRIANGLES, 0, 6);

    // RCAS: reads the EASU result, never the scene texture. Input and output
    // extents are equal, so texelFetch(gl_FragCoord) addresses the matching texel.
    GLES.glBindFramebuffer(GL_FRAMEBUFFER, FSR1_Context::g_targetFBO);
    GLES.glClear(GL_COLOR_BUFFER_BIT);
    GLES.glBindTexture(GL_TEXTURE_2D, FSR1_Context::g_easuTexture);
    GLES.glUniform1i(FSR1_Context::g_passLoc, static_cast<GLint>(Pass::Rcas));
    GLES.glUniform2f(FSR1_Context::g_inputSizeLoc, static_cast<GLfloat>(FSR1_Context::g_targetWidth),
                     static_cast<GLfloat>(FSR1_Context::g_targetHeight));
    GLES.glUniform2f(FSR1_Context::g_outputSizeLoc, static_cast<GLfloat>(FSR1_Context::g_targetWidth),
                     static_cast<GLfloat>(FSR1_Context::g_targetHeight));
    GLES.glDrawArrays(GL_TRIANGLES, 0, 6);

    // Present. The target is the surface extent, so this is a 1:1 copy onto the
    // real drawable; the guard puts the read and draw bindings back afterwards.
    GLES.glBindFramebuffer(GL_READ_FRAMEBUFFER, FSR1_Context::g_targetFBO);
    GLES.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    GLES.glBlitFramebuffer(0, 0, FSR1_Context::g_targetWidth, FSR1_Context::g_targetHeight, 0, 0,
                           FSR1_Context::g_targetWidth, FSR1_Context::g_targetHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    // g_dirty deliberately survives the present: the application keeps drawing
    // into the render target, and the next frame is upscaled without requiring a
    // new bind of framebuffer 0.
}

void CheckResolutionChange(EGLDisplay display, EGLSurface surface) {
    GLsizei width = 0;
    GLsizei height = 0;
    if (!QueryOutputSize(display, surface, &width, &height)) return;
    OnResize(width, height);

    if (!FSR1_Context::g_resolutionChanged) return;
    FSR1_Context::g_resolutionChanged = false;

    if (!fsrInitialized) {
        // Covers the case where the first shader compiled before a surface was
        // current. InitFSRResources queries the same surface itself.
        InitFSRResources();
        return;
    }

    ApplyPlanSizes(MakePlan(global_settings.fsr1_setting, FSR1_Context::g_pendingWidth,
                            FSR1_Context::g_pendingHeight));

    if (!RecreateFSRTargets()) {
        // No usable resource set: stop redirecting framebuffer 0 and let the
        // application present at the surface's own resolution. The targets are
        // already gone (RecreateFSRTargets deletes them), and the program and
        // quad are just as unusable without a target set -- they are deleted too,
        // so the failure leaves no FSR1 objects behind and no later frame can
        // quietly reuse them. InitFSRResources stays latched off so a later frame
        // cannot half-recover.
        DestroyProgramAndQuad();
        FSR1_Context::g_initFailed = true;
        FSR1_Context::g_dirty = false;
        fsrInitialized = false;
        LOG_W_FORCE("FSR1: resize to %dx%d failed; spatial upscaling disabled", FSR1_Context::g_outputWidth,
                    FSR1_Context::g_outputHeight);
    }
}

void OnResize(int width, int height) {
    if (width <= 0 || height <= 0) return;
    // The surface is the output. Comparing against the render extent -- which is
    // what this used to do -- made every application viewport change look like a
    // surface resize and let the caller's viewport become render resolution.
    if (FSR1_Context::g_outputWidth == width && FSR1_Context::g_outputHeight == height) return;

    FSR1_Context::g_pendingWidth = width;
    FSR1_Context::g_pendingHeight = height;
    FSR1_Context::g_resolutionChanged = true;
}

void glViewport(GLint x, GLint y, GLsizei w, GLsizei h) {
    LOG()
    LOG_D("glViewport: x=%d, y=%d, w=%d, h=%d", x, y, w, h);

    // A pass-through. Surface size comes from EGL in CheckResolutionChange; the
    // application's viewport describes its own scene framing and is not evidence
    // that the drawable changed size. Feeding it into the render extent was the
    // false resize trigger that made target dimensions a multiple of the
    // viewport.
    //
    // The viewport is deliberately not rescaled to the reduced render target.
    // Doing that here would change what the application believes it is drawing
    // into while screen-size uniforms still report the surface extent; reduced
    // scene rendering needs the adapter (or a deliberate viewport/scissor policy
    // agreed with it). Say so once, at the point where the mismatch is visible,
    // instead of silently cropping the scene.
    if (fsrInitialized && gl_state->current_draw_fbo == FSR1_Context::g_renderFBO &&
        (w > FSR1_Context::g_renderWidth || h > FSR1_Context::g_renderHeight)) {
        static bool warned = false;
        if (!warned) {
            warned = true;
            LOG_W_FORCE("FSR1: application viewport %dx%d exceeds the %dx%d render target; reduced scene rendering "
                        "requires the adapter to render at the render extent (the viewport is left untouched)",
                        w, h, FSR1_Context::g_renderWidth, FSR1_Context::g_renderHeight);
        }
    }

    FSR3AdjustViewport(x, y, w, h);
    GLES.glViewport(x, y, w, h);
}

// ---------------------------------------------------------------------------

namespace {

struct fsr1_ctx_state_t {
    GLuint renderFBO = 0, renderTexture = 0, depthStencilRBO = 0;
    GLuint quadVAO = 0, quadVBO = 0, fsrProgram = 0;
    // Locations belong to fsrProgram, so they travel with it rather than being
    // re-resolved after a context switch.
    GLint inputTexLoc = -1, passLoc = -1, inputSizeLoc = -1, outputSizeLoc = -1;
    GLuint easuFBO = 0, easuTexture = 0;
    GLuint targetFBO = 0, targetTexture = 0, currentDrawFBO = 0;
    GLsizei outputWidth = 0, outputHeight = 0;
    GLsizei targetWidth = 0, targetHeight = 0, renderWidth = 0, renderHeight = 0;
    // Whether this context's render texture has ever been bound for drawing.
    // It travels with the objects because it describes them.
    bool dirty = false;
    bool initialised = false;
    bool initFailed = false;
};

std::mutex g_fsr_mutex;
// Plain value, not a unique_ptr like the other per-context tables: nothing here
// keeps the address of an entry. Both operator[] calls in mg_fsr1_bind_context
// are separate statements, so the first reference is dead before the second one
// can rehash the map.
ska::flat_hash_map<unsigned long long, fsr1_ctx_state_t> g_fsr_states;
fsr1_ctx_state_t g_fsr_default;
thread_local unsigned long long g_fsr_current_id = 0;

void store_into(fsr1_ctx_state_t& d) {
    d.renderFBO = FSR1_Context::g_renderFBO;
    d.renderTexture = FSR1_Context::g_renderTexture;
    d.depthStencilRBO = FSR1_Context::g_depthStencilRBO;
    d.quadVAO = FSR1_Context::g_quadVAO;
    d.quadVBO = FSR1_Context::g_quadVBO;
    d.fsrProgram = FSR1_Context::g_fsrProgram;
    d.inputTexLoc = FSR1_Context::g_inputTexLoc;
    d.passLoc = FSR1_Context::g_passLoc;
    d.inputSizeLoc = FSR1_Context::g_inputSizeLoc;
    d.outputSizeLoc = FSR1_Context::g_outputSizeLoc;
    d.easuFBO = FSR1_Context::g_easuFBO;
    d.easuTexture = FSR1_Context::g_easuTexture;
    d.targetFBO = FSR1_Context::g_targetFBO;
    d.targetTexture = FSR1_Context::g_targetTexture;
    d.currentDrawFBO = FSR1_Context::g_currentDrawFBO;
    d.outputWidth = FSR1_Context::g_outputWidth;
    d.outputHeight = FSR1_Context::g_outputHeight;
    d.targetWidth = FSR1_Context::g_targetWidth;
    d.targetHeight = FSR1_Context::g_targetHeight;
    d.renderWidth = FSR1_Context::g_renderWidth;
    d.renderHeight = FSR1_Context::g_renderHeight;
    d.dirty = FSR1_Context::g_dirty;
    d.initialised = fsrInitialized;
    d.initFailed = FSR1_Context::g_initFailed;
}

void load_from(const fsr1_ctx_state_t& s) {
    FSR1_Context::g_renderFBO = s.renderFBO;
    FSR1_Context::g_renderTexture = s.renderTexture;
    FSR1_Context::g_depthStencilRBO = s.depthStencilRBO;
    FSR1_Context::g_quadVAO = s.quadVAO;
    FSR1_Context::g_quadVBO = s.quadVBO;
    FSR1_Context::g_fsrProgram = s.fsrProgram;
    FSR1_Context::g_inputTexLoc = s.inputTexLoc;
    FSR1_Context::g_passLoc = s.passLoc;
    FSR1_Context::g_inputSizeLoc = s.inputSizeLoc;
    FSR1_Context::g_outputSizeLoc = s.outputSizeLoc;
    FSR1_Context::g_easuFBO = s.easuFBO;
    FSR1_Context::g_easuTexture = s.easuTexture;
    FSR1_Context::g_targetFBO = s.targetFBO;
    FSR1_Context::g_targetTexture = s.targetTexture;
    FSR1_Context::g_currentDrawFBO = s.currentDrawFBO;
    FSR1_Context::g_outputWidth = s.outputWidth;
    FSR1_Context::g_outputHeight = s.outputHeight;
    FSR1_Context::g_targetWidth = s.targetWidth;
    FSR1_Context::g_targetHeight = s.targetHeight;
    FSR1_Context::g_renderWidth = s.renderWidth;
    FSR1_Context::g_renderHeight = s.renderHeight;
    FSR1_Context::g_dirty = s.dirty;
    fsrInitialized = s.initialised;
    FSR1_Context::g_initFailed = s.initFailed;
    // Left alone deliberately: g_resolutionChanged and the pending size describe
    // work queued for the frame in flight, not the context's objects. So is
    // g_initializing, which only spans a single synchronous build.
}

} // namespace

void mg_fsr1_bind_context(unsigned long long ctx_id) {
    if (ctx_id == g_fsr_current_id) return;
    std::lock_guard<std::mutex> lock(g_fsr_mutex);
    store_into(g_fsr_current_id == 0 ? g_fsr_default : g_fsr_states[g_fsr_current_id]);
    load_from(ctx_id == 0 ? g_fsr_default : g_fsr_states[ctx_id]);
    g_fsr_current_id = ctx_id;
}

void mg_fsr1_forget_context(unsigned long long ctx_id) {
    if (ctx_id == 0) return;
    std::lock_guard<std::mutex> lock(g_fsr_mutex);
    // If this is still the loaded set, the live globals describe a context that is
    // gone. Drop back to the default set rather than storing them into the entry
    // about to be erased.
    if (g_fsr_current_id == ctx_id) {
        load_from(g_fsr_default);
        g_fsr_current_id = 0;
    }
    g_fsr_states.erase(ctx_id);
}
