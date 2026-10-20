// MobileGlues - upscaling/fsr3/fsr3_backend.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header
//
// FSR 3.1.4 upscaler over the existing GLES 3.2 provider. The shaders are the
// adapted MIT-licensed ESSL generated from the pinned AMD source; this file owns
// resources, history, bindings, barriers and state restoration. It deliberately
// contains no CPU readback and no frame-generation path.

#include "fsr3_backend.h"
#include "fsr3_shaders.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <sstream>

#include "../../gles/gles.h"

namespace mg_fsr3 {
namespace {

constexpr int kMaxTextureUnits = 32;
constexpr int kMaxImageUnits = 32;
constexpr int kMaxUboUnits = 16;

struct FsrConsts {
    int32_t renderSize[2], previousFrameRenderSize[2];
    int32_t upscaleSize[2], previousFrameUpscaleSize[2];
    int32_t maxRenderSize[2], maxUpscaleSize[2];
    float deviceToViewDepth[4];
    float jitterOffset[2], previousFrameJitterOffset[2];
    float motionVectorScale[2], downscaleFactor[2];
    float motionVectorJitterCancellation[2];
    float tanHalfFOV, jitterPhaseCount;
    float deltaTime, deltaPreExposure, viewSpaceToMetersFactor, frameIndex;
    float velocityFactor, reactivenessScale, shadingChangeScale;
    float accumulationAddedPerFrame, minDisocclusionAccumulation;
};
static_assert(sizeof(FsrConsts) == 148, "FSR3 constant layout");

struct SpdConsts {
    uint32_t mips;
    uint32_t numWorkGroups;
    uint32_t workGroupOffset[2];
    uint32_t renderSize[2];
    uint32_t pad;
};
static_assert(sizeof(SpdConsts) == 28, "SPD constant layout");

struct RcasConsts {
    uint32_t rcasConfig[4];
};

struct DispatchGuard {
    GLint program = 0;
    GLint active_texture = GL_TEXTURE0;
    GLint draw_fbo = 0;
    GLint read_fbo = 0;
    GLint viewport[4]{};
    GLint scissor[4]{};
    GLint texture_binding[kMaxTextureUnits]{};
    GLint sampler_binding[kMaxTextureUnits]{};
    struct ImageBinding {
        GLint name = 0, level = 0, layered = 0, layer = 0, access = 0, format = 0;
    } images[kMaxImageUnits];
    struct UboBinding {
        GLint buffer = 0, start = 0, size = 0;
    } ubos[kMaxUboUnits];
    GLint generic_ubo = 0;
    GLint blend = 0, depth = 0, stencil = 0, cull = 0, scissor_test = 0;
    GLint depth_mask = 0, depth_func = 0, cull_mode = 0;
    GLint color_mask[4]{};
    GLfloat clear_color[4]{};

    DispatchGuard() {
        GLES.glGetIntegerv(GL_CURRENT_PROGRAM, &program);
        GLES.glGetIntegerv(GL_ACTIVE_TEXTURE, &active_texture);
        GLES.glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw_fbo);
        GLES.glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read_fbo);
        GLES.glGetIntegerv(GL_VIEWPORT, viewport);
        GLES.glGetIntegerv(GL_SCISSOR_BOX, scissor);
        GLES.glGetIntegerv(GL_UNIFORM_BUFFER_BINDING, &generic_ubo);
        GLES.glGetIntegerv(GL_BLEND, &blend);
        GLES.glGetIntegerv(GL_DEPTH_TEST, &depth);
        GLES.glGetIntegerv(GL_STENCIL_TEST, &stencil);
        GLES.glGetIntegerv(GL_CULL_FACE, &cull);
        GLES.glGetIntegerv(GL_SCISSOR_TEST, &scissor_test);
        GLES.glGetIntegerv(GL_DEPTH_WRITEMASK, &depth_mask);
        GLES.glGetIntegerv(GL_DEPTH_FUNC, &depth_func);
        GLES.glGetIntegerv(GL_CULL_FACE_MODE, &cull_mode);
        GLES.glGetBooleanv(GL_COLOR_WRITEMASK, reinterpret_cast<GLboolean*>(color_mask));
        GLES.glGetFloatv(GL_COLOR_CLEAR_VALUE, clear_color);

