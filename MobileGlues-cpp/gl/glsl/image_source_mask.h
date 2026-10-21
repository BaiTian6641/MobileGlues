// MobileGlues - gl/glsl/image_source_mask.h
// SPDX-License-Identifier: LGPL-2.1-only
#ifndef MOBILEGLUES_IMAGE_SOURCE_MASK_H
#define MOBILEGLUES_IMAGE_SOURCE_MASK_H

#include <string>

// Keep offsets stable while excluding comments, strings, and directive logical
// lines. An unused macro body is not an active declaration and must not acquire
// an unconditional #error. The compiler still validates its expanded use.
inline std::string mg_image_code_mask(const std::string& source) {
    std::string mask = source;
    for (size_t i = 0; i < source.size();) {
        size_t end = i;
        bool directive = false;
        if (source[i] == '#') {
            const size_t previous_newline = i == 0 ? std::string::npos : source.rfind('\n', i - 1);
            const size_t line_start = previous_newline == std::string::npos ? 0 : previous_newline + 1;
            directive = mask.substr(line_start, i - line_start).find_first_not_of(" \t\r") == std::string::npos;
        }
        if (directive) {
            end = source.find('\n', i);
            while (end != std::string::npos) {
                size_t last = end;
                if (last > i && source[last - 1] == '\r') --last;
                if (last == i || source[last - 1] != '\\') break;
                end = source.find('\n', end + 1);
            }
            if (end == std::string::npos) end = source.size();
        } else if (source[i] == '/' && i + 1 < source.size() && source[i + 1] == '/') {
            end = source.find('\n', i + 2);
            if (end == std::string::npos) end = source.size();
        } else if (source[i] == '/' && i + 1 < source.size() && source[i + 1] == '*') {
            end = source.find("*/", i + 2);
            end = end == std::string::npos ? source.size() : end + 2;
        } else if (source[i] == '"' || source[i] == '\'') {
            const char quote = source[i];
            end = i + 1;
            while (end < source.size()) {
                if (source[end] == '\\' && end + 1 < source.size()) end += 2;
                else if (source[end++] == quote) break;
            }
        } else {
            ++i;
            continue;
        }
        for (; i < end; ++i) if (mask[i] != '\n') mask[i] = ' ';
    }
    return mask;
}

#endif
