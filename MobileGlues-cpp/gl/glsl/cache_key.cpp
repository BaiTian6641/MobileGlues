// MobileGlues - gl/glsl/cache_key.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header

#include "cache_key.h"

#include <string>

std::string build_translation_cache_key(const char* source, unsigned shader_type, unsigned essl_version,
                                        unsigned backend_es_version, bool emulate_texture_buffer,
                                        bool texture_query_lod, bool compute_enabled, bool dsa_enabled,
                                        int hide_mg_env_level, unsigned major, unsigned minor, unsigned revision,
                                        unsigned patch) {
    // Bump whenever a translation rule changes so old entries cannot be reused.
    constexpr int kTranslationCacheSchema = 4;
    std::string key(source ? source : "");
    key += "\n//schema=" + std::to_string(kTranslationCacheSchema) + "|" + std::to_string(major) + "." +
           std::to_string(minor) + "." + std::to_string(revision) + "." + std::to_string(patch) +
           "|stage=" + std::to_string(shader_type) + "|essl=" + std::to_string(essl_version) +
           "|backend=" + std::to_string(backend_es_version) +
           "|compute=" + std::to_string(compute_enabled ? 1 : 0) +
           "|dsa=" + std::to_string(dsa_enabled ? 1 : 0) +
           "|hide=" + std::to_string(hide_mg_env_level) +
           "|tbuf=" + std::to_string(emulate_texture_buffer ? 1 : 0) +
           "|lod=" + std::to_string(texture_query_lod ? 1 : 0);
    return key;
}
