// A single probe binary for the three pure functions in utils/, so the fuzz
// suites can drive them directly instead of going through the CLI. Going
// through the CLI would mean one process per case; at ~11,000 cases that is
// the difference between seconds and minutes, and the point of these functions
// being pure is that they can be tested directly.
//
// Three modes, chosen by argv[1], each reading tab-separated records on stdin:
//
//   affinity   <declared-type>                      -> affinity as an int
//   like       <text> <pattern>                     -> 0 or 1
//   match      <declared-type> <value> <op> <lit>   -> 0 or 1
//
// Values are tagged so a TEXT "1" and an INTEGER 1 stay distinguishable --
// that distinction is the entire subject of the affinity tests, so a loose
// format would quietly destroy what is being measured.

#include "types/query.hpp"
#include "utils/like.hpp"
#include "utils/value_compare.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using namespace sqlite;

static std::vector<std::string> splitTabs(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : line) {
        if (c == '\t') { out.push_back(cur); cur.clear(); }
        else cur += c;
    }
    out.push_back(cur);
    return out;
}

// Records are tab-separated and line-oriented, so a value containing a tab or
// a newline has to be escaped on the way in. SQLite's LIKE is perfectly happy
// to be handed a pattern with a newline in it, so dropping those cases would
// quietly leave a real hole rather than tidy the format up.
static std::string unescape(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            char n = s[++i];
            if (n == 't') out += '\t';
            else if (n == 'n') out += '\n';
            else if (n == 'r') out += '\r';
            else if (n == '\\') out += '\\';
            else out += n;
        } else {
            out += s[i];
        }
    }
    return out;
}

// Affinity ints must match the probe driver's enum, which mirrors Affinity:
// Blob=0 Text=1 Numeric=2 Integer=3 Real=4
static int affinityMode(const std::string& declaredType) {
    std::cout << static_cast<int>(columnAffinity(declaredType)) << "\n";
    return 0;
}

static int likeMode(const std::vector<std::string>& f) {
    if (f.size() < 2) return 2;
    std::cout << (sqlLikeMatch(unescape(f[0]), unescape(f[1])) ? 1 : 0) << "\n";
    return 0;
}

// "<kind>:<payload>", where kind is n/i/r/t/b for the ColumnValue side and
// n/i/r/t/b for the LiteralValue side (n is only valid on the left).
static ColumnValue parseColumnValue(const std::string& spec) {
    const size_t colon = spec.find(':');
    const std::string kind = spec.substr(0, colon);
    const std::string payload = unescape(spec.substr(colon + 1));
    if (kind == "n") return ColumnValue{};
    if (kind == "i") return ColumnValue{static_cast<int64_t>(std::strtoll(payload.c_str(), nullptr, 10))};
    if (kind == "r") return ColumnValue{std::strtod(payload.c_str(), nullptr)};
    if (kind == "b") {
        std::vector<uint8_t> bytes;
        for (size_t i = 0; i + 1 < payload.size(); i += 2) {
            bytes.push_back(static_cast<uint8_t>(std::strtoul(payload.substr(i, 2).c_str(), nullptr, 16)));
        }
        return ColumnValue{bytes};
    }
    return ColumnValue{payload};
}

static LiteralValue parseLiteral(const std::string& spec) {
    const size_t colon = spec.find(':');
    const std::string kind = spec.substr(0, colon);
    const std::string payload = unescape(spec.substr(colon + 1));
    if (kind == "i") return LiteralValue{static_cast<int64_t>(std::strtoll(payload.c_str(), nullptr, 10))};
    if (kind == "r") return LiteralValue{std::strtod(payload.c_str(), nullptr)};
    if (kind == "b") return LiteralValue{payload == "1"};
    return LiteralValue{payload};
}

// Ops are indexed to match the probe driver: = != < <= > >= LIKE
static CompareOp parseOp(const std::string& op) {
    if (op == "=")  return CompareOp::EQUAL;
    if (op == "!=") return CompareOp::NOT_EQUAL;
    if (op == "<")  return CompareOp::LESS_THAN;
    if (op == "<=") return CompareOp::LESS_EQUAL;
    if (op == ">")  return CompareOp::GREATER_THAN;
    if (op == ">=") return CompareOp::GREATER_EQUAL;
    return CompareOp::LIKE;
}

static int matchMode(const std::vector<std::string>& f) {
    if (f.size() < 4) return 2;
    const Affinity aff = columnAffinity(f[0]);
    const ColumnValue value = parseColumnValue(f[1]);
    const CompareOp op = parseOp(f[2]);
    const LiteralValue lit = parseLiteral(f[3]);
    std::cout << (valueMatches(value, aff, op, lit) ? 1 : 0) << "\n";
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: sqlite_probe <affinity|like|match>\n";
        return 2;
    }
    const std::string mode = argv[1];
    // In affinity mode the whole line IS the declared type, and an empty
    // declared type is the case that must return Blob -- so an empty line is a
    // record, not a blank to skip. In the other modes an empty line carries no
    // fields and is meaningless. Getting this wrong is not a cosmetic bug: it
    // drops one output line and silently misaligns every result after it.
    const bool empty_is_record = (mode == "affinity");
    std::string line;
    while (std::getline(std::cin, line)) {
        if (!line.empty()) {
            // strip a trailing CR so CRLF input does not corrupt the last field
            if (line.back() == '\r') line.pop_back();
        } else if (!empty_is_record) {
            continue;
        }
        const auto f = splitTabs(line);
        if (mode == "affinity") {
            if (affinityMode(f[0])) return 2;
        } else if (mode == "like") {
            if (likeMode(f)) return 2;
        } else if (mode == "match") {
            if (matchMode(f)) return 2;
        } else {
            std::cerr << "unknown mode: " << mode << "\n";
            return 2;
        }
    }
    return 0;
}