        for (int i = 0; i < kMaxTextureUnits; ++i) {
            GLES.glActiveTexture(GL_TEXTURE0 + i);
            GLES.glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture_binding[i]);
            GLES.glGetIntegerv(GL_SAMPLER_BINDING, &sampler_binding[i]);
        }
        for (int i = 0; i < kMaxImageUnits; ++i) {
            GLES.glGetIntegeri_v(GL_IMAGE_BINDING_NAME, i, &images[i].name);
            GLES.glGetIntegeri_v(GL_IMAGE_BINDING_LEVEL, i, &images[i].level);
            GLES.glGetIntegeri_v(GL_IMAGE_BINDING_LAYERED, i, &images[i].layered);
            GLES.glGetIntegeri_v(GL_IMAGE_BINDING_LAYER, i, &images[i].layer);
            GLES.glGetIntegeri_v(GL_IMAGE_BINDING_ACCESS, i, &images[i].access);
            GLES.glGetIntegeri_v(GL_IMAGE_BINDING_FORMAT, i, &images[i].format);
        }
        for (int i = 0; i < kMaxUboUnits; ++i) {
            GLES.glGetIntegeri_v(GL_UNIFORM_BUFFER_BINDING, i, &ubos[i].buffer);
            GLES.glGetIntegeri_v(GL_UNIFORM_BUFFER_START, i, &ubos[i].start);
            GLES.glGetIntegeri_v(GL_UNIFORM_BUFFER_SIZE, i, &ubos[i].size);
        }
        GLES.glActiveTexture(active_texture);
    }

    ~DispatchGuard() {
        GLES.glUseProgram(program);
        GLES.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, draw_fbo);
        GLES.glBindFramebuffer(GL_READ_FRAMEBUFFER, read_fbo);
        GLES.glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
        GLES.glScissor(scissor[0], scissor[1], scissor[2], scissor[3]);

        if (blend) GLES.glEnable(GL_BLEND); else GLES.glDisable(GL_BLEND);
        if (depth) GLES.glEnable(GL_DEPTH_TEST); else GLES.glDisable(GL_DEPTH_TEST);
        if (stencil) GLES.glEnable(GL_STENCIL_TEST); else GLES.glDisable(GL_STENCIL_TEST);
        if (cull) GLES.glEnable(GL_CULL_FACE); else GLES.glDisable(GL_CULL_FACE);
        if (scissor_test) GLES.glEnable(GL_SCISSOR_TEST); else GLES.glDisable(GL_SCISSOR_TEST);
        GLES.glDepthMask(static_cast<GLboolean>(depth_mask));
        GLES.glDepthFunc(static_cast<GLenum>(depth_func));
        GLES.glCullFace(static_cast<GLenum>(cull_mode));
        GLES.glColorMask(static_cast<GLboolean>(color_mask[0]), static_cast<GLboolean>(color_mask[1]),
                         static_cast<GLboolean>(color_mask[2]), static_cast<GLboolean>(color_mask[3]));
        GLES.glClearColor(clear_color[0], clear_color[1], clear_color[2], clear_color[3]);

        for (int i = 0; i < kMaxTextureUnits; ++i) {
            GLES.glActiveTexture(GL_TEXTURE0 + i);
            GLES.glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texture_binding[i]));
            GLES.glBindSampler(i, static_cast<GLuint>(sampler_binding[i]));
        }
        GLES.glActiveTexture(active_texture);

        for (int i = 0; i < kMaxImageUnits; ++i) {
            GLES.glBindImageTexture(i, static_cast<GLuint>(images[i].name), images[i].level,
                                    static_cast<GLboolean>(images[i].layered), images[i].layer,
                                    static_cast<GLenum>(images[i].access), static_cast<GLenum>(images[i].format));
        }

        for (int i = 0; i < kMaxUboUnits; ++i) {
            if (ubos[i].buffer == 0) {
                GLES.glBindBufferBase(GL_UNIFORM_BUFFER, i, 0);
            } else {
                GLES.glBindBufferRange(GL_UNIFORM_BUFFER, i, static_cast<GLuint>(ubos[i].buffer), ubos[i].start,
                                       ubos[i].size);
            }
        }
        GLES.glBindBuffer(GL_UNIFORM_BUFFER, static_cast<GLuint>(generic_ubo));
    }
};

struct ClearGuard {
    GLint draw_fbo = 0;
    GLint viewport[4]{};
    GLint scissor[4]{};
    GLint scissor_test = 0;
    GLint color_mask[4]{};
    GLfloat clear_color[4]{};

    ClearGuard() {
        GLES.glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw_fbo);
        GLES.glGetIntegerv(GL_VIEWPORT, viewport);
        GLES.glGetIntegerv(GL_SCISSOR_BOX, scissor);
        GLES.glGetIntegerv(GL_SCISSOR_TEST, &scissor_test);
        GLES.glGetBooleanv(GL_COLOR_WRITEMASK, reinterpret_cast<GLboolean*>(color_mask));
        GLES.glGetFloatv(GL_COLOR_CLEAR_VALUE, clear_color);
    }
    ~ClearGuard() {
        GLES.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, draw_fbo);
        GLES.glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
        GLES.glScissor(scissor[0], scissor[1], scissor[2], scissor[3]);
        if (scissor_test) GLES.glEnable(GL_SCISSOR_TEST); else GLES.glDisable(GL_SCISSOR_TEST);
        GLES.glColorMask(static_cast<GLboolean>(color_mask[0]), static_cast<GLboolean>(color_mask[1]),
                         static_cast<GLboolean>(color_mask[2]), static_cast<GLboolean>(color_mask[3]));
        GLES.glClearColor(clear_color[0], clear_color[1], clear_color[2], clear_color[3]);
    }
};

uint32_t mip_levels_for(int width, int height) {
    int m = std::max(width, height);
    uint32_t n = 1;
    while (m > 1) {
        m >>= 1;
        ++n;
    }
    return n;
}

uint32_t ceil_div(uint32_t a, uint32_t b) { return b == 0 ? 0 : (a + b - 1) / b; }

bool starts_with(const std::string& s, const char* p) { return s.rfind(p, 0) == 0; }
bool ends_with(const std::string& s, const char* p) {
    const size_t n = std::strlen(p);
    return s.size() >= n && s.compare(s.size() - n, n, p) == 0;
}

std::string trim(std::string s) {
    const size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return {};
    const size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

int int_after(const std::string& s, const char* key) {
    const size_t p = s.find(key);
    if (p == std::string::npos) return -1;
    return std::atoi(s.c_str() + p + std::strlen(key));
}

GLenum image_format_from_qualifier(const std::string& q) {
    if (q == "rgba8") return GL_RGBA8;
    if (q == "rgba16f") return GL_RGBA16F;
    if (q == "rgba32f") return GL_RGBA32F;
    if (q == "r32f") return GL_R32F;
    if (q == "r32ui") return GL_R32UI;
    if (q == "r32i") return GL_R32I;
    if (q == "rgba8_snorm") return GL_RGBA8_SNORM;
    return 0;
}

} // namespace


Backend::Backend() = default;
Backend::~Backend() { destroy(); }

void Backend::set_error(std::string message) {
    error_ = std::move(message);
    failed_ = true;
}

bool Backend::clear_texture(const Texture& tex, int level, GLenum format, GLenum type, const void* value) {
    if (tex.texture == 0) return false;
    ClearGuard guard;
    if (!clear_fbo_) return false;
    GLES.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, clear_fbo_);
    GLES.glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex.texture, level);
    const GLenum draw_buffer = GL_COLOR_ATTACHMENT0;
    GLES.glDrawBuffers(1, &draw_buffer);
    if (GLES.glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) return false;
    const int w = std::max(1, tex.width >> level);
    const int h = std::max(1, tex.height >> level);
    GLES.glViewport(0, 0, w, h);
    GLES.glDisable(GL_SCISSOR_TEST);
    GLES.glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    (void)format;
    (void)type;
    GLES.glClearBufferfv(GL_COLOR, 0, reinterpret_cast<const GLfloat*>(value));
    GLES.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, guard.draw_fbo);
    return GLES.glGetError() == GL_NO_ERROR;
}

