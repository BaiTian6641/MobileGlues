// MobileGlues - tests/shader_source_test.cpp
// SPDX-License-Identifier: LGPL-2.1-only
//
// Host regression for the glShaderSource submission contract. The previous
// frontend concatenated the array but passed the original count with a
// one-element pointer array to the backend; a multi-string submission read past
// the array and, on the X1E device, segfaulted. This test pins the exact byte
// collection layer; the device wrapper_probe pins the backend submission.

#include "gl/shader_source.h"

#include <cstdio>
#include <cstring>
#include <string>

static int g_failures = 0;

static void check(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++g_failures;
    }
}

static void test_explicit_lengths() {
    const char* pieces[] = {"abc", "def", "ghi"};
    const GLint lengths[] = {3, 2, 1};
    std::string out;
    GLenum error = GL_NO_ERROR;
    const bool ok = mg_collect_shader_source(3, pieces, lengths, &out, &error);
    check(ok, "explicit lengths collected");
    check(error == GL_NO_ERROR, "explicit lengths no error");
    check(out == "abcdeg", "explicit lengths exact bytes");
}

static void test_negative_length_means_nul_terminated() {
    const char* pieces[] = {"abc", "def"};
    const GLint lengths[] = {-1, 2};
    std::string out;
    GLenum error = GL_NO_ERROR;
    const bool ok = mg_collect_shader_source(2, pieces, lengths, &out, &error);
    check(ok, "negative length collected");
    check(out == "abcde", "negative length uses strlen and copies the following explicit piece");
}

static void test_embedded_nul_preserved() {
    const char with_nul[] = {'a', 'b', '\0', 'c', 'd'};
    const char* pieces[] = {with_nul};
    const GLint lengths[] = {5};
    std::string out;
    GLenum error = GL_NO_ERROR;
    const bool ok = mg_collect_shader_source(1, pieces, lengths, &out, &error);
    check(ok, "embedded NUL collected");
    check(out.size() == 5, "embedded NUL size preserved");
    check(std::memcmp(out.data(), with_nul, 5) == 0, "embedded NUL bytes preserved");
}

static void test_null_lengths() {
    const char* pieces[] = {"abc", "def"};
    std::string out;
    GLenum error = GL_NO_ERROR;
    const bool ok = mg_collect_shader_source(2, pieces, nullptr, &out, &error);
    check(ok, "null lengths collected");
    check(out == "abcdef", "null lengths use strlen");
}

static void test_zero_count() {
    std::string out = "unchanged";
    GLenum error = GL_NO_ERROR;
    const bool ok = mg_collect_shader_source(0, nullptr, nullptr, &out, &error);
    check(ok, "zero count is accepted");
    check(out.empty(), "zero count produces empty source");
    check(error == GL_NO_ERROR, "zero count no error");
}

static void test_invalid_arguments() {
    const char* pieces[] = {"abc"};
    const GLint lengths[] = {3};
    std::string out;
    GLenum error = GL_NO_ERROR;

    check(!mg_collect_shader_source(-1, pieces, lengths, &out, &error), "negative count rejected");
    check(error == GL_INVALID_VALUE, "negative count sets GL_INVALID_VALUE");

    error = GL_NO_ERROR;
    check(!mg_collect_shader_source(1, nullptr, lengths, &out, &error), "null string array rejected");
    check(error == GL_INVALID_VALUE, "null string array sets GL_INVALID_VALUE");

    const char* null_piece[] = {nullptr};
    error = GL_NO_ERROR;
    check(!mg_collect_shader_source(1, null_piece, lengths, &out, &error), "null string rejected");
    check(error == GL_INVALID_VALUE, "null string sets GL_INVALID_VALUE");
}

static void test_late_uniform_hoist() {
    const std::string source = "#version 320 es\nprecision highp float;\n"
        "float f() { return 1.0; }\nuniform highp vec4 tint;\nvoid main() {}\n";
    const std::string expected = "#version 320 es\nprecision highp float;\n"
        "uniform highp vec4 tint;\nfloat f() { return 1.0; }\nvoid main() {}\n";
    check(mg_hoist_late_global_uniforms(source) == expected, "late built-in uniform moves before first function");
    check(mg_hoist_late_global_uniforms(expected) == expected, "hoist is idempotent");
    const std::string split = "#version 320 es\nfloat\nf() { return 1.0; }\nuniform float exposure;";
    check(mg_hoist_late_global_uniforms(split) ==
          "#version 320 es\nuniform float exposure;\nfloat\nf() { return 1.0; }",
          "hoist preserves a split return type and missing final newline");
    const char* unchanged[] = {
        "#define UNUSED \\\nfloat f() {} \\\nuniform float tint;\n",
        "/* docs\n*/ float f() {}\nuniform float tint;\n",
        "/* docs\n*/ float\nf() {}\nuniform float tint;\n",
        "float f() {}\nprecision\tlowp float;\nuniform float tint;\n",
        "float f() {}\nprecision lowp float;\nuniform float tint;\n",
        "float f() {}\nstruct Material { float value; };\nuniform Material material;\n",
        "float f() {}\nconst int count = 4;\nuniform float weights[count];\n",
        "float f() {}\n#define SCALE 2\nuniform float scale;\n",
        "float f() {}\n#extension GL_EXT_test : enable\nuniform float scale;\n",
        "float f() {}\n#if FEATURE\nuniform float scale;\n#endif\n",
        "float f() {}\nuniform Block { float scale; };\n",
        "float f() {}\nlayout(location=0) uniform float scale;\n",
        "float f() {}\nuniform float\nscale;\n",
        "float f() {}\n/* comment\nuniform float fake;\n*/\n",
        "float f() {}\nuniform float scale; /* comment\ncontinued */\n",
        "float f() {\nuniform float invalidLocal;\n}\n"
    };
    for (const char* text : unchanged) {
        check(mg_hoist_late_global_uniforms(text) == text, "dependent or non-global declaration stays unchanged");
    }
}

int main() {
    test_explicit_lengths();
    test_negative_length_means_nul_terminated();
    test_embedded_nul_preserved();
    test_null_lengths();
    test_zero_count();
    test_invalid_arguments();
    test_late_uniform_hoist();
    if (g_failures) {
        std::fprintf(stderr, "%d shader-source checks failed\n", g_failures);
        return 1;
    }
    std::printf("shader source: all checks passed (0 failures)\n");
    return 0;
}
