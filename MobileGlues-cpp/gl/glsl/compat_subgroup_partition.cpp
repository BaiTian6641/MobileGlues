#include "compat_subgroup_partition.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr size_t kNpos = std::string::npos;

bool ident_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

bool is_space(char c) {
    return std::isspace(static_cast<unsigned char>(c)) != 0;
}

size_t find_token(const std::string& s, const char* token, size_t from = 0, size_t limit = kNpos) {
    const size_t len = std::strlen(token);
    if (limit > s.size()) limit = s.size();
    size_t pos = from;
    while (pos + len <= limit) {
        const size_t hit = s.find(token, pos);
        if (hit == kNpos || hit + len > limit) return kNpos;
        const bool left_ok = hit == 0 || !ident_char(s[hit - 1]);
        const bool right_ok = hit + len >= s.size() || !ident_char(s[hit + len]);
        if (left_ok && right_ok) return hit;
        pos = hit + 1;
    }
    return kNpos;
}

std::string trim_copy(const std::string& s) {
    size_t begin = 0;
    size_t end = s.size();
    while (begin < end && is_space(s[begin])) ++begin;
    while (end > begin && is_space(s[end - 1])) --end;
    return s.substr(begin, end - begin);
}

size_t count_tokens(const std::string& s, const char* token, size_t from, size_t to) {
    size_t count = 0;
    size_t pos = from;
    while (pos < to) {
        pos = find_token(s, token, pos, to);
        if (pos == kNpos) break;
        ++count;
        pos += std::strlen(token);
    }
    return count;
}

// Skip a line comment, block comment or string literal starting at `i`.
bool skip_trivia(const std::string& s, size_t& i) {
    if (i >= s.size()) return false;
    if (s[i] == '/' && i + 1 < s.size() && s[i + 1] == '/') {
        const size_t eol = s.find('\n', i);
        i = eol == kNpos ? s.size() : eol + 1;
        return true;
    }
    if (s[i] == '/' && i + 1 < s.size() && s[i + 1] == '*') {
        const size_t close = s.find("*/", i + 2);
        i = close == kNpos ? s.size() : close + 2;
        return true;
    }
    if (s[i] == '"') {
        size_t j = i + 1;
        while (j < s.size() && s[j] != '"') {
            if (s[j] == '\\' && j + 1 < s.size()) ++j;
            ++j;
        }
        i = j < s.size() ? j + 1 : s.size();
        return true;
    }
    return false;
}

size_t matching_brace(const std::string& s, size_t open) {
    int depth = 0;
    size_t i = open;
    while (i < s.size()) {
        if (skip_trivia(s, i)) continue;
        if (s[i] == '{') {
            ++depth;
        } else if (s[i] == '}') {
            --depth;
            if (depth == 0) return i;
            if (depth < 0) return kNpos;
        }
        ++i;
    }
    return kNpos;
}

size_t matching_paren(const std::string& s, size_t open, size_t limit) {
    int depth = 0;
    for (size_t i = open; i < limit && i < s.size(); ++i) {
        const char c = s[i];
        if (c == '(') {
            ++depth;
        } else if (c == ')') {
            --depth;
            if (depth == 0) return i;
        }
    }
    return kNpos;
}

std::vector<std::string> split_args(const std::string& s, size_t begin, size_t end) {
    std::vector<std::string> args;
    int depth = 0;
    size_t start = begin;
    for (size_t i = begin; i < end; ++i) {
        const char c = s[i];
        if (c == '(' || c == '[' || c == '{') {
            ++depth;
        } else if (c == ')' || c == ']' || c == '}') {
            --depth;
        } else if (c == ',' && depth == 0) {
            args.push_back(trim_copy(s.substr(start, i - start)));
            start = i + 1;
        }
    }
    args.push_back(trim_copy(s.substr(start, end - start)));
    return args;
}

bool find_call_in_range(const std::string& s, size_t from, size_t to, const char* name,
                        std::vector<std::string>* args) {
    const size_t len = std::strlen(name);
    const size_t pos = find_token(s, name, from, to);
    if (pos == kNpos) return false;
    size_t open = pos + len;
    while (open < to && is_space(s[open])) ++open;
    if (open >= to || s[open] != '(') return false;
    const size_t close = matching_paren(s, open, to);
    if (close == kNpos) return false;
    if (args) *args = split_args(s, open + 1, close);
    return true;
}

std::string directive_name(const std::string& line) {
    size_t i = 0;
    while (i < line.size() && is_space(line[i])) ++i;
    if (i >= line.size() || line[i] != '#') return {};
    ++i;
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
    const size_t begin = i;
    while (i < line.size() && ident_char(line[i])) ++i;
    return line.substr(begin, i - begin);
}

