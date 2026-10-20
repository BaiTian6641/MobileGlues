// MobileGlues - gl/diagnostic_format.h
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header

#ifndef MOBILEGLUES_DIAGNOSTIC_FORMAT_H
#define MOBILEGLUES_DIAGNOSTIC_FORMAT_H

#include <cstddef>
#include <cstdint>
#include <string>

// Small, GL-free helpers shared by the diagnostic writer and its host tests.
namespace mg_diag {

inline void json_escape(std::string& out, const std::string& value) {
    out.push_back('"');
    static const char* hex = "0123456789abcdef";
    for (unsigned char c : value) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (c < 0x20) {
                out += "\\u00";
                out.push_back(hex[(c >> 4) & 0xf]);
                out.push_back(hex[c & 0xf]);
            } else {
                out.push_back(static_cast<char>(c));
            }
        }
    }
    out.push_back('"');
}

inline void json_null(std::string& out) {
    out += "null";
}

inline void json_bool(std::string& out, bool value) {
    out += value ? "true" : "false";
}

inline void json_int(std::string& out, int64_t value) {
    out += std::to_string(value);
}

// Writes a JSON array of integers, preserving an explicitly empty vector.
inline void json_int_array(std::string& out, const int* values, size_t count) {
    out.push_back('[');
    for (size_t i = 0; i < count; ++i) {
        if (i) out.push_back(',');
        out += std::to_string(values[i]);
    }
    out.push_back(']');
}

} // namespace mg_diag

#endif // MOBILEGLUES_DIAGNOSTIC_FORMAT_H
