// MobileGlues - gl/image_bindings.cpp
// SPDX-License-Identifier: LGPL-2.1-only
#define DEBUG 0
#include "image_bindings.h"
#include "shader_state.h"
#include "mg.h"
#include "../egl/context.h"

#include <memory>
#include <mutex>
#include <regex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {
    std::string strip_array_suffix(std::string name) {
        const size_t bracket = name.find('[');
        if (bracket != std::string::npos) name.resize(bracket);
        return name;
    }

    // Fallback assignment for image uniforms that name no usable unit. Scoped
    // per share group; the unique_ptr keeps entries stable while the outer map
    // rehashes.
    struct fallback_units_t {
        std::mutex mutex;
        std::unordered_map<std::string, GLuint> units;
        GLuint next_unit = 0;
    };

    std::mutex g_fallback_map_mutex;
    std::unordered_map<unsigned long long, std::unique_ptr<fallback_units_t>> g_fallback_units;

    fallback_units_t& fallback_units(unsigned long long share_group) {
        std::lock_guard<std::mutex> lock(g_fallback_map_mutex);
        auto& slot = g_fallback_units[share_group];
        if (!slot) slot = std::make_unique<fallback_units_t>();
        return *slot;
    }

    GLuint fallback_unit(unsigned long long share_group, const std::string& name) {
        fallback_units_t& state = fallback_units(share_group);
        std::lock_guard<std::mutex> lock(state.mutex);
        auto it = state.units.find(name);
        if (it != state.units.end()) return it->second;
        constexpr GLuint kFallbackImageUnitLimit = 32;
        if (state.next_unit >= kFallbackImageUnitLimit) {
            LOG_E("Image fallback: more than %u image names in one share group; '%s' reuses unit 0",
                  kFallbackImageUnitLimit, name.c_str())
            return 0;
        }
        const GLuint unit = state.next_unit++;
        state.units.emplace(name, unit);
        return unit;
    }

    struct image_decl_t {
        size_t position = 0;
        size_t length = 0;
        std::string layout;
        std::string qualifiers_before;
        std::string precision;
        std::string qualifiers_after;
        std::string type;
        std::string name;
        bool has_binding = false;
        GLuint binding = 0;
    };

    // Captures the whole declaration, including a preceding layout group and
    // image qualifiers on either side of `uniform`. The whole-source iterator
    // handles several declarations on one line and declarations whose layout
    // sits on a previous line.
    const std::regex& image_decl_regex() {
        static const std::regex re(
            R"((layout\s*\([^)]*\)\s*)?((?:(?:coherent|volatile|restrict|readonly|writeonly)\s+)*)uniform\s+((?:(?:highp|mediump|lowp)\s+)?)((?:(?:coherent|volatile|restrict|readonly|writeonly)\s+)*)(u?image(?:1D|2D|3D|Cube|CubeArray|2DArray|Buffer))\s+([A-Za-z_][A-Za-z0-9_]*))");
        return re;
    }

    const std::regex& binding_regex() {
        static const std::regex re(R"(\bbinding\s*=\s*(\d+))");
        return re;
    }

    std::string trim(std::string value) {
        while (!value.empty() && (value.front() == ' ' || value.front() == '\t' || value.front() == ','))
            value.erase(value.begin());
        while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == ','))
            value.pop_back();
        return value;
    }

    std::string layout_formats(const std::string& layout) {
        const size_t open = layout.find('(');
        const size_t close = layout.rfind(')');
        if (open == std::string::npos || close == std::string::npos || close <= open) return {};
        std::string contents = layout.substr(open + 1, close - open - 1);
        std::string formats;
        size_t start = 0;
        while (start <= contents.size()) {
            const size_t comma = contents.find(',', start);
            const size_t end = comma == std::string::npos ? contents.size() : comma;
            std::string token = trim(contents.substr(start, end - start));
            if (!token.empty() && token.find("binding") == std::string::npos) {
                if (!formats.empty()) formats += ", ";
                formats += token;
            }
            if (comma == std::string::npos) break;
            start = comma + 1;
        }
        return formats;
    }

    std::vector<image_decl_t> find_image_decls(const std::string& source) {
        std::vector<image_decl_t> decls;
        const std::regex& re = image_decl_regex();
        for (std::sregex_iterator it(source.begin(), source.end(), re), end; it != end; ++it) {
            image_decl_t decl;
            decl.position = static_cast<size_t>(it->position(0));
            decl.length = static_cast<size_t>(it->length(0));
            decl.layout = (*it)[1].str();
            decl.qualifiers_before = (*it)[2].str();
            decl.precision = (*it)[3].str();
            decl.qualifiers_after = (*it)[4].str();
            decl.type = (*it)[5].str();
            decl.name = (*it)[6].str();
            std::smatch binding_match;
            if (!decl.layout.empty() && std::regex_search(decl.layout, binding_match, binding_regex())) {
                decl.has_binding = true;
                decl.binding = static_cast<GLuint>(std::stoul(binding_match[1].str()));
            }
            decls.push_back(std::move(decl));
        }
        return decls;
    }

    // Rewrites every declaration whose name appears in `units`. When
    // `observed` is non-null it receives the name -> unit map actually emitted,
    // so reflection does not have to parse the source again.
    void rewrite_image_decls(std::string& source, const std::vector<image_decl_t>& decls,
                             const std::unordered_map<std::string, GLuint>& units,
                             std::unordered_map<std::string, GLuint>* observed) {
        std::string result;
        result.reserve(source.size() + decls.size() * 32);
        size_t last = 0;
        bool changed = false;
        for (const image_decl_t& decl : decls) {
            auto unit_it = units.find(decl.name);
            if (unit_it == units.end()) continue;
            result.append(source, last, decl.position - last);
            std::string formats = layout_formats(decl.layout);
            std::string layout = "layout(binding = " + std::to_string(unit_it->second);
            if (!formats.empty()) layout += ", " + formats;
            layout += ") ";
            result += layout + decl.qualifiers_before + "uniform " + decl.precision + decl.qualifiers_after + decl.type +
                      " " + decl.name;
            if (observed) (*observed)[decl.name] = unit_it->second;
            last = decl.position + decl.length;
            changed = true;
        }
        if (!changed) return;
        result.append(source, last, std::string::npos);
        source.swap(result);
    }
} // namespace

