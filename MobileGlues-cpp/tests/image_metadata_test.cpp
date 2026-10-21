// Host checks for the logical-image-format to ESSL backing/qualifier mapping.
#include "../gl/image_metadata.h"

#include <cstdio>
#include <cstring>

static int failures = 0;

static void check_format(const char* name, GLenum logical, GLenum want_backing, const char* want_qualifier) {
    const GLenum backing = mg_image_backing_format(logical);
    const char* qualifier = mg_image_qualifier(logical);
    if (backing != want_backing || qualifier == nullptr || std::strcmp(qualifier, want_qualifier) != 0) {
        std::printf("FAIL %s backing=0x%x qualifier=%s\n", name, backing, qualifier ? qualifier : "(null)");
        ++failures;
        return;
    }
    std::printf("ok   %s -> 0x%x %s\n", name, backing, qualifier);
}

int main() {
    check_format("rg16f", GL_RG16F, GL_RGBA16F, "rgba16f");
    check_format("rg32f", GL_RG32F, GL_RGBA32F, "rgba32f");
    check_format("rgba16", GL_RGBA16, GL_RGBA32F, "rgba32f");
    check_format("rg16", GL_RG16, GL_RGBA32F, "rgba32f");
    check_format("r16ui", GL_R16UI, GL_R32UI, "r32ui");
    check_format("rgba16f", GL_RGBA16F, GL_RGBA16F, "rgba16f");

    if (mg_image_unit_format(GL_RG16F, 0) != GL_RGBA16F) {
        std::printf("FAIL rg16f unit format\n");
        ++failures;
    }
    if (mg_image_unit_format(GL_RGBA16, GL_RGBA32F) != GL_RGBA32F) {
        std::printf("FAIL rgba16 texture-bound unit format\n");
        ++failures;
    }

    if (failures) {
        std::printf("image_metadata: %d failures\n", failures);
        return 1;
    }
    std::printf("image_metadata: all checks passed\n");
    return 0;
}