std::string directive_rest(const std::string& line) {
    size_t i = 0;
    while (i < line.size() && is_space(line[i])) ++i;
    if (i >= line.size() || line[i] != '#') return {};
    ++i;
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
    while (i < line.size() && ident_char(line[i])) ++i;
    return trim_copy(line.substr(i));
}

bool is_vendor_condition(const std::string& line) {
    const std::string name = directive_name(line);
    if (name == "ifdef") {
        const std::string rest = directive_rest(line);
        size_t end = 0;
        while (end < rest.size() && ident_char(rest[end])) ++end;
        return rest.substr(0, end) == "MC_GL_VENDOR_NVIDIA";
    }
    if (name == "if") {
        const std::string rest = directive_rest(line);
        return rest.find("MC_GL_VENDOR_NVIDIA") != std::string::npos &&
               rest.find("defined") != std::string::npos && rest.find('!') == std::string::npos;
    }
    return false;
}

struct Guard {
    size_t start;
    size_t end;
    size_t else_pos;
    bool has_else;
};

std::vector<Guard> collect_vendor_guards(const std::string& s) {
    struct Cond {
        bool vendor;
        size_t start;
        size_t else_pos;
        bool has_else;
    };
    std::vector<Cond> stack;
    std::vector<Guard> guards;

    size_t pos = 0;
    while (pos < s.size()) {
        const size_t eol = s.find('\n', pos);
        const size_t line_end = eol == kNpos ? s.size() : eol;
        const std::string line = s.substr(pos, line_end - pos);
        const std::string name = directive_name(line);

        if (name == "ifdef" || name == "ifndef" || name == "if") {
            stack.push_back({is_vendor_condition(line), pos, kNpos, false});
        } else if (name == "else" || name == "elif") {
            if (!stack.empty() && name == "else") {
                stack.back().else_pos = pos;
                stack.back().has_else = true;
            }
        } else if (name == "endif") {
            if (!stack.empty()) {
                const Cond cond = stack.back();
                stack.pop_back();
                if (cond.vendor) {
                    const size_t end = eol == kNpos ? s.size() : eol + 1;
                    guards.push_back({cond.start, end, cond.has_else ? cond.else_pos : end,
                                      cond.has_else});
                }
            }
        }

        if (eol == kNpos) break;
        pos = eol + 1;
    }
    return guards;
}

struct ParsedPattern {
    size_t start;
    size_t end;
    std::string key;
    std::string coord;
    std::string value;
    std::string image;
};

bool parse_candidate(const std::string& s, const Guard& guard, ParsedPattern& out,
                     std::string& why) {
    if (!guard.has_else) {
        why = "MC_GL_VENDOR_NVIDIA partition block has no #else XOR fallback";
        return false;
    }
    const size_t nv_end = guard.else_pos;
    if (count_tokens(s, "subgroupPartitionNV", guard.start, nv_end) != 1 ||
        count_tokens(s, "subgroupPartitionedMaxNV", guard.start, nv_end) != 1 ||
        count_tokens(s, "imageAtomicMax", guard.start, nv_end) != 1) {
        why = "NV partition block contains an unexpected number of partition/image operations";
        return false;
    }

    std::vector<std::string> args;
    if (!find_call_in_range(s, guard.start, nv_end, "subgroupPartitionNV", &args) ||
        args.size() != 1) {
        why = "could not parse subgroupPartitionNV(...)";
        return false;
    }
    const std::string key = trim_copy(args[0]);
    if (!find_call_in_range(s, guard.start, nv_end, "subgroupPartitionedMaxNV", &args) ||
        args.size() != 2) {
        why = "could not parse subgroupPartitionedMaxNV(...)";
        return false;
    }
    const std::string importance = trim_copy(args[0]);
    if (!find_call_in_range(s, guard.start, nv_end, "imageAtomicMax", &args) || args.size() != 3) {
        why = "could not parse partition imageAtomicMax(...)";
        return false;
    }
    const std::string image = trim_copy(args[0]);
    const std::string coord = trim_copy(args[1]);
    const std::string value = trim_copy(args[2]);
    if (key.empty() || importance.empty() || image.empty() || coord.empty() || value.empty()) {
        why = "NV partition block has an empty key, value, image or coordinate expression";
        return false;
    }
    if (value.find("floatBitsToUint") == std::string::npos ||
        value.find(importance) == std::string::npos) {
        why = "partition imageAtomicMax value is not floatBitsToUint(importance)";
        return false;
    }

    if (count_tokens(s, "subgroupShuffleXor", guard.else_pos, guard.end) != 1) {
        why = "XOR fallback does not contain exactly one subgroupShuffleXor(...)";
        return false;
    }
    if (!find_call_in_range(s, guard.else_pos, guard.end, "subgroupShuffleXor", &args) ||
        args.size() != 2) {
        why = "could not parse subgroupShuffleXor(...) fallback";
        return false;
    }
    if (trim_copy(args[0]) != key) {
        why = "NV partition key and XOR-shuffle key differ";
        return false;
    }
    if (find_token(s, "gl_SubgroupSize", guard.else_pos, guard.end) == kNpos ||
        find_token(s, "gl_SubgroupInvocationID", guard.else_pos, guard.end) == kNpos) {
        why = "XOR fallback does not use gl_SubgroupSize/gl_SubgroupInvocationID";
        return false;
    }

    out.start = guard.start;
    out.end = guard.end;
    out.key = key;
    out.coord = coord;
    out.value = value;
    out.image = image;
    return true;
}

