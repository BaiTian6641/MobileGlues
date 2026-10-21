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
          "layout (rgba32f) writeonly uniform image2D b;\n");
    check("colorimg8-alt-output",
          mg_lower_legacy_image_formats("layout (rgba16) writeonly uniform image2D colorimg8;\n"),
          "layout (rgba16f) writeonly uniform image2D colorimg8;\n");
    check("binding-preserved",
          mg_lower_legacy_image_formats("layout(rgba16, binding = 3) uniform image2D c;\n"),
          "layout(rgba32f, binding = 3) uniform image2D c;\n");
    check("multiline",
          mg_lower_legacy_image_formats("layout(rgba16)\nuniform image2D d[2];\n"),
          "layout(rgba32f)\nuniform image2D d[2];\n");
    check("rgba16f-untouched", mg_lower_legacy_image_formats("layout(rgba16f) uniform image2D e;\n"),
          "layout(rgba16f) uniform image2D e;\n");
    check("rg16f-widened", mg_lower_legacy_image_formats("layout(rg16f) uniform image2D f;\n"),
          "layout(rgba16f) uniform image2D f;\n");
    check("rg32f-widened", mg_lower_legacy_image_formats("layout(rg32f) uniform image2D f32;\n"),
          "layout(rgba32f) uniform image2D f32;\n");
    check("r16f-untouched", mg_lower_legacy_image_formats("layout(r16f) uniform image2D rf;\n"),
          "layout(r16f) uniform image2D rf;\n");
    check("rgba16ui-untouched", mg_lower_legacy_image_formats("layout(rgba16ui) uniform uimage2D g;\n"),
          "layout(rgba16ui) uniform uimage2D g;\n");
    check("non-image-untouched", mg_lower_legacy_image_formats("const int colortex1Format = RGBA16;\n"),
          "const int colortex1Format = RGBA16;\n");
    check("adjacent-lines",
          mg_lower_legacy_image_formats("layout(rgba16) uniform image2D h;\nlayout(rgba16f) uniform image2D i;\n"),
          "layout(rgba32f) uniform image2D h;\nlayout(rgba16f) uniform image2D i;\n");
    check("line-lengthening",
          mg_lower_legacy_image_formats("layout(rgba16) uniform image2D j;\nlogical line after\n"),
          "layout(rgba32f) uniform image2D j;\nlogical line after\n");
    if (failures) {
        printf("%d failures\n", failures);
        return 1;
    }
    printf("compat_image: all checks passed\n");
    return 0;
}
