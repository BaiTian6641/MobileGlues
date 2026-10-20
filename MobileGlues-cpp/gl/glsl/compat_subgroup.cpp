// MobileGlues - gl/glsl/compat_subgroup.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header
//
// Desktop GLSL subgroup lowering for the ES 3.2 path. The desktop shader may
// use a 32/64-wide hardware subgroup and NV partitioned groups, while the ES
// backend has neither. Every subgroup is mapped to 32 consecutive local
// invocations and all communication goes through workgroup shared memory.
//
// Only compute shaders are touched: the constructs are workgroup-shaped and
// the other stages have no gl_LocalInvocationIndex. Sources without a
// subgroup construct are returned byte-for-byte unchanged.

#include "compat_subgroup.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <regex>
#include <string>
#include <utility>
#include <vector>

#include "compat_subgroup_partition.h"

namespace {

bool is_ident_char(char c) {
    const unsigned char uc = static_cast<unsigned char>(c);
    return std::isalnum(uc) != 0 || c == '_';
}

// 1 outside comments and string literals. GLSL has no string literals but the
// translator runs before parsing, so a stray quote must not swallow the file.
std::vector<char> lex_mask(const std::string& s) {
    std::vector<char> mask(s.size(), 1);
    const size_t n = s.size();
    size_t i = 0;
    while (i < n) {
        const char c = s[i];
        if (c == '/' && i + 1 < n && s[i + 1] == '/') {
            while (i < n && s[i] != '\n') mask[i++] = 0;
            continue;
        }
        if (c == '/' && i + 1 < n && s[i + 1] == '*') {
            mask[i++] = 0;
            mask[i++] = 0;
            while (i < n) {
                if (s[i] == '*' && i + 1 < n && s[i + 1] == '/') {
                    mask[i++] = 0;
                    mask[i++] = 0;
                    break;
                }
                mask[i++] = 0;
            }
            continue;
        }
        if (c == '"') {
            mask[i++] = 0;
            while (i < n) {
                if (s[i] == '\\' && i + 1 < n) {
                    mask[i++] = 0;
                    mask[i++] = 0;
                    continue;
                }
                if (s[i] == '"') {
                    mask[i++] = 0;
                    break;
                }
                mask[i++] = 0;
            }
            continue;
        }
        ++i;
    }
    return mask;
}

// 1 only in tokens that may be rewritten: outside comments/strings and outside
// preprocessor directives (including backslash continuation lines).
std::vector<char> code_mask(const std::string& s) {
    std::vector<char> mask = lex_mask(s);
    const size_t n = s.size();
    size_t pos = 0;
    bool in_directive = false;
    while (pos < n) {
        size_t eol = s.find('\n', pos);
        if (eol == std::string::npos) eol = n;
        if (!in_directive) {
            // A comment before the '#' still leaves the directive first in the
            // line (comments count as whitespace to the preprocessor).
            size_t p = pos;
            while (p < eol && (std::isspace(static_cast<unsigned char>(s[p])) != 0 || mask[p] == 0)) ++p;
            if (p < eol && s[p] == '#') in_directive = true;
        }
        if (in_directive) {
            for (size_t k = pos; k < eol; ++k) mask[k] = 0;
            size_t last = eol;
            if (last > pos && s[last - 1] == '\r') --last;
            const bool continued = last > pos && s[last - 1] == '\\';
            if (!continued) in_directive = false;
        }
        pos = (eol < n) ? eol + 1 : n;
    }
    return mask;
}

size_t find_in_code(const std::string& s, const char* needle, const std::vector<char>& mask) {
    const size_t len = std::strlen(needle);
    if (len == 0) return std::string::npos;
    size_t pos = 0;
    while ((pos = s.find(needle, pos)) != std::string::npos) {
        if (pos < mask.size() && mask[pos] != 0) return pos;
        ++pos;
    }
    return std::string::npos;
}

bool contains_in_code(const std::string& s, const char* needle) {
    return find_in_code(s, needle, lex_mask(s)) != std::string::npos;
}

// Replace whole identifier tokens only. Preprocessor directives, comments and
// strings are never touched.
bool replace_token(std::string& s, const char* token, const std::string& replacement) {
    const std::vector<char> mask = code_mask(s);
    const size_t token_len = std::strlen(token);
    if (token_len == 0 || s.size() < token_len) return false;

    std::string out;
    out.reserve(s.size());
    bool changed = false;
    size_t i = 0;
    while (i < s.size()) {
        if (i + token_len <= s.size() && mask[i] != 0 && s.compare(i, token_len, token) == 0) {
            const bool left_ok = i == 0 || !is_ident_char(s[i - 1]);
            const bool right_ok = i + token_len >= s.size() || !is_ident_char(s[i + token_len]);
            if (left_ok && right_ok) {
                out += replacement;
                i += token_len;
                changed = true;
                continue;
            }
        }
        out += s[i++];
    }
    if (changed) s.swap(out);
    return changed;
}

// Replace a function definition, found by a signature prefix such as
// "vec4 SpdReduceQuad(". Braces inside comments/strings do not count.
bool replace_function(std::string& s, const char* signature, const std::string& replacement) {
    const std::vector<char> mask = code_mask(s);
    const size_t sig_len = std::strlen(signature);
    size_t search = 0;
    while (search < s.size()) {
        const size_t start = s.find(signature, search);
        if (start == std::string::npos) return false;
        search = start + sig_len;
        if (start >= mask.size() || mask[start] == 0) continue;

        // A prototype before the definition must not consume the next body.
        size_t open = start;
        bool has_body = false;
        while (open < s.size()) {
            if (mask[open] != 0) {
                if (s[open] == ';') break;
                if (s[open] == '{') {
                    has_body = true;
                    break;
                }
            }
            ++open;
        }
        if (!has_body || open >= s.size()) continue;

        int depth = 0;
        size_t close = open;
        for (; close < s.size(); ++close) {
            if (mask[close] == 0) continue;
            if (s[close] == '{') {
                ++depth;
            } else if (s[close] == '}') {
                --depth;
                if (depth == 0) break;
            }
        }
        if (close >= s.size()) return false;
        s.replace(start, close - start + 1, replacement);
        return true;
    }
    return false;
}

bool is_extension_directive(const std::string& s, size_t line_start, size_t line_end,
                            const std::vector<char>& lex) {
    size_t p = line_start;
    while (p < line_end && (std::isspace(static_cast<unsigned char>(s[p])) != 0 || lex[p] == 0)) ++p;
    if (p >= line_end || s[p] != '#') return false;
    ++p;
    while (p < line_end && std::isspace(static_cast<unsigned char>(s[p])) != 0) ++p;
    return line_end - p >= 9 && s.compare(p, 9, "extension") == 0;
}

std::string remove_subgroup_extensions(const std::string& s) {
    const std::vector<char> lex = lex_mask(s);
    std::string out;
    out.reserve(s.size());
    size_t pos = 0;
    while (pos < s.size()) {
        size_t eol = s.find('\n', pos);
        if (eol == std::string::npos) eol = s.size();
        bool drop = false;
        if (is_extension_directive(s, pos, eol, lex)) {
            const std::string line = s.substr(pos, eol - pos);
            if (line.find("GL_KHR_shader_subgroup_") != std::string::npos ||
                line.find("GL_NV_shader_subgroup_partitioned") != std::string::npos ||
                line.find("GL_EXT_shader_subgroup_") != std::string::npos) {
                drop = true;
            }
        }
        if (!drop) out.append(s, pos, (eol < s.size() ? eol - pos + 1 : eol - pos));
        pos = (eol < s.size()) ? eol + 1 : s.size();
    }
    return out;
}

std::string directive_name(const std::string& s, size_t line_start, size_t line_end,
                           const std::vector<char>& lex) {
    size_t p = line_start;
    while (p < line_end && (std::isspace(static_cast<unsigned char>(s[p])) != 0 || lex[p] == 0)) ++p;
    if (p >= line_end || s[p] != '#') return {};
    ++p;
    while (p < line_end && std::isspace(static_cast<unsigned char>(s[p])) != 0) ++p;
    const size_t begin = p;
    while (p < line_end && (std::isalnum(static_cast<unsigned char>(s[p])) != 0 || s[p] == '_')) ++p;
    return s.substr(begin, p - begin);
}

// Insert after the #version/#extension header but outside preprocessor
// conditionals: an injected global declaration inside an inactive #if branch
// would not exist in the branch that the preprocessor actually selects.
size_t header_insert_position(const std::string& s) {
    const std::vector<char> lex = lex_mask(s);
    const size_t n = s.size();
    size_t pos = 0;
    while (pos < n) {
        size_t eol = s.find('\n', pos);
        if (eol == std::string::npos) eol = n;

        size_t code = pos;
        while (code < eol && std::isspace(static_cast<unsigned char>(s[code])) != 0) ++code;

        if (code >= eol || lex[code] == 0) {
            // Blank or comment-only line.
            pos = (eol < n) ? eol + 1 : n;
            continue;
        }

        if (s[code] != '#') return pos; // first real token

        const std::string name = directive_name(s, pos, eol, lex);
        if (name == "if" || name == "ifdef" || name == "ifndef") return pos;
        // #version, #extension, #define, #pragma, #include, ... stay before the
        // injected declarations.
        pos = (eol < n) ? eol + 1 : n;
    }
    return n;
}

unsigned parse_axis(const std::string& spec, char axis) {
    const std::string pattern = "local_size_";
    size_t pos = 0;
    while ((pos = spec.find(pattern, pos)) != std::string::npos) {
        const size_t name = pos + pattern.size();
        if (name < spec.size() && spec[name] == axis) {
            const size_t eq = spec.find('=', name + 1);
            if (eq != std::string::npos) {
                size_t begin = eq + 1;
                while (begin < spec.size() && std::isspace(static_cast<unsigned char>(spec[begin])) != 0) ++begin;
                size_t end = begin;
                unsigned long long value = 0;
                while (end < spec.size() && std::isdigit(static_cast<unsigned char>(spec[end])) != 0) {
                    value = value * 10ull + static_cast<unsigned>(spec[end] - '0');
                    if (value > 0xffffffffull) value = 0xffffffffull;
                    ++end;
                }
                if (end > begin) return static_cast<unsigned>(value);
            }
        }
        pos = name;
    }
    return 1u;
}

struct WorkgroupSize {
    // Size of the shared scratch arrays. For conditional layout declarations
    // this is the largest declared product so every active branch fits.
    unsigned scratch_size;
    // More than one layout declaration means a preprocessor conditional picks
    // the real size. The literal parsed value would then size the scan wrong,
    // so use gl_WorkGroupSize for the gl_NumSubgroups translation instead.
    bool conditional;
};

WorkgroupSize parse_workgroup_size(const std::string& s) {
    static const std::regex layout_re(R"(layout\s*\(([^)]*)\)\s*in\s*;)");
    const std::vector<char> mask = code_mask(s);
    unsigned max_size = 0u;
    size_t declarations = 0;
    for (std::sregex_iterator it(s.begin(), s.end(), layout_re), end; it != end; ++it) {
        const size_t pos = static_cast<size_t>(it->position());
        if (pos >= mask.size() || mask[pos] == 0) continue;
        const std::string spec = (*it)[1].str();
        if (spec.find("local_size_") == std::string::npos) continue;
        const unsigned long long px = parse_axis(spec, 'x');
        const unsigned long long py = parse_axis(spec, 'y');
        const unsigned long long pz = parse_axis(spec, 'z');
        unsigned long long product = 0xffffffffull;
        if (px <= 0xffffffffull / py) {
            const unsigned long long xy = px * py;
            if (xy <= 0xffffffffull / pz) product = xy * pz;
        }
        if (product == 0) continue;
        const unsigned size = static_cast<unsigned>(product);
        max_size = std::max(max_size, size);
        ++declarations;
    }
    if (declarations == 0) return {1024u, false};
    return {max_size, declarations > 1};
}

// Float and float-vector overloads. Integer scans would need their own shared
// storage (or bit-cast round trips for every step); the target pack only uses
// the float forms, and a leftover integer call fails at compile time.
std::string make_scan_functions() {
    return R"(
float mgSubgroupInclusiveAdd(float value) {
    uint lane = gl_LocalInvocationIndex & 31u;
    uint base = (gl_LocalInvocationIndex >> 5u) << 5u;
    mgSubgroupScratch[base + lane] = vec4(value);
    barrier();
    for (uint offset = 1u; offset < 32u; offset <<= 1u) {
        vec4 current = mgSubgroupScratch[base + lane];
        vec4 previous = (lane >= offset) ? mgSubgroupScratch[base + lane - offset] : vec4(0.0);
        barrier();
        mgSubgroupScratch[base + lane] = current + previous;
        barrier();
    }
    return mgSubgroupScratch[base + lane].x;
}

vec2 mgSubgroupInclusiveAdd(vec2 value) {
    uint lane = gl_LocalInvocationIndex & 31u;
    uint base = (gl_LocalInvocationIndex >> 5u) << 5u;
    mgSubgroupScratch[base + lane] = vec4(value, 0.0, 0.0);
    barrier();
    for (uint offset = 1u; offset < 32u; offset <<= 1u) {
        vec4 current = mgSubgroupScratch[base + lane];
        vec4 previous = (lane >= offset) ? mgSubgroupScratch[base + lane - offset] : vec4(0.0);
        barrier();
        mgSubgroupScratch[base + lane] = current + previous;
        barrier();
    }
    return mgSubgroupScratch[base + lane].xy;
}

vec3 mgSubgroupInclusiveAdd(vec3 value) {
    uint lane = gl_LocalInvocationIndex & 31u;
    uint base = (gl_LocalInvocationIndex >> 5u) << 5u;
    mgSubgroupScratch[base + lane] = vec4(value, 0.0);
    barrier();
    for (uint offset = 1u; offset < 32u; offset <<= 1u) {
        vec4 current = mgSubgroupScratch[base + lane];
        vec4 previous = (lane >= offset) ? mgSubgroupScratch[base + lane - offset] : vec4(0.0);
        barrier();
        mgSubgroupScratch[base + lane] = current + previous;
        barrier();
    }
    return mgSubgroupScratch[base + lane].xyz;
}

vec4 mgSubgroupInclusiveAdd(vec4 value) {
    uint lane = gl_LocalInvocationIndex & 31u;
    uint base = (gl_LocalInvocationIndex >> 5u) << 5u;
    mgSubgroupScratch[base + lane] = value;
    barrier();
    for (uint offset = 1u; offset < 32u; offset <<= 1u) {
        vec4 current = mgSubgroupScratch[base + lane];
        vec4 previous = (lane >= offset) ? mgSubgroupScratch[base + lane - offset] : vec4(0.0);
        barrier();
        mgSubgroupScratch[base + lane] = current + previous;
        barrier();
    }
    return mgSubgroupScratch[base + lane];
}
)";
}

