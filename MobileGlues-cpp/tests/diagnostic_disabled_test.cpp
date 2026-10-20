#include "gl/diagnostic.h"

#include <cstdio>

int main() {
    if (mg_diag_enabled()) {
        std::printf("FAIL trace unexpectedly enabled\n");
        return 1;
    }
    mg_diag_role_start(1, 1, 1);
    mg_diag_on_draw();
    if (mg_diag_draw_count_for_test() != 0) {
        std::printf("FAIL disabled trace accumulated a draw\n");
        return 1;
    }
    mg_diag_set_enabled(true);
    // The release/stub header remains inert even if a caller flips the bit.
    if (mg_diag_enabled()) {
        std::printf("FAIL stub header enabled the trace\n");
        return 1;
    }
    mg_diag_reset();
    std::printf("diagnostic_disabled: all checks passed\n");
    return 0;
}
