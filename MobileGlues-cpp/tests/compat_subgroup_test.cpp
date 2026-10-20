// MobileGlues - tests/compat_subgroup_test.cpp
// SPDX-License-Identifier: LGPL-2.1-only
//
// Host checks for the compute-shader subgroup lowering. These pin the exact
// rewrite shapes: virtual 32-wide subgroup IDs, the workgroup-shared scan,
// the FidelityFX SPD quad reduction, extension removal and the loud #error
// path for an unsupported NV partition block.

#include "../gl/glsl/compat_subgroup.h"

#include <cstdio>
#include <string>

static int g_failures = 0;

static void check(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++g_failures;
    }
}

static bool contains(const std::string& text, const char* needle) {
    return text.find(needle) != std::string::npos;
}

static bool equals(const std::string& a, const std::string& b) {
    return a == b;
}

static void test_noop() {
    const std::string source = "#version 450\nlayout(local_size_x = 16, local_size_y = 16) in;\n"
                               "void main() { uint i = gl_LocalInvocationIndex; }\n";
    const MgSubgroupLoweringResult r = mg_lower_subgroup_constructs(source, GL_COMPUTE_SHADER);
    check(!r.transformed, "no-op source is not transformed");
    check(!r.unsupported, "no-op source is supported");
    check(equals(r.source, source), "no-op source is byte-identical");

    const std::string fragment = "#version 450\nvoid main() { uint i = gl_SubgroupInvocationID; }\n";
    const MgSubgroupLoweringResult f = mg_lower_subgroup_constructs(fragment, GL_FRAGMENT_SHADER);
    check(!f.transformed, "non-compute stage is not transformed");
    check(equals(f.source, fragment), "non-compute stage is byte-identical");
}

static void test_ids() {
    const std::string source =
        "#version 450\n"
        "#extension GL_KHR_shader_subgroup_arithmetic : enable\n"
        "#extension GL_KHR_shader_subgroup_basic : enable\n"
        "layout(local_size_x = 32, local_size_y = 16) in;\n"
        "// gl_SubgroupSize in a comment stays put\n"
        "/* gl_SubgroupID in a block comment stays put */\n"
        "#define MG_DEBUG_SUBGROUP gl_SubgroupSize\n"
        "void main() {\n"
        "  uint a = gl_SubgroupInvocationID;\n"
        "  uint b = gl_SubgroupSize;\n"
        "  uint c = gl_SubgroupID;\n"
        "  uint d = gl_NumSubgroups;\n"
        "  const char* s = \"gl_SubgroupSize\";\n"
        "}\n";
    const MgSubgroupLoweringResult r = mg_lower_subgroup_constructs(source, GL_COMPUTE_SHADER);
    check(r.transformed, "ID source is transformed");
    check(!r.unsupported, "ID source is supported");
    check(contains(r.source, "(gl_LocalInvocationIndex & 31u)"), "invocation id lowered");
    check(contains(r.source, "uint b = 32u;"), "subgroup size lowered to 32");
    check(contains(r.source, "(gl_LocalInvocationIndex >> 5u)"), "subgroup id lowered");
    check(contains(r.source, "(MG_SUBGROUP_WORKGROUP_SIZE / 32u)"), "num subgroups lowered");
    check(contains(r.source, "const uint MG_SUBGROUP_WORKGROUP_SIZE = 512u;"),
          "workgroup size parsed from 32x16");
    check(!contains(r.source, "#extension GL_KHR_shader_subgroup_"), "KHR extension lines removed");
    check(contains(r.source, "// gl_SubgroupSize in a comment stays put"),
          "comment token not rewritten");
    check(contains(r.source, "/* gl_SubgroupID in a block comment stays put */"),
          "block comment token not rewritten");
    check(contains(r.source, "#define MG_DEBUG_SUBGROUP gl_SubgroupSize"),
          "preprocessor token not rewritten");
    check(contains(r.source, "\"gl_SubgroupSize\""), "string token not rewritten");
}

static void test_default_workgroup_size() {
    const std::string source = "#version 450\nvoid main() { uint a = gl_NumSubgroups; }\n";
    const MgSubgroupLoweringResult r = mg_lower_subgroup_constructs(source, GL_COMPUTE_SHADER);
    check(r.transformed && !r.unsupported, "default-size source is transformed");
    check(contains(r.source, "const uint MG_SUBGROUP_WORKGROUP_SIZE = 1024u;"),
          "workgroup size defaults to 1024");
}

