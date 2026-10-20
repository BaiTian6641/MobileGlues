// Host test for the NV subgroup partition lowering pre-pass. It uses synthetic
// snippets only: no shaderpack source is included, and no GPU is required.

#include "../gl/glsl/compat_subgroup_partition.h"

#include <cstdio>
#include <string>

static int failures = 0;

static void expect(bool condition, const char* what) {
    if (condition) return;
    std::printf("FAIL %s\n", what);
    ++failures;
}

static size_t count(const std::string& haystack, const std::string& needle) {
    size_t total = 0;
    size_t pos = 0;
    while ((pos = haystack.find(needle, pos)) != std::string::npos) {
        ++total;
        pos += needle.size();
    }
    return total;
}

static bool braces_balanced(const std::string& source) {
    int depth = 0;
    for (char c : source) {
        if (c == '{') ++depth;
        if (c == '}') {
            --depth;
            if (depth < 0) return false;
        }
    }
    return depth == 0;
}

static const char* kNoPattern = R"GLSL(#version 450
layout (local_size_x = 8, local_size_y = 8) in;
layout (r32ui) uniform uimage2D img_importance;

void main() {
    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
    imageAtomicMax(img_importance, coord, 1u);
}
)GLSL";

static const char* kSinglePhase = R"GLSL(#version 450
layout (local_size_x = 16, local_size_y = 8) in;
layout (r32ui) uniform uimage2D img_importance;

void main() {
    ivec2 texelCoord = ivec2(gl_GlobalInvocationID.xy);
    float depth = 1.0;
    if (depth >= 0.7 && depth < 1.0) {
        float importance = depth;
        ivec2 shadowTexelCoord = texelCoord;
        uint drawTexelData = uint(shadowTexelCoord.x | (shadowTexelCoord.y << 16));
#ifdef MC_GL_VENDOR_NVIDIA
        uvec4 partitionballot = subgroupPartitionNV(drawTexelData);
        if (subgroupBallotFindLSB(partitionballot) == gl_SubgroupInvocationID) {
            importance = subgroupPartitionedMaxNV(importance, partitionballot);
            imageAtomicMax(img_importance, shadowTexelCoord, floatBitsToUint(importance));
        }
#else
        for (uint u = 1u; u < gl_SubgroupSize; u = u << 1u) {
            if (drawTexelData != subgroupShuffleXor(drawTexelData, u)) {
                imageAtomicMax(img_importance, shadowTexelCoord, floatBitsToUint(importance));
                return;
            } else {
                if ((gl_SubgroupInvocationID & u) == u) return;
            }
        }
        imageAtomicMax(img_importance, shadowTexelCoord, floatBitsToUint(importance));
#endif
    }
}
)GLSL";

static const char* kTwoPhase = R"GLSL(#version 450
layout (local_size_x = 16, local_size_y = 8) in;
layout (r32ui) uniform uimage2D img_importance;

