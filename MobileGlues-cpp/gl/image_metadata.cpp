// MobileGlues - gl/image_metadata.cpp
// SPDX-License-Identifier: LGPL-2.1-only
#include "image_metadata.h"

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
    switch (logical_format) {
    // Strict ESSL 3.10 image format qualifiers do not include r8/r16/r8ui/
    // r16ui/r8i/r16i. Widen to the narrowest legal integer/float image format.
    case GL_R8UI:
    case GL_R16UI:
        return GL_R32UI;
    case GL_R8I:
    case GL_R16I:
        return GL_R32I;
    case GL_R8:
    case GL_R16:
    case GL_R16F:
        return GL_R32F;
    case GL_RG16F:
        return GL_RG16F; // legal image qualifier on ES 3.10 where supported
    case GL_RGBA16F:
        return GL_RGBA16F;
    case GL_RGBA32F:
        return GL_RGBA32F;
    case GL_RGBA8:
        return GL_RGBA8;
    case GL_RGBA8_SNORM:
        return GL_RGBA8_SNORM;
    default:
        return logical_format;
    }
}

const char* mg_image_qualifier(GLenum logical_format) {
    switch (mg_image_backing_format(logical_format)) {
    case GL_R32UI:
        return "r32ui";
    case GL_R32I:
        return "r32i";
    case GL_R32F:
        return "r32f";
    case GL_RG16F:
        return "rg16f";
    case GL_RGBA16F:
        return "rgba16f";
    case GL_RGBA32F:
        return "rgba32f";
    case GL_RGBA8:
        return "rgba8";
    case GL_RGBA8_SNORM:
        return "rgba8_snorm";
    default:
        return nullptr;
    }
}

void mg_normalize_image_qualifiers(std::string& source, const std::unordered_map<std::string, GLenum>& formats) {
    if (formats.empty()) return;

    // One declaration per line is what the shaderpacks and Iris emit. Match the
    // uniform declaration, capture the variable name, and rewrite only that
    // declaration's layout qualifier.
    static const std::regex image_decl(
        R"((.*\buniform\s+)((?:(?:highp|mediump|lowp|coherent|volatile|restrict|readonly|writeonly)\s+)*)(u?image(?:1D|2D|3D|Cube|CubeArray|2DArray|Buffer))\s+([A-Za-z_][A-Za-z0-9_]*)\s*;.*)");
    std::string result;
    result.reserve(source.size() + 64);
    size_t start = 0;
    while (start <= source.size()) {
        const size_t end = source.find('\n', start);
        const size_t length = (end == std::string::npos) ? source.size() - start : end - start;
        std::string line = source.substr(start, length);
        std::smatch match;
        if (std::regex_match(line, match, image_decl)) {
            const std::string name = match[4].str();
            auto it = formats.find(name);
            if (it != formats.end()) {
                const char* qualifier = mg_image_qualifier(it->second);
                if (qualifier != nullptr) {
                    const std::string layout_qualifier = std::string("layout(") + qualifier + ") ";
                    const size_t uniform_pos = line.find("uniform");
                    const size_t layout_pos = line.rfind("layout", uniform_pos);
                    if (layout_pos != std::string::npos) {
                        const size_t close = line.find(')', layout_pos);
                        if (close != std::string::npos) {
                            std::string contents = line.substr(layout_pos + 7, close - (layout_pos + 7));
                            // Drop any existing format token; preserve other
                            // qualifiers such as binding.
                            static const std::regex format_token(
                                R"(\b(?:r8ui|r16ui|r32ui|r8i|r16i|r32i|r8|r16|r32f|r16f|rg16f|rg32f|rgba8|rgba8_snorm|rgba16f|rgba32f|rgba16|rgba32i|rgba32ui)\b)");
                            contents = std::regex_replace(contents, format_token, "");
                            while (!contents.empty() && (contents.front() == ' ' || contents.front() == ',')) {
                                contents.erase(contents.begin());
                            }
                            while (!contents.empty() && (contents.back() == ' ' || contents.back() == ',')) {
                                contents.pop_back();
                            }
                            std::string replacement = "layout(";
                            replacement += qualifier;
                            if (!contents.empty()) {
                                replacement += ", ";
                                replacement += contents;
                            }
                            replacement += ")";
                            line.replace(layout_pos, close - layout_pos + 1, replacement);
                        }
                    } else {
                        line.insert(uniform_pos, layout_qualifier);
                    }
                    // ESSL requires a precision qualifier on image uniforms.
                    // The desktop pack omits it; add highp unless the source
                    // already names one.
                    const size_t uniform_after_layout = line.find("uniform");
                    if (uniform_after_layout != std::string::npos) {
                        size_t after = uniform_after_layout + strlen("uniform");
                        while (after < line.size() && (line[after] == ' ' || line[after] == '\t')) ++after;
                        const size_t type_pos = line.find(match[3].str(), after);
                        const std::string qualifier_run =
                            type_pos == std::string::npos ? std::string() : line.substr(after, type_pos - after);
                        const bool has_precision = qualifier_run.find("highp") != std::string::npos ||
                                                   qualifier_run.find("mediump") != std::string::npos ||
                                                   qualifier_run.find("lowp") != std::string::npos;
                        if (!has_precision && type_pos != std::string::npos) line.insert(type_pos, "highp ");
                    }
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
