// MobileGlues - gl/glsl/compat_image.cpp
// SPDX-License-Identifier: LGPL-2.1-only
#include "compat_image.h"

#include <regex>
#include <string>
#include <vector>

namespace {

std::string trim(const std::string& value) {
    const size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return std::string();
    const size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::vector<std::string> split_layout_items(const std::string& contents) {
    std::vector<std::string> items;
    size_t start = 0;
    while (start <= contents.size()) {
        const size_t comma = contents.find(',', start);
        const size_t end = comma == std::string::npos ? contents.size() : comma;
        std::string item = trim(contents.substr(start, end - start));
        if (!item.empty()) items.push_back(item);
        if (comma == std::string::npos) break;
        start = comma + 1;
    }
    return items;
}

std::string join_layout_items(const std::vector<std::string>& items) {
    std::string result;
    for (const std::string& item : items) {
        if (!result.empty()) result += ", ";
        result += item;
    }
    return result;
}

} // namespace

std::string mg_lower_legacy_image_formats(const std::string& glsl) {
    // ES 3.2 has no unorm16 image qualifier. Widen only the desktop unorm
    // formats: rgba16 -> rgba32f (or rgba16f for colorimg8, whose backing is
    // RGBA16F) and rg16 -> rgba32f (backing RGBA32F). The provider also rejects
    // rg16f and rg32f image qualifiers; widen those to the RGBA backing the
    // core storage path actually allocates.
    static const std::regex decl(
        R"((layout\s*\()([^)]*)(\))([^;{}]*?)\b(u?image(?:1D|2D|3D|Cube|CubeArray|2DArray|Buffer))\s+([A-Za-z_][A-Za-z0-9_]*)\s*(?:\[[^\]]*\])?\s*;)");

    std::string out;
    out.reserve(glsl.size() + 64);
    size_t copied = 0;
    for (std::sregex_iterator it(glsl.begin(), glsl.end(), decl), end; it != end; ++it) {
        const std::smatch& match = *it;
        std::vector<std::string> items = split_layout_items(match[2].str());
        bool changed = false;
        const bool alt_output = match[6].str() == "colorimg8";
        for (std::string& item : items) {
            if (item == "rgba16") {
                // Most rgba16 image targets are backed by RGBA32F after the
                // null-allocation widening. Bloom's alt output is the common
                // exception: Iris backs colorimg8 with RGBA16F.
                item = alt_output ? "rgba16f" : "rgba32f";
                changed = true;
            } else if (item == "rg16") {
                // RG16 unorm backs are widened to RGBA32F by the storage path.
                item = "rgba32f";
                changed = true;
            } else if (item == "rg16f") {
                // The provider rejects rg16f; RG16F storage is widened to
                // RGBA16F by glTexImage/glTexStorage.
                item = "rgba16f";
                changed = true;
            } else if (item == "rg32f") {
                // The provider rejects rg32f; RG32F storage is widened to
                // RGBA32F by the storage path.
                item = "rgba32f";
                changed = true;
            }
        }
        if (!changed) continue;

        out.append(glsl, copied, static_cast<size_t>(match.position()) - copied);
        out += match[1].str() + join_layout_items(items) + match[3].str() + match[4].str();
        const size_t type_offset = static_cast<size_t>(match.position(5) - match.position(0));
        out.append(match[0].first + type_offset, match[0].second);
        copied = static_cast<size_t>(match.position() + match.length());
    }
    if (copied == 0) return glsl;
    out.append(glsl, copied, glsl.size() - copied);
    return out;
}
