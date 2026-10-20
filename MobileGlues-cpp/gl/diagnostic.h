// MobileGlues - gl/diagnostic.h
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header

#ifndef MOBILEGLUES_DIAGNOSTIC_H
#define MOBILEGLUES_DIAGNOSTIC_H

#include <cstdint>
#include <string>

// Experiment A diagnostic core. This module is compiled only for the
// diagnostic build (MG_FSR3_DIAGNOSTICS=1). The release build gets inline
// no-ops, so every hook below disappears from the draw path.
#ifdef MG_FSR3_DIAGNOSTICS

bool mg_diag_enabled();
void mg_diag_set_enabled(bool enabled);
void mg_diag_reset();

// Ordered render-thread role window. The role id is opaque to this module;
// the Java writer owns its name. START resets draw accumulation.
void mg_diag_role_start(int role, int frame, int event);
std::string mg_diag_snapshot_json();
std::string mg_diag_role_end_json();

// Called immediately before a top-level driver draw. When tracing is disabled
// this is a single atomic load; it performs no GL queries.
void mg_diag_on_draw();

uint64_t mg_diag_draw_count_for_test();

#else

inline bool mg_diag_enabled() { return false; }
inline void mg_diag_set_enabled(bool) {}
inline void mg_diag_reset() {}
inline void mg_diag_role_start(int, int, int) {}
inline std::string mg_diag_snapshot_json() { return {}; }
inline std::string mg_diag_role_end_json() { return {}; }
inline void mg_diag_on_draw() {}
inline uint64_t mg_diag_draw_count_for_test() { return 0; }

#endif // MG_FSR3_DIAGNOSTICS

#endif // MOBILEGLUES_DIAGNOSTIC_H