bool Backend::clear_float(const Texture& tex, int level, float r, float g, float b, float a) {
    const float v[4] = {r, g, b, a};
    return clear_texture(tex, level, GL_RGBA, GL_FLOAT, v);
}

bool Backend::clear_uint(const Texture& tex, int level, uint32_t r) {
    if (tex.texture == 0) return false;
    ClearGuard guard;
    if (!clear_fbo_) return false;
    GLES.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, clear_fbo_);
    GLES.glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex.texture, level);
    const GLenum draw_buffer = GL_COLOR_ATTACHMENT0;
    GLES.glDrawBuffers(1, &draw_buffer);
    if (GLES.glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) return false;
    GLES.glViewport(0, 0, std::max(1, tex.width >> level), std::max(1, tex.height >> level));
    GLES.glDisable(GL_SCISSOR_TEST);
    GLES.glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    const GLuint v[4] = {r, 0, 0, 0};
    GLES.glClearBufferuiv(GL_COLOR, 0, v);
    GLES.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, guard.draw_fbo);
    return GLES.glGetError() == GL_NO_ERROR;
}

bool Backend::create_textures() {
    auto make = [&](Texture& t, GLenum internal, int w, int h, int levels) {
        t.internal = internal;
        t.width = w;
        t.height = h;
        t.levels = levels;
        GLES.glGenTextures(1, &t.texture);
        if (!t.texture) return false;
        GLES.glBindTexture(GL_TEXTURE_2D, t.texture);
        GLES.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, levels > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
        GLES.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        GLES.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        GLES.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        GLES.glTexStorage2D(GL_TEXTURE_2D, levels, internal, w, h);
        GLES.glBindTexture(GL_TEXTURE_2D, 0);
        return GLES.glGetError() == GL_NO_ERROR;
    };

    const int rw = static_cast<int>(render_width_);
    const int rh = static_cast<int>(render_height_);
    const int ow = static_cast<int>(output_width_);
    const int oh = static_cast<int>(output_height_);
    const int hw = std::max(1, rw / 2);
    const int hh = std::max(1, rh / 2);
    // The SDK binds six SPD mip levels (0..5) even when the render extent is
    // small; allocate at least six so every declared image binding is in range.
    const uint32_t spd_levels = std::max<uint32_t>(6, mip_levels_for(hw, hh));

    if (!make(input_exposure_, GL_R32F, 1, 1, 1)) return false;
    if (!make(input_reactive_, GL_R8, 1, 1, 1)) return false;
    if (!make(input_transparency_, GL_R8, 1, 1, 1)) return false;
    if (!output_owned_) {
        if (!make(output_, GL_RGBA8, ow, oh, 1)) return false;
        output_owned_ = true;
    }
    if (!make(acc_[0], GL_RGBA8, rw, rh, 1) || !make(acc_[1], GL_RGBA8, rw, rh, 1)) return false;
    if (!make(luma_[0], GL_RGBA16F, rw, rh, 1) || !make(luma_[1], GL_RGBA16F, rw, rh, 1)) return false;
    if (!make(intermediate_, GL_RGBA16F, rw, rh, 1)) return false;
    if (!make(lh_[0], GL_RGBA16F, rw, rh, 1) || !make(lh_[1], GL_RGBA16F, rw, rh, 1)) return false;
    if (!make(iuc_[0], GL_RGBA16F, ow, oh, 1) || !make(iuc_[1], GL_RGBA16F, ow, oh, 1)) return false;
    if (!make(new_locks_, GL_RGBA8, ow, oh, 1)) return false;
    if (!make(dilated_reactive_, GL_RGBA8, rw, rh, 1)) return false;
    if (!make(shading_change_, GL_RGBA8, hw, hh, 1)) return false;
    if (!make(spd_mips_, GL_RGBA16F, hw, hh, static_cast<int>(spd_levels))) return false;
    if (!make(farthest_mip1_, GL_RGBA16F, hw, hh, 1)) return false;
    if (!make(spd_atomic_, GL_R32UI, 1, 1, 1)) return false;
    if (!make(frame_info_, GL_RGBA32F, 1, 1, 1)) return false;
    if (!make(dilated_motion_, GL_RGBA16F, rw, rh, 1)) return false;
    if (!make(dilated_depth_, GL_R32F, rw, rh, 1)) return false;
    if (!make(recon_depth_, GL_R32UI, rw, rh, 1)) return false;
    if (!make(lanczos_lut_, GL_R16F, 128, 1, 1)) return false;
    return true;
}

void Backend::destroy_textures() {
    auto del = [](Texture& t) {
        if (t.texture) GLES.glDeleteTextures(1, &t.texture);
        t = {};
    };
    del(input_exposure_);
    del(input_reactive_);
    del(input_transparency_);
    if (output_owned_) del(output_);
    output_owned_ = false;
    for (auto& t : acc_) del(t);
    for (auto& t : luma_) del(t);
    del(intermediate_);
    for (auto& t : lh_) del(t);
    for (auto& t : iuc_) del(t);
    del(new_locks_);
    del(dilated_reactive_);
    del(shading_change_);
    del(spd_mips_);
    del(farthest_mip1_);
    del(spd_atomic_);
    del(frame_info_);
    del(dilated_motion_);
    del(dilated_depth_);
    del(recon_depth_);
    del(lanczos_lut_);
}

bool Backend::upload_defaults() {
    const float exposure = 1.0f;
    GLES.glBindTexture(GL_TEXTURE_2D, input_exposure_.texture);
    GLES.glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RED, GL_FLOAT, &exposure);
    const uint8_t zero = 0;
    GLES.glBindTexture(GL_TEXTURE_2D, input_reactive_.texture);
    GLES.glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RED, GL_UNSIGNED_BYTE, &zero);
    GLES.glBindTexture(GL_TEXTURE_2D, input_transparency_.texture);
    GLES.glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RED, GL_UNSIGNED_BYTE, &zero);
    std::vector<float> lut(128, 0.0f);
    GLES.glBindTexture(GL_TEXTURE_2D, lanczos_lut_.texture);
    GLES.glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 128, 1, GL_RED, GL_FLOAT, lut.data());
    GLES.glBindTexture(GL_TEXTURE_2D, 0);
    return GLES.glGetError() == GL_NO_ERROR;
}