size_t skip_space_and_comments(const std::string& s, size_t i, size_t limit) {
    if (limit > s.size()) limit = s.size();
    for (;;) {
        while (i < limit && is_space(s[i])) ++i;
        size_t next = i;
        if (!skip_trivia(s, next) || next == i) return i;
        i = next;
    }
}

// Whitespace- and comment-free view of a source fragment, for exact structural
// comparisons that do not care about formatting.
std::string compact_code(const std::string& s) {
    std::string out;
    size_t i = 0;
    while (i < s.size()) {
        if (skip_trivia(s, i)) continue;
        if (!is_space(s[i])) out.push_back(s[i]);
        ++i;
    }
    return out;
}

bool is_identifier(const std::string& s) {
    if (s.empty() || (s[0] >= '0' && s[0] <= '9')) return false;
    for (char c : s) {
        if (!ident_char(c)) return false;
    }
    return true;
}

std::vector<std::string> split_top_level(const std::string& s, char separator) {
    std::vector<std::string> parts;
    int depth = 0;
    size_t start = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        const char c = s[i];
        if (c == '(' || c == '[' || c == '{') {
            ++depth;
        } else if (c == ')' || c == ']' || c == '}') {
            --depth;
        } else if (c == separator && depth == 0) {
            parts.push_back(s.substr(start, i - start));
            start = i + 1;
        }
    }
    parts.push_back(s.substr(start));
    return parts;
}

size_t find_top_level(const std::string& s, const char* token, size_t begin, size_t end) {
    const size_t len = std::strlen(token);
    if (end > s.size()) end = s.size();
    int depth = 0;
    for (size_t i = begin; i + len <= end; ++i) {
        const char c = s[i];
        if (c == '(' || c == '[' || c == '{') {
            ++depth;
        } else if (c == ')' || c == ']' || c == '}') {
            --depth;
        } else if (depth == 0 && s.compare(i, len, token) == 0) {
            return i;
        }
    }
    return kNpos;
}

struct ForLoopInfo {
    size_t for_pos = kNpos;
    size_t header_open = kNpos;
    size_t header_close = kNpos;
    size_t body_open = kNpos;
    size_t body_close = kNpos;
};

// Locates the innermost `for` loop starting before `pos` whose braced body
// contains `pos`. Non-candidate loops simply report false.
bool find_enclosing_for(const std::string& s, size_t pos, ForLoopInfo& out) {
    bool found = false;
    size_t search = 0;
    while (true) {
        const size_t f = find_token(s, "for", search, pos);
        if (f == kNpos) break;
        size_t open = f + 3;
        while (open < s.size() && is_space(s[open])) ++open;
        if (open < s.size() && s[open] == '(') {
            const size_t close = matching_paren(s, open, s.size());
            if (close != kNpos) {
                size_t brace = close + 1;
                while (brace < s.size() && is_space(s[brace])) ++brace;
                if (brace < s.size() && s[brace] == '{') {
                    const size_t body_close = matching_brace(s, brace);
                    if (body_close != kNpos && brace < pos && pos < body_close) {
                        out.for_pos = f;
                        out.header_open = open;
                        out.header_close = close;
                        out.body_open = brace;
                        out.body_close = body_close;
                        found = true;
                    }
                }
            }
        }
        search = f + 3;
    }
    return found;
}

bool parse_call_at(const std::string& s, size_t pos, size_t limit, const char* name,
                   std::vector<std::string>& args, size_t& close_pos) {
    const size_t len = std::strlen(name);
    if (find_token(s, name, pos, limit) != pos) return false;
    size_t open = pos + len;
    while (open < limit && is_space(s[open])) ++open;
    if (open >= limit || s[open] != '(') return false;
    const size_t close = matching_paren(s, open, limit);
    if (close == kNpos) return false;
    args = split_args(s, open + 1, close);
    close_pos = close;
    return true;
}

