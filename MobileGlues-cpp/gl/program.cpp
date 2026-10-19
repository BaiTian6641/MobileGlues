// MobileGlues - gl/program.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header

#include <regex.h>
#include "GL/glext.h"
#include "GLES3/gl32.h"
#include "log.h"
#include "shader.h"
#include "program.h"
#include <regex>
#include <algorithm>
#include <cstring>
#include <iostream>
#include "../config/settings.h"
#include "drawing.h"
#include "shader_state.h"
#include "image_bindings.h"

#define DEBUG 0

extern UnorderedMap<GLuint, SamplerInfo> g_samplerCacheForSamplerBuffer;
UnorderedMap<GLuint, bool> program_map_is_sampler_buffer_emulated;

enum class ShouldGenerateFSState : int {
    Never = 0,
    Maybe = 1,
    Unknown = 2
};

UnorderedMap<GLuint, ShouldGenerateFSState> program_map_should_generate_fs;

std::string updateLayoutLocation(const std::string& esslSource, GLuint color, const char* name) {
    const std::string& shaderCode = esslSource;

    // The optional group must match an existing layout qualifier entirely.
    // The previous pattern used '$' -- a regex end-of-line anchor -- where the
    // escaped parentheses were intended, so an existing layout was never
    // consumed and rebinding inserted a second location qualifier, which is a
    // compile error.
    std::string pattern = std::string(R"((layout\s*\([^)]*location\s*=\s*\d+[^)]*\)\s*)?)") +
                          R"(out\s+((?:highp|mediump|lowp|\w+\s+)*\w+)\s+)" + name + R"(\s*;)";

    std::string replacement = "layout (location = " + std::to_string(color) + ") out $2 " + name + ";";

    std::regex reg(pattern);
    std::string modifiedCode = std::regex_replace(shaderCode, reg, replacement);

    return modifiedCode;
}

void glBindFragDataLocation(GLuint program, GLuint color, const GLchar* name) {
    LOG()
    LOG_D("glBindFragDataLocation(%d, %d, %s)", program, color, name)

    if (name == nullptr) return;

    if (strlen(name) > 8 && strncmp(name, "outColor", 8) == 0) {
        const char* numberStr = name + 8;
        bool isNumber = true;
        for (int i = 0; numberStr[i] != '\0'; ++i) {
            if (!isdigit(numberStr[i])) {
                isNumber = false;
                break;
            }
        }

        if (isNumber) {
            unsigned int extractedColor = static_cast<unsigned int>(std::stoul(numberStr));
            if (extractedColor == color) {
                // outColor was bound in glsl process. exit now
                LOG_D("Find outColor* with color *, skipping")
                return;
            }
        }
    }

    // The binding belongs to this program, not to whichever shader happened to
    // be sourced last. It is applied to the program's attached fragment shaders
    // at the next link, so calling this before attaching the shader also works.
    MgProgramObject& state = g_mg_programs[mg_object_key(program)];
    state.frag_data_locations[name] = color;
}

static std::string DefaultFSSource;
static unsigned CurrentDefaultFSSourceVersion = 0; // the version (hardware->es_version) may change during runtime

void GenerateDefaultFSSource() {
    if (CurrentDefaultFSSourceVersion != hardware->es_version) {
        CurrentDefaultFSSourceVersion = hardware->es_version;
        std::ostringstream ss;
        ss << "#version " << CurrentDefaultFSSourceVersion << " es\n";
        ss << "precision mediump float;\n\n";
        ss << "out vec4 fragColor;\n\n";
        ss << "void main() {\n";
        ss << "    fragColor = vec4(1.0, 1.0, 1.0, 1.0);\n";
        ss << "}\n";

        DefaultFSSource = ss.str();
    }
}

