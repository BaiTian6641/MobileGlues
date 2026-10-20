// Focused consumer-side checks for the frozen FSR frame contract. These are
// ABI/lifetime invariants the backend and adapter both rely on, not a mirror of
// any backend implementation.
#include "../upscaling/mg_fsr_abi.h"

#include <cstddef>
#include <cstdio>
#include <cstring>

static int failures = 0;

#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                                                \
            ++failures;                                                                                                \
        }                                                                                                              \
    } while (0)

int main() {
    CHECK(MG_FSR_ABI_VERSION == 1);
    CHECK(MG_FSR_MODE_OFF == 0);
    CHECK(MG_FSR_MODE_FSR3_TEMPORAL == 2);
    CHECK(MG_FSR_COLOR_LDR_PERCEPTUAL == 0);
    CHECK(MG_FSR_COLOR_LINEAR_HDR == 1);
    CHECK(MG_FSR_NS_GL_FRONTEND == 1);
    CHECK(MG_FSR_OK == 0);
    CHECK(MG_FSR_ERROR_ABI < 0);

    mg_fsr_dispatch_t d{};
    d.abi_version = MG_FSR_ABI_VERSION;
    d.struct_size = sizeof(d);
    d.render_width = 1707;
    d.render_height = 1067;
    d.output_width = 2560;
    d.output_height = 1600;
    d.color_active.width = d.render_width;
    d.color_active.height = d.render_height;
    d.output_active.width = d.output_width;
    d.output_active.height = d.output_height;
    d.pre_exposure = 1.0f;
    d.frame_delta_seconds = 1.0f / 60.0f;
    d.motion_scale_x = -2.0f;

    CHECK(d.abi_version == 1);
    CHECK(d.struct_size == sizeof(d));
    CHECK(d.render_width * 100u / d.output_width >= 60u); // Quality-scale sanity, not a policy
    CHECK(d.motion_scale_x == -2.0f);                     // current-to-previous sign retained
    CHECK(d.color.handle == 0);                           // zero handle means absent
    CHECK(d.output.handle == 0);                          // backend-owned output sentinel

    // The producer must be able to copy the descriptor without hidden pointers.
    mg_fsr_dispatch_t copy{};
    std::memcpy(&copy, &d, sizeof(d));
    CHECK(std::memcmp(&copy, &d, sizeof(d)) == 0);

    mg_fsr_caps_t caps{};
    caps.abi_version = MG_FSR_ABI_VERSION;
    caps.struct_size = sizeof(caps);
    caps.max_workgroup[0] = 16;
    caps.max_workgroup[1] = 8;
    caps.max_workgroup[2] = 1;
    CHECK(sizeof(caps.backend_name) == 64);
    CHECK(sizeof(caps.failure_reason) == 256);

    if (failures == 0) {
        std::printf("fsr_contract: all checks passed\n");
        return 0;
    }
    std::printf("fsr_contract: %d failure(s)\n", failures);
    return 1;
}