namespace {
struct ParsedImage {
    GLuint unit = 0;
    std::string name;
    GLenum access = GL_READ_WRITE;
    GLenum format = 0;
};
struct ParsedUbo {
    GLuint binding = 0;
    std::string name;
};
} // namespace

bool Backend::create_programs() {
    struct PassSource {
        const char* source;
        const char* name;
        int lx;
        int ly;
    };
    static const PassSource sources[] = {
        {fsr3_shaders::kPrepareInputs, "prepare_inputs", 8, 8},
        {fsr3_shaders::kLumaPyramid, "luma_pyramid", 256, 1},
        {fsr3_shaders::kShadingChangePyramid, "shading_change_pyramid", 256, 1},
        {fsr3_shaders::kShadingChange, "shading_change", 8, 8},
        {fsr3_shaders::kPrepareReactivity, "prepare_reactivity", 8, 8},
        {fsr3_shaders::kLumaInstability, "luma_instability", 8, 8},
        {fsr3_shaders::kAccumulate, "accumulate", 8, 8},
        {fsr3_shaders::kRcas, "rcas", 64, 1},
    };

    programs_.clear();
    programs_.resize(std::size(sources));
    for (size_t i = 0; i < std::size(sources); ++i) {
        Program& p = programs_[i];
        static const PassDesc descriptors[] = {
            {fsr3_shaders::kPrepareInputs, "prepare_inputs", 8, 8, false, false},
            {fsr3_shaders::kLumaPyramid, "luma_pyramid", 256, 1, true, false},
            {fsr3_shaders::kShadingChangePyramid, "shading_change_pyramid", 256, 1, true, false},
            {fsr3_shaders::kShadingChange, "shading_change", 8, 8, false, false},
            {fsr3_shaders::kPrepareReactivity, "prepare_reactivity", 8, 8, false, false},
            {fsr3_shaders::kLumaInstability, "luma_instability", 8, 8, false, false},
            {fsr3_shaders::kAccumulate, "accumulate", 8, 8, false, false},
            {fsr3_shaders::kRcas, "rcas", 64, 1, false, true},
        };
        p.desc = &descriptors[i];
        p.program = GLES.glCreateProgram();
        GLuint shader = GLES.glCreateShader(GL_COMPUTE_SHADER);
        const char* src = sources[i].source;
        GLES.glShaderSource(shader, 1, &src, nullptr);
        GLES.glCompileShader(shader);
        GLint ok = 0;
        GLES.glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            GLint len = 0;
            GLES.glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
            std::string log(static_cast<size_t>(std::max(1, len)), '\0');
            GLES.glGetShaderInfoLog(shader, len, nullptr, log.data());
            GLES.glDeleteShader(shader);
            set_error(std::string("FSR3 shader compile failed (") + sources[i].name + "): " + log);
            return false;
        }
        GLES.glAttachShader(p.program, shader);
        GLES.glLinkProgram(p.program);
        GLES.glGetProgramiv(p.program, GL_LINK_STATUS, &ok);
        if (!ok) {
            GLint len = 0;
            GLES.glGetProgramiv(p.program, GL_INFO_LOG_LENGTH, &len);
            std::string log(static_cast<size_t>(std::max(1, len)), '\0');
            GLES.glGetProgramInfoLog(p.program, len, nullptr, log.data());
            GLES.glDeleteShader(shader);
            set_error(std::string("FSR3 program link failed (") + sources[i].name + "): " + log);
            return false;
        }
        GLES.glDeleteShader(shader);

        std::istringstream in(sources[i].source);
        std::string line;
        while (std::getline(in, line)) {
            if (line.find("std140) uniform ") != std::string::npos && line.find("binding = ") != std::string::npos) {
                ParsedUbo u;
                u.binding = static_cast<GLuint>(int_after(line, "binding = "));
                const size_t q = line.find("uniform ");
                std::string rest = trim(line.substr(q + 8));
                const size_t sp = rest.find_first_of(" \t");
                u.name = (sp == std::string::npos) ? rest : rest.substr(0, sp);
                const GLuint index = GLES.glGetUniformBlockIndex(p.program, u.name.c_str());
                if (index != GL_INVALID_INDEX) GLES.glUniformBlockBinding(p.program, index, u.binding);
                p.ubos.emplace_back(u.binding, u.name);
                continue;
            }
            if ((line.find("image2D ") != std::string::npos) && line.find("binding = ") != std::string::npos) {
                ParsedImage im;
                im.unit = static_cast<GLuint>(int_after(line, "binding = "));
                const size_t comma = line.find(',', line.find("binding = "));
                const size_t close = line.find(')', comma);
                if (comma != std::string::npos && close != std::string::npos)
                    im.format = image_format_from_qualifier(trim(line.substr(comma + 1, close - comma - 1)));
                const size_t q = line.find("image2D ");
                std::string rest = trim(line.substr(q + 8));
                const size_t semi = rest.find(';');
                if (semi != std::string::npos) rest = rest.substr(0, semi);
                const size_t sp = rest.find_last_of(" \t");
                im.name = (sp == std::string::npos) ? rest : rest.substr(sp + 1);
                im.access = line.find("writeonly") != std::string::npos ? GL_WRITE_ONLY
                          : line.find("readonly") != std::string::npos ? GL_READ_ONLY
                          : GL_READ_WRITE;
                if (im.format == 0) {
                    set_error(std::string("FSR3 unsupported image format in ") + sources[i].name);
                    return false;
                }
                ImageBinding binding;
                binding.unit = im.unit;
                binding.name = im.name;
                binding.access = im.access;
                binding.format = im.format;
                p.images.push_back(std::move(binding));
            }
        }

        GLint active = 0;
        GLES.glGetProgramiv(p.program, GL_ACTIVE_UNIFORMS, &active);
        for (GLint u = 0; u < active; ++u) {
            char name[256]{};
            GLsizei len = 0;
            GLint size = 0;
            GLenum type = 0;
            GLES.glGetActiveUniform(p.program, static_cast<GLuint>(u), sizeof(name), &len, &size, &type, name);
            if (type != GL_SAMPLER_2D) continue;
            std::string full(name);
            std::string base = full;
            const char* pre = "SPIRV_Cross_Combined";
            if (starts_with(base, pre)) base = base.substr(std::strlen(pre));
            if (ends_with(base, "SPIRV_Cross_DummySampler")) base.resize(base.size() - std::strlen("SPIRV_Cross_DummySampler"));
            else if (ends_with(base, "s_LinearClamp")) base.resize(base.size() - std::strlen("s_LinearClamp"));
            p.samplers.emplace_back(base, GLES.glGetUniformLocation(p.program, full.c_str()));
        }
        GLES.glUseProgram(0);
        p.compiled = true;
    }
    return true;
}

