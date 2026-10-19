// Host test for translation cache identity. No GPU or translator is needed:
// the point is that every input which changes generated GLSL changes the key.

#include "../gl/glsl/cache_key.h"

#include <cstdio>
#include <string>

static int failures = 0;

static void expect_changed(const char* what, const std::string& a, const std::string& b) {
    if (a != b) return;
    std::printf("FAIL %s: keys are equal\n", what);
    ++failures;
}

int main() {
    const std::string base =
        build_translation_cache_key("#version 460\nvoid main() {}\n", 0x8B31u, 310, 320, false, false, false,
                                    false, 0, 2, 0, 0, 0);
    const std::string same =
        build_translation_cache_key("#version 460\nvoid main() {}\n", 0x8B31u, 310, 320, false, false, false,
                                    false, 0, 2, 0, 0, 0);
    if (base != same) {
        std::printf("FAIL deterministic: identical inputs differ\n");
        ++failures;
    }
    expect_changed("source",
                   base, build_translation_cache_key("#version 460\nvoid main() { int x; }\n", 0x8B31u, 310, 320,
                                                     false, false, false, false, 0, 2, 0, 0, 0));
    expect_changed("stage", base,
                   build_translation_cache_key("#version 460\nvoid main() {}\n", 0x8B30u, 310, 320, false, false,
                                               false, false, 0, 2, 0, 0, 0));
    expect_changed("essl", base,
                   build_translation_cache_key("#version 460\nvoid main() {}\n", 0x8B31u, 300, 320, false, false,
                                               false, false, 0, 2, 0, 0, 0));
    expect_changed("backend", base,
                   build_translation_cache_key("#version 460\nvoid main() {}\n", 0x8B31u, 310, 300, false, false,
                                               false, false, 0, 2, 0, 0, 0));
    expect_changed("texture-buffer emulation",
                   base, build_translation_cache_key("#version 460\nvoid main() {}\n", 0x8B31u, 310, 320, true,
                                                     false, false, false, 0, 2, 0, 0, 0));
    expect_changed("texture-query-lod",
                   base, build_translation_cache_key("#version 460\nvoid main() {}\n", 0x8B31u, 310, 320, false,
                                                     true, false, false, 0, 2, 0, 0, 0));
    expect_changed("compute", base,
                   build_translation_cache_key("#version 460\nvoid main() {}\n", 0x8B31u, 310, 320, false, false,
                                               true, false, 0, 2, 0, 0, 0));
    expect_changed("dsa", base,
                   build_translation_cache_key("#version 460\nvoid main() {}\n", 0x8B31u, 310, 320, false, false,
                                               false, true, 0, 2, 0, 0, 0));
    expect_changed("hide level", base,
                   build_translation_cache_key("#version 460\nvoid main() {}\n", 0x8B31u, 310, 320, false, false,
                                               false, false, 1, 2, 0, 0, 0));
    expect_changed("patch version", base,
                   build_translation_cache_key("#version 460\nvoid main() {}\n", 0x8B31u, 310, 320, false, false,
                                               false, false, 0, 2, 0, 0, 1));
    if (failures == 0) std::printf("cache key: all identities distinct\n");
    return failures == 0 ? 0 : 1;
}