bool mg_is_image_uniform_type(GLenum type) {
    // GL_IMAGE_1D .. GL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE_ARRAY.
    return type >= 0x904C && type <= 0x906C;
}

void mg_bind_image_uniforms(std::string& source, unsigned long long share_group,
                            std::unordered_map<std::string, GLuint>* observed_units) {
    if (source.empty()) return;
    if (observed_units) observed_units->clear();

    const std::vector<image_decl_t> decls = find_image_decls(source);
    std::unordered_map<GLuint, int> binding_count;
    for (const image_decl_t& decl : decls) {
        if (decl.has_binding) binding_count[decl.binding]++;
    }

    // A repeated binding is an alias, not an authoritative mapping. The Iris
    // bridge emits unique bindings; Iris's default image declarations do not.
    std::unordered_set<GLuint> ambiguous_bindings;
    for (const auto& entry : binding_count) {
        if (entry.second > 1) ambiguous_bindings.insert(entry.first);
    }

    std::unordered_map<std::string, GLuint> units;
    for (const image_decl_t& decl : decls) {
        if (decl.has_binding && ambiguous_bindings.find(decl.binding) == ambiguous_bindings.end()) {
            units[decl.name] = decl.binding;
        } else {
            units[decl.name] = fallback_unit(share_group, decl.name);
        }
    }
    rewrite_image_decls(source, decls, units, observed_units);
}

