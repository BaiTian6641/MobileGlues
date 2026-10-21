// MobileGlues - gl/FSR3Direct.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header
//
// Experimental renderer-agnostic presentation path. It does not claim to
// recover scene motion: the final display image is copied to an RGBA8 texture,
// depth is a constant 0.5 texture and motion is zero. The FSR3 algorithm runs,
// but without valid temporal inputs this is only a diagnostic/experimental
// composition, not supported FSR3 quality. The path is selected by the app
// config key "fsr3Setting" and never runs when the key is absent/disabled.

#include "FSR3Direct.h"

#include "../config/settings.h"
#include "log.h"
#include "../egl/context.h"
#include "../gles/gles.h"
#include "../upscaling/mg_fsr_abi.h"
#include "../upscaling/mg_fsr_lifecycle.h"

#include <EGL/egl.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

struct ContextState {
    void* session = nullptr;
    GLuint color = 0;
    GLuint depth = 0;
    GLuint motion = 0;
    GLuint input_fbo = 0;
    GLuint output_fbo = 0;
    GLuint attached_output = 0;
    int width = 0;
    int height = 0;
    int render_width = 0;
    int render_height = 0;
    uint64_t frame_id = 0;
    bool failed = false;
    bool warned = false;
    std::chrono::steady_clock::time_point last_time{};
};

std::mutex g_mutex;
std::unordered_map<unsigned long long, ContextState> g_states;
thread_local bool g_in_present = false;
std::atomic<bool> g_bridge_presented_this_frame{false};
std::atomic<bool> g_bridge_mode{false};
thread_local bool g_final_scope = false;
thread_local unsigned int g_bridge_depth = 0;

struct PresentGuard {
    GLint draw = 0, read = 0, active = GL_TEXTURE0, tex0 = 0, sampler0 = 0;
    GLint viewport[4]{};

    PresentGuard() {
        GLES.glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw);
        GLES.glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read);
        GLES.glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
        GLES.glGetIntegerv(GL_VIEWPORT, viewport);
        GLES.glActiveTexture(GL_TEXTURE0);
        GLES.glGetIntegerv(GL_TEXTURE_BINDING_2D, &tex0);
        GLES.glGetIntegerv(GL_SAMPLER_BINDING, &sampler0);
        GLES.glActiveTexture(active);
    }
    ~PresentGuard() {
        GLES.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, draw);
        GLES.glBindFramebuffer(GL_READ_FRAMEBUFFER, read);
        GLES.glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
        GLES.glActiveTexture(GL_TEXTURE0);
        GLES.glBindTexture(GL_TEXTURE_2D, tex0);
        GLES.glBindSampler(0, sampler0);
        GLES.glActiveTexture(active);
    }
};

GLuint make_texture(GLenum internal, int w, int h) {
    GLuint tex = 0;
    GLES.glGenTextures(1, &tex);
    if (!tex) return 0;
    GLES.glBindTexture(GL_TEXTURE_2D, tex);
    GLES.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    GLES.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    GLES.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    GLES.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    GLES.glTexStorage2D(GL_TEXTURE_2D, 1, internal, w, h);
    GLES.glBindTexture(GL_TEXTURE_2D, 0);
    return GLES.glGetError() == GL_NO_ERROR ? tex : 0;
}

bool create_resources(ContextState& state, int width, int height) {
    state.width = width;
    state.height = height;
    const float scale = fsr3_quality_scale(global_settings.fsr3_quality);
    state.render_width = std::max(64, static_cast<int>(std::lround(width / scale)));
    state.render_height = std::max(64, static_cast<int>(std::lround(height / scale)));
    const int rw = state.render_width;
    const int rh = state.render_height;
    state.color = make_texture(GL_RGBA8, rw, rh);
    state.depth = make_texture(GL_R32F, rw, rh);
    state.motion = make_texture(GL_RG16F, rw, rh);
    if (!state.color || !state.depth || !state.motion) return false;

    std::vector<float> depth(static_cast<size_t>(rw) * rh, 0.5f);
    std::vector<float> motion(static_cast<size_t>(rw) * rh * 2, 0.0f);
    GLES.glBindTexture(GL_TEXTURE_2D, state.depth);
    GLES.glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, rw, rh, GL_RED, GL_FLOAT, depth.data());
    GLES.glBindTexture(GL_TEXTURE_2D, state.motion);
    GLES.glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, rw, rh, GL_RG, GL_FLOAT, motion.data());
    GLES.glBindTexture(GL_TEXTURE_2D, 0);

    GLES.glGenFramebuffers(1, &state.input_fbo);
    GLES.glGenFramebuffers(1, &state.output_fbo);
    if (!state.input_fbo || !state.output_fbo) return false;
    GLES.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, state.input_fbo);
    GLES.glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, state.color, 0);
    const GLenum draw_buffer = GL_COLOR_ATTACHMENT0;
    GLES.glDrawBuffers(1, &draw_buffer);
    if (GLES.glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) return false;
    GLES.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

    state.session = mg_fsr_create_session();
    if (!state.session) return false;
    state.failed = false;
    state.warned = false;
    state.frame_id = 0;
    state.last_time = std::chrono::steady_clock::now();
    return GLES.glGetError() == GL_NO_ERROR;
}

