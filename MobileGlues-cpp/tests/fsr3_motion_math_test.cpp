#include "upscaling/fsr3_motion_math.h"
#include <cmath>
#include <cstdio>

static int fails = 0;
static void near(const char* what, float got, float want) {
    if (std::fabs(got - want) > 1e-5f) {
        printf("  FAIL %s got=%g want=%g\n", what, got, want);
        ++fails;
    } else {
        printf("  ok   %s\n", what);
    }
}

int main() {
    const float identity[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    float inv[16], mul[16];
    if (!mg_fsr3_motion::invert(identity, inv)) { printf("  FAIL identity invert\n"); ++fails; }
    for (int i = 0; i < 16; ++i) near("identity inverse", inv[i], identity[i]);
    mg_fsr3_motion::multiply(identity, identity, mul);
    for (int i = 0; i < 16; ++i) near("identity multiply", mul[i], identity[i]);

    // Column-major translation by (2,3,4): inverse translates by (-2,-3,-4).
    const float t[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 2,3,4,1};
    if (!mg_fsr3_motion::invert(t, inv)) { printf("  FAIL translation invert\n"); ++fails; }
    near("translation inverse x", inv[12], -2.0f);
    near("translation inverse y", inv[13], -3.0f);
    near("translation inverse z", inv[14], -4.0f);
    near("translation inverse w", inv[15], 1.0f);

    // A singular matrix must be rejected.
    const float singular[16] = {1,0,0,0, 2,0,0,0, 0,0,1,0, 0,0,0,1};
    if (mg_fsr3_motion::invert(singular, inv)) { printf("  FAIL singular accepted\n"); ++fails; }

    printf(fails ? "fsr3_motion_math: %d checks failed\n" : "fsr3_motion_math: all checks passed\n", fails);
    return fails ? 1 : 0;
}
