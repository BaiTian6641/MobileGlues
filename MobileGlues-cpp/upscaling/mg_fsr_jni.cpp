// MobileGlues - upscaling/mg_fsr_jni.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header
//
// Optional JNI entry points for the Minecraft/Iris bridge. The bridge passes
// no resources: it calls immediately after Iris' final pass while the main
// framebuffer is current, and this file resolves the current color/depth
// attachments. This keeps the Java side renderer-agnostic and avoids exposing
// Minecraft classes in the native contract.

#if defined(__ANDROID__)

#include "mg_fsr_abi.h"
#include "../config/settings.h"
#include "../egl/context.h"
#include "../gles/gles.h"
#include "../gl/log.h"

#include <EGL/egl.h>

#include <jni.h>

#include <atomic>
#include <chrono>
#include <mutex>
#include <unordered_map>

namespace {

struct JniState {
    void* session = nullptr;
    GLuint motion = 0;
    GLuint output_fbo = 0;
    GLuint attached_output = 0;
    int width = 0;
    int height = 0;
    uint64_t frame = 0;
    bool failed = false;
    bool warned = false;
    std::chrono::steady_clock::time_point last{};
};

std::mutex g_mutex;
std::unordered_map<unsigned long long, JniState> g_states;

std::atomic<bool> g_bridge_failed_logged{false};
void bridge_fail(const char* why) {
    if (!g_bridge_failed_logged.exchange(true)) LOG_W_FORCE("FSR3 bridge: %s", why);
}

bool attachment_texture(GLenum attachment, GLuint& texture, int& width, int& height) {
    GLint type = 0;
    GLES.glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, attachment,
                                               GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &type);
    if (type != GL_TEXTURE) return false;
    GLint name = 0;
    GLES.glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, attachment,
                                               GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &name);
    if (name == 0) return false;
    GLint prev_unit = 0;
    GLES.glGetIntegerv(GL_ACTIVE_TEXTURE, &prev_unit);
    GLES.glActiveTexture(GL_TEXTURE0);
    GLint prev_tex = 0;
    GLES.glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev_tex);
    GLES.glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(name));
    GLint w = 0, h = 0;
    GLES.glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &w);
    GLES.glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &h);
    GLES.glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(prev_tex));
    GLES.glActiveTexture(prev_unit);
    if (w <= 0 || h <= 0) return false;
    texture = static_cast<GLuint>(name);
    width = w;
    height = h;
    return true;
}

bool make_motion(JniState& s, int w, int h) {
    if (s.motion) GLES.glDeleteTextures(1, &s.motion);
    GLES.glGenTextures(1, &s.motion);
    if (!s.motion) return false;
    GLES.glBindTexture(GL_TEXTURE_2D, s.motion);
    GLES.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    GLES.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    GLES.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    GLES.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    GLES.glTexStorage2D(GL_TEXTURE_2D, 1, GL_RG16F, w, h);
    std::vector<float> zero(static_cast<size_t>(w) * h * 2, 0.0f);
    GLES.glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RG, GL_FLOAT, zero.data());
    GLES.glBindTexture(GL_TEXTURE_2D, 0);
    return GLES.glGetError() == GL_NO_ERROR;
}

void destroy_state(JniState& s) {
    if (s.session) mg_fsr_destroy_session(s.session);
    s.session = nullptr;
    if (s.motion) GLES.glDeleteTextures(1, &s.motion);
    if (s.output_fbo) GLES.glDeleteFramebuffers(1, &s.output_fbo);
    s.motion = s.output_fbo = s.attached_output = 0;
    s.width = s.height = 0;
    s.failed = false;
}

} // namespace

extern "C" JNIEXPORT jboolean JNICALL
Java_com_googlebook_fsrbridge_Fsr3Bridge_nativeAvailable(JNIEnv*, jclass) {
    return (global_settings.fsr3_setting == FSR3_Mode::Experimental && GLES.glDispatchCompute &&
            GLES.glBindImageTexture)
               ? JNI_TRUE
               : JNI_FALSE;
}