void destroy_resources(ContextState& state) {
    if (state.session) mg_fsr_destroy_session(state.session);
    state.session = nullptr;
    if (state.input_fbo) GLES.glDeleteFramebuffers(1, &state.input_fbo);
    if (state.output_fbo) GLES.glDeleteFramebuffers(1, &state.output_fbo);
    if (state.color) GLES.glDeleteTextures(1, &state.color);
    if (state.depth) GLES.glDeleteTextures(1, &state.depth);
    if (state.motion) GLES.glDeleteTextures(1, &state.motion);
    state.input_fbo = state.output_fbo = state.color = state.depth = state.motion = 0;
    state.attached_output = 0;
    state.width = state.height = 0;
    state.render_width = state.render_height = 0;
}

bool surface_size(int& width, int& height) {
    EGLDisplay display = eglGetCurrentDisplay();
    EGLSurface surface = eglGetCurrentSurface(EGL_DRAW);
    if (display == EGL_NO_DISPLAY || surface == EGL_NO_SURFACE) return false;
    EGLint w = 0, h = 0;
    if (!eglQuerySurface(display, surface, EGL_WIDTH, &w) ||
        !eglQuerySurface(display, surface, EGL_HEIGHT, &h) ||
        w <= 0 || h <= 0)
        return false;
    width = w;
    height = h;
    return true;
}

} // namespace

bool FSR3GetSurfaceSize(int& width, int& height) {
    return surface_size(width, height);
}

void FSR3SetBridgeMode() {
    g_bridge_mode.store(true, std::memory_order_release);
}

bool FSR3BridgeMode() {
    return g_bridge_mode.load(std::memory_order_acquire);
}

bool FSR3AdjustViewport(int& x, int& y, int& width, int& height) {
    if (global_settings.fsr3_setting != FSR3_Mode::Experimental) return false;
    if (width <= 0 || height <= 0) return false;
    // The current EGL surface owns the output extent. A previously bound
    // intermediate framebuffer cannot define it, and a resize must take effect
    // before the next bridge submission.
    int surface_w = 0, surface_h = 0;
    if (!surface_size(surface_w, surface_h)) return false;
    GLint fbo = 0;
    GLES.glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &fbo);
    if (fbo == 0) return false;
    GLint type = 0;
    GLES.glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                               GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &type);
    if (type != GL_TEXTURE) return false;
    GLint tex = 0;
    GLES.glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                               GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &tex);
    if (tex == 0) return false;
    GLint prev_unit = GL_TEXTURE0, prev_tex = 0;
    GLES.glGetIntegerv(GL_ACTIVE_TEXTURE, &prev_unit);
    GLES.glActiveTexture(GL_TEXTURE0);
    GLES.glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev_tex);
    GLES.glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(tex));
    GLint target_w = 0, target_h = 0;
    GLES.glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &target_w);
    GLES.glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &target_h);
    GLES.glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(prev_tex));
    GLES.glActiveTexture(static_cast<GLenum>(prev_unit));
    if (target_w >= surface_w && target_h >= surface_h) {
        if (width == surface_w && height == surface_h) return false;
        x = 0;
        y = 0;
        width = surface_w;
        height = surface_h;
        static bool logged_native = false;
        if (!logged_native) {
            logged_native = true;
            LOG_W_FORCE("FSR3 native-target viewport %dx%d", width, height);
        }
        return true;
    }
    if (width != surface_w || height != surface_h) return false;
    const FSR3ViewportPlan plan = FSR3PlanViewport(surface_w, surface_h, target_w, target_h,
                                                    x, y, width, height);
    if (!plan.adjust) return false;
    x = plan.x;
    y = plan.y;
    width = plan.width;
    height = plan.height;
    static bool logged = false;
    if (!logged) {
        logged = true;
        LOG_W_FORCE("FSR3 scene viewport %dx%d -> %dx%d", surface_w, surface_h, width, height);
    }
    return true;
}

