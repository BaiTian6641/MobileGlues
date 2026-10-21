// Pure tests for the Minecraft/Iris scene viewport rule.
#include "gl/FSR3ScenePlan.h"
#include <cstdio>

static int fails = 0;
static void expect(const char* what, const FSR3ViewportPlan& p, bool adjust,
                   int x, int y, int w, int h) {
    if (p.adjust != adjust || (adjust && (p.x != x || p.y != y || p.width != w || p.height != h))) {
        printf("  FAIL %-48s adjust=%d got=(%d,%d,%d,%d)\n", what, p.adjust, p.x, p.y, p.width, p.height);
        ++fails;
    } else {
        printf("  ok   %s\n", what);
    }
}

int main() {
    const int S = 2880, T = 1800;
    auto a = FSR3PlanViewport(S, T, 1920, 1200, 0, 0, S, T);
    expect("surface-sized viewport scales to Quality target", a, true, 0, 0, 1920, 1200);

    auto b = FSR3PlanViewport(S, T, 1440, 900, 0, 0, S, T);
    expect("surface-sized viewport scales to Performance target", b, true, 0, 0, 1440, 900);

    auto c = FSR3PlanViewport(S, T, S, T, 0, 0, S, T);
    expect("native target left alone", c, false, 0, 0, S, T);

    auto d = FSR3PlanViewport(S, T, 1920, 1200, 0, 0, 1920, 1200);
    expect("already-scaled viewport left alone", d, false, 0, 0, 1920, 1200);

    auto e = FSR3PlanViewport(S, T, 1024, 1024, 0, 0, 2048, 2048);
    expect("shadow/off-surface viewport left alone", e, false, 0, 0, 2048, 2048);

    auto f = FSR3PlanViewport(S, T, 1920, 1200, 100, 50, 640, 360);
    expect("partial viewport left alone", f, false, 100, 50, 640, 360);

    auto balanced = FSR3PlanViewport(2880, 1585, 1694, 932, 0, 0, 2880, 1585);
    expect("explicit Balanced scene request scales once", balanced, true, 0, 0, 1694, 932);

    auto shadow = FSR3PlanViewport(2880, 1585, 2048, 2048, 0, 0, 2048, 2048);
    expect("observed 26.2 shadow request retains square extent", shadow, false, 0, 0, 2048, 2048);

    auto g = FSR3PlanViewport(S, T, 0, 0, 0, 0, S, T);
    expect("zero target left alone", g, false, 0, 0, S, T);

    auto h = FSR3PlanViewport(2880, 1800, 2400, 1200, 0, 0, 2880, 1800);
    expect("independent axes use their own ratio", h, true, 0, 0, 2400, 1200);

    // Iris 26.2 may return with its smaller swap FBO bound. It must not become
    // the output extent or the next frame's viewport reference.
    if (!FSR3IsNativeOutputExtent(2880, 1585, 2880, 1585) ||
        FSR3IsNativeOutputExtent(2880, 1585, 1694, 932) ||
        FSR3IsNativeOutputExtent(2880, 1585, 996, 548) ||
        FSR3IsNativeOutputExtent(1920, 1080, 2880, 1585) ||
        FSR3IsNativeOutputExtent(0, 0, 0, 0) ||
        !FSR3IsNativeOutputExtent(1920, 1080, 1920, 1080)) {
        printf("  FAIL final target rejects intermediate and stale resize extents\n");
        ++fails;
    } else {
        printf("  ok   final target rejects intermediate and stale resize extents\n");
    }

    printf(fails ? "fsr3_viewport: %d checks failed\n" : "fsr3_viewport: all checks passed\n", fails);
    return fails ? 1 : 0;
}
