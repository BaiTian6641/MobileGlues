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
#include "fsr3_motion_math.h"
#include "../config/settings.h"
#include "../egl/context.h"
#include "../gles/gles.h"
#include "../gl/log.h"
#include "../gl/FSR3Direct.h"
#include "../gl/drawing.h"
#include "../gl/diagnostic.h"
#include "fsr3/fsr3_backend.h"

#include <EGL/egl.h>

#include <jni.h>

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <chrono>
#include <cmath>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

struct JniState {
    void* session = nullptr;
    GLuint motion = 0;
    GLuint color_input = 0;
    GLuint depth_synth = 0;
    GLuint input_fbo = 0;
    GLuint output_fbo = 0;
    GLuint attached_output = 0;
    GLuint motion_program = 0;
    GLint u_inv_vp = -1;
    GLint u_prev_vp = -1;
    GLint u_size = -1;
    GLint u_depth = -1;
    GLint u_camera_delta = -1;
    GLint u_has_prev = -1;
    bool motion_warned = false;
    bool motion_compile_failed = false;
    bool output_logged = false;
    bool motion_logged = false;
    int width = 0;
    int height = 0;
    int render_width = 0;
    int render_height = 0;
    uint64_t frame = 0;
    bool failed = false;
    bool warned = false;
    std::chrono::steady_clock::time_point last{};
};

std::mutex g_mutex;
std::unordered_map<unsigned long long, JniState> g_states;

struct CameraState {
    bool valid = false;
    bool has_previous = false;
    float model_view[16]{};
    float projection[16]{};
    double position[3]{};
    float previous_model_view[16]{};
    float previous_projection[16]{};
    double previous_position[3]{};
    std::chrono::steady_clock::time_point last_capture{};
};
CameraState g_camera;
std::mutex g_camera_mutex;

std::atomic<bool> g_bridge_failed_logged{false};
void bridge_fail(const char* why) {
    if (!g_bridge_failed_logged.exchange(true)) LOG_W_FORCE("FSR3 bridge: %s", why);
}

bool texture_size_2d(GLuint texture, int& width, int& height) {
    if (texture == 0) return false;
    GLint previous = 0;
    GLES.glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous);
    GLES.glBindTexture(GL_TEXTURE_2D, texture);
    GLint w = 0, h = 0;
    GLES.glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &w);
    GLES.glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &h);
    GLES.glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previous));
    if (w <= 0 || h <= 0) return false;
    width = w;
    height = h;
    return true;
}

struct FramebufferGuard {
    GLint read = 0;
    GLint draw = 0;
    GLint viewport[4]{};
    GLint scissor[4]{};
    GLint scissor_test = 0;
    GLboolean color_mask[4]{};
    bool active = false;

    FramebufferGuard() {
        GLES.glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read);
        GLES.glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw);
        GLES.glGetIntegerv(GL_VIEWPORT, viewport);
        GLES.glGetIntegerv(GL_SCISSOR_BOX, scissor);
        GLES.glGetIntegerv(GL_SCISSOR_TEST, &scissor_test);
        GLES.glGetBooleanv(GL_COLOR_WRITEMASK, color_mask);
        active = (read != 0 || draw != 0);
        // Blits are geometry, so the caller's scissor and color mask apply to
        // them. The final pass may leave a sub-rect scissor active; without this
        // guard FSR3 writes only that rectangle and the untouched edge appears
        // as a one-frame black band.
        if (scissor_test) GLES.glDisable(GL_SCISSOR_TEST);
        GLES.glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    }
    ~FramebufferGuard() {
        if (!active) {
            if (scissor_test) GLES.glEnable(GL_SCISSOR_TEST);
            GLES.glColorMask(color_mask[0], color_mask[1], color_mask[2], color_mask[3]);
            return;
        }
        GLES.glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(read));
        GLES.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(draw));
        GLES.glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
        GLES.glScissor(scissor[0], scissor[1], scissor[2], scissor[3]);
        if (scissor_test) GLES.glEnable(GL_SCISSOR_TEST); else GLES.glDisable(GL_SCISSOR_TEST);
        GLES.glColorMask(color_mask[0], color_mask[1], color_mask[2], color_mask[3]);
    }
};

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