bool Backend::create_constants() {
    GLES.glGenFramebuffers(1, &clear_fbo_);
    if (!clear_fbo_) return false;
    GLES.glGenBuffers(1, &ubo_fsr_);
    GLES.glGenBuffers(1, &ubo_spd_);
    GLES.glGenBuffers(1, &ubo_rcas_);
    if (!ubo_fsr_ || !ubo_spd_ || !ubo_rcas_) return false;
    GLES.glBindBuffer(GL_UNIFORM_BUFFER, ubo_fsr_);
    GLES.glBufferData(GL_UNIFORM_BUFFER, 160, nullptr, GL_DYNAMIC_DRAW);
    GLES.glBindBuffer(GL_UNIFORM_BUFFER, ubo_spd_);
    GLES.glBufferData(GL_UNIFORM_BUFFER, 32, nullptr, GL_DYNAMIC_DRAW);
    GLES.glBindBuffer(GL_UNIFORM_BUFFER, ubo_rcas_);
    GLES.glBufferData(GL_UNIFORM_BUFFER, sizeof(RcasConsts), nullptr, GL_DYNAMIC_DRAW);
    GLES.glBindBuffer(GL_UNIFORM_BUFFER, 0);
    return GLES.glGetError() == GL_NO_ERROR;
}

void Backend::destroy_programs() {
    for (auto& p : programs_) {
        if (p.program) GLES.glDeleteProgram(p.program);
        p = {};
    }
    programs_.clear();
    if (ubo_fsr_) GLES.glDeleteBuffers(1, &ubo_fsr_);
    if (ubo_spd_) GLES.glDeleteBuffers(1, &ubo_spd_);
    if (ubo_rcas_) GLES.glDeleteBuffers(1, &ubo_rcas_);
    if (clear_fbo_) GLES.glDeleteFramebuffers(1, &clear_fbo_);
    ubo_fsr_ = ubo_spd_ = ubo_rcas_ = clear_fbo_ = 0;
}

void Backend::destroy() {
    destroy_textures();
    destroy_programs();
    initialized_ = false;
    failed_ = false;
    error_.clear();
    first_frame_ = true;
    frame_index_ = 0;
    last_reset_ = true;
    frame_color_ = frame_depth_ = frame_motion_ = frame_exposure_ = frame_reactive_ = frame_transparency_ =
        frame_output_ = 0;
}

bool Backend::initialize(uint32_t render_width, uint32_t render_height, uint32_t output_width,
                         uint32_t output_height) {
    if (render_width == 0 || render_height == 0 || output_width == 0 || output_height == 0) {
        set_error("FSR3 invalid extents");
        return false;
    }
    if (render_width < 64 || render_height < 64) {
        set_error("FSR3 render extent is too small for the SPD mip chain");
        return false;
    }
    destroy();
    render_width_ = render_width;
    render_height_ = render_height;
    output_width_ = output_width;
    output_height_ = output_height;
    first_frame_ = true;
    frame_index_ = 0;
    last_reset_ = true;
    if (!create_textures()) {
        destroy_textures();
        set_error("FSR3 texture allocation failed");
        return false;
    }
    if (!create_programs()) {
        destroy_textures();
        destroy_programs();
        return false;
    }
    if (!create_constants() || !upload_defaults()) {
        destroy_textures();
        destroy_programs();
        set_error("FSR3 constant/default upload failed");
        return false;
    }
    previous_render_width_ = previous_render_height_ = 0;
    previous_output_width_ = previous_output_height_ = 0;
    previous_jitter_x_ = previous_jitter_y_ = 0.0f;
    previous_pre_exposure_ = 1.0f;
    initialized_ = true;
    return true;
}

bool Backend::resize(uint32_t render_width, uint32_t render_height, uint32_t output_width, uint32_t output_height) {
    if (render_width == render_width_ && render_height == render_height_ && output_width == output_width_ &&
        output_height == output_height_)
        return initialized_;
    const bool was_initialized = initialized_;
    destroy_textures();
    render_width_ = render_width;
    render_height_ = render_height;
    output_width_ = output_width;
    output_height_ = output_height;
    first_frame_ = true;
    frame_index_ = 0;
    last_reset_ = true;
    previous_render_width_ = previous_render_height_ = 0;
    previous_output_width_ = previous_output_height_ = 0;
    previous_jitter_x_ = previous_jitter_y_ = 0.0f;
    previous_pre_exposure_ = 1.0f;
    if (!create_textures() || !upload_defaults()) {
        destroy_textures();
        initialized_ = false;
        set_error("FSR3 resize failed");
        return false;
    }
    initialized_ = was_initialized;
    return initialized_;
}