// vec4 is the shape used by every quad consumer in the target pack; the
// scalar/vec2/vec3 float overloads forward to it. Integer quad types are not
// emitted and remain a loud compile error if a shader uses them.
std::string make_quad_functions() {
    return R"(
vec4 mgSubgroupQuadSwapHorizontal(vec4 value) {
    uint lane = gl_LocalInvocationIndex;
    mgSubgroupQuadScratch[lane] = value;
    barrier();
    vec4 result = mgSubgroupQuadScratch[lane ^ 1u];
    barrier();
    return result;
}

vec4 mgSubgroupQuadSwapVertical(vec4 value) {
    uint lane = gl_LocalInvocationIndex;
    mgSubgroupQuadScratch[lane] = value;
    barrier();
    vec4 result = mgSubgroupQuadScratch[lane ^ 2u];
    barrier();
    return result;
}

vec4 mgSubgroupQuadSwapDiagonal(vec4 value) {
    uint lane = gl_LocalInvocationIndex;
    mgSubgroupQuadScratch[lane] = value;
    barrier();
    vec4 result = mgSubgroupQuadScratch[lane ^ 3u];
    barrier();
    return result;
}

float mgSubgroupQuadSwapHorizontal(float value) {
    return mgSubgroupQuadSwapHorizontal(vec4(value, 0.0, 0.0, 0.0)).x;
}
vec2 mgSubgroupQuadSwapHorizontal(vec2 value) {
    return mgSubgroupQuadSwapHorizontal(vec4(value, 0.0, 0.0)).xy;
}
vec3 mgSubgroupQuadSwapHorizontal(vec3 value) {
    return mgSubgroupQuadSwapHorizontal(vec4(value, 0.0)).xyz;
}

float mgSubgroupQuadSwapVertical(float value) {
    return mgSubgroupQuadSwapVertical(vec4(value, 0.0, 0.0, 0.0)).x;
}
vec2 mgSubgroupQuadSwapVertical(vec2 value) {
    return mgSubgroupQuadSwapVertical(vec4(value, 0.0, 0.0)).xy;
}
vec3 mgSubgroupQuadSwapVertical(vec3 value) {
    return mgSubgroupQuadSwapVertical(vec4(value, 0.0)).xyz;
}

float mgSubgroupQuadSwapDiagonal(float value) {
    return mgSubgroupQuadSwapDiagonal(vec4(value, 0.0, 0.0, 0.0)).x;
}
vec2 mgSubgroupQuadSwapDiagonal(vec2 value) {
    return mgSubgroupQuadSwapDiagonal(vec4(value, 0.0, 0.0)).xy;
}
vec3 mgSubgroupQuadSwapDiagonal(vec3 value) {
    return mgSubgroupQuadSwapDiagonal(vec4(value, 0.0)).xyz;
}
)";
}