const char* kMotionShader = R"(#version 320 es
layout(local_size_x = 8, local_size_y = 8) in;
layout(rgba16f, binding = 0) uniform writeonly highp image2D uMotion;
layout(r32f, binding = 1) uniform writeonly highp image2D uDepthOut;
layout(binding = 0) uniform highp sampler2D uDepth;
uniform highp mat4 uInvVP;
uniform highp mat4 uPrevVP;
uniform highp vec2 uSize;
uniform highp vec3 uCameraDelta;
uniform int uHasPrev;
void main() {
    ivec2 p = ivec2(gl_GlobalInvocationID.xy);
    if (p.x >= int(uSize.x) || p.y >= int(uSize.y)) return;
    float d = texelFetch(uDepth, p, 0).x;
    imageStore(uDepthOut, p, vec4(d, 0.0, 0.0, 1.0));
    if (uHasPrev == 0) {
        imageStore(uMotion, p, vec4(0.0));
        return;
    }
    vec2 uv = (vec2(p) + 0.5) / uSize;
    vec4 clip = uInvVP * vec4(uv * 2.0 - 1.0, d * 2.0 - 1.0, 1.0);
    if (abs(clip.w) < 1e-7) {
        imageStore(uMotion, p, vec4(0.0));
        return;
    }
    vec3 world = clip.xyz / clip.w;
    vec4 prev = uPrevVP * vec4(world + uCameraDelta, 1.0);
    if (abs(prev.w) < 1e-7) {
        imageStore(uMotion, p, vec4(0.0));
        return;
    }
    vec2 prev_uv = prev.xy / prev.w * 0.5 + 0.5;
    imageStore(uMotion, p, vec4((prev_uv - uv) * uSize, 0.0, 1.0));
}
)";

bool ensure_motion_program(JniState& s) {
    if (s.motion_program) return true;
    if (s.motion_compile_failed) return false;
    if (!GLES.glCreateShader || !GLES.glCreateProgram) return false;
    GLuint shader = GLES.glCreateShader(GL_COMPUTE_SHADER);
    if (!shader) return false;
    GLES.glShaderSource(shader, 1, &kMotionShader, nullptr);
    GLES.glCompileShader(shader);
    GLint ok = GL_FALSE;
    GLES.glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        if (!s.motion_warned) {
            s.motion_warned = true;
            char log[512] = {};
            GLES.glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
            LOG_W_FORCE("FSR3 bridge: camera-motion shader failed: %s", log);
        }
        GLES.glDeleteShader(shader);
        s.motion_compile_failed = true;
        return false;
    }
    GLuint program = GLES.glCreateProgram();
    GLES.glAttachShader(program, shader);
    GLES.glLinkProgram(program);
    GLES.glDeleteShader(shader);
    GLES.glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        if (!s.motion_warned) {
            s.motion_warned = true;
            char log[512] = {};
            GLES.glGetProgramInfoLog(program, sizeof(log), nullptr, log);
            LOG_W_FORCE("FSR3 bridge: camera-motion link failed: %s", log);
        }
        GLES.glDeleteProgram(program);
        s.motion_compile_failed = true;
        return false;
    }
    s.motion_program = program;
    s.u_inv_vp = GLES.glGetUniformLocation(program, "uInvVP");
    s.u_prev_vp = GLES.glGetUniformLocation(program, "uPrevVP");
    s.u_size = GLES.glGetUniformLocation(program, "uSize");
    s.u_depth = GLES.glGetUniformLocation(program, "uDepth");
    s.u_camera_delta = GLES.glGetUniformLocation(program, "uCameraDelta");
    s.u_has_prev = GLES.glGetUniformLocation(program, "uHasPrev");
    return s.u_inv_vp >= 0 && s.u_prev_vp >= 0 && s.u_size >= 0 && s.u_depth >= 0 &&
           s.u_camera_delta >= 0 && s.u_has_prev >= 0;
}