static UnorderedMap<unsigned, GLuint> DefaultFSMap; // essl version <-> shader id
void glLinkProgram(GLuint program) {
    LOG()

    LOG_D("glLinkProgram(%d)", program)
    MgProgramObject* program_state = mg_find_program(program);
    if (program_state) {
        const bool has_locations = !program_state->frag_data_locations.empty();
        for (GLuint shader_name : program_state->attached_shaders) {
            MgShaderObject* shader_state = mg_find_shader(shader_name);
            if (!shader_state || shader_state->type != GL_FRAGMENT_SHADER ||
                shader_state->converted_source.empty()) {
                continue;
            }
            const bool restore = shader_state->patched_for_program != 0 &&
                                 (shader_state->patched_for_program != program || !has_locations);
            if (restore) {
                const GLchar* source = shader_state->converted_source.data();
                GLint source_length = static_cast<GLint>(shader_state->converted_source.size());
                GLES.glShaderSource(shader_name, 1, &source, &source_length);
                GLES.glCompileShader(shader_name);
                shader_state->patched_for_program = 0;
                GLint status = GL_FALSE;
                GLES.glGetShaderiv(shader_name, GL_COMPILE_STATUS, &status);
                if (status != GL_TRUE) {
                    GLchar info_log[512] = {0};
                    GLES.glGetShaderInfoLog(shader_name, sizeof(info_log), nullptr, info_log);
                    LOG_E("Failed to restore fragment shader %u before linking program %u, log:\n%s", shader_name,
                          program, info_log)
                }
            }
            if (!has_locations) continue;

            std::string patched = shader_state->converted_source;
            for (const auto& [name, color] : program_state->frag_data_locations) {
                patched = updateLayoutLocation(patched, color, name.c_str());
            }
            if (patched == shader_state->converted_source) continue;

            const GLchar* source = patched.data();
            GLint source_length = static_cast<GLint>(patched.size());
            GLES.glShaderSource(shader_name, 1, &source, &source_length);
            GLES.glCompileShader(shader_name);
            shader_state->patched_for_program = program;
            GLint status = GL_FALSE;
            GLES.glGetShaderiv(shader_name, GL_COMPILE_STATUS, &status);
            if (status != GL_TRUE) {
                GLchar info_log[512] = {0};
                GLES.glGetShaderInfoLog(shader_name, sizeof(info_log), nullptr, info_log);
                LOG_E("Failed to compile program %u's patched fragment shader %u, log:\n%s", program,
                      shader_name, info_log)
            }
            CHECK_GL_ERROR
        }
    }

    // Generate defaut fragment shader if needed
    if (program_map_should_generate_fs[program] == ShouldGenerateFSState::Maybe) {
        GenerateDefaultFSSource();
        GLuint& default_fs = DefaultFSMap[CurrentDefaultFSSourceVersion];
        if (!default_fs) {
            default_fs = GLES.glCreateShader(GL_FRAGMENT_SHADER);
            const char* src = DefaultFSSource.c_str();
            GLES.glShaderSource(default_fs, 1, &src, nullptr);

            GLES.glCompileShader(default_fs);

            GLint success = 0;
            GLES.glGetShaderiv(default_fs, GL_COMPILE_STATUS, &success);
            if (!success) {
                GLint logLength = 0;
                GLES.glGetShaderiv(default_fs, GL_INFO_LOG_LENGTH, &logLength);
                std::vector<char> log(logLength);
                GLES.glGetShaderInfoLog(default_fs, logLength, nullptr, log.data());
                LOG_E("Default fragment shader compile error for program %u :\n%s\n", program, log.data());
                GLES.glDeleteShader(default_fs);
                default_fs = 0;
            }
        }

        if (default_fs) {
            LOG_D("Try to attach missing default FS for program %u...", program);
            GLES.glAttachShader(program, default_fs);
        }
    }

    GLES.glLinkProgram(program);

    GLint link_status = GL_FALSE;
    GLES.glGetProgramiv(program, GL_LINK_STATUS, &link_status);
    if (link_status == GL_TRUE) {
        // A relink can change every reflected uniform location. The sampler-
        // buffer cache is filled after a previous link and must not survive.
        g_samplerCacheForSamplerBuffer.erase(program);
        mg_reflect_image_uniforms(program);
        mg_resolve_duplicate_image_bindings(program);
        g_samplerCacheForSamplerBuffer.erase(program);
        // A relink can change the physical image-unit plan while the program
        // is already current.
        if (gl_state->current_program == program) {
            MgProgramObject* current_state = mg_find_program(program);
            gl_state->current_program_has_image_uniforms = current_state && current_state->has_image_uniforms;
            mg_materialize_image_bindings();
        }
    }

    CHECK_GL_ERROR
}