void mg_resolve_duplicate_image_bindings(GLuint program) {
    MgProgramObject* state = mg_find_program(program);
    if (state == nullptr || state->image_uniform_units.size() < 2) return;

    std::unordered_map<std::string, GLuint> assignment;
    std::unordered_map<GLuint, std::string> unit_owner;
    bool changed = false;
    for (const auto& entry : state->image_uniform_units) {
        auto name_it = state->image_uniform_names.find(entry.first);
        if (name_it == state->image_uniform_names.end()) continue;
        const std::string& name = name_it->second;
        auto assigned = assignment.find(name);
        if (assigned != assignment.end()) {
            if (assigned->second != entry.second) changed = true;
            continue;
        }
        GLuint unit = entry.second;
        auto owner = unit_owner.find(unit);
        if (owner != unit_owner.end() && owner->second != name) {
            unit = 0;
            while (unit_owner.find(unit) != unit_owner.end()) unit++;
            changed = true;
        }
        assignment[name] = unit;
        unit_owner[unit] = name;
    }
    if (!changed) return;

    bool compiled = true;
    for (GLuint shader_name : state->attached_shaders) {
        MgShaderObject* shader = mg_find_shader(shader_name);
        if (shader == nullptr || shader->converted_source.empty()) continue;
        std::vector<image_decl_t> decls = find_image_decls(shader->converted_source);
        std::unordered_map<std::string, GLuint> observed;
        rewrite_image_decls(shader->converted_source, decls, assignment, &observed);
        if (!observed.empty()) shader->image_binding_units = observed;
        const GLchar* source = shader->converted_source.data();
        GLint source_length = static_cast<GLint>(shader->converted_source.size());
        GLES.glShaderSource(shader_name, 1, &source, &source_length);
        GLES.glCompileShader(shader_name);
        GLint status = GL_FALSE;
        GLES.glGetShaderiv(shader_name, GL_COMPILE_STATUS, &status);
        if (status != GL_TRUE) {
            GLchar info_log[512] = {0};
            GLES.glGetShaderInfoLog(shader_name, sizeof(info_log), nullptr, info_log);
            LOG_E("Failed to compile shader %u after image-unit separation:\n%s", shader_name, info_log)
            compiled = false;
        }
    }
    if (!compiled) return;

    GLES.glLinkProgram(program);
    GLint link_status = GL_FALSE;
    GLES.glGetProgramiv(program, GL_LINK_STATUS, &link_status);
    if (link_status != GL_TRUE) {
        LOG_E("Program %u failed to relink after image-unit separation", program)
        return;
    }
    mg_reflect_image_uniforms(program);
    LOG_W_FORCE("Image bindings: program %u relinked to separate duplicate image units", program)
}

void mg_reflect_image_uniforms(GLuint program) {
    MgProgramObject* state = mg_find_program(program);
    if (state == nullptr) return;
    state->image_uniform_units.clear();
    state->image_uniform_names.clear();
    state->image_unit_remap.clear();
    state->image_unit_names.clear();
    state->image_declaration_stage.clear();

    GLint count = 0;
    GLES.glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &count);
    for (GLint i = 0; i < count; i++) {
        char name[256] = {0};
        GLsizei written = 0;
        GLint size = 0;
        GLenum type = 0;
        GLES.glGetActiveUniform(program, i, sizeof(name) - 1, &written, &size, &type, name);
        if (!mg_is_image_uniform_type(type) || written <= 0) continue;

        std::string base = strip_array_suffix(name);
        const GLint location = GLES.glGetUniformLocation(program, base.c_str());
        if (location < 0) continue;

        GLuint unit = 0;
        bool found = false;
        for (GLuint shader_name : state->attached_shaders) {
            MgShaderObject* shader = mg_find_shader(shader_name);
            if (shader == nullptr) continue;
            auto it = shader->image_binding_units.find(base);
            if (it != shader->image_binding_units.end()) {
                unit = it->second;
                found = true;
                state->image_declaration_stage[base] = shader->type;
                break;
            }
        }
        if (!found) {
            // Should not happen for a declaration produced by the transformer;
            // assign a stable slot without growing a process-global table.
            unit = static_cast<GLuint>(state->image_unit_names.size());
        }
        state->image_uniform_units[location] = unit;
        state->image_uniform_names[location] = base;
        state->image_unit_names[unit] = base;
    }
    if (!state->image_uniform_units.empty()) {
        LOG_W_FORCE("Image bindings: program %u has %zu active image uniforms", program,
                    state->image_uniform_units.size())
    }
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
bool mg_image_uniform_value(GLint location, GLint value, GLint* out_value) {
    if (out_value) *out_value = value;
    if (value < 0) return false;
    MgProgramObject* state = mg_find_program(gl_state->current_program);
    if (state == nullptr) return false;
    auto it = state->image_uniform_units.find(location);
    if (it == state->image_uniform_units.end()) return false;
    const GLuint app_unit = static_cast<GLuint>(value);
    if (state->image_unit_remap.find(app_unit) == state->image_unit_remap.end()) {
        LOG_W_FORCE("Image bindings: program %u location %d app unit %u -> core unit %u", gl_state->current_program,
                    location, app_unit, it->second)
    }
    state->image_unit_remap[app_unit] = it->second;
    if (out_value) *out_value = static_cast<GLint>(it->second);
    return true;
}

GLuint mg_remap_image_unit(GLuint unit) {
    MgProgramObject* state = mg_find_program(gl_state->current_program);
    if (state == nullptr) return unit;
    auto it = state->image_unit_remap.find(unit);
    return it == state->image_unit_remap.end() ? unit : it->second;
}