static void test_conditional_layouts() {
    const std::string source =
        "#version 450\n"
        "#if RTW_RESOLUTION == 256\n"
        "layout(local_size_x = 256) in;\n"
        "#elif RTW_RESOLUTION == 1024\n"
        "layout(local_size_x = 1024) in;\n"
        "#endif\n"
        "void main() { uint n = gl_NumSubgroups; float f = subgroupInclusiveAdd(1.0); }\n";
    const MgSubgroupLoweringResult r = mg_lower_subgroup_constructs(source, GL_COMPUTE_SHADER);
    check(r.transformed && !r.unsupported, "conditional-layout source is transformed");
    check(contains(r.source, "const uint MG_SUBGROUP_WORKGROUP_SIZE = "
                             "uint(gl_WorkGroupSize.x * gl_WorkGroupSize.y * gl_WorkGroupSize.z);"),
          "conditional layout uses the active gl_WorkGroupSize");
    check(contains(r.source, "shared vec4 mgSubgroupScratch[1024];"),
          "conditional layout scratch covers the largest branch");
}

static void test_unrelated_identifier() {
    const std::string source = "#version 450\nlayout(local_size_x = 32) in;\n"
                               "void main() { uint mySubgroups = 1u; }\n";
    const MgSubgroupLoweringResult r = mg_lower_subgroup_constructs(source, GL_COMPUTE_SHADER);
    check(!r.transformed, "unrelated identifier is not treated as a subgroup construct");
    check(equals(r.source, source), "unrelated identifier source is untouched");
}

static void test_inclusive_add() {
    const std::string source =
        "#version 450\n"
        "#extension GL_KHR_shader_subgroup_arithmetic : enable\n"
        "layout(local_size_x = 512) in;\n"
        "void main() {\n"
        "  float f = subgroupInclusiveAdd(1.0);\n"
        "  vec2 v2 = subgroupInclusiveAdd(vec2(1.0));\n"
        "  vec3 v3 = subgroupInclusiveAdd(vec3(1.0));\n"
        "  vec4 v4 = subgroupInclusiveAdd(vec4(1.0));\n"
        "}\n";
    const MgSubgroupLoweringResult r = mg_lower_subgroup_constructs(source, GL_COMPUTE_SHADER);
    check(r.transformed && !r.unsupported, "inclusive-add source is transformed");
    check(contains(r.source, "float f = mgSubgroupInclusiveAdd(1.0);"), "scalar call renamed");
    check(contains(r.source, "vec2 v2 = mgSubgroupInclusiveAdd(vec2(1.0));"), "vec2 call renamed");
    check(contains(r.source, "vec3 v3 = mgSubgroupInclusiveAdd(vec3(1.0));"), "vec3 call renamed");
    check(contains(r.source, "vec4 v4 = mgSubgroupInclusiveAdd(vec4(1.0));"), "vec4 call renamed");
    check(!contains(r.source, "= subgroupInclusiveAdd("), "original inclusive-add calls gone");
    check(contains(r.source, "shared vec4 mgSubgroupScratch[512];"), "scan scratch declared");
    check(contains(r.source, "float mgSubgroupInclusiveAdd(float value)"), "float overload injected");
    check(contains(r.source, "vec2 mgSubgroupInclusiveAdd(vec2 value)"), "vec2 overload injected");
    check(contains(r.source, "vec3 mgSubgroupInclusiveAdd(vec3 value)"), "vec3 overload injected");
    check(contains(r.source, "vec4 mgSubgroupInclusiveAdd(vec4 value)"), "vec4 overload injected");
    check(contains(r.source, "uint lane = gl_LocalInvocationIndex & 31u;"), "lane mask uses 32 lanes");
    check(contains(r.source, "uint base = (gl_LocalInvocationIndex >> 5u) << 5u;"),
          "base = localIndex/32*32");
    check(contains(r.source, "offset < 32u"), "scan is 32 lanes wide");
}