void glGetProgramiv(GLuint program, GLenum pname, GLint* params) {
    LOG()
    GLES.glGetProgramiv(program, pname, params);
    if (params && (pname == GL_LINK_STATUS || pname == GL_VALIDATE_STATUS) && !*params) {
        // Never convert a failed link into success. The previous ignore-error
        // path did that and made incomplete shader programs look qualified.
        GLchar infoLog[512] = {0};
        GLES.glGetProgramInfoLog(program, sizeof(infoLog), nullptr, infoLog);
        LOG_W_FORCE("Program %u link/validate failed: \n%s", program, infoLog);
    }
    CHECK_GL_ERROR
}

void glUseProgram(GLuint program) {
    LOG()
    LOG_D("glUseProgram(%d)", program)
    if (program != gl_state->current_program) {
        gl_state->current_program = program;
        MgProgramObject* program_state = mg_find_program(program);
        gl_state->current_program_has_image_uniforms = program_state && program_state->has_image_uniforms;
        GLES.glUseProgram(program);
        // Image units are context state; the program's physical plan may map
        // them differently from the previous program.
        mg_materialize_image_bindings();
        CHECK_GL_ERROR
    }
}

void glAttachShader(GLuint program, GLuint shader) {
    LOG()
    LOG_D("glAttachShader(%u, %u)", program, shader)
    MgShaderObject* shader_state = mg_find_shader(shader);
    if (shader_state && hardware->emulate_texture_buffer && shader_state->sampler_buffer_emulated) {
        program_map_is_sampler_buffer_emulated[program] = true;
    }

    MgProgramObject& program_state = g_mg_programs[mg_object_key(program)];
    if (std::find(program_state.attached_shaders.begin(), program_state.attached_shaders.end(), shader) ==
        program_state.attached_shaders.end()) {
        program_state.attached_shaders.push_back(shader);
    }
    MgShaderObject& owned_shader = g_mg_shaders[mg_object_key(shader)];
    owned_shader.attached_programs.insert(mg_object_key(program));

    GLint type = 0;
    GLES.glGetShaderiv(shader, GL_SHADER_TYPE, &type);
    auto& should_gen_fs_map = program_map_should_generate_fs;
    if (type == GL_FRAGMENT_SHADER) {
        should_gen_fs_map[program] = ShouldGenerateFSState::Never;
    } else if (type == GL_VERTEX_SHADER) {
        auto it = should_gen_fs_map.find(program);
        if (it == should_gen_fs_map.end() || should_gen_fs_map[program] != ShouldGenerateFSState::Never) {
            should_gen_fs_map[program] = ShouldGenerateFSState::Maybe;
        }
    }

    GLES.glAttachShader(program, shader);
    CHECK_GL_ERROR
}

void glDetachShader(GLuint program, GLuint shader) {
    LOG()
    LOG_D("glDetachShader(%u, %u)", program, shader)
    MgProgramObject* program_state = mg_find_program(program);
    if (program_state) {
        auto& attached = program_state->attached_shaders;
        attached.erase(std::remove(attached.begin(), attached.end(), shader), attached.end());
    }
    MgShaderObject* shader_state = mg_find_shader(shader);
    if (shader_state) {
        shader_state->attached_programs.erase(mg_object_key(program));
        if (shader_state->deleted && shader_state->attached_programs.empty()) {
            g_mg_shaders.erase(mg_object_key(shader));
        }
    }
    GLES.glDetachShader(program, shader);
    CHECK_GL_ERROR
}

