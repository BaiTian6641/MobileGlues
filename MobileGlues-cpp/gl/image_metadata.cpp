// MobileGlues - gl/image_metadata.cpp
// SPDX-License-Identifier: LGPL-2.1-only
#include "image_metadata.h"
#include "image_format.h"
#include "glsl/image_source_mask.h"

#include <cstring>
#include <regex>
#include <vector>

namespace {
struct PendingMetadata {
    std::unordered_map<std::string, GLenum> formats;
    bool active = false;
};

thread_local PendingMetadata g_pending;
} // namespace

extern "C" __attribute__((visibility("default"))) void mg_image_metadata_begin() {
    g_pending.formats.clear();
    g_pending.active = true;
}

extern "C" __attribute__((visibility("default"))) void mg_image_metadata_add(const char* name, GLenum format) {
    if (name == nullptr || *name == '\0') return;
    if (!g_pending.active) g_pending.active = true;
    g_pending.formats[name] = format;
}

extern "C" __attribute__((visibility("default"))) void mg_image_metadata_end() {
    g_pending.active = false;
}

std::unordered_map<std::string, GLenum> mg_image_metadata_take() {
    std::unordered_map<std::string, GLenum> result;
    // end() closes registration but the next source still has to consume what
    // was registered. Only the presence of entries matters here.
    if (g_pending.formats.empty()) {
        g_pending.active = false;
        return result;
    }
    result.swap(g_pending.formats);
    g_pending.active = false;
    return result;
}

GLenum mg_image_backing_format(GLenum logical_format) {
    const auto* entry = mg_find_image_format(logical_format);
    return entry != nullptr ? entry->backing : GL_NONE;
}

GLenum mg_image_unbind_format(GLenum logical_format) {
    return mg_is_desktop_image_format(logical_format) ? GL_RGBA8 : GL_NONE;
}

GLenum mg_image_unit_format(GLenum logical_format, GLenum texture_internal_format) {
    const GLenum backing = mg_image_backing_format(logical_format);
    if (backing == GL_NONE || texture_internal_format == 0) return backing;

    // Retain the established float-backed unorm16 compatibility path. A raw
    // data-backed RGBA16 is not a legal ES image-unit format, even when the
    // provider exposes EXT_texture_norm16 for ordinary texture storage.
    if ((logical_format == GL_RGBA16 || logical_format == GL_RG16) &&
        (texture_internal_format == GL_RGBA16F || texture_internal_format == GL_RGBA32F)) {
        return texture_internal_format;
    }
    if (backing == texture_internal_format) return backing;
    const unsigned storage_bits = mg_image_format_bits(texture_internal_format);
    const unsigned binding_bits = mg_image_format_bits(backing);
    // GL-allocated texture image views are compatible by size. Do not confuse
    // widening the binding enum with widening the texture's actual storage.
    return storage_bits != 0 && storage_bits == binding_bits ? backing : GL_NONE;
}

const char* mg_image_qualifier(GLenum logical_format) {
    const auto* entry = mg_find_image_format(logical_format);
    return entry != nullptr ? entry->essl_qualifier : nullptr;
}

