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
#include "../gl/FSR3Direct.h"

#include <EGL/egl.h>

#include <jni.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <mutex>
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
    bool motion_warned = false;
    bool motion_compile_failed = false;
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
    float previous_model_view[16]{};
    float previous_projection[16]{};
};
CameraState g_camera;
std::mutex g_camera_mutex;
bool g_camera_warned = false;

static void mat4_multiply(const float a[16], const float b[16], float out[16]) {
    // JOML/Iris matrices are column-major.
    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            out[c * 4 + r] = a[0 * 4 + r] * b[c * 4 + 0] +
                             a[1 * 4 + r] * b[c * 4 + 1] +
                             a[2 * 4 + r] * b[c * 4 + 2] +
                             a[3 * 4 + r] * b[c * 4 + 3];
        }
    }
}

static bool mat4_invert(const float m[16], float out[16]) {
    float inv[16];
    inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] +
             m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
    inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] -
             m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
    inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] +
             m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
    inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] -
              m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
    inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] -
             m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
    inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] +
             m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
    inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] -
             m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
    inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] +
              m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
    inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] +
             m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
    inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] -
             m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
    inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] +
              m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
    inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] -
              m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
    inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] -
             m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
    inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] +
             m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
    inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] -
              m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
    inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] +
              m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];
    const float det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
    if (std::abs(det) < 1e-9f) return false;
    const float inv_det = 1.0f / det;
    for (int i = 0; i < 16; ++i) out[i] = inv[i] * inv_det;
    return true;
}

std::atomic<bool> g_bridge_failed_logged{false};
void bridge_fail(const char* why) {
    if (!g_bridge_failed_logged.exchange(true)) LOG_W_FORCE("FSR3 bridge: %s", why);
}

struct FramebufferGuard {
    GLint read = 0;
    GLint draw = 0;
    GLint viewport[4]{};
    bool active = false;

    FramebufferGuard() {
        GLES.glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read);
        GLES.glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw);
        GLES.glGetIntegerv(GL_VIEWPORT, viewport);
        active = (read != 0 || draw != 0);
    }
    ~FramebufferGuard() {
        if (!active) return;
        GLES.glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(read));
        GLES.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(draw));
        GLES.glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
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

const char* kMotionShader = R"(
#version 320 es
layout(local_size_x = 8, local_size_y = 8) in;
layout(rgba16f, binding = 0) writeonly highp image2D uMotion;
layout(binding = 0) uniform highp sampler2D uDepth;
uniform highp mat4 uInvVP;
uniform highp mat4 uPrevVP;
uniform highp vec2 uSize;
void main() {
    ivec2 p = ivec2(gl_GlobalInvocationID.xy);
    if (p.x >= int(uSize.x) || p.y >= int(uSize.y)) return;
    vec2 uv = (vec2(p) + 0.5) / uSize;
    float d = texelFetch(uDepth, p, 0).x;
    vec4 clip = uInvVP * vec4(uv * 2.0 - 1.0, d * 2.0 - 1.0, 1.0);
    if (abs(clip.w) < 1e-7) return;
    vec3 world = clip.xyz / clip.w;
    vec4 prev = uPrevVP * vec4(world, 1.0);
    if (abs(prev.w) < 1e-7) return;
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
    return s.u_inv_vp >= 0 && s.u_prev_vp >= 0 && s.u_size >= 0 && s.u_depth >= 0;
}

struct MotionGuard {
    GLint program = 0;
    GLint active = GL_TEXTURE0;
    GLint texture = 0;
    MotionGuard() {
        GLES.glGetIntegerv(GL_CURRENT_PROGRAM, &program);
        GLES.glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
        GLES.glActiveTexture(GL_TEXTURE0);
        GLES.glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
    }
    ~MotionGuard() {
        GLES.glBindImageTexture(0, 0, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA16F);
        GLES.glActiveTexture(GL_TEXTURE0);
        GLES.glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texture));
        GLES.glActiveTexture(static_cast<GLenum>(active));
        GLES.glUseProgram(static_cast<GLuint>(program));
    }
};