// The proven X1E port of the FidelityFX SPD quad reduce. Every invocation
// participates in the shared-memory exchange.
std::string spd_reduce_quad_replacement(unsigned workgroup_size) {
    return "shared vec4 spdQuadScratch[" + std::to_string(workgroup_size) +
           "];\n"
           "vec4 SpdReduceQuad(vec4 v){\n"
           "\tuint lane = gl_LocalInvocationIndex;\n"
           "\tspdQuadScratch[lane] = v;\n"
           "\tbarrier();\n"
           "\tvec4 v0 = spdQuadScratch[lane];\n"
           "\tvec4 v1 = spdQuadScratch[lane ^ 1u];\n"
           "\tvec4 v2 = spdQuadScratch[lane ^ 2u];\n"
           "\tvec4 v3 = spdQuadScratch[lane ^ 3u];\n"
           "\tbarrier();\n"
           "\treturn SpdReduce4(v0, v1, v2, v3);\n"
           "}\n";
}

// Mips 3, 4 and 5 were written for hardware quads: only the first 64/16/4
// invocations reached the quad swap, so barriers could not be used. All
// invocations now run the reduction; owners alone load and store.
std::string spd_mip_replacement(int mip) {
    if (mip == 3) {
        return R"(void SpdDownsampleMip_3(uint x, uint y, uvec2 workGroupID, uint localInvocationIndex, uint mip, uint slice)
{
	bool spdActive = localInvocationIndex < 64u;
	vec4 v = vec4(0.0);
	if (spdActive)
		v = SpdLoadIntermediate(x * 2 + y % 2,y * 2);
	v = SpdReduceQuad(v);
	if (spdActive && localInvocationIndex % 4 == 0){
		SpdStore(ivec2(workGroupID.xy * 4) + ivec2(x/2, y/2), v, mip, slice);
		SpdStoreIntermediate(x * 2 + y/2, y * 2, v);
	}
}
)";
    }
    if (mip == 4) {
        return R"(void SpdDownsampleMip_4(uint x, uint y, uvec2 workGroupID, uint localInvocationIndex, uint mip, uint slice)
{
	bool spdActive = localInvocationIndex < 16u;
	vec4 v = vec4(0.0);
	if (spdActive)
		v = SpdLoadIntermediate(x * 4 + y,y * 4);
	v = SpdReduceQuad(v);
	if (spdActive && localInvocationIndex % 4 == 0){
		SpdStore(ivec2(workGroupID.xy * 2) + ivec2(x/2, y/2), v, mip, slice);
		SpdStoreIntermediate(x / 2 + y, 0, v);
	}
}
)";
    }
    return R"(void SpdDownsampleMip_5(uvec2 workGroupID, uint localInvocationIndex, uint mip, uint slice)
{
	bool spdActive = localInvocationIndex < 4u;
	vec4 v = vec4(0.0);
	if (spdActive)
		v = SpdLoadIntermediate(localInvocationIndex,0);
	v = SpdReduceQuad(v);
	if (spdActive && localInvocationIndex % 4 == 0){
		SpdStore(ivec2(workGroupID.xy), v, mip, slice);
	}
}
)";
}