struct ImageUnitState {
    GLint name = 0, level = 0, layered = 0, layer = 0, access = GL_READ_ONLY, format = GL_RGBA8;
};
struct MotionGuard {
    GLint program = 0;
    GLint active = GL_TEXTURE0;
    GLint texture = 0;
    ImageUnitState images[2];
    bool image_valid[2]{};
    MotionGuard() {
        GLES.glGetIntegerv(GL_CURRENT_PROGRAM, &program);
        GLES.glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
        GLES.glActiveTexture(GL_TEXTURE0);
        GLES.glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
        if (GLES.glGetIntegeri_v) {
            for (int i = 0; i < 2; ++i) {
                GLint v = 0;
                GLES.glGetIntegeri_v(GL_IMAGE_BINDING_NAME, i, &v); images[i].name = v;
                GLES.glGetIntegeri_v(GL_IMAGE_BINDING_LEVEL, i, &v); images[i].level = v;
                GLES.glGetIntegeri_v(GL_IMAGE_BINDING_LAYERED, i, &v); images[i].layered = v;
                GLES.glGetIntegeri_v(GL_IMAGE_BINDING_LAYER, i, &v); images[i].layer = v;
                GLES.glGetIntegeri_v(GL_IMAGE_BINDING_ACCESS, i, &v); images[i].access = v;
                GLES.glGetIntegeri_v(GL_IMAGE_BINDING_FORMAT, i, &v); images[i].format = v;
                image_valid[i] = true;
            }
        }
    }
    ~MotionGuard() {
        if (GLES.glBindImageTexture) {
            for (int i = 0; i < 2; ++i) {
                if (!image_valid[i]) continue;
                GLES.glBindImageTexture(static_cast<GLuint>(i), static_cast<GLuint>(images[i].name),
                                        images[i].level, images[i].layered ? GL_TRUE : GL_FALSE,
                                        images[i].layer, static_cast<GLenum>(images[i].access),
                                        static_cast<GLenum>(images[i].format));
            }
        }
        mg_invalidate_image_binding_shadow();
        GLES.glActiveTexture(GL_TEXTURE0);
        GLES.glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texture));
        GLES.glActiveTexture(static_cast<GLenum>(active));
        GLES.glUseProgram(static_cast<GLuint>(program));
    }
};