// Recognizes the portable (KHR-only) duplicate-suppression loop emitted when
// the NVIDIA branch has been preprocessed away:
//
//   for (uint u = 1u; u < gl_SubgroupSize; u = u << 1u) {
//       if (key != subgroupShuffleXor(key, u)) {
//           imageAtomicMax(image, coord, value);
//           return;
//       } else {
//           if ((gl_SubgroupInvocationID & u) == u) return;
//       }
//   }
//   imageAtomicMax(image, coord, value);
//
// The parsed span covers only the loop and its trailing write; the flush block
// is emitted after the surrounding top-level conditional by the caller.
bool parse_khr_candidate(const std::string& s, const ForLoopInfo& loop, ParsedPattern& out,
                         std::string& why) {
    const std::vector<std::string> header = split_top_level(
        s.substr(loop.header_open + 1, loop.header_close - loop.header_open - 1), ';');
    if (header.size() != 3) {
        why = "KHR XOR loop header is not a for(init; condition; increment) triple";
        return false;
    }
    const std::string init = compact_code(header[0]);
    const std::string condition = compact_code(header[1]);
    const std::string increment = compact_code(header[2]);
    const size_t assign = init.find('=');
    if (assign == kNpos || init.find('=', assign + 1) != kNpos ||
        init.compare(0, 4, "uint") != 0) {
        why = "KHR XOR loop header is not 'uint <var> = 1u'";
        return false;
    }
    const std::string var = init.substr(4, assign - 4);
    if (!is_identifier(var)) {
        why = "KHR XOR loop variable is not a simple identifier";
        return false;
    }
    const std::string one = init.substr(assign + 1);
    if (one != "1u" && one != "1") {
        why = "KHR XOR loop does not start at one";
        return false;
    }
    if (condition != var + "<gl_SubgroupSize") {
        why = "KHR XOR loop condition is not 'var < gl_SubgroupSize'";
        return false;
    }
    if (increment != var + "=" + var + "<<1u" && increment != var + "=" + var + "<<1" &&
        increment != var + "<<=1u" && increment != var + "<<=1") {
        why = "KHR XOR loop increment is not a left shift by one";
        return false;
    }

    size_t p = skip_space_and_comments(s, loop.body_open + 1, loop.body_close);
    if (find_token(s, "if", p, loop.body_close) != p) {
        why = "KHR XOR loop body does not begin with the duplicate-suppression if";
        return false;
    }
    size_t cond_open = skip_space_and_comments(s, p + 2, loop.body_close);
    if (cond_open >= loop.body_close || s[cond_open] != '(') {
        why = "could not parse the KHR XOR loop's if condition";
        return false;
    }
    const size_t cond_close = matching_paren(s, cond_open, loop.body_close);
    if (cond_close == kNpos) {
        why = "could not parse the KHR XOR loop's if condition";
        return false;
    }
    const std::string compare = s.substr(cond_open + 1, cond_close - cond_open - 1);
    const size_t neq = find_top_level(compare, "!=", 0, compare.size());
    if (neq == kNpos || find_top_level(compare, "!=", neq + 2, compare.size()) != kNpos) {
        why = "KHR XOR loop condition is not a single key != subgroupShuffleXor(key, var)";
        return false;
    }
    const std::string key = trim_copy(compare.substr(0, neq));
    if (key.empty()) {
        why = "KHR XOR loop has an empty key expression";
        return false;
    }
    const std::string shuffle_expected =
        "subgroupShuffleXor(" + compact_code(key) + "," + var + ")";
    if (compact_code(compare.substr(neq + 2)) != shuffle_expected) {
        why = "KHR XOR loop does not compare the key against subgroupShuffleXor(key, var)";
        return false;
    }

    size_t then_open = skip_space_and_comments(s, cond_close + 1, loop.body_close);
    if (then_open >= loop.body_close || s[then_open] != '{') {
        why = "KHR XOR loop's divergent if body is not braced";
        return false;
    }
    const size_t then_close = matching_brace(s, then_open);
    if (then_close == kNpos || then_close > loop.body_close) {
        why = "could not parse the KHR XOR loop's divergent if body";
        return false;
    }

    p = skip_space_and_comments(s, then_open + 1, then_close);
    std::vector<std::string> args;
    size_t call_close = kNpos;
    if (!parse_call_at(s, p, then_close, "imageAtomicMax", args, call_close) || args.size() != 3) {
        why = "KHR XOR loop branch does not begin with imageAtomicMax(image, coord, value)";
        return false;
    }
    const std::string image = trim_copy(args[0]);
    const std::string coord = trim_copy(args[1]);
    const std::string value = trim_copy(args[2]);
    if (image.empty() || coord.empty() || value.empty() ||
        value.find("floatBitsToUint") == std::string::npos) {
        why = "KHR XOR loop branch has an invalid image, coordinate or importance value";
        return false;
    }
    p = skip_space_and_comments(s, call_close + 1, then_close);
    if (p >= then_close || s[p] != ';') {
        why = "KHR XOR loop branch imageAtomicMax is not a complete statement";
        return false;
    }
    p = skip_space_and_comments(s, p + 1, then_close);
    if (find_token(s, "return", p, then_close) != p) {
        why = "KHR XOR loop branch does not return after imageAtomicMax";
        return false;
    }
    p = skip_space_and_comments(s, p + 6, then_close);
    if (p >= then_close || s[p] != ';') {
        why = "KHR XOR loop branch return is not a complete statement";
        return false;
    }
    p = skip_space_and_comments(s, p + 1, then_close);
    if (p != then_close) {
        why = "KHR XOR loop branch contains statements besides imageAtomicMax and return";
        return false;
    }

    size_t else_pos = skip_space_and_comments(s, then_close + 1, loop.body_close);
    if (find_token(s, "else", else_pos, loop.body_close) != else_pos) {
        why = "KHR XOR loop's duplicate-suppression if has no else branch";
        return false;
    }
    size_t else_open = skip_space_and_comments(s, else_pos + 4, loop.body_close);
    if (else_open >= loop.body_close || s[else_open] != '{') {
        why = "KHR XOR loop's else body is not braced";
        return false;
    }
    const size_t else_close = matching_brace(s, else_open);
    if (else_close == kNpos || else_close > loop.body_close) {
        why = "could not parse the KHR XOR loop's else body";
        return false;
    }
    const std::string expected_else =
        "if((gl_SubgroupInvocationID&" + var + ")==" + var + ")return;";
    if (compact_code(s.substr(else_open + 1, else_close - else_open - 1)) != expected_else) {
        why = "KHR XOR loop's else body is not the lowest-invocation leader test";
        return false;
    }
    if (skip_space_and_comments(s, else_close + 1, loop.body_close) != loop.body_close) {
        why = "KHR XOR loop body contains statements after the duplicate-suppression if";
        return false;
    }

    size_t trailing = skip_space_and_comments(s, loop.body_close + 1, s.size());
    args.clear();
    call_close = kNpos;
    if (!parse_call_at(s, trailing, s.size(), "imageAtomicMax", args, call_close) ||
        args.size() != 3) {
        why = "KHR XOR loop is not followed by the leader imageAtomicMax write";
        return false;
    }
    if (compact_code(trim_copy(args[0])) != compact_code(image) ||
        compact_code(trim_copy(args[1])) != compact_code(coord) ||
        compact_code(trim_copy(args[2])) != compact_code(value)) {
        why = "KHR XOR loop's trailing imageAtomicMax differs from the branch write";
        return false;
    }
    const size_t semicolon = skip_space_and_comments(s, call_close + 1, s.size());
    if (semicolon >= s.size() || s[semicolon] != ';') {
        why = "KHR XOR loop's trailing imageAtomicMax is not a complete statement";
        return false;
    }

    out.start = loop.for_pos;
    out.end = semicolon + 1;
    out.key = key;
    out.coord = coord;
    out.value = value;
    out.image = image;
    return true;
}

