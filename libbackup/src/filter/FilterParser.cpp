#include "filter/FilterParser.h"
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <string>
#include <sstream>
#include <pwd.h>
#include <grp.h>

// ===== parse from argc/argv =====

void FilterParser::parse(CompositeFilter& composite, int argc, char* argv[]) {
    const std::string prefix = "--filter-";
    for (int i = 0; i < argc; ++i) {
        std::string arg(argv[i]);
        if (arg.size() <= prefix.size() ||
            arg.compare(0, prefix.size(), prefix) != 0) {
            continue;
        }
        std::string key = arg.substr(prefix.size());
        std::string value;
        if (i + 1 < argc) {
            value = argv[i + 1];
            ++i; // consume value
        }
        if (!parseOne(composite, key, value)) {
            // parseOne prints its own error; just continue
        }
    }
}

// ===== parse single key/value =====

bool FilterParser::parseOne(CompositeFilter& composite,
                            const std::string& key,
                            const std::string& value) {
    if (value.empty()) return false;

    // Determine include/exclude and dimension
    FilterAction action;
    std::string dimStr;

    if (key.compare(0, 8, "include-") == 0) {
        action = FilterAction::INCLUDE;
        dimStr = key.substr(8);
    } else if (key.compare(0, 8, "exclude-") == 0) {
        action = FilterAction::EXCLUDE;
        dimStr = key.substr(8);
    } else {
        return false;
    }

    // --- Path / Name ---
    if (dimStr == "path") {
        composite.addRule(std::make_unique<FilterRule>(
            FilterDimension::PATH, action, value));
        return true;
    }
    if (dimStr == "name") {
        composite.addRule(std::make_unique<FilterRule>(
            FilterDimension::NAME, action, value));
        return true;
    }

    // --- Type ---
    if (dimStr == "type") {
        if (value.size() == 1) {
            composite.addRule(std::make_unique<FilterRule>(action, value[0]));
            return true;
        }
        return false;
    }

    // --- Time: mtime / atime / ctime ---
    if (dimStr == "mtime" || dimStr == "atime" || dimStr == "ctime") {
        FilterDimension dim;
        if (dimStr == "mtime")      dim = FilterDimension::MTIME;
        else if (dimStr == "atime") dim = FilterDimension::ATIME;
        else                         dim = FilterDimension::CTIME;

        TimeOp op;
        time_t t1 = 0, t2 = 0;
        if (!parseTimeValue(value, op, t1, t2)) return false;

        composite.addRule(std::make_unique<FilterRule>(
            dim, action, op, t1, t2));
        return true;
    }

    // --- Size ---
    if (dimStr == "size") {
        SizeOp op;
        off_t v1 = 0, v2 = 0;
        if (!parseSizeValue(value, op, v1, v2)) return false;

        composite.addRule(std::make_unique<FilterRule>(action, op, v1, v2));
        return true;
    }

    // --- Owner ---
    if (dimStr == "owner") {
        // Try as numeric uid first
        char* end = nullptr;
        long id = std::strtol(value.c_str(), &end, 10);
        if (end && *end == '\0' && id >= 0) {
            composite.addRule(std::make_unique<FilterRule>(
                FilterDimension::OWNER, action, static_cast<uid_t>(id)));
        } else {
            composite.addRule(std::make_unique<FilterRule>(
                FilterDimension::OWNER, action, value, ByName{}));
        }
        return true;
    }

    // --- Group ---
    if (dimStr == "group") {
        char* end = nullptr;
        long id = std::strtol(value.c_str(), &end, 10);
        if (end && *end == '\0' && id >= 0) {
            composite.addRule(std::make_unique<FilterRule>(
                FilterDimension::GROUP, action, static_cast<uid_t>(id)));
        } else {
            composite.addRule(std::make_unique<FilterRule>(
                FilterDimension::GROUP, action, value, ByName{}));
        }
        return true;
    }

    return false;
}

// ===== Size parser =====

bool FilterParser::parseSizeValue(const std::string& s,
                                   SizeOp& op, off_t& v1, off_t& v2) {
    if (s.empty()) return false;

    // Range format: "100:1000"
    size_t colon = s.find(':');
    if (colon != std::string::npos) {
        v1 = static_cast<off_t>(std::strtoll(s.substr(0, colon).c_str(), nullptr, 10));
        v2 = static_cast<off_t>(std::strtoll(s.substr(colon + 1).c_str(), nullptr, 10));
        op = SizeOp::RANGE;
        return true;
    }

    // Operator format: "+1M", "-500K", "=1024", ">1G", "<1M", ">=100", "<=50"
    char opChar = s[0];
    std::string numStr;
    if (opChar == '+' || opChar == '-' || opChar == '=' ||
        opChar == '>' || opChar == '<') {
        numStr = s.substr(1);
    } else {
        // No operator prefix — treat as exact match
        numStr = s;
        opChar = '=';
    }

    // Handle ">=" and "<="
    if (opChar == '>' && !numStr.empty() && numStr[0] == '=') {
        op = SizeOp::GE;
        numStr = numStr.substr(1);
    } else if (opChar == '<' && !numStr.empty() && numStr[0] == '=') {
        op = SizeOp::LE;
        numStr = numStr.substr(1);
    } else {
        switch (opChar) {
        case '+': case '>': op = SizeOp::GT; break;
        case '-': case '<': op = SizeOp::LT; break;
        case '=': default:  op = SizeOp::EQ; break;
        }
    }

    // Parse unit suffix: B, K, M, G
    off_t multiplier = 1;
    if (!numStr.empty()) {
        char last = numStr.back();
        switch (last) {
        case 'G': case 'g': multiplier = 1024LL * 1024 * 1024; break;
        case 'M': case 'm': multiplier = 1024LL * 1024;        break;
        case 'K': case 'k': multiplier = 1024LL;               break;
        case 'B': case 'b': multiplier = 1;                    break;
        default: break;
        }
        if (multiplier > 1) numStr.pop_back();
    }

    v1 = static_cast<off_t>(std::strtoll(numStr.c_str(), nullptr, 10)) * multiplier;
    v2 = 0;
    return true;
}

// ===== Time parser =====

static time_t parseDate(const std::string& s) {
    // Expect "YYYY-MM-DD"
    struct tm tm {};
    if (s.size() >= 10) {
        tm.tm_year = std::stoi(s.substr(0, 4)) - 1900;
        tm.tm_mon  = std::stoi(s.substr(5, 2)) - 1;
        tm.tm_mday = std::stoi(s.substr(8, 2));
    }
    tm.tm_hour = 0;
    tm.tm_min  = 0;
    tm.tm_sec  = 0;
    tm.tm_isdst = -1;
    return mktime(&tm);
}

bool FilterParser::parseTimeValue(const std::string& s,
                                   TimeOp& op, time_t& t1, time_t& t2) {
    if (s.empty()) return false;

    size_t colon = s.find(':');
    if (colon == std::string::npos) return false;

    std::string opStr  = s.substr(0, colon);
    std::string valStr = s.substr(colon + 1);

    if (opStr == "before") {
        op = TimeOp::BEFORE;
        t1 = parseDate(valStr);
    } else if (opStr == "after") {
        op = TimeOp::AFTER;
        t1 = parseDate(valStr);
    } else if (opStr == "between") {
        op = TimeOp::BETWEEN;
        size_t comma = valStr.find(',');
        if (comma == std::string::npos) return false;
        t1 = parseDate(valStr.substr(0, comma));
        t2 = parseDate(valStr.substr(comma + 1));
    } else {
        return false;
    }

    return true;
}