static void test_generic_quad_swap() {
    const std::string source =
        "#version 450\n"
        "#extension GL_KHR_shader_subgroup_quad : enable\n"
        "layout(local_size_x = 16, local_size_y = 16) in;\n"
        "void main() {\n"
        "  vec4 a = vec4(0.0);\n"
        "  a = subgroupQuadSwapHorizontal(a);\n"
        "  a = subgroupQuadSwapVertical(a);\n"
        "  a = subgroupQuadSwapDiagonal(a);\n"
        "}\n";
    const MgSubgroupLoweringResult r = mg_lower_subgroup_constructs(source, GL_COMPUTE_SHADER);
    check(r.transformed && !r.unsupported, "generic quad source is transformed");
    check(!contains(r.source, "= subgroupQuadSwap"), "generic quad calls renamed");
    check(contains(r.source, "mgSubgroupQuadSwapHorizontal(a)"), "horizontal call renamed");
    check(contains(r.source, "mgSubgroupQuadSwapVertical(a)"), "vertical call renamed");
    check(contains(r.source, "mgSubgroupQuadSwapDiagonal(a)"), "diagonal call renamed");
    check(contains(r.source, "shared vec4 mgSubgroupQuadScratch[256];"), "quad scratch declared");
    check(contains(r.source, "lane ^ 1u"), "horizontal swap uses lane ^ 1");
    check(contains(r.source, "lane ^ 2u"), "vertical swap uses lane ^ 2");
    check(contains(r.source, "lane ^ 3u"), "diagonal swap uses lane ^ 3");
}

static void test_spd_rewrite() {
    const std::string source =
        "#version 450\n"
        "#extension GL_KHR_shader_subgroup_quad:require\n"
        "shared vec4 spdIntermediate[16][16];\n"
        "vec4 SpdLoadIntermediate(uint x, uint y){ return spdIntermediate[x][y]; }\n"
        "void SpdStoreIntermediate(uint x, uint y, vec4 value){ spdIntermediate[x][y] = value; }\n"
        "void SpdStore(ivec2 p, vec4 value, uint mip, uint slice) { }\n"
        "vec4 SpdReduce4(vec4 v0, vec4 v1, vec4 v2, vec4 v3){ return v0; }\n"
        "vec4 SpdReduceQuad(vec4 v){\n"
        "    vec4 v0 = v;\n"
        "    vec4 v1 = subgroupQuadSwapHorizontal(v);\n"
        "    vec4 v2 = subgroupQuadSwapVertical(v);\n"
        "    vec4 v3 = subgroupQuadSwapDiagonal(v);\n"
        "    return SpdReduce4(v0, v1, v2, v3);\n"
        "    /* the AMD intrinsic fallback mentioned subgroupQuadSwapHorizontal */\n"
        "    return v;\n"
        "}\n"
        "void SpdDownsampleMip_3(uint x, uint y, uvec2 workGroupID, uint localInvocationIndex, uint mip, uint slice)\n"
        "{\n"
        "    if (localInvocationIndex < 64)\n"
        "    {\n"
        "        vec4 v = SpdLoadIntermediate(x * 2 + y % 2,y * 2);\n"
        "        v = SpdReduceQuad(v);\n"
        "        if (localInvocationIndex % 4 == 0){ SpdStore(ivec2(0), v, mip, slice); }\n"
        "    }\n"
        "}\n"
        "void SpdDownsampleMip_4(uint x, uint y, uvec2 workGroupID, uint localInvocationIndex, uint mip, uint slice)\n"
        "{\n"
        "    if (localInvocationIndex < 16)\n"
        "    {\n"
        "        vec4 v = SpdLoadIntermediate(x * 4 + y,y * 4);\n"
        "        v = SpdReduceQuad(v);\n"
        "        if (localInvocationIndex % 4 == 0){ SpdStore(ivec2(0), v, mip, slice); }\n"
        "    }\n"
        "}\n"
        "void SpdDownsampleMip_5(uvec2 workGroupID, uint localInvocationIndex, uint mip, uint slice)\n"
        "{\n"
        "    if (localInvocationIndex < 4)\n"
        "    {\n"
        "        vec4 v = SpdLoadIntermediate(localInvocationIndex,0);\n"
        "        v = SpdReduceQuad(v);\n"
        "        if (localInvocationIndex % 4 == 0){ SpdStore(ivec2(0), v, mip, slice); }\n"
        "    }\n"
        "}\n"
        "layout (local_size_x = 16, local_size_y = 16) in;\n";
    const MgSubgroupLoweringResult r = mg_lower_subgroup_constructs(source, GL_COMPUTE_SHADER);
    check(r.transformed && !r.unsupported, "SPD source is transformed");
    check(!contains(r.source, "subgroupQuadSwap"), "SPD quad intrinsics removed");
    check(!contains(r.source, "#extension GL_KHR_shader_subgroup_"), "SPD extension removed");
    check(contains(r.source, "shared vec4 spdQuadScratch[256];"), "SPD scratch declared for 16x16");
    check(contains(r.source, "spdQuadScratch[lane] = v;"), "SPD reduce stores through scratch");
    check(contains(r.source, "spdQuadScratch[lane ^ 1u]"), "SPD horizontal partner");
    check(contains(r.source, "spdQuadScratch[lane ^ 2u]"), "SPD vertical partner");
    check(contains(r.source, "spdQuadScratch[lane ^ 3u]"), "SPD diagonal partner");
    check(contains(r.source, "return SpdReduce4(v0, v1, v2, v3);"), "SPD reduce keeps SpdReduce4");
    size_t v3_decls = 0, pos = 0;
    while ((pos = r.source.find("vec4 v3 = spdQuadScratch", pos)) != std::string::npos) {
        ++v3_decls;
        pos += 8;
    }
    check(v3_decls == 1, "SPD reduce declares v3 once");
    check(!contains(r.source, "return v;\n}"), "old divergent AMD fallback body removed");

    check(contains(r.source, "bool spdActive = localInvocationIndex < 64u;"), "mip 3 owner test");
    check(contains(r.source, "bool spdActive = localInvocationIndex < 16u;"), "mip 4 owner test");
    check(contains(r.source, "bool spdActive = localInvocationIndex < 4u;"), "mip 5 owner test");
    check(contains(r.source, "vec4 v = vec4(0.0);"), "non-owners get an identity value");
    check(contains(r.source, "v = SpdReduceQuad(v);"), "all invocations call SpdReduceQuad");
    check(contains(r.source, "if (spdActive && localInvocationIndex % 4 == 0)"),
          "only owners store");
    check(!contains(r.source, "if (localInvocationIndex < 64)\n"),
          "old divergent mip 3 guard removed");
    check(!contains(r.source, "if (localInvocationIndex < 16)\n"),
          "old divergent mip 4 guard removed");
    check(!contains(r.source, "if (localInvocationIndex < 4)\n"),
          "old divergent mip 5 guard removed");
}

