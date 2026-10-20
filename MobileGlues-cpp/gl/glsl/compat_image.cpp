// MobileGlues - gl/glsl/compat_image.cpp
// SPDX-License-Identifier: LGPL-2.1-only
#include "compat_image.h"

#include <cctype>
#include <cstring>

static void replace_qualifier(std::string& line, const char* from, const char* to) {
    const size_t flen = std::strlen(from);
    std::string result;
    result.reserve(line.size());
    for (size_t i = 0; i < line.size();) {
        if (line.compare(i, flen, from) == 0) {
            const bool left_ok =
                i == 0 || !(std::isalnum(static_cast<unsigned char>(line[i - 1])) || line[i - 1] == '_');
            const size_t after = i + flen;
            const bool right_ok =
                after >= line.size() ||
                !(std::isalnum(static_cast<unsigned char>(line[after])) || line[after] == '_');
            if (left_ok && right_ok) {
                result += to;
                i = after;
                continue;
            }
        }
        result += line[i++];
    }
    line.swap(result);
}

std::string mg_lower_legacy_image_formats(const std::string& glsl) {
    std::string out;
    out.reserve(glsl.size() + 64);
    size_t start = 0;
    while (start <= glsl.size()) {
        const size_t end = glsl.find('\n', start);
        const size_t length = (end == std::string::npos) ? glsl.size() - start : end - start;
        std::string line = glsl.substr(start, length);
        if (line.find("image") != std::string::npos && line.find("layout") != std::string::npos) {
            replace_qualifier(line, "rgba16", "rgba32f");
            replace_qualifier(line, "rg16", "rg32f");
        }
        out += line;
        if (end == std::string::npos) break;
        out += '\n';
        start = end + 1;
    }
    return out;
}
