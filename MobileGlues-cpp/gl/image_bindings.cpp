// MobileGlues - gl/image_bindings.cpp
// SPDX-License-Identifier: LGPL-2.1-only
#define DEBUG 0
#include "image_bindings.h"
#include "shader_state.h"
#include "mg.h"
#include "../egl/context.h"

#include <cstdlib>
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

    GLuint fallback_unit(unsigned long long share_group, const std::string& name,
                         const std::unordered_set<GLuint>& used) {
        fallback_units_t& state = fallback_units(share_group);
        std::lock_guard<std::mutex> lock(state.mutex);
        constexpr GLuint kFallbackImageUnitLimit = 32;
        auto it = state.units.find(name);
        if (it != state.units.end() && !used.count(it->second)) return it->second;
        while (state.next_unit < kFallbackImageUnitLimit && used.count(state.next_unit)) ++state.next_unit;
        if (state.next_unit >= kFallbackImageUnitLimit) {
            LOG_E("Image fallback: more than %u image names in one share group; '%s' reuses unit 0",
                  kFallbackImageUnitLimit, name.c_str())
            return 0;
        }
        const GLuint unit = state.next_unit++;
        state.units[name] = unit;
        return unit;
    }

    struct image_decl_t {
        size_t position = 0;
        size_t length = 0;
        std::string layout;
        std::string qualifiers_before;
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
            R"((layout\s*\([^)]*\)\s*)?((?:(?:coherent|volatile|restrict|readonly|writeonly)\s+)*)uniform\s+((?:(?:coherent|volatile|restrict|readonly|writeonly|highp|mediump|lowp)\s+)*)(u?image(?:1D|2D|3D|Cube|CubeArray|2DArray|Buffer))\s+([A-Za-z_][A-Za-z0-9_]*))");
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
            decl.qualifiers_after = (*it)[3].str();
            decl.type = (*it)[4].str();
            decl.name = (*it)[5].str();
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
            result += layout + decl.qualifiers_before + "uniform " + decl.qualifiers_after + decl.type + " " +
                      decl.name;
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

    // Explicit units are reserved before any fallback is chosen. Otherwise a
    // bare declaration could receive unit 0 while another declaration in the
    // same program explicitly binds unit 0, aliasing the two images.
    std::unordered_set<GLuint> used_units;
    for (const image_decl_t& decl : decls) {
        if (decl.has_binding) used_units.insert(decl.binding);
    }
    std::unordered_map<std::string, GLuint> units;
    for (const image_decl_t& decl : decls) {
        if (decl.has_binding && ambiguous_bindings.find(decl.binding) == ambiguous_bindings.end()) {
            units[decl.name] = decl.binding;
        } else {
            const GLuint unit = fallback_unit(share_group, decl.name, used_units);
            units[decl.name] = unit;
            used_units.insert(unit);
        }
    }
    rewrite_image_decls(source, decls, units, observed_units);
}

void mg_resolve_duplicate_image_bindings(GLuint program) {
    MgProgramObject* state = mg_find_program(program);
    if (state == nullptr || state->image_uniform_units.size() < 2) return;

    // One image array owns a run of consecutive units and consecutive uniform
    // locations, so a name covers [lowest location, highest location] and
    // occupies that many units starting at the unit of its first element.
    struct name_span_t {
        GLint base_location = 0;
        GLint last_location = 0;
        GLuint unit = 0;
        bool have_unit = false;
    };
    std::unordered_map<std::string, name_span_t> spans;
    for (const auto& entry : state->image_uniform_units) {
        auto name_it = state->image_uniform_names.find(entry.first);
        if (name_it == state->image_uniform_names.end()) continue;
        name_span_t& span = spans[name_it->second];
        if (!span.have_unit || entry.first < span.base_location) {
            span.base_location = entry.first;
            span.unit = entry.second;
            span.have_unit = true;
        }
        if (entry.first > span.last_location) span.last_location = entry.first;
    }

    std::unordered_map<std::string, GLuint> assignment;
    std::unordered_map<GLuint, std::string> unit_owner;
    bool changed = false;
    for (const auto& entry : spans) {
        const std::string& name = entry.first;
        const name_span_t& span = entry.second;
        if (!span.have_unit) continue;
        const GLuint elements = static_cast<GLuint>(span.last_location - span.base_location + 1);
        auto overlaps_other = [&](GLuint candidate) {
            for (GLuint offset = 0; offset < elements; ++offset) {
                auto owner = unit_owner.find(candidate + offset);
                if (owner != unit_owner.end() && owner->second != name) return true;
            }
            return false;
        };
        GLuint unit = span.unit;
        if (overlaps_other(unit)) {
            unit = 0;
            while (overlaps_other(unit)) ++unit;
            changed = true;
        }
        assignment[name] = unit;
        for (GLuint offset = 0; offset < elements; ++offset) unit_owner[unit + offset] = name;
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

        std::string uniform_name(name, static_cast<size_t>(written));
        std::string base = strip_array_suffix(uniform_name);
        const GLint location = GLES.glGetUniformLocation(program, base.c_str());
        if (location < 0) continue;

        // Arrays are reported once as name[0] with the element count, but a
        // provider that reports each element separately still names its index.
        // Parsing the index keeps element locations correct in both shapes.
        GLint array_index = 0;
        const size_t bracket = uniform_name.find('[');
        if (bracket != std::string::npos) array_index = std::atoi(uniform_name.c_str() + bracket + 1);

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
        // An image array's elements take consecutive units from the binding
        // the transformer wrote on the declaration, and consecutive uniform
        // locations from the array's base. Registering every element lets a
        // 1iv write remap each one instead of only the first.
        const GLint elements = size > 0 ? size : 1;
        for (GLint element = array_index; element < array_index + elements; ++element) {
            const GLint element_location = location + element;
            const GLuint element_unit = unit + static_cast<GLuint>(element);
            state->image_uniform_units[element_location] = element_unit;
            state->image_uniform_names[element_location] = base;
            state->image_unit_names[element_unit] = base;
        }
    }
    state->has_image_uniforms = !state->image_uniform_units.empty();
    if (state->has_image_uniforms) {
        LOG_W_FORCE("Image bindings: program %u has %zu active image uniforms", program,
                    state->image_uniform_units.size())
    }
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
bool mg_image_uniform_value_for_program(GLuint program, GLint location, GLint value, GLint* out_value) {
    if (out_value) *out_value = value;
    if (value < 0) return false;
    MgProgramObject* state = mg_find_program(program);
    if (state == nullptr || !state->has_image_uniforms) return false;
    auto it = state->image_uniform_units.find(location);
    if (it == state->image_uniform_units.end()) return false;
    const GLuint app_unit = static_cast<GLuint>(value);
    if (state->image_unit_remap.find(app_unit) == state->image_unit_remap.end()) {
        LOG_W_FORCE("Image bindings: program %u location %d app unit %u -> core unit %u", program, location, app_unit,
                    it->second)
    }
    state->image_unit_remap[app_unit] = it->second;
    if (out_value) *out_value = static_cast<GLint>(it->second);
    return true;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
bool mg_image_uniform_value(GLint location, GLint value, GLint* out_value) {
    return mg_image_uniform_value_for_program(gl_state->current_program, location, value, out_value);
}

GLuint mg_remap_image_unit(GLuint unit) {
    MgProgramObject* state = mg_find_program(gl_state->current_program);
    if (state == nullptr || !state->has_image_uniforms) return unit;
    auto it = state->image_unit_remap.find(unit);
    return it == state->image_unit_remap.end() ? unit : it->second;
}