void glDeleteProgram(GLuint program) {
    LOG()
    LOG_D("glDeleteProgram(%u)", program)
    MgProgramObject* program_state = mg_find_program(program);
    if (program_state) {
        for (GLuint shader : program_state->attached_shaders) {
            auto shader_it = g_mg_shaders.find(mg_object_key(shader));
            if (shader_it == g_mg_shaders.end()) continue;
            shader_it->second.attached_programs.erase(mg_object_key(program));
            if (shader_it->second.deleted && shader_it->second.attached_programs.empty()) {
                g_mg_shaders.erase(shader_it);
            }
        }
        g_mg_programs.erase(mg_object_key(program));
    }
    program_map_is_sampler_buffer_emulated.erase(program);
    program_map_should_generate_fs.erase(program);
    g_samplerCacheForSamplerBuffer.erase(program);
    if (gl_state->current_program == program) gl_state->current_program_has_image_uniforms = false;
    GLES.glDeleteProgram(program);
    CHECK_GL_ERROR
}

namespace {

bool rewrite_storage_block_binding(std::string& source, const std::string& block, GLuint binding) {
    const std::string block_pattern = R"(((?:\w+\s+)*)buffer\s+)";
    // Three shapes the translated ESSL can have for a storage block:
    //   layout(std430, binding = 0) buffer Name { ... };
    //   layout(std430) buffer Name { ... };
    //   buffer Name { ... };              (no qualifier at all)
    // Replace an existing binding or insert one, but only for the named block.
    const std::string layout_prefix = R"(layout\s*\(([^)]*)\)\s*)";
    std::regex existing(layout_prefix + block_pattern + block + R"(\b)");
    std::smatch match;
    if (std::regex_search(source, match, existing)) {
        std::string qualifiers = match[1].str();
        std::regex binding_re(R"(\bbinding\s*=\s*\d+)");
        if (std::regex_search(qualifiers, binding_re)) {
            qualifiers = std::regex_replace(qualifiers, binding_re, "binding = " + std::to_string(binding));
        } else {
            qualifiers += ", binding = " + std::to_string(binding);
        }
        const std::string replacement = "layout(" + qualifiers + ") " + match[2].str() + "buffer " + block;
        source.replace(match.position(), match.length(), replacement);
        return true;
    }

    std::regex bare(block_pattern + block + R"(\b)");
    if (std::regex_search(source, match, bare)) {
        source.insert(match.position(), "layout(binding = " + std::to_string(binding) + ") ");
        return true;
    }
    return false;
}

} // namespace

void glShaderStorageBlockBinding(GLuint program, GLuint storageBlockIndex, GLuint storageBlockBinding) {
    LOG()
    LOG_D("glShaderStorageBlockBinding(%u, %u, %u)", program, storageBlockIndex, storageBlockBinding)
    if (!GLES.glGetProgramResourceName) {
        mg_set_gl_error(GL_INVALID_OPERATION);
        return;
    }
    char name[256] = {0};
    GLsizei name_length = 0;
    GLES.glGetProgramResourceName(program, GL_SHADER_STORAGE_BLOCK, storageBlockIndex, sizeof(name) - 1,
                                  &name_length, name);
    if (name_length <= 0) {
        mg_set_gl_error(GL_INVALID_VALUE);
        return;
    }

    MgProgramObject& state = g_mg_programs[mg_object_key(program)];
    state.storage_block_bindings[name] = storageBlockBinding;

    bool changed = false;
    for (GLuint shader_name : state.attached_shaders) {
        MgShaderObject* shader = mg_find_shader(shader_name);
        if (!shader || shader->converted_source.empty()) continue;
        std::string rewritten = shader->converted_source;
        if (!rewrite_storage_block_binding(rewritten, name, storageBlockBinding)) continue;
        if (rewritten == shader->converted_source) continue;
        shader->converted_source = rewritten;
        const GLchar* source = shader->converted_source.data();
        GLint source_length = static_cast<GLint>(shader->converted_source.size());
        GLES.glShaderSource(shader_name, 1, &source, &source_length);
        GLES.glCompileShader(shader_name);
        GLint status = GL_FALSE;
        GLES.glGetShaderiv(shader_name, GL_COMPILE_STATUS, &status);
        if (status != GL_TRUE) {
            GLchar info_log[512] = {0};
            GLES.glGetShaderInfoLog(shader_name, sizeof(info_log), nullptr, info_log);
            LOG_E("Failed to compile shader %u after storage-block rebinding:\n%s", shader_name, info_log)
        }
        changed = true;
    }
    if (changed) {
        // The desktop API changes the binding without requiring a relink. The
        // backend has no equivalent call, so apply the source change and relink
        // through the frontend so fragment-output bindings are re-applied too.
        glLinkProgram(program);
    }
    CHECK_GL_ERROR
}

