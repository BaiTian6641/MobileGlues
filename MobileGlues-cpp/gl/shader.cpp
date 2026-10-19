// MobileGlues - gl/shader.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header

#include <cctype>
#include <limits>
#include "shader.h"
#include "shader_source.h"
#include "shader_state.h"
#include "image_metadata.h"

#include <GL/gl.h>
#include "getter.h"
#include "log.h"
#include "program.h"
#include "../gles/loader.h"
#include "../includes.h"
#include "glsl/glsl_for_es.h"
#include "../config/settings.h"
#include "FSR1/FSR1.h"

#define DEBUG 0

bool can_run_essl3(unsigned int esversion, const char* glsl) {
    if (strncmp(glsl, "#version 100", 12) == 0) {
        return true;
    }

    unsigned int glsl_version = 0;
    if (strncmp(glsl, "#version 300 es", 15) == 0) {
        glsl_version = 300;
    } else if (strncmp(glsl, "#version 310 es", 15) == 0) {
        glsl_version = 310;
    } else if (strncmp(glsl, "#version 320 es", 15) == 0) {
        glsl_version = 320;
    } else {
        return false;
    }
    return esversion >= glsl_version;
}

bool is_direct_shader(const char* glsl) {
    bool es3_ability = can_run_essl3(hardware->es_version, glsl);
    return es3_ability;
}

bool check_if_sampler_buffer_used(std::string str) {
    return str.find("samplerBuffer") != std::string::npos;
}

void glShaderSource(GLuint shader, GLsizei count, const GLchar* const* string, const GLint* length) {
    LOG()
    MgShaderObject& state = g_mg_shaders[mg_object_key(shader)];
    state.converted_source.clear();

    std::string glsl_src, essl_src;
    GLenum source_error = GL_NO_ERROR;
    if (!mg_collect_shader_source(count, string, length, &glsl_src, &source_error)) {
        mg_set_gl_error(source_error);
        LOG_E("Rejected glShaderSource(%u): invalid count/string/length argument", shader)
        return;
    }

    // Consume the loader's image-format metadata for this source before any
    // compile decision. The format qualifier must be part of the source that
    // ESSL sees; glBindImageTexture happens after linking and cannot supply it.
    const std::unordered_map<std::string, GLenum> image_formats = mg_image_metadata_take();
    mg_normalize_image_qualifiers(glsl_src, image_formats);

    bool is_sampler_buffer_emulated = hardware->emulate_texture_buffer && check_if_sampler_buffer_used(glsl_src);

    if (is_direct_shader(glsl_src.c_str())) {
        LOG_D("[INFO] [Shader] Direct shader source: ")
        LOG_D("%s", glsl_src.c_str())
        essl_src = glsl_src;
    } else {
        int glsl_version = getGLSLVersion(glsl_src.c_str());
        LOG_D("[INFO] [Shader] Shader source: ")
        LOG_D("%s", glsl_src.c_str())
        GLint shaderType;
        GLES.glGetShaderiv(shader, GL_SHADER_TYPE, &shaderType);
        int return_code = 0;
        essl_src = GLSLtoGLSLES(glsl_src.c_str(), shaderType, hardware->es_version, glsl_version, return_code);

        if (essl_src.empty()) {
            LOG_E("Failed to convert shader %d.", shader)
            std::string preview = glsl_src.substr(0, 8000);
            __android_log_print(ANDROID_LOG_ERROR, "MobileGlues",
                                "[MG-GLSL-ERROR] translation returned empty for shader %u type %d:\n%s", shader,
                                shaderType, preview.c_str());
            fprintf(stderr, "[MG-GLSL-ERROR] translation returned empty for shader %u type %d:\n%s\n", shader,
                    shaderType, preview.c_str());
            fflush(stderr);
            write_log("[MG-GLSL-ERROR] translation returned empty for shader %u type %d:\n%s\n", shader, shaderType,
                      preview.c_str());
            return;
        }
        LOG_D("\n[INFO] [Shader] Converted Shader source: \n%s", essl_src.c_str())
    }
    mg_normalize_image_qualifiers(essl_src, image_formats);
    if (!essl_src.empty()) {
        if (essl_src.size() > static_cast<size_t>(std::numeric_limits<GLint>::max())) {
            mg_set_gl_error(GL_OUT_OF_MEMORY);
            LOG_E("Translated shader %u exceeds the GLint source length limit", shader)
            return;
        }
        state.converted_source = essl_src;
        state.sampler_buffer_emulated = is_sampler_buffer_emulated;
        // The frontend has already performed the concatenation. The backend
        // contract is one pointer, one explicit length: passing the original
        // count with a single-element array made the driver read past the
        // array (observed as a segfault on the X1E device), and dropping the
        // length would truncate at an embedded NUL.
        const GLchar* s = essl_src.data();
        GLint s_length = static_cast<GLint>(essl_src.size());
        GLES.glShaderSource(shader, 1, &s, &s_length);
    } else
        LOG_E("Failed to convert glsl.")
    CHECK_GL_ERROR
}

void glGetShaderiv(GLuint shader, GLenum pname, GLint* params) {
    LOG()
    GLES.glGetShaderiv(shader, pname, params);
    if (params && pname == GL_COMPILE_STATUS && !*params) {
        // A compile failure is a result, not a compatibility annoyance. The
        // ignore-error setting used to rewrite this to GL_TRUE, which made a
        // missing effect indistinguishable from a working program in every
        // qualification run. Keep the driver's answer and put the reason in the
        // log where it survives release logging settings.
        GLchar infoLog[512] = {0};
        GLES.glGetShaderInfoLog(shader, sizeof(infoLog), nullptr, infoLog);
        LOG_W_FORCE("Shader %u compilation failed: \n%s", shader, infoLog)
    }
    CHECK_GL_ERROR
}

GLuint glCreateShader(GLenum shaderType) {
    if (global_settings.fsr1_setting != FSR1_Quality_Preset::Disabled && !fsrInitialized) {
        InitFSRResources();
    }

    LOG()
    LOG_D("glCreateShader(%s)", glEnumToString(shaderType))
    GLuint shader = GLES.glCreateShader(shaderType);
    if (shader != 0) {
        MgShaderObject& state = g_mg_shaders[mg_object_key(shader)];
        state = MgShaderObject{};
        state.type = shaderType;
    }
    CHECK_GL_ERROR
    return shader;
}

void glDeleteShader(GLuint shader) {
    LOG()
    LOG_D("glDeleteShader(%u)", shader)
    auto it = g_mg_shaders.find(mg_object_key(shader));
    if (it != g_mg_shaders.end()) {
        if (it->second.attached_programs.empty()) {
            g_mg_shaders.erase(it);
        } else {
            // GL defers deletion until every program detaches the object. Keep
            // the translated source for a later link, but do not let a new
            // glCreateShader reuse of the name inherit it.
            it->second.deleted = true;
        }
    }
    GLES.glDeleteShader(shader);
    CHECK_GL_ERROR
}