void mg_normalize_image_qualifiers(std::string& source, const std::unordered_map<std::string, GLenum>& formats) {
    if (formats.empty()) return;

    // One declaration per line is what the shaderpacks and Iris emit. Match the
    // uniform declaration, capture the variable name, and rewrite only that
    // declaration's layout qualifier.
    static const std::regex image_decl(
        R"((.*\buniform\s+)((?:(?:highp|mediump|lowp|coherent|volatile|restrict|readonly|writeonly)\s+)*)([iu]?image(?:1D|2D|3D|Cube|CubeArray|2DArray|Buffer))\s+([A-Za-z_][A-Za-z0-9_]*)\s*;.*)");
    const std::string masked = mg_image_code_mask(source);
    std::string result;
    result.reserve(source.size() + 64);
    size_t start = 0;
    while (start <= source.size()) {
        const size_t end = source.find('\n', start);
        const size_t length = (end == std::string::npos) ? source.size() - start : end - start;
        std::string line = source.substr(start, length);
        const std::string code_line = masked.substr(start, length);
        std::smatch match;
        if (std::regex_match(code_line, match, image_decl)) {
            const std::string name = match[4].str();
            const std::string image_type = match[3].str();
            auto it = formats.find(name);
            // Explicit unorm16 declarations retain the established compat_image
            // policy, including its documented colorimg8 exception. Formatless
            // unorm16 declarations still need the registered backing qualifier.
            if (it != formats.end() &&
                !((it->second == GL_RGBA16 || it->second == GL_RG16) &&
                  std::regex_search(code_line, std::regex(R"(\b(?:rgba16|rg16|rgba16f|rgba32f)\b)")))) {
                const char* qualifier = mg_image_qualifier(it->second);
                if (qualifier != nullptr) {
                    const std::string layout_qualifier = std::string("layout(") + qualifier + ") ";
                    const size_t uniform_pos = code_line.find("uniform");
                    const size_t layout_pos = code_line.rfind("layout", uniform_pos);
                    if (layout_pos != std::string::npos) {
                        const size_t open = code_line.find('(', layout_pos);
                        const size_t close = code_line.find(')', layout_pos);
                        if (open != std::string::npos && close != std::string::npos) {
                            std::string contents = line.substr(open + 1, close - open - 1);
                            // Replace a whole layout item using the comment
                            // mask, preserving binding expressions and comments.
                            const std::string code_contents = code_line.substr(open + 1, close - open - 1);
                            static const std::regex format_item(R"(r(?:g(?:b(?:a)?)?)?[0-9]+[a-zA-Z0-9_]*)");
                            bool replaced = false;
                            size_t item_start = 0;
                            while (item_start <= code_contents.size()) {
                                const size_t comma = code_contents.find(',', item_start);
                                const size_t item_end = comma == std::string::npos ? code_contents.size() : comma;
                                const std::string item = code_contents.substr(item_start, item_end - item_start);
                                const size_t first = item.find_first_not_of(" \t\r\n");
                                const size_t last = item.find_last_not_of(" \t\r\n");
                                if (first != std::string::npos &&
                                    std::regex_match(item.substr(first, last - first + 1), format_item)) {
                                    contents.replace(item_start + first, last - first + 1, qualifier);
                                    replaced = true;
                                    break;
                                }
                                if (comma == std::string::npos) break;
                                item_start = comma + 1;
                            }
                            if (!replaced) {
                                if (code_contents.find_first_not_of(" \t\r\n") != std::string::npos) contents.insert(0, ", ");
                                contents.insert(0, qualifier);
                            }
                            std::string replacement = "layout(" + contents + ")";
                            line.replace(layout_pos, close - layout_pos + 1, replacement);
                        }
                    } else {
                        line.insert(uniform_pos, layout_qualifier);
                    }
                    // ESSL requires a precision qualifier on image uniforms.
                    // The desktop pack omits it; add highp unless the source
                    // already names one.
                    const std::string updated_mask = mg_image_code_mask(line);
                    const size_t uniform_after_layout = updated_mask.find("uniform");
                    if (uniform_after_layout != std::string::npos) {
                        size_t after = uniform_after_layout + strlen("uniform");
                        while (after < line.size() && (line[after] == ' ' || line[after] == '\t')) ++after;
                        const size_t type_pos = updated_mask.find(image_type, after);
                        const std::string qualifier_run =
                            type_pos == std::string::npos ? std::string() : line.substr(after, type_pos - after);
                        const bool has_precision = qualifier_run.find("highp") != std::string::npos ||
                                                   qualifier_run.find("mediump") != std::string::npos ||
                                                   qualifier_run.find("lowp") != std::string::npos;
                        if (!has_precision && type_pos != std::string::npos) line.insert(type_pos, "highp ");
                    }
                } else {
                    const auto* entry = mg_find_image_format(it->second);
                    std::string diagnostic = "#error MG_IMAGE_FORMAT_UNMAPPABLE: ";
                    diagnostic += name;
                    if (entry != nullptr) {
                        diagnostic += " (";
                        diagnostic += entry->desktop_qualifier;
                        diagnostic += "; store conversion is not implemented)";
                    } else {
                        diagnostic += " (unknown format)";
                    }
                    diagnostic += '\n';
                    line.insert(0, diagnostic);
                }
            }
        }
        result += line;
        if (end == std::string::npos) break;
        result += '\n';
        start = end + 1;
    }
    source.swap(result);
}
