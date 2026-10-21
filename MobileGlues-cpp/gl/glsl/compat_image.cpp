// MobileGlues - gl/glsl/compat_image.cpp
// SPDX-License-Identifier: LGPL-2.1-only
#include "compat_image.h"
#include "image_source_mask.h"
#include "../image_format.h"

#include <regex>
#include <string>
#include <vector>

std::string mg_lower_legacy_image_formats(const std::string& glsl) {
    // Storage and source lowering share one legal-format table. Existing
    // widenings are compatibility deviations; new lossy conversions fail with
    // a specific diagnostic until their numerical semantics are implemented.
    static const std::regex decl(
        R"((layout\s*\()([^)]*)(\))([^;{}]*?)\b([iu]?image(?:1D|2D|3D|Cube|CubeArray|2DArray|Buffer))\s+([A-Za-z_][A-Za-z0-9_]*)\s*(?:\[[^\]]*\])?\s*;)");

    const std::string masked = mg_image_code_mask(glsl);
    std::string out;
    out.reserve(glsl.size() + 64);
    size_t copied = 0;
    for (std::sregex_iterator it(masked.begin(), masked.end(), decl), end; it != end; ++it) {
        const std::smatch& match = *it;
        const std::string contents = match[2].str();
        const size_t layout_start = static_cast<size_t>(match.position(2));
        std::string replacement = glsl.substr(layout_start, contents.size());
        struct Edit { size_t start; size_t length; std::string text; };
        std::vector<Edit> edits;
        static const std::regex token(R"(\b[A-Za-z_][A-Za-z0-9_]*\b)");
        bool changed = false;
        std::string diagnostic;
        const bool alt_output = match[6].str() == "colorimg8";
        for (std::sregex_iterator tok(contents.begin(), contents.end(), token), end; tok != end; ++tok) {
            const size_t token_start = static_cast<size_t>(tok->position());
            const size_t token_end = token_start + static_cast<size_t>(tok->length());
            const size_t comma_before = contents.rfind(',', token_start);
            const size_t item_start = comma_before == std::string::npos ? 0 : comma_before + 1;
            const size_t comma_after = contents.find(',', token_end);
            const size_t item_end = comma_after == std::string::npos ? contents.size() : comma_after;
            if (contents.substr(item_start, token_start - item_start).find_first_not_of(" \t\r\n") != std::string::npos ||
                contents.substr(token_end, item_end - token_end).find_first_not_of(" \t\r\n") != std::string::npos) continue;
            const std::string item = tok->str();
            const auto* entry = mg_find_image_format(std::string_view(item));
            if (entry != nullptr) {
                if (entry->essl_qualifier == nullptr) {
                    diagnostic = "MG_IMAGE_FORMAT_UNMAPPABLE: " + match[6].str() +
                                 " (" + item + "; store conversion is not implemented)";
                    changed = true;
                } else if (item != entry->essl_qualifier) {
                    // Retain the existing named exception for the current pack;
                    // replacing it requires actual render-target metadata.
                    edits.push_back({static_cast<size_t>(tok->position()), static_cast<size_t>(tok->length()),
                                     alt_output && item == "rgba16" ? "rgba16f" : entry->essl_qualifier});
                    changed = true;
                }
            } else if (std::regex_match(item, std::regex(R"(r(?:g(?:b(?:a)?)?)?[0-9]+[a-zA-Z0-9_]*)"))) {
                diagnostic = "MG_IMAGE_FORMAT_UNMAPPABLE: " + match[6].str() + " (" + item + ")";
                changed = true;
            }
        }
        if (!changed) continue;

        out.append(glsl, copied, static_cast<size_t>(match.position()) - copied);
        if (!diagnostic.empty()) out += "\n#error " + diagnostic + "\n";
        const size_t layout_end = layout_start + static_cast<size_t>(match.length(2));
        const size_t match_start = static_cast<size_t>(match.position());
        const size_t match_end = match_start + static_cast<size_t>(match.length());
        out.append(glsl, match_start, layout_start - match_start);
        // For rejected declarations retain the original source for diagnostics.
        if (diagnostic.empty()) {
            for (auto edit = edits.rbegin(); edit != edits.rend(); ++edit) {
                replacement.replace(edit->start, edit->length, edit->text);
            }
        }
        out += replacement;
        out.append(glsl, layout_end, match_end - layout_end);
        copied = static_cast<size_t>(match.position() + match.length());
    }
    if (copied == 0) return glsl;
    out.append(glsl, copied, glsl.size() - copied);
    return out;
}
