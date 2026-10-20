#include "../gl/glsl/compat_image.h"
#include <cstdio>
#include <string>

static int failures = 0;
static void check(const char* name, const std::string& got, const std::string& want) {
    if (got == want) {
        printf("ok   %s\n", name);
    } else {
        printf("FAIL %s\ngot:  %s\nwant: %s\n", name, got.c_str(), want.c_str());
        ++failures;
    }
}

int main() {
    check("rgba16", mg_lower_legacy_image_formats("layout(rgba16) uniform image2D a;\n"),
          "layout(rgba32f) uniform image2D a;\n");
    check("rg16", mg_lower_legacy_image_formats("layout (rg16) writeonly uniform image2D b;\n"),
          "layout (rg32f) writeonly uniform image2D b;\n");
    check("rgba16f-untouched", mg_lower_legacy_image_formats("layout(rgba16f) uniform image2D c;\n"),
          "layout(rgba16f) uniform image2D c;\n");
    check("rg16f-untouched", mg_lower_legacy_image_formats("layout(rg16f) uniform image2D d;\n"),
          "layout(rg16f) uniform image2D d;\n");
    check("non-image-untouched", mg_lower_legacy_image_formats("const int colortex1Format = RGBA16;\n"),
          "const int colortex1Format = RGBA16;\n");
    check("adjacent-lines",
          mg_lower_legacy_image_formats("layout(rgba16) uniform image2D a;\nlayout(rgba16f) uniform image2D b;\n"),
          "layout(rgba32f) uniform image2D a;\nlayout(rgba16f) uniform image2D b;\n");
    check("line-lengthening",
          mg_lower_legacy_image_formats("layout(rgba16) uniform image2D a;\nlogical line after\n"),
          "layout(rgba32f) uniform image2D a;\nlogical line after\n");
    if (failures) {
        printf("%d failures\n", failures);
        return 1;
    }
    printf("compat_image: all checks passed\n");
    return 0;
}