void FSR3SetBridgeDepthTexture(unsigned int texture) { g_bridge_depth = texture; }
unsigned int FSR3BridgeDepthTexture() { return g_bridge_depth; }

void FSR3SetFinalScope(bool active) { g_final_scope = active; }

void FSR3ApplyFinalViewportUniforms(int currentProgram) {
    if (!g_final_scope) return;
    int width = 0, height = 0;
    if (!FSR3GetSurfaceSize(width, height) || width <= 0 || height <= 0) return;
    const GLint wl = GLES.glGetUniformLocation(currentProgram, "viewWidth");
    const GLint hl = GLES.glGetUniformLocation(currentProgram, "viewHeight");
    const GLint al = GLES.glGetUniformLocation(currentProgram, "aspectRatio");
    if (wl >= 0) GLES.glUniform1f(wl, static_cast<GLfloat>(width));
    if (hl >= 0) GLES.glUniform1f(hl, static_cast<GLfloat>(height));
    if (al >= 0) GLES.glUniform1f(al, static_cast<GLfloat>(width) / static_cast<GLfloat>(height));
}

void FSR3NativeViewport() {
    int width = 0, height = 0;
    if (!surface_size(width, height)) return;
    static bool logged = false;
    if (!logged) {
        logged = true;
        LOG_W_FORCE("FSR3 native viewport %dx%d", width, height);
    }
    GLES.glViewport(0, 0, width, height);
}

void FSR3MarkBridgePresent() {
    g_bridge_presented_this_frame.store(true, std::memory_order_release);
}

bool FSR3BridgePresentedThisFrame() {
    return g_bridge_presented_this_frame.exchange(false, std::memory_order_acq_rel);
}

