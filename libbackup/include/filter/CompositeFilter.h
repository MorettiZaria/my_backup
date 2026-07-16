#ifndef BACKUP_COMPOSITEFILTER_H
#define BACKUP_COMPOSITEFILTER_H

#include "filter/IFileFilter.h"
#include "filter/FilterRule.h"
#include <memory>
#include <vector>

/// Combines multiple FilterRules with composable logic:
///   - Same-dimension rules: OR  (any match passes that dimension)
///   - Different-dimension rules: AND (all dimensions must pass)
///   - Exclude rules: take priority (any exclude match ⇒ reject)
///   - Empty filter (no rules): everything passes
class CompositeFilter : public IFileFilter {
public:
    CompositeFilter() = default;

    // Deep-copy (needed because rules_ holds unique_ptrs)
    CompositeFilter(const CompositeFilter& other);
    CompositeFilter& operator=(const CompositeFilter& other);
    CompositeFilter(CompositeFilter&&) = default;
    CompositeFilter& operator=(CompositeFilter&&) = default;

    void addRule(std::unique_ptr<FilterRule> rule);

    // Convenience: add include/exclude rules by dimension
    void addIncludePath(const std::string& glob);
    void addExcludePath(const std::string& glob);
    void addIncludeName(const std::string& glob);
    void addExcludeName(const std::string& glob);
    void addIncludeType(char typeCode);
    void addExcludeType(char typeCode);
    void addIncludeMtime(TimeOp op, time_t t1, time_t t2 = 0);
    void addExcludeMtime(TimeOp op, time_t t1, time_t t2 = 0);
    void addIncludeAtime(TimeOp op, time_t t1, time_t t2 = 0);
    void addExcludeAtime(TimeOp op, time_t t1, time_t t2 = 0);
    void addIncludeCtime(TimeOp op, time_t t1, time_t t2 = 0);
    void addExcludeCtime(TimeOp op, time_t t1, time_t t2 = 0);
    void addIncludeSize(SizeOp op, off_t v1, off_t v2 = 0);
    void addExcludeSize(SizeOp op, off_t v1, off_t v2 = 0);
    void addIncludeOwner(uid_t uid);
    void addExcludeOwner(uid_t uid);
    void addIncludeOwner(const std::string& name);
    void addExcludeOwner(const std::string& name);
    void addIncludeGroup(gid_t gid);
    void addExcludeGroup(gid_t gid);
    void addIncludeGroup(const std::string& name);
    void addExcludeGroup(const std::string& name);

    bool matches(const FileInfo& info) const override;
    bool isExcluded(const FileInfo& info) const override;
    bool isEmpty() const { return rules_.empty(); }
    void clear() { rules_.clear(); }

private:
    std::vector<std::unique_ptr<FilterRule>> rules_;
};

#endif // BACKUP_COMPOSITEFILTER_H
