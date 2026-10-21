// MobileGlues - upscaling/mg_fsr_lifecycle.h
// SPDX-License-Identifier: LGPL-2.1-only
#pragma once

// Internal teardown after the owning GL context is no longer current. Unlike
// mg_fsr_destroy_session, this never inspects or mutates a replacement context.
void mg_fsr_abandon_session(void* session);

#if defined(__ANDROID__)
void mg_fsr_jni_forget_context(unsigned long long context_id);
#else
inline void mg_fsr_jni_forget_context(unsigned long long) {}
#endif
