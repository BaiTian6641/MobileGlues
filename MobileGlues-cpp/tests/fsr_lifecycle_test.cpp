// MobileGlues - tests/fsr_lifecycle_test.cpp
// SPDX-License-Identifier: LGPL-2.1-only
// Host ownership test: link the real session API against a recording backend.
// This checks teardown routing, not GPU resource or rendering behavior.
#include "upscaling/mg_fsr_abi.h"
#include "upscaling/mg_fsr_lifecycle.h"
#include "upscaling/fsr3/fsr3_backend.h"
#include "egl/context.h"

#include <cstdio>

thread_local MGContext* g_current_ctx = nullptr;

namespace {
int constructed = 0;
int abandoned = 0;
int deleted = 0;
int unsafe_destructions = 0;
}

namespace mg_fsr3 {
Backend::Backend() { ++constructed; initialized_ = true; }
Backend::~Backend() {
    ++deleted;
    if (initialized_) ++unsafe_destructions;
}
void Backend::abandon() { ++abandoned; initialized_ = false; }
}

int main() {
    MGContext old_context{};
    old_context.id = 17;
    MGContext replacement_context{};
    replacement_context.id = 18;
    g_current_ctx = &old_context;
    void* first = mg_fsr_create_session();
    void* second = mg_fsr_create_session();

    // The backend has switched to a replacement before the old bookkeeping is
    // released. Both a tracked replacement and an untracked current context
    // must use the explicit no-GL teardown route.
    g_current_ctx = &replacement_context;
    mg_fsr_abandon_session(first);
    g_current_ctx = nullptr;
    mg_fsr_abandon_session(second);
    mg_fsr_abandon_session(nullptr);

    if (constructed != 2 || abandoned != 2 || deleted != 2 || unsafe_destructions != 0) {
        std::fprintf(stderr, "FSR lifecycle failed: created=%d abandoned=%d deleted=%d unsafe=%d\n",
                     constructed, abandoned, deleted, unsafe_destructions);
        return 1;
    }
    std::puts("FSR session lifecycle: replacement-context teardown releases ownership without GL");
    return 0;
}
