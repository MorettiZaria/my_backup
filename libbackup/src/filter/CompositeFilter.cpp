#include "filter/CompositeFilter.h"

// Deep-copy
CompositeFilter::CompositeFilter(const CompositeFilter& other) {
    for (const auto& r : other.rules_) {
        rules_.push_back(r->clone());
    }
}
CompositeFilter& CompositeFilter::operator=(const CompositeFilter& other) {
    if (this != &other) {
        rules_.clear();
        for (const auto& r : other.rules_) {
            rules_.push_back(r->clone());
        }
    }
    return *this;
}

void CompositeFilter::addRule(std::unique_ptr<FilterRule> rule) {
    rules_.push_back(std::move(rule));
}

// ===== Convenience builders =====

void CompositeFilter::addIncludePath(const std::string& glob) {
    rules_.push_back(std::make_unique<FilterRule>(
        FilterDimension::PATH, FilterAction::INCLUDE, glob));
}
void CompositeFilter::addExcludePath(const std::string& glob) {
    rules_.push_back(std::make_unique<FilterRule>(
        FilterDimension::PATH, FilterAction::EXCLUDE, glob));
}
void CompositeFilter::addIncludeName(const std::string& glob) {
    rules_.push_back(std::make_unique<FilterRule>(
        FilterDimension::NAME, FilterAction::INCLUDE, glob));
}
void CompositeFilter::addExcludeName(const std::string& glob) {
    rules_.push_back(std::make_unique<FilterRule>(
        FilterDimension::NAME, FilterAction::EXCLUDE, glob));
}
void CompositeFilter::addIncludeType(char typeCode) {
    rules_.push_back(std::make_unique<FilterRule>(FilterAction::INCLUDE, typeCode));
}
void CompositeFilter::addExcludeType(char typeCode) {
    rules_.push_back(std::make_unique<FilterRule>(FilterAction::EXCLUDE, typeCode));
}
void CompositeFilter::addIncludeMtime(TimeOp op, time_t t1, time_t t2) {
    rules_.push_back(std::make_unique<FilterRule>(
        FilterDimension::MTIME, FilterAction::INCLUDE, op, t1, t2));
}
void CompositeFilter::addExcludeMtime(TimeOp op, time_t t1, time_t t2) {
    rules_.push_back(std::make_unique<FilterRule>(
        FilterDimension::MTIME, FilterAction::EXCLUDE, op, t1, t2));
}
void CompositeFilter::addIncludeAtime(TimeOp op, time_t t1, time_t t2) {
    rules_.push_back(std::make_unique<FilterRule>(
        FilterDimension::ATIME, FilterAction::INCLUDE, op, t1, t2));
}
void CompositeFilter::addExcludeAtime(TimeOp op, time_t t1, time_t t2) {
    rules_.push_back(std::make_unique<FilterRule>(
        FilterDimension::ATIME, FilterAction::EXCLUDE, op, t1, t2));
}
void CompositeFilter::addIncludeCtime(TimeOp op, time_t t1, time_t t2) {
    rules_.push_back(std::make_unique<FilterRule>(
        FilterDimension::CTIME, FilterAction::INCLUDE, op, t1, t2));
}
void CompositeFilter::addExcludeCtime(TimeOp op, time_t t1, time_t t2) {
    rules_.push_back(std::make_unique<FilterRule>(
        FilterDimension::CTIME, FilterAction::EXCLUDE, op, t1, t2));
}
void CompositeFilter::addIncludeSize(SizeOp op, off_t v1, off_t v2) {
    rules_.push_back(std::make_unique<FilterRule>(FilterAction::INCLUDE, op, v1, v2));
}
void CompositeFilter::addExcludeSize(SizeOp op, off_t v1, off_t v2) {
    rules_.push_back(std::make_unique<FilterRule>(FilterAction::EXCLUDE, op, v1, v2));
}
void CompositeFilter::addIncludeOwner(uid_t uid) {
    rules_.push_back(std::make_unique<FilterRule>(
        FilterDimension::OWNER, FilterAction::INCLUDE, uid));
}
void CompositeFilter::addExcludeOwner(uid_t uid) {
    rules_.push_back(std::make_unique<FilterRule>(
        FilterDimension::OWNER, FilterAction::EXCLUDE, uid));
}
void CompositeFilter::addIncludeOwner(const std::string& name) {
    rules_.push_back(std::make_unique<FilterRule>(
        FilterDimension::OWNER, FilterAction::INCLUDE, name, ByName{}));
}
void CompositeFilter::addExcludeOwner(const std::string& name) {
    rules_.push_back(std::make_unique<FilterRule>(
        FilterDimension::OWNER, FilterAction::EXCLUDE, name, ByName{}));
}
void CompositeFilter::addIncludeGroup(gid_t gid) {
    rules_.push_back(std::make_unique<FilterRule>(
        FilterDimension::GROUP, FilterAction::INCLUDE, static_cast<uid_t>(gid)));
}
void CompositeFilter::addExcludeGroup(gid_t gid) {
    rules_.push_back(std::make_unique<FilterRule>(
        FilterDimension::GROUP, FilterAction::EXCLUDE, static_cast<uid_t>(gid)));
}
void CompositeFilter::addIncludeGroup(const std::string& name) {
    rules_.push_back(std::make_unique<FilterRule>(
        FilterDimension::GROUP, FilterAction::INCLUDE, name, ByName{}));
}
void CompositeFilter::addExcludeGroup(const std::string& name) {
    rules_.push_back(std::make_unique<FilterRule>(
        FilterDimension::GROUP, FilterAction::EXCLUDE, name, ByName{}));
}

// ===== matches() =====

bool CompositeFilter::matches(const FileInfo& info) const {
    if (rules_.empty()) return true;

    // Per-dimension tracking for AND logic across different dimensions
    bool hasInclude[9] = {};
    bool matched[9] = {};

    for (const auto& r : rules_) {
        bool hit = r->matches(info);

        if (r->action() == FilterAction::EXCLUDE) {
            if (hit) return false;  // exclude wins immediately
        } else {
            int dimIdx = static_cast<int>(r->dimension());
            hasInclude[dimIdx] = true;
            // Same-dimension includes are OR'd
            matched[dimIdx] = matched[dimIdx] || hit;
        }
    }

    // All dimensions that have include rules must have at least one match
    for (int d = 0; d < 9; ++d) {
        if (hasInclude[d] && !matched[d]) return false;
    }

    return true;
}