void ApplyFSR3Present() {
    if (global_settings.fsr3_setting != FSR3_Mode::Experimental) return;
    // The Minecraft bridge is the product route. The direct path consumes the
    // already-composed framebuffer and must never touch menu/UI-only frames.
    if (FSR3BridgeMode()) return;
    if (FSR3BridgePresentedThisFrame()) return;
    if (g_in_present) return;
    if (!GLES.glDispatchCompute || !GLES.glBindImageTexture) return;
    if (!g_current_ctx && (!GLES.glGetString || GLES.glGetString(GL_VERSION) == nullptr)) return;
    const unsigned long long context_key = g_current_ctx ? g_current_ctx->id : 1;
    int width = 0, height = 0;
    if (!surface_size(width, height) || width < 64 || height < 64) return;

    g_in_present = true;
    std::lock_guard<std::mutex> lock(g_mutex);
    ContextState& state = g_states[context_key];
    const float selected_scale = fsr3_quality_scale(global_settings.fsr3_quality);
    const int desired_render_w = std::max(64, static_cast<int>(std::lround(width / selected_scale)));
    const int desired_render_h = std::max(64, static_cast<int>(std::lround(height / selected_scale)));
    if (!state.session || state.width != width || state.height != height ||
        state.render_width != desired_render_w || state.render_height != desired_render_h) {
        if (state.session) destroy_resources(state);
        if (!create_resources(state, width, height)) {
            destroy_resources(state);
            if (!state.warned) {
                state.warned = true;
                LOG_W_FORCE("FSR3 direct: presentation resources unavailable; leaving the frame untouched");
            }
            g_in_present = false;
            return;
        }
    }

    PresentGuard guard;

    // The final image is already composed by the application. Copy it into a
    // texture; no scene target or semantic motion source exists here.
    GLES.glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    GLES.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, state.input_fbo);
    GLES.glViewport(0, 0, state.render_width, state.render_height);
    GLES.glBlitFramebuffer(0, 0, width, height, 0, 0, state.render_width, state.render_height,
                           GL_COLOR_BUFFER_BIT, GL_LINEAR);

    const auto now = std::chrono::steady_clock::now();
    float delta = std::chrono::duration<float>(now - state.last_time).count();
    state.last_time = now;
    if (!(delta > 0.0f) || delta > 0.25f) delta = 1.0f / 60.0f;

    mg_fsr_dispatch_t desc{};
    desc.abi_version = MG_FSR_ABI_VERSION;
    desc.struct_size = sizeof(desc);
    desc.frame_id = ++state.frame_id;
    desc.session_id = 0;
    desc.context_id = context_key;
    desc.surface_id = 1;
    desc.resource_generation = 1;
    desc.color = {state.color, MG_FSR_NS_GL_FRONTEND, GL_TEXTURE_2D, 0, 0, GL_RGBA8, 0, 1};
    desc.depth = {state.depth, MG_FSR_NS_GL_FRONTEND, GL_TEXTURE_2D, 0, 0, GL_R32F, 0, 1};
    desc.motion = {state.motion, MG_FSR_NS_GL_FRONTEND, GL_TEXTURE_2D, 0, 0, GL_RG16F, 0, 1};
    desc.render_width = static_cast<uint32_t>(state.render_width);
    desc.render_height = static_cast<uint32_t>(state.render_height);
    desc.output_width = static_cast<uint32_t>(width);
    desc.output_height = static_cast<uint32_t>(height);
    desc.color_active = {0, 0, static_cast<uint32_t>(state.render_width), static_cast<uint32_t>(state.render_height)};
    desc.output_active = {0, 0, static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
    desc.color_space = MG_FSR_COLOR_LDR_PERCEPTUAL;
    desc.output_alpha_mode = 0;
    desc.pre_exposure = 1.0f;
    desc.depth_valid = 1;
    desc.camera_near = 0.1f;
    desc.camera_far = 1000.0f;
    desc.camera_fov_y_radians = 1.0471975511965976f;
    desc.jitter_x = 0.0f;
    desc.jitter_y = 0.0f;
    desc.motion_valid = 1;
    desc.motion_confidence = MG_FSR_MOTION_NONE;
    desc.frame_delta_seconds = delta;
    // No temporal inputs exist. Resetting each frame prevents unbounded history
    // smearing from constant depth/zero motion; this is an experimental
    // composition, not a supported temporal-quality path.
    desc.reset_history = 1;
    desc.reset_reason = MG_FSR_RESET_SESSION;

    const int rc = mg_fsr_dispatch(state.session, &desc);
    if (rc != MG_FSR_OK) {
        if (!state.warned) {
            state.warned = true;
            LOG_W_FORCE("FSR3 direct: dispatch failed (%s); leaving the frame untouched", mg_fsr_last_error(state.session));
        }
        g_in_present = false;
        return;
    }

    static bool logged_active = false;
    if (!logged_active) {
        logged_active = true;
        LOG_W_FORCE("FSR3 direct: experimental presentation %dx%d -> %dx%d; synthetic depth/motion, no scene-work claim",
                    state.render_width, state.render_height, width, height);
    }

    mg_fsr_resource_t output{};
    if (mg_fsr_get_output(state.session, &output) != MG_FSR_OK || output.handle == 0) {
        g_in_present = false;
        return;
    }
    GLES.glBindFramebuffer(GL_READ_FRAMEBUFFER, state.output_fbo);
    if (state.attached_output != static_cast<GLuint>(output.handle)) {
        GLES.glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                    static_cast<GLuint>(output.handle), 0);
        state.attached_output = static_cast<GLuint>(output.handle);
    }
    GLES.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    GLES.glViewport(0, 0, width, height);
    GLES.glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    g_in_present = false;
}

void mg_fsr3_forget_context(unsigned long long ctx_id) {
    std::lock_guard<std::mutex> lock(g_mutex);
    auto it = g_states.find(ctx_id);
    if (it == g_states.end()) return;
    if (g_current_ctx && g_current_ctx->id == ctx_id) {
        destroy_resources(it->second);
    } else {
        // The owning context is gone; free CPU/session ownership without
        // deleting similarly numbered objects in whichever context replaced it.
        mg_fsr_abandon_session(it->second.session);
    }
    g_states.erase(it);
}