bool run_camera_motion(JniState& s, GLuint depth, int width, int height) {
    CameraState camera;
    {
        std::lock_guard<std::mutex> lock(g_camera_mutex);
        if (!g_camera.valid || !g_camera.has_previous) return false;
        camera = g_camera;
    }
    if (!depth || width <= 0 || height <= 0 || !ensure_motion_program(s)) return false;
    float current_vp[16], previous_vp[16], inverse_vp[16];
    mat4_multiply(camera.projection, camera.model_view, current_vp);
    mat4_multiply(camera.previous_projection, camera.previous_model_view, previous_vp);
    if (!mat4_invert(current_vp, inverse_vp)) return false;

    MotionGuard guard;
    GLES.glUseProgram(s.motion_program);
    GLES.glUniformMatrix4fv(s.u_inv_vp, 1, GL_FALSE, inverse_vp);
    GLES.glUniformMatrix4fv(s.u_prev_vp, 1, GL_FALSE, previous_vp);
    GLES.glUniform2f(s.u_size, static_cast<float>(width), static_cast<float>(height));
    GLES.glActiveTexture(GL_TEXTURE0);
    GLES.glBindTexture(GL_TEXTURE_2D, depth);
    GLES.glUniform1i(s.u_depth, 0);
    GLES.glBindImageTexture(0, s.motion, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
    GLES.glDispatchCompute(static_cast<GLuint>((width + 7) / 8), static_cast<GLuint>((height + 7) / 8), 1);
    GLES.glMemoryBarrier(GL_ALL_BARRIER_BITS);
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
    return (global_settings.fsr3_setting == FSR3_Mode::Experimental && GLES.glDispatchCompute &&
            GLES.glBindImageTexture)
               ? JNI_TRUE
               : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_com_googlebook_fsrbridge_Fsr3Bridge_nativeCamera(JNIEnv* env, jclass, jfloatArray mv, jfloatArray proj) {
    if (!mv || !proj) return;
    if (env->GetArrayLength(mv) < 16 || env->GetArrayLength(proj) < 16) return;
    float current_mv[16], current_proj[16];
    env->GetFloatArrayRegion(mv, 0, 16, current_mv);
    env->GetFloatArrayRegion(proj, 0, 16, current_proj);
    std::lock_guard<std::mutex> lock(g_camera_mutex);
    if (g_camera.valid) {
        std::copy(std::begin(g_camera.model_view), std::end(g_camera.model_view), std::begin(g_camera.previous_model_view));
        std::copy(std::begin(g_camera.projection), std::end(g_camera.projection), std::begin(g_camera.previous_projection));
        g_camera.has_previous = true;
    }
    std::copy(std::begin(current_mv), std::end(current_mv), std::begin(g_camera.model_view));
    std::copy(std::begin(current_proj), std::end(current_proj), std::begin(g_camera.projection));
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
    const bool have_depth = attachment_texture(GL_DEPTH_ATTACHMENT, depth, depth_width, depth_height) &&
                            depth_width == width && depth_height == height;

    std::lock_guard<std::mutex> lock(g_mutex);
    JniState& s = g_states[context_key];
    // The scene adapter scales Iris RenderTargets. The bridge therefore consumes
    // the native final image without a second downsample; the quality slider
    // belongs to RenderTargetsMixin, not to this final-frame path.
    const int rw = width;
    const int rh = height;
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
    const bool scaled = (rw != width || rh != height);
    if (scaled) {
        GLES.glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(fbo));
        GLES.glReadBuffer(GL_COLOR_ATTACHMENT0);
        GLES.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, s.input_fbo);
        GLES.glBlitFramebuffer(0, 0, width, height, 0, 0, rw, rh, GL_COLOR_BUFFER_BIT, GL_LINEAR);
        GLES.glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(fbo));
    } else if (!have_depth) {
        bridge_fail("no depth texture attachment");
        return -1;
    }

    const bool camera_motion = run_camera_motion(s, scaled ? s.depth_synth : depth, rw, rh);

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
    desc.color = {scaled ? s.color_input : color, MG_FSR_NS_GL_FRONTEND, GL_TEXTURE_2D, 0, 0, GL_RGBA8, 0, 1};
    desc.depth = {scaled ? s.depth_synth : depth, MG_FSR_NS_GL_FRONTEND, GL_TEXTURE_2D, 0, 0,
                  static_cast<GLenum>(scaled ? GL_R32F : GL_DEPTH_COMPONENT24), 0, 1};
    desc.motion = {s.motion, MG_FSR_NS_GL_FRONTEND, GL_TEXTURE_2D, 0, 0, GL_RGBA16F, 0, 1};
    desc.render_width = static_cast<uint32_t>(rw);
    desc.render_height = static_cast<uint32_t>(rh);
    desc.output_width = static_cast<uint32_t>(width);
    desc.output_height = static_cast<uint32_t>(height);
    desc.color_active = {0, 0, static_cast<uint32_t>(rw), static_cast<uint32_t>(rh)};
    desc.output_active = {0, 0, static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
    desc.color_space = MG_FSR_COLOR_LDR_PERCEPTUAL;
    desc.pre_exposure = 1.0f;
    desc.depth_valid = 1;
    desc.camera_near = 0.1f;
    desc.camera_far = 1000.0f;
    desc.camera_fov_y_radians = 1.0471975511965976f;
    desc.motion_valid = 1;
    desc.motion_confidence = camera_motion ? MG_FSR_MOTION_CAMERA_ONLY : MG_FSR_MOTION_NONE;
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
        LOG_W_FORCE("FSR3 bridge: experimental submit %dx%d -> %dx%d (render %dx%d); camera_motion=%d, Iris scene adapter",
                      rw, rh, width, height, rw, rh, camera_motion ? 1 : 0);
    }
    mg_fsr_resource_t out{};
    if (mg_fsr_get_output(s.session, &out) != MG_FSR_OK || out.handle == 0) {
        s.failed = true;
        bridge_fail("no FSR output");
        return -1;
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

#endif // __ANDROID__