GLint glGetFragDataLocation(GLuint program, const GLchar* name) {
    LOG()
    LOG_D("glGetFragDataLocation(%u, %s)", program, name ? name : "(null)")
    if (name == nullptr) return -1;
    MgProgramObject* program_state = mg_find_program(program);
    if (!program_state) return -1;
    // The link-assigned location is authoritative, including layout(location)
    // and automatic assignment. glBindFragDataLocation is only the fallback
    // when the backend does not expose the query.
    if (GLES.glGetFragDataLocation) {
        const GLint driver_location = GLES.glGetFragDataLocation(program, name);
        if (driver_location >= 0) return driver_location;
    }
    auto it = program_state->frag_data_locations.find(name);
    return it == program_state->frag_data_locations.end() ? -1 : static_cast<GLint>(it->second);
}

GLuint glCreateProgram() {
    LOG()
    LOG_D("glCreateProgram")
    GLuint program = GLES.glCreateProgram();
    if (program != 0) g_mg_programs[mg_object_key(program)] = MgProgramObject{};
    if (hardware->emulate_texture_buffer) {
        program_map_is_sampler_buffer_emulated[program] = false;
        if (g_samplerCacheForSamplerBuffer.find(program) != g_samplerCacheForSamplerBuffer.end()) {
            g_samplerCacheForSamplerBuffer.erase(program);
        }
    }
    program_map_should_generate_fs[program] = ShouldGenerateFSState::Unknown;

    CHECK_GL_ERROR
    return program;
}

// GL 3.1's name-only half of the active-uniform query, on top of the ES call that
// already returns the same string.
//
// It was a stub -- a no-op that wrote neither the name nor the length and, being a
// stub rather than an error, left glGetError clean. Callers got whatever was
// already in the buffer they passed.
//
// That is not a cosmetic gap. The standard way to build a name -> location map is
// to walk the active uniforms by index and ask for each name, and a caller doing
// that ended up with a map keyed on garbage: every later lookup missed, so the
// uniforms never got set and kept whatever the driver had zero-initialised them
// to. NeoForge's early loading window does exactly this, and a screenSize of
// (0, 0) turned its every vertex into a division by zero -- gl_Position came out
// non-finite, every primitive was discarded, and the window rendered black with
// nothing anywhere reporting a problem.
void glGetActiveUniformName(GLuint program, GLuint uniformIndex, GLsizei bufSize, GLsizei* length,
                            GLchar* uniformName) {
    LOG()
    LOG_D("glGetActiveUniformName(program: %u, index: %u, bufSize: %d)", program, uniformIndex, bufSize)

    if (length) *length = 0;
    if (bufSize <= 0 || uniformName == nullptr) {
        // Nothing to write. Still forwarded when bufSize is negative so the driver
        // raises the GL_INVALID_VALUE the caller is owed.
        if (bufSize < 0) GLES.glGetActiveUniform(program, uniformIndex, bufSize, nullptr, nullptr, nullptr, nullptr);
        CHECK_GL_ERROR
        return;
    }

    // Same buffer contract in both calls: at most bufSize-1 characters plus the
    // terminator, and a length that excludes it. The size and type this also
    // returns are what glGetActiveUniformsiv is for; they are discarded here.
    GLint size = 0;
    GLenum type = 0;
    GLsizei written = 0;
    uniformName[0] = '\0';
    GLES.glGetActiveUniform(program, uniformIndex, bufSize, &written, &size, &type, uniformName);
    if (length) *length = written;

    LOG_D("  -> \"%s\"", uniformName)
    CHECK_GL_ERROR
}
