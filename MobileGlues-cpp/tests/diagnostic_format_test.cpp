#include "gl/diagnostic_format.h"

#include <cstdio>
#include <string>

static int failures = 0;
static void expect(const char* what, const std::string& got, const std::string& want) {
    if (got != want) {
        std::printf("FAIL %s: got %s want %s\n", what, got.c_str(), want.c_str());
        ++failures;
    }
}

int main() {
    std::string out;
    mg_diag::json_escape(out, "a\"b\\c\n\t\x01");
    expect("escape", out, "\"a\\\"b\\\\c\\n\\t\\u0001\"");

    int values[3] = {1, -2, 3};
    out.clear();
    mg_diag::json_int_array(out, values, 3);
    expect("int array", out, "[1,-2,3]");

    out.clear();
    mg_diag::json_bool(out, true);
    mg_diag::json_int(out, 42);
    expect("scalars", out, "true42");

    if (failures == 0) std::printf("diagnostic_format: all checks passed\n");
    return failures == 0 ? 0 : 1;
}