extern "C" JNIEXPORT jint JNICALL
Java_com_googlebook_fsrbridge_Fsr3Bridge_nativeSubmit(JNIEnv*, jclass) {
    if (global_settings.fsr3_setting != FSR3_Mode::Experimental) return 0;
    if (!GLES.glDispatchCompute || !GLES.glBindImageTexture) return -1;
    const unsigned long long context_key = g_current_ctx ? g_current_ctx->id : 1;

    GLint fbo = 0;
    GLES.glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &fbo);
    if (fbo == 0) { bridge_fail("no draw framebuffer at final pass"); return -1; }
    GLuint color = 0, depth = 0;
    int width = 0, height = 0;
    if (!attachment_texture(GL_COLOR_ATTACHMENT0, color, width, height) ||
        width <= 0 || height <= 0) {
        bridge_fail("no color texture attachment");
        return -1;
    }
    if (!attachment_texture(GL_DEPTH_ATTACHMENT, depth, width, height)) {
        bridge_fail("no depth texture attachment");
        return -1;
    }

    std::lock_guard<std::mutex> lock(g_mutex);
    JniState& s = g_states[context_key];
    if (!s.session || s.width != width || s.height != height) {
        if (s.session) destroy_state(s);
        s.width = width;
        s.height = height;
        if (!make_motion(s, width, height)) {
            destroy_state(s);
            bridge_fail("motion texture allocation failed");
            return -1;
        }
        s.session = mg_fsr_create_session();
        if (!s.session) {
            destroy_state(s);
            bridge_fail("FSR session creation failed");
            return -1;
        }
        s.last = std::chrono::steady_clock::now();
        s.frame = 0;
    }
    if (s.failed) return -1;

    const auto now = std::chrono::steady_clock::now();
    float delta = std::chrono::duration<float>(now - s.last).count();
    s.last = now;
    if (!(delta > 0.0f) || delta > 0.25f) delta = 1.0f / 60.0f;

    mg_fsr_dispatch_t desc{};
    desc.abi_version = MG_FSR_ABI_VERSION;
    desc.struct_size = sizeof(desc);
    desc.frame_id = ++s.frame;
    desc.session_id = 0;
    desc.context_id = 0;
    desc.surface_id = 1;
    desc.resource_generation = 1;
    desc.color = {color, MG_FSR_NS_GL_FRONTEND, GL_TEXTURE_2D, 0, 0, GL_RGBA8, 0, 1};
    desc.depth = {depth, MG_FSR_NS_GL_FRONTEND, GL_TEXTURE_2D, 0, 0, GL_DEPTH_COMPONENT24, 0, 1};
    desc.motion = {s.motion, MG_FSR_NS_GL_FRONTEND, GL_TEXTURE_2D, 0, 0, GL_RG16F, 0, 1};
    desc.render_width = width;
    desc.render_height = height;
    desc.output_width = width;
    desc.output_height = height;
    desc.color_active = {0, 0, static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
    desc.output_active = {0, 0, static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
    desc.color_space = MG_FSR_COLOR_LDR_PERCEPTUAL;
    desc.pre_exposure = 1.0f;
    desc.depth_valid = 1;
    desc.camera_near = 0.1f;
    desc.camera_far = 1000.0f;
    desc.camera_fov_y_radians = 1.0471975511965976f;
    desc.motion_valid = 1;
    desc.motion_confidence = MG_FSR_MOTION_NONE;
    desc.frame_delta_seconds = delta;
    desc.reset_history = 1;
    desc.reset_reason = MG_FSR_RESET_SESSION;

    if (mg_fsr_dispatch(s.session, &desc) != MG_FSR_OK) {
        if (!s.warned) {
            s.warned = true;
            LOG_W_FORCE("FSR3 bridge: dispatch failed (%s)", mg_fsr_last_error(s.session));
        }
        s.failed = true;
        return -1;
    }
    if (!s.warned) {
        s.warned = true;
        LOG_W_FORCE("FSR3 bridge: experimental submit %dx%d; synthetic motion, pack final TAA retained", width, height);
    }
    mg_fsr_resource_t out{};
    if (mg_fsr_get_output(s.session, &out) != MG_FSR_OK || out.handle == 0) {
        s.failed = true;
        bridge_fail("no FSR output");
        return -1;
    }
    GLint prev_read = 0, prev_draw = 0, viewport[4]{};
    GLES.glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prev_read);
    GLES.glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prev_draw);
    GLES.glGetIntegerv(GL_VIEWPORT, viewport);
    if (!s.output_fbo) GLES.glGenFramebuffers(1, &s.output_fbo);
    GLES.glBindFramebuffer(GL_READ_FRAMEBUFFER, s.output_fbo);
    if (s.attached_output != out.handle) {
        GLES.glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                    static_cast<GLuint>(out.handle), 0);
        s.attached_output = static_cast<GLuint>(out.handle);
    }
    GLES.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(fbo));
    GLES.glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    GLES.glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(prev_read));
    GLES.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(prev_draw));
    GLES.glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    return 0;
}

#endif // __ANDROID__
