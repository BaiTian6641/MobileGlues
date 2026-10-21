#include "upscaling/fsr3_depth_contract.h"

#include <algorithm>
#include <cstdio>
#include <limits>

static int failures = 0;
static void check(bool value, const char* what) {
    if (!value) { std::printf("FAIL %s\n", what); ++failures; }
}
static bool near(float actual, float expected, float relative = 1e-4f) {
    return std::isfinite(actual) && std::fabs(actual - expected) <=
        relative * std::max(1.0f, std::fabs(expected));
}

int main() {
    using namespace mg_fsr3_depth;
    check(supported_convention(0) && supported_convention(1), "known profile conventions");
    check(!supported_convention(-1) && !supported_convention(2) &&
          !supported_convention(3), "reject unknown projection convention flags");
    check(history_allowed(0, true) && !history_allowed(0, false), "legacy opt-in unchanged");
    check(!history_allowed(1, true) && !history_allowed(1, false), "reversed sample profile forces reset");
    check(supported_dispatch(false, false, false, true), "existing forward finite dispatch");
    check(supported_dispatch(false, true, true, false), "forward infinite spatial dispatch");
    check(!supported_dispatch(false, true, false, false), "infinite history rejected");
    check(!supported_dispatch(false, true, true, true), "infinite motion rejected");
    check(!supported_dispatch(true, false, true, false) &&
          !supported_dispatch(true, true, true, false), "raw reversed depth rejected");

    // A real finite perspective: n=.5, f=100, fov=pi/2, aspect=2.
    // Generate stored GPU reverse depth by reversing its projected forward
    // depth; independently reconstruct distance with the decoded FSR planes.
    float p[16] = {.5f,0,0,0, 0,1,0,0, .01f,-.02f,-100.5f/99.5f,-1,
                   0,0,-100.0f/99.5f,0};
    Projection decoded;
    check(forward_projection(p, decoded), "finite off-center captured projection accepted");
    check(near(decoded.near_plane,.5f) && near(decoded.far_plane,100.0f) &&
          near(decoded.fov_y,1.5707963268f) && !decoded.infinite_far, "finite planes and FOV");
    for (float distance : {.5f, 1.0f, 10.0f, 100.0f}) {
        const float ndc = -p[10] + p[14] / distance;
        const float sampled_reverse = 1.0f - (ndc * .5f + .5f);
        const float normalized = 1.0f - sampled_reverse;
        const float q = decoded.far_plane / (decoded.far_plane - decoded.near_plane);
        const float reconstructed = q * decoded.near_plane / (q - normalized);
        check(near(reconstructed,distance), "reverse sample normalization preserves view distance");
    }

    p[10] = -1.0f;
    p[14] = -.1f;
    check(forward_projection(p,decoded) && decoded.infinite_far &&
          near(decoded.near_plane,.05f) && decoded.far_plane==std::numeric_limits<float>::max(),
          "infinite far decoded without invented finite distance");
    for (float distance : {.05f, 1.0f, 100.0f}) {
        const float sampled_reverse = .05f / distance;
        const float normalized = 1.0f - sampled_reverse;
        check(near(.05f / (1.0f-normalized),distance,2e-4f), "infinite reverse normalization");
    }
    check(!forward_projection(nullptr,decoded), "missing projection rejected");
    const float saved = p[10];
    p[10]=0; check(!forward_projection(p,decoded), "raw reversed zero-to-one projection rejected");
    p[10]=saved;
    p[2]=.01f; check(!forward_projection(p,decoded), "oblique depth projection rejected"); p[2]=0;
    p[15]=1; check(!forward_projection(p,decoded), "orthographic projection rejected"); p[15]=0;
    p[5]=std::numeric_limits<float>::quiet_NaN();
    check(!forward_projection(p,decoded), "nonfinite projection rejected");
    std::printf("fsr3_depth_contract: %s\n", failures ? "FAILED" : "all checks passed");
    return failures ? 1 : 0;
}