bool run_camera_motion(JniState& s, GLuint depth, int width, int height, bool allow_motion, bool& motion_active) {
    motion_active = false;
    CameraState camera;
    bool has_prev = false;
    {
        std::lock_guard<std::mutex> lock(g_camera_mutex);
        if (g_camera.valid) {
            camera = g_camera;
            has_prev = g_camera.has_previous;
        }
    }
    if (!depth || width <= 0 || height <= 0 || !ensure_motion_program(s)) return false;

    const float identity[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    float current_vp[16], previous_vp[16], inverse_vp[16];
    float camera_delta[3] = {0, 0, 0};
    if (has_prev && allow_motion) {
        mg_fsr3_motion::multiply(camera.projection, camera.model_view, current_vp);
        mg_fsr3_motion::multiply(camera.previous_projection, camera.previous_model_view, previous_vp);
        if (!mg_fsr3_motion::invert(current_vp, inverse_vp)) return false;
        camera_delta[0] = static_cast<float>(camera.position[0] - camera.previous_position[0]);
        camera_delta[1] = static_cast<float>(camera.position[1] - camera.previous_position[1]);
        camera_delta[2] = static_cast<float>(camera.position[2] - camera.previous_position[2]);
    } else {
        has_prev = false;
        std::copy(std::begin(identity), std::end(identity), std::begin(inverse_vp));
        std::copy(std::begin(identity), std::end(identity), std::begin(previous_vp));
    }

    MotionGuard guard;
    GLES.glUseProgram(s.motion_program);
    GLES.glUniformMatrix4fv(s.u_inv_vp, 1, GL_FALSE, inverse_vp);
    GLES.glUniformMatrix4fv(s.u_prev_vp, 1, GL_FALSE, previous_vp);
    GLES.glUniform2f(s.u_size, static_cast<float>(width), static_cast<float>(height));
    GLES.glUniform3f(s.u_camera_delta, camera_delta[0], camera_delta[1], camera_delta[2]);
    GLES.glUniform1i(s.u_has_prev, has_prev ? 1 : 0);
    GLES.glActiveTexture(GL_TEXTURE0);
    GLES.glBindTexture(GL_TEXTURE_2D, depth);
    GLES.glUniform1i(s.u_depth, 0);
    GLES.glBindImageTexture(0, s.motion, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
    GLES.glBindImageTexture(1, s.depth_synth, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);
    GLES.glDispatchCompute(static_cast<GLuint>((width + 7) / 8), static_cast<GLuint>((height + 7) / 8), 1);
    GLES.glMemoryBarrier(GL_ALL_BARRIER_BITS);
    motion_active = has_prev;
    return GLES.glGetError() == GL_NO_ERROR;
}


bool make_aux(JniState& s, int w, int h, int rw, int rh) {
    if (s.motion) GLES.glDeleteTextures(1, &s.motion);
    if (s.color_input) GLES.glDeleteTextures(1, &s.color_input);
    if (s.depth_synth) GLES.glDeleteTextures(1, &s.depth_synth);
    if (s.input_fbo) GLES.glDeleteFramebuffers(1, &s.input_fbo);
    s.motion = s.color_input = s.depth_synth = 0;
    s.input_fbo = 0;
    GLES.glGenTextures(1, &s.motion);
    GLES.glGenTextures(1, &s.color_input);
    GLES.glGenTextures(1, &s.depth_synth);
    if (!s.motion || !s.color_input || !s.depth_synth) return false;
    auto setup = [&](GLuint tex, GLint internal) {
        GLES.glBindTexture(GL_TEXTURE_2D, tex);
        GLES.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        GLES.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        GLES.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        GLES.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        GLES.glTexStorage2D(GL_TEXTURE_2D, 1, internal, rw, rh);
    };
    setup(s.motion, GL_RGBA16F);
    setup(s.color_input, GL_RGBA8);
    setup(s.depth_synth, GL_R32F);
    std::vector<float> depth(static_cast<size_t>(rw) * rh, 0.5f);
    std::vector<float> zero(static_cast<size_t>(rw) * rh * 4, 0.0f);
    GLES.glBindTexture(GL_TEXTURE_2D, s.depth_synth);
    GLES.glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, rw, rh, GL_RED, GL_FLOAT, depth.data());
    GLES.glBindTexture(GL_TEXTURE_2D, s.motion);
    GLES.glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, rw, rh, GL_RGBA, GL_FLOAT, zero.data());
    GLES.glBindTexture(GL_TEXTURE_2D, 0);
    GLES.glGenFramebuffers(1, &s.input_fbo);
    GLES.glBindFramebuffer(GL_FRAMEBUFFER, s.input_fbo);
    GLES.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s.color_input, 0);
    const bool ok = GLES.glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    GLES.glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return ok && GLES.glGetError() == GL_NO_ERROR;
}

