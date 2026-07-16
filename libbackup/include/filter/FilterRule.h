#ifndef BACKUP_FILTERRULE_H
#define BACKUP_FILTERRULE_H

#include "filter/IFileFilter.h"
#include "filter/FilterDefs.h"
#include <memory>
#include <string>
#include <sys/types.h>

/// Tag type for owner/group-by-name constructors
struct ByName {};

/// A single filter rule: checks one dimension of a FileInfo.
class FilterRule : public IFileFilter {
public:
    // --- Path / Name: glob pattern ---
    FilterRule(FilterDimension dim, FilterAction action,
               const std::string& globPattern);

    // --- Type: single char (f/d/l/p/b/c/s) ---
    FilterRule(FilterAction action, char typeCode);

    // --- Time: mtime/atime/ctime ---
    FilterRule(FilterDimension dim, FilterAction action,
               TimeOp op, time_t t1, time_t t2 = 0);

    // --- Size ---
    FilterRule(FilterAction action, SizeOp op,
               off_t v1, off_t v2 = 0);

    // --- Owner / Group: by numeric id (uid_t only — gid_t is same width on Linux) ---
    FilterRule(FilterDimension dim, FilterAction action, uid_t id);

    // --- Owner / Group: by name (tag dispatch to disambiguate from glob constructor) ---
    FilterRule(FilterDimension dim, FilterAction action,
               const std::string& name, ByName);

    bool matches(const FileInfo& info) const override;

    /// Deep-clone this rule
    std::unique_ptr<FilterRule> clone() const;

    FilterDimension dimension() const { return dim_; }
    FilterAction    action()    const { return action_; }

private:
    FilterDimension dim_;
    FilterAction    action_;

    std::string globPattern_;
    mode_t      typeMask_     = 0;
    TimeOp      timeOp_       = TimeOp::AFTER;
    time_t      timeVal1_     = 0;
    time_t      timeVal2_     = 0;
    SizeOp      sizeOp_       = SizeOp::EQ;
    off_t       sizeVal1_     = 0;
    off_t       sizeVal2_     = 0;
    uid_t       owner_        = static_cast<uid_t>(-1);
    gid_t       group_        = static_cast<gid_t>(-1);

    bool matchPath(const FileInfo& info) const;
    bool matchName(const FileInfo& info) const;
    bool matchType(const FileInfo& info) const;
    bool matchTime(const FileInfo& info) const;
    bool matchSize(const FileInfo& info) const;
    bool matchOwner(const FileInfo& info) const;
    bool matchGroup(const FileInfo& info) const;
};

#endif // BACKUP_FILTERRULE_H
