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
};

#endif // BACKUP_IFILEFILTER_H