void destroy_state(JniState& s) {
    if (s.session) mg_fsr_destroy_session(s.session);
    s.session = nullptr;
    if (s.motion) GLES.glDeleteTextures(1, &s.motion);
    if (s.color_input) GLES.glDeleteTextures(1, &s.color_input);
    if (s.depth_synth) GLES.glDeleteTextures(1, &s.depth_synth);
    if (s.input_fbo) GLES.glDeleteFramebuffers(1, &s.input_fbo);
    if (s.output_fbo) GLES.glDeleteFramebuffers(1, &s.output_fbo);
    s.motion = s.color_input = s.depth_synth = 0;
    s.input_fbo = s.output_fbo = s.attached_output = 0;
    s.width = s.height = 0;
    s.render_width = s.render_height = 0;
    s.failed = false;
}

} // namespace

extern "C" JNIEXPORT void JNICALL
Java_com_googlebook_fsrbridge_Fsr3Bridge_nativeBridgeMode(JNIEnv*, jclass) {
    FSR3SetBridgeMode();
}

extern "C" JNIEXPORT jfloat JNICALL
Java_com_googlebook_fsrbridge_Fsr3Bridge_nativeScale(JNIEnv*, jclass) {
    if (global_settings.fsr3_setting != FSR3_Mode::Experimental) return 1.0f;
    return fsr3_quality_scale(global_settings.fsr3_quality);
}