// Returns the position of the `if` keyword whose braced body starts at `open`,
// or npos when the brace does not open an if body.
size_t owning_if_position(const std::string& s, size_t open) {
    size_t i = open;
    while (i > 0 && is_space(s[i - 1])) --i;
    if (i == 0 || s[i - 1] != ')') return kNpos;

    int depth = 0;
    size_t j = i - 1;
    for (;;) {
        if (s[j] == ')') {
            ++depth;
        } else if (s[j] == '(') {
            --depth;
            if (depth == 0) break;
        }
        if (j == 0) return kNpos;
        --j;
    }
    size_t k = j;
    while (k > 0 && is_space(s[k - 1])) --k;
    const size_t end = k;
    while (k > 0 && ident_char(s[k - 1])) --k;
    return s.compare(k, end - k, "if") == 0 ? k : kNpos;
}

// True when the if-owning `block_open` is itself a top-level statement, not the
// unbraced body of an enclosing control statement (where a barrier could run a
// non-uniform number of times).
bool if_is_top_level_statement(const std::string& s, size_t if_pos) {
    size_t i = if_pos;
    while (i > 0 && is_space(s[i - 1])) --i;
    if (i == 0) return false;
    if (s[i - 1] == ')' || s[i - 1] == ']') return false;
    size_t token_end = i;
    while (i > 0 && ident_char(s[i - 1])) --i;
    return s.compare(i, token_end - i, "else") != 0;
}

bool else_follows(const std::string& s, size_t close) {
    size_t i = close + 1;
    while (i < s.size() && is_space(s[i])) ++i;
    return find_token(s, "else", i) == i;
}

