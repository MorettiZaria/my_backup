#ifndef BACKUP_FILTERDEFS_H
#define BACKUP_FILTERDEFS_H

#include <cstdint>

/// Filter dimensions
enum class FilterDimension : uint8_t {
    PATH,
    NAME,
    TYPE,
    MTIME,
    ATIME,
    CTIME,
    SIZE,
    OWNER,
    GROUP
};

/// Include or exclude
enum class FilterAction : uint8_t {
    INCLUDE,
    EXCLUDE
};

/// Time comparison operators
enum class TimeOp : uint8_t {
    BEFORE,
    AFTER,
    BETWEEN
};

/// Size comparison operators
enum class SizeOp : uint8_t {
    EQ,
    GT,
    LT,
    GE,
    LE,
    RANGE
};

#endif // BACKUP_FILTERDEFS_H