extern "C" JNIEXPORT void JNICALL
Java_com_googlebook_fsrbridge_Fsr3Bridge_nativeViewport(JNIEnv*, jclass) {
    if (global_settings.fsr3_setting != FSR3_Mode::Experimental) return;
    FSR3NativeViewport();
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_googlebook_fsrbridge_Fsr3Bridge_nativeAvailable(JNIEnv*, jclass) {
    const bool available = global_settings.fsr3_setting == FSR3_Mode::Experimental &&
                           GLES.glDispatchCompute && GLES.glBindImageTexture;
    if (available) FSR3SetBridgeMode();
    return available ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_com_googlebook_fsrbridge_Fsr3Bridge_nativeCameraInvalid(JNIEnv*, jclass) {
    std::lock_guard<std::mutex> lock(g_camera_mutex);
    g_camera.valid = false;
    g_camera.has_previous = false;
}

extern "C" JNIEXPORT void JNICALL
Java_com_googlebook_fsrbridge_Fsr3Bridge_nativeCamera(JNIEnv* env, jclass, jfloatArray mv, jfloatArray proj,
                                                       jdoubleArray position) {
    if (!mv || !proj || !position) return;
    if (env->GetArrayLength(mv) < 16 || env->GetArrayLength(proj) < 16 ||
        env->GetArrayLength(position) < 3) return;
    float current_mv[16], current_proj[16];
    double current_pos[3];
    env->GetFloatArrayRegion(mv, 0, 16, current_mv);
    env->GetFloatArrayRegion(proj, 0, 16, current_proj);
    env->GetDoubleArrayRegion(position, 0, 3, current_pos);
    std::lock_guard<std::mutex> lock(g_camera_mutex);
    const auto now = std::chrono::steady_clock::now();
    const bool time_gap = g_camera.valid && g_camera.last_capture.time_since_epoch().count() != 0 &&
                          std::chrono::duration<float>(now - g_camera.last_capture).count() > 0.25f;
    bool keep_previous = g_camera.valid && !time_gap;
    if (g_camera.valid) {
        const double dx = current_pos[0] - g_camera.position[0];
        const double dy = current_pos[1] - g_camera.position[1];
        const double dz = current_pos[2] - g_camera.position[2];
        if (dx * dx + dy * dy + dz * dz > 64.0 * 64.0) keep_previous = false;
        std::copy(std::begin(g_camera.model_view), std::end(g_camera.model_view), std::begin(g_camera.previous_model_view));
        std::copy(std::begin(g_camera.projection), std::end(g_camera.projection), std::begin(g_camera.previous_projection));
        std::copy(std::begin(g_camera.position), std::end(g_camera.position), std::begin(g_camera.previous_position));
    }
    g_camera.has_previous = keep_previous;
    std::copy(std::begin(current_mv), std::end(current_mv), std::begin(g_camera.model_view));
    std::copy(std::begin(current_proj), std::end(current_proj), std::begin(g_camera.projection));
    std::copy(std::begin(current_pos), std::end(current_pos), std::begin(g_camera.position));
    g_camera.last_capture = now;
    g_camera.valid = true;
}

extern "C" JNIEXPORT jint JNICALL
Java_com_googlebook_fsrbridge_Fsr3Bridge_nativeSubmit(JNIEnv*, jclass) {
    if (global_settings.fsr3_setting != FSR3_Mode::Experimental) return 0;
    if (!GLES.glDispatchCompute || !GLES.glBindImageTexture) return -1;
    const unsigned long long context_key = g_current_ctx ? g_current_ctx->id : 1;
    FramebufferGuard restore_guard;

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
    int depth_width = 0, depth_height = 0;
    bool have_depth = false;
    const GLuint bridge_depth = FSR3BridgeDepthTexture();
    if (bridge_depth != 0) {
        have_depth = texture_size_2d(bridge_depth, depth_width, depth_height);
        depth = bridge_depth;
    } else {
        have_depth = attachment_texture(GL_DEPTH_ATTACHMENT, depth, depth_width, depth_height) &&
                     depth_width == width && depth_height == height;
    }

    if (!have_depth) {
        bridge_fail("no depth texture attachment");
        return -1;
    }
    FSR3SetNativeExtent(width, height);

    std::lock_guard<std::mutex> lock(g_mutex);
    JniState& s = g_states[context_key];
    // Scene targets are allocated at the quality extent. Build matching FSR3
    // color/depth inputs from the final image and the scene sub-rect depth; the
    // output remains native so HUD/final composition stay sharp.
    const float scene_scale = fsr3_quality_scale(global_settings.fsr3_quality);
    const int rw = std::max(64, static_cast<int>(std::lround(width / scene_scale)));
    const int rh = std::max(64, static_cast<int>(std::lround(height / scene_scale)));
    if (!s.session || s.width != width || s.height != height ||
        s.render_width != rw || s.render_height != rh) {
        if (s.session) destroy_state(s);
        s.width = width;
        s.height = height;
        s.render_width = rw;
        s.render_height = rh;
        if (!make_aux(s, width, height, rw, rh)) {
            destroy_state(s);
            bridge_fail("FSR bridge aux allocation failed");
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

    // Downsample the pre-HUD final image into the scene color extent. The Iris
    // targets were rendered at this extent; the final pass is native.
    GLES.glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(fbo));
    GLES.glReadBuffer(GL_COLOR_ATTACHMENT0);
    GLES.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, s.input_fbo);
    GLES.glBlitFramebuffer(0, 0, width, height, 0, 0, rw, rh, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    GLES.glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(fbo));

    // Stable spatial profile: advertise no motion when history is off. The
    // backend always samples the motion texture, so zero it via uHasPrev=0
    // instead of feeding camera-only vectors for hands/sky.
    static const bool history_enabled = std::getenv("MG_FSR3_HISTORY") != nullptr;
    bool motion_active = false;
    const bool depth_input_ready = run_camera_motion(s, depth, rw, rh, history_enabled, motion_active);
    const int input_w = depth_input_ready ? rw : width;
    const int input_h = depth_input_ready ? rh : height;
    const GLuint color_input = depth_input_ready ? s.color_input : color;
    const GLuint depth_input = depth_input_ready ? s.depth_synth : depth;
    const GLenum depth_format = depth_input_ready ? GL_R32F : GL_DEPTH_COMPONENT24;

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
    desc.color = {color_input, MG_FSR_NS_GL_FRONTEND, GL_TEXTURE_2D, 0, 0, GL_RGBA8, 0, 1};
    desc.depth = {depth_input, MG_FSR_NS_GL_FRONTEND, GL_TEXTURE_2D, 0, 0, depth_format, 0, 1};
    desc.motion = {s.motion, MG_FSR_NS_GL_FRONTEND, GL_TEXTURE_2D, 0, 0, GL_RGBA16F, 0, 1};
    desc.render_width = static_cast<uint32_t>(input_w);
    desc.render_height = static_cast<uint32_t>(input_h);
    desc.output_width = static_cast<uint32_t>(width);
    desc.output_height = static_cast<uint32_t>(height);
    desc.color_active = {0, 0, static_cast<uint32_t>(input_w), static_cast<uint32_t>(input_h)};
    desc.output_active = {0, 0, static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
    desc.color_space = MG_FSR_COLOR_LDR_PERCEPTUAL;
    desc.pre_exposure = 1.0f;
    desc.depth_valid = 1;
    desc.camera_near = 0.1f;
    desc.camera_far = 1000.0f;
    desc.camera_fov_y_radians = 1.0471975511965976f;
    desc.motion_valid = history_enabled ? 1 : 0;
    desc.motion_confidence = motion_active ? MG_FSR_MOTION_CAMERA_ONLY : MG_FSR_MOTION_NONE;
    desc.frame_delta_seconds = delta;
    // Camera-only motion is experimental: keep history only when reprojection
    // actually ran; otherwise reset every frame to avoid unbounded ghosting from
    // the zero-motion fallback.
    // Temporal history stays off until scene jitter, depth parameters and the
    // pack TAA boundary are owned coherently. MG_FSR3_HISTORY is a developer
    // opt-in for that later experiment.
    desc.reset_history = (s.frame <= 1 || !motion_active || !history_enabled) ? 1 : 0;
    desc.reset_reason = desc.reset_history ? MG_FSR_RESET_SESSION : MG_FSR_RESET_NONE;

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
        int sw = 0, sh = 0;
        FSR3GetSurfaceSize(sw, sh);
        LOG_W_FORCE("FSR3 bridge: experimental submit %dx%d -> %dx%d (render %dx%d, surface %dx%d); camera_motion=%d, Iris scene adapter",
                      rw, rh, width, height, rw, rh, sw, sh, motion_active ? 1 : 0);
    }
    mg_fsr_resource_t out{};
    if (mg_fsr_get_output(s.session, &out) != MG_FSR_OK || out.handle == 0) {
        s.failed = true;
        bridge_fail("no FSR output");
        return -1;
    }
    if (!s.output_logged) {
        s.output_logged = true;
        GLint prev_unit = 0, prev_tex = 0, ow = 0, oh = 0;
        GLES.glGetIntegerv(GL_ACTIVE_TEXTURE, &prev_unit);
        GLES.glActiveTexture(GL_TEXTURE0);
        GLES.glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev_tex);
        GLES.glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(out.handle));
        GLES.glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &ow);
        GLES.glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &oh);
        GLES.glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(prev_tex));
        GLES.glActiveTexture(static_cast<GLenum>(prev_unit));
        LOG_W_FORCE("FSR3 bridge: output %dx%d frame=%llu motion=%d", ow, oh,
                    static_cast<unsigned long long>(s.frame), motion_active ? 1 : 0);
    }
    if (motion_active && !s.motion_logged) {
        s.motion_logged = true;
        LOG_W_FORCE("FSR3 bridge: camera motion active at frame %llu", static_cast<unsigned long long>(s.frame));
    }
    if (!s.output_fbo) GLES.glGenFramebuffers(1, &s.output_fbo);
    GLES.glBindFramebuffer(GL_READ_FRAMEBUFFER, s.output_fbo);
    if (s.attached_output != out.handle) {
        GLES.glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                    static_cast<GLuint>(out.handle), 0);
        s.attached_output = static_cast<GLuint>(out.handle);
    }
    GLES.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(fbo));
    GLES.glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    FSR3MarkBridgePresent();
    return 0;
}