void main() {
    ivec2 texelCoord = ivec2(gl_GlobalInvocationID.xy);
    float depth = 1.0;
    if (depth >= 0.7 && depth < 1.0) {
        float importance = depth;
        ivec2 shadowTexelCoord = texelCoord;
        uint drawTexelData = uint(shadowTexelCoord.x | (shadowTexelCoord.y << 16));
#ifdef MC_GL_VENDOR_NVIDIA
        uvec4 partitionballot = subgroupPartitionNV(drawTexelData);
        if (subgroupBallotFindLSB(partitionballot) == gl_SubgroupInvocationID) {
            importance = subgroupPartitionedMaxNV(importance, partitionballot);
            imageAtomicMax(img_importance, shadowTexelCoord, floatBitsToUint(importance));
        }
#else
        for (uint u = 1u; u < gl_SubgroupSize; u = u << 1u) {
            if (drawTexelData != subgroupShuffleXor(drawTexelData, u)) {
                imageAtomicMax(img_importance, shadowTexelCoord, floatBitsToUint(importance));
                return;
            } else {
                if ((gl_SubgroupInvocationID & u) == u) return;
            }
        }
        imageAtomicMax(img_importance, shadowTexelCoord, floatBitsToUint(importance));
#endif
    }

    float hitFlag = 1.0;
    if (hitFlag > 0.0) {
        float hitImportance = hitFlag;
        ivec2 hitTexelCoord = texelCoord + ivec2(1);
        uint hitTexelData = uint(hitTexelCoord.x | (hitTexelCoord.y << 16));
#ifdef MC_GL_VENDOR_NVIDIA
        uvec4 hitBallot = subgroupPartitionNV(hitTexelData);
        if (subgroupBallotFindLSB(hitBallot) == gl_SubgroupInvocationID) {
            hitImportance = subgroupPartitionedMaxNV(hitImportance, hitBallot);
            imageAtomicMax(img_importance, hitTexelCoord, floatBitsToUint(hitImportance));
        }
#else
        for (uint v = 1u; v < gl_SubgroupSize; v = v << 1u) {
            if (hitTexelData != subgroupShuffleXor(hitTexelData, v)) {
                imageAtomicMax(img_importance, hitTexelCoord, floatBitsToUint(hitImportance));
                return;
            } else {
                if ((gl_SubgroupInvocationID & v) == v) return;
            }
        }
        imageAtomicMax(img_importance, hitTexelCoord, floatBitsToUint(hitImportance));
#endif
    }
}
)GLSL";

// Same tokens, but the vendor block is not inside a conditional, so the
// barriers cannot be inserted without changing control flow. The rewrite must
// refuse rather than emit a half-transformed shader.
static const char* kUnsupported = R"GLSL(#version 450
layout (local_size_x = 16, local_size_y = 8) in;
layout (r32ui) uniform uimage2D img_importance;

void main() {
    float importance = 1.0;
    ivec2 shadowTexelCoord = ivec2(0);
    uint drawTexelData = 1u;
#ifdef MC_GL_VENDOR_NVIDIA
    uvec4 partitionballot = subgroupPartitionNV(drawTexelData);
    if (subgroupBallotFindLSB(partitionballot) == gl_SubgroupInvocationID) {
        importance = subgroupPartitionedMaxNV(importance, partitionballot);
        imageAtomicMax(img_importance, shadowTexelCoord, floatBitsToUint(importance));
    }
#else
    for (uint u = 1u; u < gl_SubgroupSize; u = u << 1u) {
        if (drawTexelData != subgroupShuffleXor(drawTexelData, u)) {
            imageAtomicMax(img_importance, shadowTexelCoord, floatBitsToUint(importance));
            return;
        } else {
            if ((gl_SubgroupInvocationID & u) == u) return;
        }
    }
    imageAtomicMax(img_importance, shadowTexelCoord, floatBitsToUint(importance));
#endif
}
)GLSL";

static void test_no_op() {
    std::string source = kNoPattern;
    const std::string original = source;
    std::string reason = "stale";
    expect(mg_lower_nv_partition_pattern(source, &reason), "no-op returns true");
    expect(source == original, "no-op leaves the source unchanged");
    expect(reason.empty(), "no-op clears the reason");

    // A plain KHR shuffle without an NV partition block is not this pattern.
    std::string khr = "#version 450\nlayout (local_size_x = 8) in;\nvoid main() {\n"
                      "    float x = subgroupShuffleXor(1.0, 1u);\n}\n";
    const std::string khr_original = khr;
    expect(mg_lower_nv_partition_pattern(khr, nullptr), "KHR-only source returns true");
    expect(khr == khr_original, "KHR-only source is unchanged");
}

