// MobileGlues - gl/glsl/cache_key.h
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header

#ifndef MOBILEGLUES_GLSL_CACHE_KEY_H
#define MOBILEGLUES_GLSL_CACHE_KEY_H

#include <string>

// Identity for one translated shader. Translation output depends on the source,
// the stage, the backend ESSL level, provider preprocessing branches, the
// wrapper version (the injected MG_MOBILEGLUES_VERSION macro includes PATCH),
// and the settings that change generated GLSL. Keep every one of those inputs
// in the key: a cache hit with a different input is a wrong shader, not a stale
// comment.
std::string build_translation_cache_key(const char* source, unsigned shader_type, unsigned essl_version,
                                        unsigned backend_es_version, bool emulate_texture_buffer,
                                        bool texture_query_lod, bool compute_enabled, bool dsa_enabled,
                                        int hide_mg_env_level, unsigned major, unsigned minor, unsigned revision,
                                        unsigned patch);

#endif