extern "C" JNIEXPORT void JNICALL
Java_com_googlebook_fsrbridge_Fsr3Bridge_nativeSetSceneDepth(JNIEnv*, jclass, jint texture) {
    FSR3SetBridgeDepthTexture(static_cast<unsigned int>(texture));
}

extern "C" JNIEXPORT void JNICALL
Java_com_googlebook_fsrbridge_Fsr3Bridge_nativeFinalScope(JNIEnv*, jclass, jboolean active) {
    FSR3SetFinalScope(active == JNI_TRUE);
}

#ifdef MG_FSR3_DIAGNOSTICS
namespace {

const char* fsr3_quality_name(FSR3_Quality_Preset preset) {
    switch (preset) {
    case FSR3_Quality_Preset::NativeAA: return "NativeAA";
    case FSR3_Quality_Preset::Quality: return "Quality";
    case FSR3_Quality_Preset::Balanced: return "Balanced";
    case FSR3_Quality_Preset::Performance: return "Performance";
    case FSR3_Quality_Preset::UltraPerformance: return "UltraPerformance";
    default: return "unknown";
    }
}

const char* fsr3_setting_name(FSR3_Mode mode) {
    switch (mode) {
    case FSR3_Mode::Disabled: return "Disabled";
    case FSR3_Mode::Experimental: return "Experimental";
    default: return "unknown";
    }
}

std::string trace_config_json() {
    const bool history = std::getenv("MG_FSR3_HISTORY") != nullptr;
    int surface_w = 0, surface_h = 0;
    FSR3GetSurfaceSize(surface_w, surface_h);
    std::string out = "{";
    bool first = true;
    auto field = [&](const char* name) {
        if (!first) out.push_back(',');
        first = false;
        out.push_back('"');
        out += name;
        out += "\":";
    };
    field("fsr_mode");
    out += "\"";
    out += fsr3_setting_name(global_settings.fsr3_setting);
    out += "\"";
    field("requested_preset");
    out += "\"";
    out += fsr3_quality_name(global_settings.fsr3_quality);
    out += "\"";
    field("effective_preset");
    out += "\"";
    out += fsr3_quality_name(global_settings.fsr3_quality);
    out += "\"";
    field("scale");
    out += std::to_string(global_settings.fsr3_setting == FSR3_Mode::Experimental
                              ? fsr3_quality_scale(global_settings.fsr3_quality)
                              : 1.0f);
    field("history_env_present");
    out += history ? "true" : "false";
    field("motion");
    out += history ? "\"camera\"" : "\"zero\"";
    field("bridge_mode");
    out += FSR3BridgeMode() ? "true" : "false";
    field("surface_width");
    out += std::to_string(surface_w);
    field("surface_height");
    out += std::to_string(surface_h);
    field("rcas_sharpness");
    out += std::to_string(mg_fsr3::kRcasSharpness);
    out += '}';
    return out;
}

} // namespace

extern "C" JNIEXPORT jstring JNICALL
Java_com_googlebook_fsrbridge_Fsr3Bridge_nativeTrace(JNIEnv* env, jclass, jint op, jint role,
                                                     jint frame, jint event) {
    switch (op) {
    case 0: // enable
        mg_diag_set_enabled(role != 0);
        return nullptr;
    case 1: // role start
        mg_diag_role_start(role, frame, event);
        return nullptr;
    case 2: { // snapshot
        std::string json = mg_diag_snapshot_json();
        return json.empty() ? nullptr : env->NewStringUTF(json.c_str());
    }
    case 3: { // role end
        std::string json = mg_diag_role_end_json();
        return json.empty() ? nullptr : env->NewStringUTF(json.c_str());
    }
    case 4: { // config
        std::string json = trace_config_json();
        return env->NewStringUTF(json.c_str());
    }
    case 5: // reset
        mg_diag_reset();
        return nullptr;
    default:
        return nullptr;
    }
}
#endif // MG_FSR3_DIAGNOSTICS

#endif // __ANDROID__
