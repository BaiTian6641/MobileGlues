// Host checks link the real mapping and metadata implementation. These verify
// legal enums and allocation/binding compatibility, not GPU image semantics.
#include "../gl/image_metadata.h"
#include "../gl/image_format.h"

#include <cstdio>
#include <cstring>

static int failures = 0;
static void check(const char* name, bool ok) {
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", name);
    if (!ok) ++failures;
}

int main() {
    const GLenum native[] = {GL_RGBA32F, GL_RGBA16F, GL_R32F, GL_RGBA32UI,
        GL_RGBA16UI, GL_RGBA8UI, GL_R32UI, GL_RGBA32I, GL_RGBA16I, GL_RGBA8I,
        GL_R32I, GL_RGBA8, GL_RGBA8_SNORM};
    for (const GLenum format : native) {
        const auto* info = mg_find_image_format(format);
        check(info->desktop_qualifier, mg_is_native_image_format(format) &&
            mg_image_backing_format(format) == format &&
            std::strcmp(mg_image_qualifier(format), info->desktop_qualifier) == 0 &&
            mg_lower_image_storage_format(format) == format &&
            mg_image_unit_format(format, format) == format);
    }
    check("RG32F allocation and binding agree",
        mg_lower_image_storage_format(GL_RG32F) == GL_RGBA32F &&
        mg_image_unit_format(GL_RG32F, GL_RGBA32F) == GL_RGBA32F);
    check("raw data-backed RG32F cannot bind widened 128-bit image",
        mg_image_unit_format(GL_RG32F, GL_RG32F) == GL_NONE);
    check("raw data-backed RGBA16 never emits illegal image enum",
        mg_image_unit_format(GL_RGBA16, GL_RGBA16) == GL_NONE);
    check("raw data-backed RG16 cannot bind widened image",
        mg_image_unit_format(GL_RG16, GL_RG16) == GL_NONE);
    check("existing unorm16 float backing", mg_image_unit_format(GL_RGBA16, GL_RGBA32F) == GL_RGBA32F);
    check("existing unorm16 half backing", mg_image_unit_format(GL_RGBA16, GL_RGBA16F) == GL_RGBA16F);
    check("native compatible by size", mg_image_unit_format(GL_R32UI, GL_RGBA8) == GL_R32UI);
    check("packed texture compatible by size", mg_image_unit_format(GL_R32UI, GL_RGB10_A2) == GL_R32UI);
    check("native incompatible by size", mg_image_unit_format(GL_R32UI, GL_RGBA16F) == GL_NONE);
    check("raw narrow integer data cannot bind wide image", mg_image_unit_format(GL_R16UI, GL_R16UI) == GL_NONE);
    check("narrow integer null allocation", mg_lower_image_storage_format(GL_R16UI) == GL_R32UI);
    check("RGBA8_SNORM immutable allowlist", mg_is_native_image_format(GL_RGBA8_SNORM));
    check("RG32F is not itself native image storage", !mg_is_native_image_format(GL_RG32F));
    check("unknown format rejected", mg_image_backing_format(GL_DEPTH_COMPONENT24) == GL_NONE &&
        mg_image_qualifier(GL_DEPTH_COMPONENT24) == nullptr);
    // glBindImageTextures(NULL) uses the desktop R8 default. All table-8.26
    // formats remain legal for texture zero without enabling their storage.
    const GLenum desktop_formats[] = {
        GL_RGBA32F, GL_RGBA16F, GL_RG32F, GL_RG16F, GL_R11F_G11F_B10F, GL_R32F, GL_R16F,
        GL_RGBA32UI, GL_RGBA16UI, GL_RGB10_A2UI, GL_RGBA8UI, GL_RG32UI, GL_RG16UI,
        GL_RG8UI, GL_R32UI, GL_R16UI, GL_R8UI,
        GL_RGBA32I, GL_RGBA16I, GL_RGBA8I, GL_RG32I, GL_RG16I, GL_RG8I, GL_R32I, GL_R16I, GL_R8I,
        GL_RGBA16, GL_RGB10_A2, GL_RGBA8, GL_RG16, GL_RG8, GL_R16, GL_R8,
        GL_RGBA16_SNORM, GL_RGBA8_SNORM, GL_RG16_SNORM, GL_RG8_SNORM, GL_R16_SNORM, GL_R8_SNORM
    };
    static_assert(sizeof(desktop_formats) / sizeof(desktop_formats[0]) == 39);
    for (const GLenum format : desktop_formats) {
        check("desktop-valid null image unbind", mg_is_desktop_image_format(format) &&
            mg_is_native_image_format(mg_image_unbind_format(format)));
    }
    for (const GLenum format : {GL_NONE, GL_DEPTH_COMPONENT24, GL_RGB8, GL_RGB9_E5, GL_SRGB8_ALPHA8}) {
        check("invalid null image format remains rejected", !mg_is_desktop_image_format(format) &&
            mg_image_unbind_format(format) == GL_NONE);
    }
    check("null binding does not enable unsupported narrow storage",
        mg_image_unbind_format(GL_R8) == GL_RGBA8 && mg_image_unit_format(GL_R8, GL_R8) == GL_NONE);
    for (const GLenum format : {GL_R8, GL_R16, GL_R16F, GL_R8I, GL_R16I}) {
        check("unimplemented numerical conversion rejected", mg_image_backing_format(format) == GL_NONE &&
            mg_image_qualifier(format) == nullptr && mg_lower_image_storage_format(format) == format);
    }
    std::string source = "layout(binding = 2) uniform iimage2D signedImage;\n";
    mg_normalize_image_qualifiers(source, {{"signedImage", GL_RGBA16I}});
    check("signed metadata qualifier and binding", source ==
        "layout(rgba16i, binding = 2) uniform highp iimage2D signedImage;\n");
    source = "layout(rgba8ui, binding = 1) uniform highp uimage2D imageA;\n";
    mg_normalize_image_qualifiers(source, {{"imageA", GL_RGBA16UI}});
    check("replace legal integer format without duplicate", source ==
        "layout(rgba16ui, binding = 1) uniform highp uimage2D imageA;\n");
    source = "uniform image2D normalizedImage;\n";
    mg_normalize_image_qualifiers(source, {{"normalizedImage", GL_R8}});
    check("unimplemented metadata fails by name", source.find(
        "#error MG_IMAGE_FORMAT_UNMAPPABLE: normalizedImage (r8;") != std::string::npos);
    source = "uniform image2D normalizedImage;\n";
    mg_normalize_image_qualifiers(source, {{"normalizedImage", GL_RGBA16}});
    check("formatless unorm16 metadata gets legal qualifier", source ==
        "layout(rgba32f) uniform highp image2D normalizedImage;\n");
    source = "uniform image2D unregistered;\n";
    mg_normalize_image_qualifiers(source, {});
    check("missing metadata does not guess", source == "uniform image2D unregistered;\n");
    source = "/* uniform iimage2D signedImage; */\n// uniform iimage2D signedImage;\n";
    const auto commented = source;
    mg_normalize_image_qualifiers(source, {{"signedImage", GL_R8I}});
    check("metadata leaves commented declarations intact", source == commented);
    source = "layout (binding=1, rgba8ui /* note */) uniform uimage2D imageA;\n";
    mg_normalize_image_qualifiers(source, {{"imageA", GL_RGBA16UI}});
    check("metadata preserves comments and layout items", source ==
        "layout(binding=1, rgba16ui /* note */) uniform highp uimage2D imageA;\n");
    mg_image_metadata_begin();
    mg_image_metadata_add("signedImage", GL_RGBA8I);
    mg_image_metadata_end();
    const auto metadata = mg_image_metadata_take();
    check("metadata consumed once", metadata.at("signedImage") == GL_RGBA8I && mg_image_metadata_take().empty());
    std::printf("image_metadata: %d failures\n", failures);
    return failures != 0;
}