bool find_main_body(const std::string& s, size_t& main_pos, size_t& body_open, size_t& body_close) {
    size_t search = 0;
    while ((search = find_token(s, "void", search)) != kNpos) {
        const size_t name = find_token(s, "main", search + 4);
        if (name == kNpos) return false;
        if (!trim_copy(s.substr(search + 4, name - (search + 4))).empty()) {
            search += 4;
            continue;
        }
        size_t open = name + 4;
        while (open < s.size() && is_space(s[open])) ++open;
        if (open >= s.size() || s[open] != '(') {
            search += 4;
            continue;
        }
        const size_t close_paren = matching_paren(s, open, s.size());
        if (close_paren == kNpos) return false;
        size_t brace = close_paren + 1;
        while (brace < s.size() && is_space(s[brace])) ++brace;
        if (brace >= s.size() || s[brace] != '{') {
            search += 4;
            continue;
        }
        const size_t close = matching_brace(s, brace);
        if (close == kNpos) return false;
        main_pos = search;
        body_open = brace;
        body_close = close;
        return true;
    }
    return false;
}

bool find_enclosing_top_block(const std::string& s, size_t body_open, size_t body_close, size_t pos,
                              size_t& block_open, size_t& block_close) {
    size_t i = body_open + 1;
    while (i < body_close) {
        if (skip_trivia(s, i)) continue;
        if (s[i] == '{') {
            const size_t close = matching_brace(s, i);
            if (close == kNpos || close > body_close) return false;
            if (i < pos && pos < close) {
                block_open = i;
                block_close = close;
                return true;
            }
            i = close + 1;
            continue;
        }
        ++i;
    }
    return false;
}

bool has_return_outside(const std::string& s, size_t from, size_t to,
                        const std::vector<std::pair<size_t, size_t>>& excluded) {
    size_t pos = from;
    while (pos < to) {
        pos = find_token(s, "return", pos, to);
        if (pos == kNpos) return false;
        bool inside = false;
        for (const auto& range : excluded) {
            if (pos >= range.first && pos < range.second) {
                inside = true;
                break;
            }
        }
        if (!inside) return true;
        pos += 6;
    }
    return false;
}

bool workgroup_size_supported(const std::string& s, std::string& why) {
    if (find_token(s, "local_size_x") == kNpos) {
        why = "partition pattern is not in a compute shader (no local_size_x declaration)";
        return false;
    }
    auto read_factor = [&s](const char* name) -> long long {
        const size_t pos = find_token(s, name);
        if (pos == kNpos) return 1;
        size_t i = pos + std::strlen(name);
        while (i < s.size() && is_space(s[i])) ++i;
        if (i >= s.size() || s[i] != '=') return 1;
        ++i;
        while (i < s.size() && is_space(s[i])) ++i;
        long long value = 0;
        bool any = false;
        while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
            value = value * 10 + (s[i] - '0');
            any = true;
            ++i;
        }
        return any ? value : 1;
    };
    const long long x = read_factor("local_size_x");
    const long long y = read_factor("local_size_y");
    const long long z = read_factor("local_size_z");
    if (x <= 0 || y <= 0 || z <= 0) {
        why = "invalid compute workgroup size";
        return false;
    }
    const long long total = x * y * z;
    if (total > 128) {
        why = "compute workgroup has " + std::to_string(total) +
              " invocations; the 256-slot partition table is proven for 128";
        return false;
    }
    return true;
}

struct Edit {
    size_t pos;
    size_t len;
    std::string text;
};

const char* const kHelpers =
    "\n// MobileGlues: workgroup-shared partition table replacing NV subgroup\n"
    "// partition/XOR duplicate suppression. One slot per distinct key; the\n"
    "// lowest local invocation (leader) writes the partition maximum.\n"
    "#define MG_SUBGROUP_PARTITION_WORKGROUP_SIZE 128u\n"
    "#define MG_SUBGROUP_PARTITION_TABLE_SIZE 256u\n"
    "shared uint mgSubgroupPartitionKeyTable[MG_SUBGROUP_PARTITION_TABLE_SIZE];\n"
    "shared uint mgSubgroupPartitionValueTable[MG_SUBGROUP_PARTITION_TABLE_SIZE];\n"
    "shared uint mgSubgroupPartitionLeaderTable[MG_SUBGROUP_PARTITION_TABLE_SIZE];\n"
    "\n"
    "uint mgSubgroupPartitionSlotFor(uint key) {\n"
    "    return ((key * 2654435761u) >> 24u) & (MG_SUBGROUP_PARTITION_TABLE_SIZE - 1u);\n"
    "}\n"
    "\n"
    "void mgSubgroupPartitionClear(uint localIndex) {\n"
    "    for (uint i = localIndex; i < MG_SUBGROUP_PARTITION_TABLE_SIZE; "
    "i += MG_SUBGROUP_PARTITION_WORKGROUP_SIZE) {\n"
    "        mgSubgroupPartitionKeyTable[i] = 0xFFFFFFFFu;\n"
    "        mgSubgroupPartitionValueTable[i] = 0u;\n"
    "        mgSubgroupPartitionLeaderTable[i] = 0xFFFFFFFFu;\n"
    "    }\n"
    "}\n"
    "\n"
    "uint mgSubgroupPartitionInsert(uint key, uint value, uint localIndex) {\n"
    "    uint slot = mgSubgroupPartitionSlotFor(key);\n"
    "    for (uint probe = 0u; probe < MG_SUBGROUP_PARTITION_TABLE_SIZE; probe++) {\n"
    "        uint previous = atomicCompSwap(mgSubgroupPartitionKeyTable[slot], 0xFFFFFFFFu, key);\n"
    "        if (previous == 0xFFFFFFFFu || previous == key) break;\n"
    "        slot = (slot + 1u) & (MG_SUBGROUP_PARTITION_TABLE_SIZE - 1u);\n"
    "    }\n"
    "    atomicMax(mgSubgroupPartitionValueTable[slot], value);\n"
    "    atomicMin(mgSubgroupPartitionLeaderTable[slot], localIndex);\n"
    "    return slot;\n"
    "}\n\n";