bool Backend::resolve_sampler(const std::string& base, Texture*& texture, int& level) {
    level = 0;
    auto set = [&](Texture* t) { texture = t; return t != nullptr; };
    const bool odd = (frame_index_ & 1u) != 0;
    if (base == "r_spd_mips") return set(&spd_mips_);
    if (base == "r_farthest_depth") return set(&intermediate_);
    if (base == "r_farthest_depth_mip1") return set(&farthest_mip1_);
    if (base == "r_dilated_reactive_masks") return set(&dilated_reactive_);
    if (base == "r_dilated_motion_vectors") return set(&dilated_motion_);
    if (base == "r_dilated_depth") return set(&dilated_depth_);
    if (base == "r_reconstructed_previous_nearest_depth") return set(&recon_depth_);
    if (base == "r_current_luma") return set(&luma_[odd ? 1 : 0]);
    if (base == "r_previous_luma") return set(&luma_[odd ? 0 : 1]);
    if (base == "r_luma_history") return set(&lh_[odd ? 1 : 0]);
    if (base == "r_luma_instability") return set(&intermediate_);
    if (base == "r_internal_upscaled_color") return set(&iuc_[odd ? 1 : 0]);
    if (base == "r_accumulation") return set(&acc_[odd ? 1 : 0]);
    if (base == "r_shading_change") return set(&shading_change_);
    if (base == "r_rcas_input") return set(&iuc_[odd ? 0 : 1]);
    if (base == "r_lanczos_lut") return set(&lanczos_lut_);
    if (base == "r_new_locks") return set(&new_locks_);
    if (base == "r_frame_info") return set(&frame_info_);
    return false;
}

bool Backend::resolve_image(const std::string& name, Texture*& texture, int& level, GLenum& access, GLenum& format) {
    level = 0;
    access = GL_READ_WRITE;
    format = GL_RGBA8;
    std::string s = name;
    if (starts_with(s, "r_rw_")) s = s.substr(2);
    const bool odd = (frame_index_ & 1u) != 0;
    if (starts_with(s, "rw_spd_mip")) {
        level = s.back() - '0';
        texture = &spd_mips_;
        format = GL_RGBA16F;
        access = GL_WRITE_ONLY;
        return true;
    }
    if (s == "rw_spd_global_atomic") { texture = &spd_atomic_; format = GL_R32UI; return true; }
    if (s == "rw_internal_upscaled_color") { texture = &iuc_[odd ? 0 : 1]; format = GL_RGBA16F; access = GL_WRITE_ONLY; return true; }
    if (s == "rw_accumulation") { texture = &acc_[odd ? 0 : 1]; format = GL_RGBA8; access = GL_WRITE_ONLY; return true; }
    if (s == "rw_luma_history") { texture = &lh_[odd ? 0 : 1]; format = GL_RGBA16F; access = GL_WRITE_ONLY; return true; }
    if (s == "rw_new_locks") { texture = &new_locks_; format = GL_RGBA8; access = GL_WRITE_ONLY; return true; }
    if (s == "rw_frame_info") { texture = &frame_info_; format = GL_RGBA32F; access = GL_WRITE_ONLY; return true; }
    if (s == "rw_upscaled_output") { texture = &output_; format = GL_RGBA8; access = GL_WRITE_ONLY; return true; }
    if (s == "rw_dilated_reactive_masks") { texture = &dilated_reactive_; format = GL_RGBA8; access = GL_WRITE_ONLY; return true; }
    if (s == "rw_shading_change") { texture = &shading_change_; format = GL_RGBA8; access = GL_WRITE_ONLY; return true; }
    if (s == "rw_farthest_depth") { texture = &intermediate_; format = GL_RGBA16F; access = GL_WRITE_ONLY; return true; }
    if (s == "rw_farthest_depth_mip1") { texture = &farthest_mip1_; format = GL_RGBA16F; access = GL_WRITE_ONLY; return true; }
    if (s == "rw_current_luma") { texture = &luma_[odd ? 1 : 0]; format = GL_RGBA16F; access = GL_WRITE_ONLY; return true; }
    if (s == "rw_luma_instability") { texture = &intermediate_; format = GL_RGBA16F; access = GL_WRITE_ONLY; return true; }
    if (s == "rw_dilated_motion_vectors") { texture = &dilated_motion_; format = GL_RGBA16F; access = GL_WRITE_ONLY; return true; }
    if (s == "rw_dilated_depth") { texture = &dilated_depth_; format = GL_R32F; access = GL_WRITE_ONLY; return true; }
    if (s == "rw_reconstructed_previous_nearest_depth") { texture = &recon_depth_; format = GL_R32UI; return true; }
    return false;
}

bool Backend::bind_samplers(Program& p) {
    int unit = 0;
    for (const auto& [base, location] : p.samplers) {
        GLuint tex = 0;
        if (base == "r_input_color_jittered" || base == "r_input_color") tex = frame_color_;
        else if (base == "r_input_depth") tex = frame_depth_;
        else if (base == "r_input_motion_vectors") tex = frame_motion_;
        else if (base == "r_input_exposure") tex = frame_exposure_;
        else if (base == "r_reactive_mask") tex = frame_reactive_;
        else if (base == "r_transparency_and_composition_mask") tex = frame_transparency_;
        else {
            Texture* t = nullptr;
            int level = 0;
            if (!resolve_sampler(base, t, level) || !t) {
                set_error("FSR3 unresolved sampler " + base);
                return false;
            }
            tex = t->texture;
        }
        if (unit >= kMaxTextureUnits) {
            set_error("FSR3 sampler unit overflow");
            return false;
        }
        GLES.glActiveTexture(GL_TEXTURE0 + unit);
        GLES.glBindTexture(GL_TEXTURE_2D, tex);
        GLES.glBindSampler(unit, 0);
        if (location >= 0) GLES.glUniform1i(location, unit);
        ++unit;
    }
    GLES.glActiveTexture(GL_TEXTURE0);
    return true;
}

bool Backend::bind_images(Program& p) {
    for (const ImageBinding& image : p.images) {
        Texture* t = nullptr;
        int level = 0;
        GLenum access = image.access;
        GLenum format = image.format;
        if (!resolve_image(image.name, t, level, access, format) || !t) {
            set_error("FSR3 unresolved image " + image.name);
            return false;
        }
        // The r_rw_ alias is the read-only view of a texture that another unit
        // writes in the same dispatch; do not let the resource mapping turn it
        // into a second write-only binding.
        if (starts_with(image.name, "r_rw_")) access = GL_READ_ONLY;
        GLES.glBindImageTexture(image.unit, t->texture, level, GL_FALSE, 0, access, format);
    }
    return true;
}

