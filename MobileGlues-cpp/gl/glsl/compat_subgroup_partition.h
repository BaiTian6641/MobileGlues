#pragma once

#include <string>

// Rewrites the NV subgroup partition duplicate-suppression pattern used by
// iterationRP's backward-analysis compute pass:
//
//   #ifdef MC_GL_VENDOR_NVIDIA
//       uvec4 ballot = subgroupPartitionNV(key);
//       if (subgroupBallotFindLSB(ballot) == gl_SubgroupInvocationID) {
//           importance = subgroupPartitionedMaxNV(importance, ballot);
//           imageAtomicMax(image, coord, floatBitsToUint(importance));
//       }
//   #else
//       ... subgroupShuffleXor/gl_SubgroupSize loop with returns ...
//   #endif
//
// The replacement is the portable workgroup-shared open-addressed partition
// table: one slot per distinct key, atomicCompSwap to claim a key, atomicMax
// for the value, atomicMin for the lowest local invocation (leader), and a
// leader-only imageAtomicMax after a barrier. All barriers stay outside the
// divergent per-invocation conditionals, so control flow remains uniform.
//
// Returns true when the source is unchanged (no pattern) or was rewritten.
// Returns false with a human-readable `reason` (when non-null) when the
// pattern is present but cannot be rewritten safely; `source` is then left
// untouched. Detection is token based, never file-name based.
bool mg_lower_nv_partition_pattern(std::string& source, std::string* reason);