const char* const kLocals =
    "\n    bool mgSubgroupPartitionActive = false;\n"
    "    uint mgSubgroupPartitionKey = 0u;\n"
    "    uint mgSubgroupPartitionValue = 0u;\n"
    "    ivec2 mgSubgroupPartitionCoord = ivec2(0);\n";

std::string replacement_for(const ParsedPattern& pattern) {
    return "\n        mgSubgroupPartitionActive = true;\n"
           "        mgSubgroupPartitionKey = (" + pattern.key + ");\n"
           "        mgSubgroupPartitionCoord = (" + pattern.coord + ");\n"
           "        mgSubgroupPartitionValue = (" + pattern.value + ");\n";
}

std::string flush_for(const ParsedPattern& pattern) {
    return "\n    {\n"
           "        mgSubgroupPartitionClear(gl_LocalInvocationIndex);\n"
           "        barrier();\n"
           "        uint mgSubgroupPartitionSlot = 0u;\n"
           "        if (mgSubgroupPartitionActive)\n"
           "            mgSubgroupPartitionSlot = mgSubgroupPartitionInsert(mgSubgroupPartitionKey, "
           "mgSubgroupPartitionValue, gl_LocalInvocationIndex);\n"
           "        barrier();\n"
           "        if (mgSubgroupPartitionActive && "
           "mgSubgroupPartitionLeaderTable[mgSubgroupPartitionSlot] == gl_LocalInvocationIndex)\n"
           "            imageAtomicMax(" + pattern.image +
           ", mgSubgroupPartitionCoord, "
           "mgSubgroupPartitionValueTable[mgSubgroupPartitionSlot]);\n"
           "        barrier();\n"
           "        mgSubgroupPartitionActive = false;\n"
           "        mgSubgroupPartitionKey = 0u;\n"
           "        mgSubgroupPartitionValue = 0u;\n"
           "    }\n";
}

bool apply_edits(std::string& source, std::vector<Edit>& edits) {
    std::sort(edits.begin(), edits.end(), [](const Edit& a, const Edit& b) {
        return a.pos > b.pos;
    });
    size_t previous = kNpos;
    for (const Edit& edit : edits) {
        if (edit.pos > source.size() || edit.len > source.size() - edit.pos) return false;
        if (edit.pos + edit.len > previous) return false;
        source.replace(edit.pos, edit.len, edit.text);
        previous = edit.pos;
    }
    return true;
}

}  // namespace