static void test_extension_only() {
    const std::string source = "#version 450\n"
                               "#extension GL_KHR_shader_subgroup_arithmetic : enable\n"
                               "#extension GL_NV_shader_subgroup_partitioned : enable\n"
                               "layout(local_size_x = 8) in;\n"
                               "void main() {}\n";
    const MgSubgroupLoweringResult r = mg_lower_subgroup_constructs(source, GL_COMPUTE_SHADER);
    check(r.transformed && !r.unsupported, "extension-only source is accepted");
    check(!contains(r.source, "#extension GL_KHR_shader_subgroup_"), "KHR extension line removed");
    check(!contains(r.source, "#extension GL_NV_shader_subgroup_partitioned"), "NV extension line removed");
    check(contains(r.source, "void main() {}"), "remaining source preserved");
}

static void test_unsupported_partition() {
    // A vendor partition block without the #else XOR fallback cannot be lowered
    // safely; the partition pass rejects it and this pass must fail loudly.
    const std::string source =
        "#version 450\n"
        "#extension GL_NV_shader_subgroup_partitioned : enable\n"
        "#extension GL_KHR_shader_subgroup_ballot : enable\n"
        "layout(local_size_x = 8, local_size_y = 8) in;\n"
        "void main() {\n"
        "#ifdef MC_GL_VENDOR_NVIDIA\n"
        "  uvec4 b = subgroupPartitionNV(1u);\n"
        "  if (subgroupBallotFindLSB(b) == gl_SubgroupInvocationID) {\n"
        "    uint importance = subgroupPartitionedMaxNV(1u, b);\n"
        "    imageAtomicMax(img, ivec2(0), importance);\n"
        "  }\n"
        "#endif\n"
        "}\n";
    const MgSubgroupLoweringResult r = mg_lower_subgroup_constructs(source, GL_COMPUTE_SHADER);
    check(r.transformed, "unsupported partition source reports a transform attempt");
    check(r.unsupported, "unsupported partition source is flagged");
    check(!r.reason.empty(), "unsupported partition source carries a reason");
    check(contains(r.source, "#error MG_SUBGROUP_UNSUPPORTED:"), "unsupported source emits #error");
    check(r.source.find(r.reason) != std::string::npos, "reason is included in the #error line");
}

int main() {
    test_noop();
    test_ids();
    test_default_workgroup_size();
    test_conditional_layouts();
    test_unrelated_identifier();
    test_inclusive_add();
    test_generic_quad_swap();
    test_spd_rewrite();
    test_extension_only();
    test_unsupported_partition();

    if (g_failures == 0) {
        std::printf("compat_subgroup: all checks passed\n");
    }
    return g_failures == 0 ? 0 : 1;
}