std::string make_unsupported(const std::string& reason) {
    std::string clean = reason;
    for (char& c : clean) {
        if (c == '\n' || c == '\r') c = ' ';
    }
    return "#error MG_SUBGROUP_UNSUPPORTED: " + clean + "\n";
}

// A real builtin/extension token, not an arbitrary identifier that merely
// contains the word ("mySubgroups"). Our injected names use an upper-case S
// (mgSubgroup...), so a lower-case "subgroup" must be followed by an
// identifier character to be a builtin.
bool is_subgroup_construct_at(const std::string& s, size_t i) {
    if (i + 11 <= s.size() && s.compare(i, 11, "gl_Subgroup") == 0) return true;
    if (i + 15 <= s.size() && s.compare(i, 15, "gl_NumSubgroups") == 0) return true;
    if (i + 8 <= s.size() && s.compare(i, 8, "subgroup") == 0) {
        const char next = (i + 8 < s.size()) ? s[i + 8] : '\0';
        return next == '_' || std::isupper(static_cast<unsigned char>(next)) != 0;
    }
    return false;
}

bool has_subgroup_construct(const std::string& s) {
    const std::vector<char> mask = lex_mask(s);
    for (size_t i = 0; i < s.size(); ++i) {
        if (mask[i] != 0 && is_subgroup_construct_at(s, i)) return true;
    }
    return false;
}

