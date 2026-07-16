#include "filter/FilterRule.h"
#include <fnmatch.h>
#include <pwd.h>
#include <grp.h>
#include <sys/stat.h>
#include <cstring>

// ===== Constructors =====

FilterRule::FilterRule(FilterDimension dim, FilterAction action,
                       const std::string& globPattern)
    : dim_(dim), action_(action), globPattern_(globPattern)
{
    if (dim_ != FilterDimension::PATH && dim_ != FilterDimension::NAME) {
        dim_ = FilterDimension::NAME;  // safety default
    }
}

FilterRule::FilterRule(FilterAction action, char typeCode)
    : dim_(FilterDimension::TYPE), action_(action)
{
    switch (typeCode) {
    case 'f': typeMask_ = S_IFREG;  break;
    case 'd': typeMask_ = S_IFDIR;  break;
    case 'l': typeMask_ = S_IFLNK;  break;
    case 'p': typeMask_ = S_IFIFO;  break;
    case 'b': typeMask_ = S_IFBLK;  break;
    case 'c': typeMask_ = S_IFCHR;  break;
    case 's': typeMask_ = S_IFSOCK; break;
    default:  typeMask_ = 0;        break;
    }
}

FilterRule::FilterRule(FilterDimension dim, FilterAction action,
                       TimeOp op, time_t t1, time_t t2)
    : dim_(dim), action_(action), timeOp_(op), timeVal1_(t1), timeVal2_(t2) {}

FilterRule::FilterRule(FilterAction action, SizeOp op,
                       off_t v1, off_t v2)
    : dim_(FilterDimension::SIZE), action_(action),
      sizeOp_(op), sizeVal1_(v1), sizeVal2_(v2) {}

FilterRule::FilterRule(FilterDimension dim, FilterAction action, uid_t id)
    : dim_(dim), action_(action)
{
    if (dim_ == FilterDimension::OWNER) {
        owner_ = id;
    } else if (dim_ == FilterDimension::GROUP) {
        group_ = static_cast<gid_t>(id);
    }
}

FilterRule::FilterRule(FilterDimension dim, FilterAction action,
                       const std::string& name, ByName)
    : dim_(dim), action_(action)
{
    if (dim_ == FilterDimension::OWNER) {
        struct passwd* pw = getpwnam(name.c_str());
        owner_ = pw ? pw->pw_uid : static_cast<uid_t>(-1);
    } else if (dim_ == FilterDimension::GROUP) {
        struct group* gr = getgrnam(name.c_str());
        group_ = gr ? gr->gr_gid : static_cast<gid_t>(-1);
    }
}

std::unique_ptr<FilterRule> FilterRule::clone() const {
    // Use private default constructor to avoid dimension-specific side effects
    // (the PATH/NAME constructor resets non-PATH/non-NAME dims to NAME).
    auto copy = std::unique_ptr<FilterRule>(new FilterRule());
    copy->dim_        = dim_;
    copy->action_      = action_;
    copy->globPattern_ = globPattern_;
    copy->typeMask_    = typeMask_;
    copy->timeOp_      = timeOp_;
    copy->timeVal1_    = timeVal1_;
    copy->timeVal2_    = timeVal2_;
    copy->sizeOp_      = sizeOp_;
    copy->sizeVal1_    = sizeVal1_;
    copy->sizeVal2_    = sizeVal2_;
    copy->owner_       = owner_;
    copy->group_       = group_;
    return copy;
}

bool FilterRule::isExcluded(const FileInfo& info) const {
    return action_ == FilterAction::EXCLUDE && matches(info);
}

// ===== matches() dispatch =====

bool FilterRule::matches(const FileInfo& info) const {
    switch (dim_) {
    case FilterDimension::PATH:  return matchPath(info);
    case FilterDimension::NAME:  return matchName(info);
    case FilterDimension::TYPE:  return matchType(info);
    case FilterDimension::MTIME: // fall through
    case FilterDimension::ATIME: // fall through
    case FilterDimension::CTIME: return matchTime(info);
    case FilterDimension::SIZE:  return matchSize(info);
    case FilterDimension::OWNER: return matchOwner(info);
    case FilterDimension::GROUP: return matchGroup(info);
    }
    return false;
}

// ===== Per-dimension matchers =====

bool FilterRule::matchPath(const FileInfo& info) const {
    return fnmatch(globPattern_.c_str(), info.relativePath.c_str(),
                   FNM_PATHNAME) == 0;
}

bool FilterRule::matchName(const FileInfo& info) const {
    const std::string& path = info.relativePath;
    size_t slash = path.rfind('/');
    std::string basename = (slash == std::string::npos) ? path
                                                         : path.substr(slash + 1);
    return fnmatch(globPattern_.c_str(), basename.c_str(), 0) == 0;
}

bool FilterRule::matchType(const FileInfo& info) const {
    return (info.fileType & S_IFMT) == (typeMask_ & S_IFMT);
}

bool FilterRule::matchTime(const FileInfo& info) const {
    time_t t = 0;
    switch (dim_) {
    case FilterDimension::MTIME: t = info.mtime; break;
    case FilterDimension::ATIME: t = info.atime; break;
    case FilterDimension::CTIME: t = info.ctime; break;
    default: return false;
    }

    switch (timeOp_) {
    case TimeOp::BEFORE:  return t <  timeVal1_;
    case TimeOp::AFTER:   return t >  timeVal1_;
    case TimeOp::BETWEEN: return t >= timeVal1_ && t <= timeVal2_;
    }
    return false;
}

bool FilterRule::matchSize(const FileInfo& info) const {
    off_t sz = info.fileSize;
    switch (sizeOp_) {
    case SizeOp::EQ:    return sz == sizeVal1_;
    case SizeOp::GT:    return sz >  sizeVal1_;
    case SizeOp::LT:    return sz <  sizeVal1_;
    case SizeOp::GE:    return sz >= sizeVal1_;
    case SizeOp::LE:    return sz <= sizeVal1_;
    case SizeOp::RANGE: return sz >= sizeVal1_ && sz <= sizeVal2_;
    }
    return false;
}

bool FilterRule::matchOwner(const FileInfo& info) const {
    return static_cast<uid_t>(info.owner) == owner_;
}

bool FilterRule::matchGroup(const FileInfo& info) const {
    return static_cast<gid_t>(info.group) == group_;
}