bool Backend::bind_ubo(Program& p) {
    for (const auto& [binding, name] : p.ubos) {
        if (binding >= kMaxUboUnits) continue;
        if (name == "cbFSR3UPSCALER_t") GLES.glBindBufferRange(GL_UNIFORM_BUFFER, binding, ubo_fsr_, 0, 160);
        else if (name == "cbSPD_t") GLES.glBindBufferRange(GL_UNIFORM_BUFFER, binding, ubo_spd_, 0, 32);
        else if (name == "cbRCAS_t") GLES.glBindBufferRange(GL_UNIFORM_BUFFER, binding, ubo_rcas_, 0, sizeof(RcasConsts));
    }
    return true;
}

bool Backend::run_pass(size_t index, uint32_t width, uint32_t height) {
    if (index >= programs_.size() || !programs_[index].compiled) {
        set_error("FSR3 pass index invalid");
        return false;
    }
    Program& p = programs_[index];
    GLES.glUseProgram(p.program);
    if (!bind_samplers(p) || !bind_images(p) || !bind_ubo(p)) return false;

    uint32_t gx = 0, gy = 0;
    if (index == 1 || index == 2) {
        gx = ceil_div(width, 64);
        gy = ceil_div(height, 64);
    } else if (index == 7) {
        gx = ceil_div(width, 16);
        gy = ceil_div(height, 16);
    } else {
        const uint32_t lx = (index == 1 || index == 2) ? 256 : 8;
        const uint32_t ly = (index == 1 || index == 2) ? 1 : 8;
        gx = ceil_div(width, lx);
        gy = ceil_div(height, ly);
    }
    GLES.glDispatchCompute(gx, gy, 1);
    GLES.glMemoryBarrier(GL_ALL_BARRIER_BITS);
    return GLES.glGetError() == GL_NO_ERROR;
}