static void test_single_phase() {
    std::string source = kSinglePhase;
    std::string reason;
    expect(mg_lower_nv_partition_pattern(source, &reason), "single phase rewrites");
    expect(reason.empty(), "single phase has no failure reason");
    expect(source.find("subgroupShuffleXor") == std::string::npos,
           "single phase has no subgroupShuffleXor");
    expect(source.find("gl_Subgroup") == std::string::npos, "single phase has no gl_Subgroup*");
    expect(source.find("atomicCompSwap") != std::string::npos, "single phase claims slots atomically");
    expect(source.find("atomicMax(mgSubgroupPartitionValueTable") != std::string::npos,
           "single phase reduces the value with atomicMax");
    expect(source.find("atomicMin(mgSubgroupPartitionLeaderTable") != std::string::npos,
           "single phase picks the leader with atomicMin");
    expect(count(source, "barrier();") == 3, "single phase has three uniform barriers");
    expect(count(source, "mgSubgroupPartitionClear(gl_LocalInvocationIndex)") == 1,
           "single phase clears one shared table");
    expect(count(source, "mgSubgroupPartitionSlot = mgSubgroupPartitionInsert(") == 1,
           "single phase inserts one partition");
    expect(source.find("MG_SUBGROUP_PARTITION_TABLE_SIZE 256u") != std::string::npos,
           "single phase uses 256 slots");
    expect(source.find("MG_SUBGROUP_PARTITION_WORKGROUP_SIZE 128u") != std::string::npos,
           "single phase is sized for 128 invocations");
    expect(source.find("imageAtomicMax(img_importance, mgSubgroupPartitionCoord, "
                       "mgSubgroupPartitionValueTable[mgSubgroupPartitionSlot])") !=
               std::string::npos,
           "single phase writes the partition maximum from the leader");
    expect(braces_balanced(source), "single phase braces are balanced");
}

static void test_two_phases() {
    std::string source = kTwoPhase;
    std::string reason;
    expect(mg_lower_nv_partition_pattern(source, &reason), "two phases rewrite");
    expect(reason.empty(), "two phases have no failure reason");
    expect(source.find("subgroupShuffleXor") == std::string::npos,
           "two phases have no subgroupShuffleXor");
    expect(source.find("gl_Subgroup") == std::string::npos, "two phases have no gl_Subgroup*");
    expect(count(source, "mgSubgroupPartitionSlot = mgSubgroupPartitionInsert(") == 2,
           "two phases claim slots atomically twice");
    expect(count(source, "mgSubgroupPartitionClear(gl_LocalInvocationIndex)") == 2,
           "two phases clear separate tables");
    expect(count(source, "barrier();") == 6, "two phases have six uniform barriers");
    expect(count(source, "imageAtomicMax(img_importance, mgSubgroupPartitionCoord, "
                         "mgSubgroupPartitionValueTable[mgSubgroupPartitionSlot])") == 2,
           "two phases each write from their leader");
    expect(source.find("mgSubgroupPartitionKey = (drawTexelData);") != std::string::npos,
           "depth phase captures its NV key");
    expect(source.find("mgSubgroupPartitionKey = (hitTexelData);") != std::string::npos,
           "hit phase captures its NV key");
    expect(braces_balanced(source), "two phase braces are balanced");
}

static void test_unsupported() {
    std::string source = kUnsupported;
    const std::string original = source;
    std::string reason = "stale";
    expect(!mg_lower_nv_partition_pattern(source, &reason), "unsafe pattern returns false");
    expect(!reason.empty(), "unsafe pattern reports a reason");
    expect(reason.find("conditional") != std::string::npos,
           "unsafe pattern reason mentions the missing conditional");
    expect(source == original, "unsafe pattern is never half-rewritten");
}

int main() {
    test_no_op();
    test_single_phase();
    test_two_phases();
    test_unsupported();

    if (failures == 0) {
        std::printf("compat subgroup partition: all checks passed\n");
    } else {
        std::printf("compat subgroup partition: %d failure(s)\n", failures);
    }
    return failures == 0 ? 0 : 1;
}
