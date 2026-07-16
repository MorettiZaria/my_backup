#ifndef BACKUP_IFILEFILTER_H
#define BACKUP_IFILEFILTER_H

#include "core/FileInfo.h"

/// File filter interface.
/// Implementations decide whether a given FileInfo passes the filter.
/// Returning true = keep this file, false = skip it.
class IFileFilter {
public:
    virtual ~IFileFilter() = default;
    virtual bool matches(const FileInfo& info) const = 0;

    /// Returns true if this file is explicitly excluded by an exclude rule.
    /// Different from !matches(): a file can fail matches() because no include
    /// rule matched it, but that doesn't mean it was excluded.
    /// When isExcluded() returns true for a directory, the scanner should
    /// skip the entire subtree.
    virtual bool isExcluded(const FileInfo& info) const { (void)info; return false; }
};

#endif // BACKUP_IFILEFILTER_H
