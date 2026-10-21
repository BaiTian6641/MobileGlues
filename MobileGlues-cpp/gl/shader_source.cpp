// MobileGlues - gl/shader_source.cpp
// SPDX-License-Identifier: LGPL-2.1-only
#include "shader_source.h"

#include <cctype>
#include <cstring>
#include <limits>
#include <regex>
#include <vector>

bool mg_collect_shader_source(GLsizei count, const GLchar* const* strings, const GLint* lengths,
                              std::string* out, GLenum* error) {
    if (error) *error = GL_NO_ERROR;
    if (count < 0 || (count > 0 && strings == nullptr)) {
        if (error) *error = GL_INVALID_VALUE;
        return false;
    }

    std::string collected;
    size_t total = 0;
    for (GLsizei i = 0; i < count; ++i) {
        const GLchar* string = strings[i];
        if (string == nullptr) {
            if (error) *error = GL_INVALID_VALUE;
            return false;
        }
        if (lengths && lengths[i] >= 0) {
            total += static_cast<size_t>(lengths[i]);
        } else {
            total += strlen(string);
        }
    }
    collected.reserve(total);

    for (GLsizei i = 0; i < count; ++i) {
        const GLchar* string = strings[i];
        if (lengths && lengths[i] >= 0) {
            collected.append(string, static_cast<size_t>(lengths[i]));
        } else {
            collected.append(string);
        }
    }

    *out = std::move(collected);
    return true;
}


namespace {

std::string mg_trim(const std::string& value) {
    size_t begin = 0;
    while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin]))) ++begin;
    size_t end = value.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1]))) --end;
    return value.substr(begin, end - begin);
}

// Remove comments and string contents for structural scanning. The original
// line is preserved separately for output; this copy only decides brace depth
// and whether a declaration is really at global scope.
std::string mg_structural_line(const std::string& line, bool& in_block_comment) {
    std::string out;
    out.reserve(line.size());
    bool in_string = false;
    char quote = 0;
    for (size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        const char n = i + 1 < line.size() ? line[i + 1] : '\0';
        if (in_block_comment) {
            if (c == '*' && n == '/') {
                in_block_comment = false;
                ++i;
            }
            continue;
        }
        if (in_string) {
            if (c == '\\' && i + 1 < line.size()) {
                ++i;
            } else if (c == quote) {
                in_string = false;
            }
            continue;
        }
        if (c == '/' && n == '/') break;
        if (c == '/' && n == '*') {
            in_block_comment = true;
            ++i;
            continue;
        }
        if (c == '"' || c == '\'') {
            in_string = true;
            quote = c;
            continue;
        }
        out.push_back(c);
    }
    return out;
}

bool mg_looks_like_function(const std::string& text) {
    if (text.empty() || text[0] == '#' || text.back() == ';') return false;
    const std::string lower_start = text.substr(0, text.find_first_of(" \t("));
    static const char* control[] = {"if", "for", "while", "switch", "struct", "uniform", "layout"};
    for (const char* word : control) {
        if (lower_start == word) return false;
    }
    return text.find('(') != std::string::npos && text.find(')') != std::string::npos;
}

// Only built-in, dependency-free declarations may cross earlier declarations.
// Custom types, symbolic array sizes, initializers and macros need an AST-aware
// dependency pass; leave them in place so compilation remains authoritative.
bool mg_is_plain_uniform(const std::string& text) {
    static const std::regex declaration(
        R"(^uniform\s+(?:(?:lowp|mediump|highp)\s+)?(?:bool|int|uint|float|[biu]?vec[234]|mat[234](?:x[234])?)\s+[A-Za-z_][A-Za-z_0-9]*(?:\s*\[\s*[0-9]+\s*\])?\s*;\s*$)");
    return std::regex_match(text, declaration);
}

} // namespace

std::string mg_hoist_late_global_uniforms(const std::string& source) {
    // A continued macro can contain function/declaration-shaped text. This
    // line-based pass cannot establish its dependencies; leave it to the parser.
    if (source.find("\\\n") != std::string::npos || source.find("\\\r\n") != std::string::npos) return source;
    std::vector<std::string> lines;
    std::string current;
    for (char c : source) {
        if (c == '\n') {
            lines.push_back(current);
            current.clear();
        } else {
            current.push_back(c);
        }
    }
    if (!current.empty()) lines.push_back(current);

    bool in_block_comment = false;
    int brace_depth = 0;
    int conditional_depth = 0;
    int first_function = -1;
    int statement_start = -1;
    bool directive_barrier = false;
    std::vector<int> late_uniforms;

    for (size_t i = 0; i < lines.size(); ++i) {
        const bool started_in_comment = in_block_comment;
        const std::string structural = mg_structural_line(lines[i], in_block_comment);
        const std::string text = mg_trim(structural);

        if (text.rfind("#if", 0) == 0) {
            ++conditional_depth;
        } else if (text.rfind("#endif", 0) == 0) {
            if (conditional_depth > 0) --conditional_depth;
        }

        const bool directive = !text.empty() && text.front() == '#';
        const bool precision_statement = text == "precision" ||
            (text.rfind("precision", 0) == 0 && text.size() > 9 &&
             std::isspace(static_cast<unsigned char>(text[9])));
        if (first_function >= 0 && (directive || precision_statement)) {
            directive_barrier = true;
        }
        if (brace_depth == 0 && !text.empty() && !directive && statement_start < 0) {
            if (started_in_comment) return source;
            statement_start = static_cast<int>(i);
        }
        if (conditional_depth == 0 && brace_depth == 0 && first_function < 0 &&
            mg_looks_like_function(text)) {
            // Inserting before a comment terminator would hide the declaration.
            if (started_in_comment) return source;
            // Include a return type on the preceding line in the insertion anchor.
            first_function = statement_start;
        }
        if (first_function >= 0 && static_cast<int>(i) > first_function &&
            conditional_depth == 0 && brace_depth == 0 && !directive_barrier &&
            !started_in_comment && !in_block_comment && mg_is_plain_uniform(text)) {
            late_uniforms.push_back(static_cast<int>(i));
        }

        for (char c : structural) {
            if (c == '{') ++brace_depth;
            else if (c == '}' && brace_depth > 0) --brace_depth;
        }
        if (directive || (brace_depth == 0 && !text.empty() &&
                          (text.back() == ';' || text.back() == '}'))) {
            statement_start = -1;
        }
    }

    if (first_function < 0 || late_uniforms.empty()) return source;

    std::vector<bool> late_mask(lines.size(), false);
    for (int index : late_uniforms) late_mask[static_cast<size_t>(index)] = true;

    std::vector<std::string> reordered;
    reordered.reserve(lines.size());
    for (size_t i = 0; i < lines.size(); ++i) {
        if (static_cast<int>(i) == first_function) {
            for (int index : late_uniforms) reordered.push_back(lines[static_cast<size_t>(index)]);
        }
        if (!late_mask[i]) reordered.push_back(lines[i]);
    }
    std::string rewritten;
    rewritten.reserve(source.size());
    for (size_t i = 0; i < reordered.size(); ++i) {
        if (i != 0) rewritten.push_back('\n');
        rewritten.append(reordered[i]);
    }
    if (!source.empty() && source.back() == '\n') rewritten.push_back('\n');
    return rewritten;
}