bool mg_lower_nv_partition_pattern(std::string& source, std::string* reason) {
    auto fail = [reason](const std::string& why) {
        if (reason) *reason = why;
        return false;
    };
    auto succeed = [reason]() {
        if (reason) reason->clear();
        return true;
    };

    const std::vector<Guard> guards = collect_vendor_guards(source);
    std::vector<ParsedPattern> patterns;
    std::vector<std::pair<size_t, size_t>> claimed_spans;
    for (const Guard& guard : guards) {
        if (find_token(source, "subgroupPartitionNV", guard.start, guard.end) == kNpos) continue;
        ParsedPattern parsed{};
        std::string why;
        if (!parse_candidate(source, guard, parsed, why)) return fail(why);
        claimed_spans.emplace_back(parsed.start, parsed.end);
        patterns.push_back(parsed);
    }

    // When the NVIDIA branch is absent (Iris preprocesses it away on Adreno),
    // the same duplicate suppression arrives as the portable XOR loop. Recognize
    // and lower it with the same partition table. XOR uses that are not this
    // loop are left alone; loops carrying the pattern's image write but with an
    // unsafe shape are refused instead of being half-rewritten.
    size_t shuffle_pos = 0;
    while ((shuffle_pos = find_token(source, "subgroupShuffleXor", shuffle_pos)) != kNpos) {
        bool claimed = false;
        for (const auto& span : claimed_spans) {
            if (shuffle_pos >= span.first && shuffle_pos < span.second) {
                claimed = true;
                break;
            }
        }
        if (claimed) {
            shuffle_pos += std::strlen("subgroupShuffleXor");
            continue;
        }
        ForLoopInfo loop{};
        if (!find_enclosing_for(source, shuffle_pos, loop)) {
            shuffle_pos += std::strlen("subgroupShuffleXor");
            continue;
        }
        if (find_token(source, "imageAtomicMax", loop.body_open, loop.body_close) == kNpos) {
            shuffle_pos = loop.body_close + 1;
            continue;
        }
        ParsedPattern parsed{};
        std::string why;
        if (!parse_khr_candidate(source, loop, parsed, why)) return fail(why);
        claimed_spans.emplace_back(parsed.start, parsed.end);
        patterns.push_back(parsed);
        shuffle_pos = parsed.end;
    }

    if (patterns.empty()) {
        // No NV partition duplicate-suppression pattern: leave the source alone.
        // A lone partition builtin means the guard form was not recognized, so
        // report it instead of silently leaving an untranslatable construct.
        if (find_token(source, "subgroupPartitionNV") != kNpos ||
            find_token(source, "subgroupPartitionedMaxNV") != kNpos) {
            return fail("subgroupPartitionNV/subgroupPartitionedMaxNV found outside a "
                        "rewritable MC_GL_VENDOR_NVIDIA/#else block");
        }
        return succeed();
    }

    if (source.find("MG_SUBGROUP_PARTITION_TABLE_SIZE") != std::string::npos) {
        return fail("source already contains the MobileGlues partition table lowering");
    }

    // Every NV partition builtin must belong to a rewritten block; otherwise the
    // result would be a half-rewrite that no longer compiles.
    for (const char* token : {"subgroupPartitionNV", "subgroupPartitionedMaxNV"}) {
        size_t pos = 0;
        while ((pos = find_token(source, token, pos)) != kNpos) {
            bool covered = false;
            for (const ParsedPattern& pattern : patterns) {
                if (pos >= pattern.start && pos < pattern.end) {
                    covered = true;
                    break;
                }
            }
            if (!covered) {
                return fail(std::string(token) + " at offset " + std::to_string(pos) +
                            " is outside a rewritable MC_GL_VENDOR_NVIDIA block");
            }
            pos += std::strlen(token);
        }
    }

    std::string why;
    if (!workgroup_size_supported(source, why)) return fail(why);

    size_t main_pos = 0;
    size_t body_open = 0;
    size_t body_close = 0;
    if (!find_main_body(source, main_pos, body_open, body_close)) {
        return fail("NV partition pattern found but no compute main() body was located");
    }

    std::vector<Edit> edits;
    std::vector<std::pair<size_t, size_t>> phase_blocks;

    for (const ParsedPattern& pattern : patterns) {
        if (pattern.start < body_open || pattern.end > body_close) {
            return fail("NV partition block is outside main()");
        }
        size_t block_open = 0;
        size_t block_close = 0;
        if (!find_enclosing_top_block(source, body_open, body_close, pattern.start, block_open,
                                      block_close)) {
            return fail("NV partition block is not guarded by a conditional block in main(); "
                        "a barrier cannot be placed without divergent control flow");
        }
        const size_t if_pos = owning_if_position(source, block_open);
        if (if_pos == kNpos) {
            return fail("NV partition block's enclosing top-level construct is not an if statement");
        }
        if (!if_is_top_level_statement(source, if_pos)) {
            return fail("NV partition block's if statement is the unbraced body of an enclosing "
                        "control statement; barrier control flow would not be uniform");
        }
        if (else_follows(source, block_close)) {
            return fail("NV partition block's enclosing if has an else branch; the barrier cannot "
                        "be placed after only one branch");
        }

        for (const auto& existing : phase_blocks) {
            if (existing.first == block_open) {
                return fail("multiple NV partition blocks share one conditional; phases would "
                            "overwrite each other's table entries");
            }
        }
        phase_blocks.emplace_back(block_open, block_close);

        if (has_return_outside(source, block_open, block_close, {{pattern.start, pattern.end}})) {
            return fail("enclosing conditional contains an early return outside the partition "
                        "block; barrier control flow would not be uniform");
        }

        edits.push_back({pattern.start, pattern.end - pattern.start, replacement_for(pattern)});
        edits.push_back({block_close + 1, 0, flush_for(pattern)});
    }

    edits.push_back({main_pos, 0, kHelpers});
    edits.push_back({body_open + 1, 0, kLocals});

    if (!apply_edits(source, edits)) {
        return fail("internal error applying partition rewrites");
    }
    return succeed();
}