// Anything left that is a builtin subgroup construct and was not lowered.
// Directives are intentionally not rewritten (they may be macros used by the
// original compiler); only tokens that reach the parser matter here.
bool find_residual_subgroup(const std::string& s, std::string* found) {
    const std::vector<char> mask = code_mask(s);
    for (size_t i = 0; i < s.size(); ++i) {
        if (mask[i] != 0 && is_subgroup_construct_at(s, i)) {
            if (found) *found = s.substr(i, 32);
            return true;
        }
    }
    return false;
}

} // namespace

MgSubgroupLoweringResult mg_lower_subgroup_constructs(const std::string& source, GLenum shader_type) {
    MgSubgroupLoweringResult result{source, false, false, ""};

    if (shader_type != GL_COMPUTE_SHADER) return result;

    if (!has_subgroup_construct(source)) return result;

    std::string out = remove_subgroup_extensions(source);
    result.transformed = true;

    // NV partitioned groups have to be lowered before the generic paths: the
    // partition rewrite changes the control flow those paths would see.
    std::string partition_reason;
    if (!mg_lower_nv_partition_pattern(out, &partition_reason)) {
        result.source = make_unsupported(partition_reason.empty() ? "NV subgroup partition pattern" : partition_reason);
        result.unsupported = true;
        result.reason = partition_reason.empty() ? "NV subgroup partition pattern" : partition_reason;
        return result;
    }

    const WorkgroupSize workgroup = parse_workgroup_size(out);

    const bool has_inclusive_add = contains_in_code(out, "subgroupInclusiveAdd");
    const bool has_quad_swap =
        contains_in_code(out, "subgroupQuadSwapHorizontal") || contains_in_code(out, "subgroupQuadSwapVertical") ||
        contains_in_code(out, "subgroupQuadSwapDiagonal");
    const bool is_spd = contains_in_code(out, "SpdReduceQuad") && has_quad_swap;

    if (is_spd) {
        if (!replace_function(out, "vec4 SpdReduceQuad(", spd_reduce_quad_replacement(workgroup.scratch_size))) {
            result.source = make_unsupported("FidelityFX SPD SpdReduceQuad definition not found");
            result.unsupported = true;
            result.reason = "FidelityFX SPD SpdReduceQuad definition not found";
            return result;
        }
        if (!replace_function(out, "void SpdDownsampleMip_3(", spd_mip_replacement(3)) ||
            !replace_function(out, "void SpdDownsampleMip_4(", spd_mip_replacement(4)) ||
            !replace_function(out, "void SpdDownsampleMip_5(", spd_mip_replacement(5))) {
            result.source = make_unsupported("FidelityFX SPD mip definitions incomplete");
            result.unsupported = true;
            result.reason = "FidelityFX SPD mip definitions incomplete";
            return result;
        }
    }

    // Arithmetic subgroup IDs. The hardware subgroup becomes 32 consecutive
    // local invocations.
    const bool has_subgroup_id =
        contains_in_code(out, "gl_SubgroupInvocationID") || contains_in_code(out, "gl_SubgroupSize") ||
        contains_in_code(out, "gl_SubgroupID") || contains_in_code(out, "gl_NumSubgroups");

    replace_token(out, "gl_SubgroupInvocationID", "(gl_LocalInvocationIndex & 31u)");
    replace_token(out, "gl_SubgroupSize", "32u");
    replace_token(out, "gl_SubgroupID", "(gl_LocalInvocationIndex >> 5u)");
    replace_token(out, "gl_NumSubgroups", "(MG_SUBGROUP_WORKGROUP_SIZE / 32u)");

    std::string injected;
    if (has_subgroup_id) {
        // With conditional layout declarations the active size is only known
        // after preprocessing; gl_WorkGroupSize carries it exactly. A single
        // declaration uses the parsed literal.
        const std::string size_expr =
            workgroup.conditional ? "uint(gl_WorkGroupSize.x * gl_WorkGroupSize.y * gl_WorkGroupSize.z)"
                                  : std::to_string(workgroup.scratch_size) + "u";
        injected += "const uint MG_SUBGROUP_WORKGROUP_SIZE = " + size_expr + ";\n";
    }

    if (has_inclusive_add) {
        replace_token(out, "subgroupInclusiveAdd", "mgSubgroupInclusiveAdd");
        injected += "shared vec4 mgSubgroupScratch[" + std::to_string(workgroup.scratch_size) + "];\n";
        injected += make_scan_functions();
    }

    if (has_quad_swap && !is_spd) {
        replace_token(out, "subgroupQuadSwapHorizontal", "mgSubgroupQuadSwapHorizontal");
        replace_token(out, "subgroupQuadSwapVertical", "mgSubgroupQuadSwapVertical");
        replace_token(out, "subgroupQuadSwapDiagonal", "mgSubgroupQuadSwapDiagonal");
        injected += "shared vec4 mgSubgroupQuadScratch[" + std::to_string(workgroup.scratch_size) + "];\n";
        injected += make_quad_functions();
    }

    if (!injected.empty()) {
        const size_t pos = header_insert_position(out);
        if (pos > 0 && pos <= out.size() && out[pos - 1] != '\n') injected = "\n" + injected;
        out.insert(pos, injected);
    }

    std::string residual;
    if (find_residual_subgroup(out, &residual)) {
        // Keep the source position in the message so a driver-side compile
        // failure points at the construct that was not covered.
        std::string reason = "unsupported subgroup construct: " + residual;
        result.source = make_unsupported(reason);
        result.unsupported = true;
        result.reason = reason;
        return result;
    }

    result.source = std::move(out);
    return result;
}