bool Backend::dispatch(const Inputs& in) {
    if (!initialized_ || failed_) {
        set_error("FSR3 backend is not initialized");
        return false;
    }
    if (in.hdr_color) {
        set_error("FSR3 HDR output profile is not qualified");
        return false;
    }
    if (in.output.handle != 0) {
        set_error("FSR3 application-provided output is not qualified");
        return false;
    }
    if (in.color.handle == 0 || in.depth.handle == 0 || in.motion.handle == 0) {
        set_error("FSR3 requires color, depth and motion resources");
        return false;
    }
    if (in.render_width != render_width_ || in.render_height != render_height_ ||
        in.output_width != output_width_ || in.output_height != output_height_) {
        if (!resize(in.render_width, in.render_height, in.output_width, in.output_height)) return false;
    }

    frame_color_ = static_cast<GLuint>(in.color.handle);
    frame_depth_ = static_cast<GLuint>(in.depth.handle);
    frame_motion_ = static_cast<GLuint>(in.motion.handle);
    frame_exposure_ = in.exposure.handle ? static_cast<GLuint>(in.exposure.handle) : input_exposure_.texture;
    frame_reactive_ = in.reactive.handle ? static_cast<GLuint>(in.reactive.handle) : input_reactive_.texture;
    frame_transparency_ = in.transparency.handle ? static_cast<GLuint>(in.transparency.handle) : input_transparency_.texture;
    frame_output_ = output_.texture;

    DispatchGuard guard;

    const bool reset = first_frame_ || in.reset_history;
    if (reset) {
        frame_index_ = 0;
        previous_render_width_ = previous_render_height_ = 0;
        previous_output_width_ = previous_output_height_ = 0;
        previous_jitter_x_ = previous_jitter_y_ = 0.0f;
        previous_pre_exposure_ = 1.0f;
        for (auto& t : acc_) if (!clear_float(t, 0, 0, 0, 0, 0)) { set_error("FSR3 acc clear failed"); return false; }
        for (auto& t : luma_) if (!clear_float(t, 0, 0, 0, 0, 0)) { set_error("FSR3 luma clear failed"); return false; }
        for (auto& t : lh_) if (!clear_float(t, 0, 0, 0, 0, 0)) { set_error("FSR3 history clear failed"); return false; }
        for (auto& t : iuc_) if (!clear_float(t, 0, 0, 0, 0, 0)) { set_error("FSR3 upscaled history clear failed"); return false; }
        if (!clear_float(frame_info_, 0, -1.0f, 1.0f, 0.0f, 0.0f)) { set_error("FSR3 frame-info clear failed"); return false; }
    }
    if (!clear_uint(recon_depth_, 0, 0x3F800000u)) { set_error("FSR3 depth clear failed"); return false; }
    if (!clear_uint(spd_atomic_, 0, 0)) { set_error("FSR3 SPD clear failed"); return false; }
    if (!clear_float(new_locks_, 0, 0, 0, 0, 0)) { set_error("FSR3 locks clear failed"); return false; }
    if (!clear_float(shading_change_, 0, 0, 0, 0, 0)) { set_error("FSR3 shading-change clear failed"); return false; }
    if (!clear_float(dilated_reactive_, 0, 0, 0, 0, 0)) { set_error("FSR3 reactive clear failed"); return false; }
    if (!clear_float(intermediate_, 0, 0, 0, 0, 0)) { set_error("FSR3 intermediate clear failed"); return false; }
    for (int level = 0; level < spd_mips_.levels; ++level)
        if (!clear_float(spd_mips_, level, 0, 0, 0, 0)) { set_error("FSR3 SPD mips clear failed"); return false; }
    if (!clear_float(farthest_mip1_, 0, 0, 0, 0, 0)) { set_error("FSR3 farthest clear failed"); return false; }

    FsrConsts k{};
    k.renderSize[0] = static_cast<int32_t>(render_width_);
    k.renderSize[1] = static_cast<int32_t>(render_height_);
    k.previousFrameRenderSize[0] = static_cast<int32_t>(previous_render_width_);
    k.previousFrameRenderSize[1] = static_cast<int32_t>(previous_render_height_);
    k.upscaleSize[0] = static_cast<int32_t>(output_width_);
    k.upscaleSize[1] = static_cast<int32_t>(output_height_);
    k.previousFrameUpscaleSize[0] = static_cast<int32_t>(previous_output_width_);
    k.previousFrameUpscaleSize[1] = static_cast<int32_t>(previous_output_height_);
    k.maxRenderSize[0] = k.renderSize[0];
    k.maxRenderSize[1] = k.renderSize[1];
    k.maxUpscaleSize[0] = k.upscaleSize[0];
    k.maxUpscaleSize[1] = k.upscaleSize[1];

    const float fMin = std::min(in.camera_near, in.camera_far);
    const float fMax = std::max(in.camera_near, in.camera_far);
    const float fQ = fMax / (fMin - fMax);
    const int inv = in.depth_reversed ? 1 : 0;
    const int inf = in.depth_infinite_far ? 1 : 0;
    const float matrix_c[2][2] = {{fQ, -1.0f - 1.192092896e-7f}, {fQ, 1.192092896e-7f}};
    const float matrix_e[2][2] = {{fQ * fMin, -fMin - 1.192092896e-7f}, {fQ * fMin, fMax}};
    k.deviceToViewDepth[0] = -matrix_c[inv][inf];
    k.deviceToViewDepth[1] = matrix_e[inv][inf];
    const float aspect = static_cast<float>(render_width_) / static_cast<float>(render_height_);
    const float fov = in.camera_fov_y_radians > 0.0f ? in.camera_fov_y_radians : 1.0471975511965976f;
    const float cotHalf = std::cos(0.5f * fov) / std::sin(0.5f * fov);
    k.deviceToViewDepth[2] = aspect / cotHalf;
    k.deviceToViewDepth[3] = 1.0f / cotHalf;

    const float motion_scale = in.motion_units == 0 ? 1.0f : static_cast<float>(render_width_);
    const float motion_scale_y = in.motion_units == 0 ? 1.0f : static_cast<float>(render_height_);
    k.motionVectorScale[0] = motion_scale / static_cast<float>(render_width_);
    k.motionVectorScale[1] = motion_scale_y / static_cast<float>(render_height_);
    if (in.motion_includes_jitter) {
        k.motionVectorJitterCancellation[0] = (previous_jitter_x_ - in.jitter_x) / static_cast<float>(render_width_);
        k.motionVectorJitterCancellation[1] = (previous_jitter_y_ - in.jitter_y) / static_cast<float>(render_height_);
    }
    k.jitterOffset[0] = in.jitter_x;
    k.jitterOffset[1] = in.jitter_y;
    k.previousFrameJitterOffset[0] = previous_jitter_x_;
    k.previousFrameJitterOffset[1] = previous_jitter_y_;
    k.downscaleFactor[0] = static_cast<float>(render_width_) / static_cast<float>(output_width_);
    k.downscaleFactor[1] = static_cast<float>(render_height_) / static_cast<float>(output_height_);
    const float horizontal_fov = std::atan(std::tan(fov * 0.5f) * aspect) * 2.0f;
    k.tanHalfFOV = std::tan(horizontal_fov * 0.5f);
    k.jitterPhaseCount = static_cast<float>(static_cast<int32_t>(8.0f * std::pow(
        static_cast<float>(output_width_) / static_cast<float>(render_width_), 2.0f)));
    k.deltaTime = std::clamp(in.frame_delta_seconds, 0.0f, 1.0f);
    k.deltaPreExposure = previous_pre_exposure_ > 0.0f ? in.pre_exposure / previous_pre_exposure_ : 1.0f;
    k.viewSpaceToMetersFactor = 1.0f;
    k.frameIndex = static_cast<float>(frame_index_);
    k.velocityFactor = 1.0f;
    k.reactivenessScale = 1.0f;
    k.shadingChangeScale = 1.0f;
    k.accumulationAddedPerFrame = 1.0f / 3.0f;
    k.minDisocclusionAccumulation = -1.0f / 3.0f;

    SpdConsts spd{};
    spd.mips = static_cast<uint32_t>(std::min(12.0f, std::floor(std::log2(
        static_cast<float>(std::max(render_width_, render_height_))))));
    spd.numWorkGroups = ceil_div(render_width_, 64) * ceil_div(render_height_, 64);
    spd.renderSize[0] = render_width_;
    spd.renderSize[1] = render_height_;

    RcasConsts rcas{};
    rcas.rcasConfig[0] = 0; // set below
    const float sharpness = 0.2f;
    const float remapped = -2.0f * sharpness + 2.0f;
    const float con = std::exp2(-remapped);
    uint32_t bits = 0;
    std::memcpy(&bits, &con, sizeof(bits));
    rcas.rcasConfig[0] = bits;

    GLES.glBindBuffer(GL_UNIFORM_BUFFER, ubo_fsr_);
    GLES.glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(k), &k);
    GLES.glBindBuffer(GL_UNIFORM_BUFFER, ubo_spd_);
    GLES.glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(spd), &spd);
    GLES.glBindBuffer(GL_UNIFORM_BUFFER, ubo_rcas_);
    GLES.glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(rcas), &rcas);

    if (!run_pass(0, render_width_, render_height_)) return false;
    if (!run_pass(1, render_width_, render_height_)) return false;
    if (!run_pass(2, render_width_, render_height_)) return false;
    if (!run_pass(3, std::max(1u, render_width_ / 2), std::max(1u, render_height_ / 2))) return false;
    if (!run_pass(4, render_width_, render_height_)) return false;
    if (!run_pass(5, render_width_, render_height_)) return false;
    if (!run_pass(6, output_width_, output_height_)) return false;
    if (!run_pass(7, output_width_, output_height_)) return false;

    previous_render_width_ = render_width_;
    previous_render_height_ = render_height_;
    previous_output_width_ = output_width_;
    previous_output_height_ = output_height_;
    previous_jitter_x_ = in.jitter_x;
    previous_jitter_y_ = in.jitter_y;
    previous_pre_exposure_ = in.pre_exposure;
    ++frame_index_;
    first_frame_ = false;
    last_reset_ = reset;
    return true;
}

} // namespace mg_fsr3
