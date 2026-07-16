#ifndef BACKUP_FILTERPARSER_H
#define BACKUP_FILTERPARSER_H

#include "filter/CompositeFilter.h"
#include <string>

/// Parses --filter-* CLI options into a CompositeFilter.
/// Supports --filter-include-<dim> and --filter-exclude-<dim> for all 9 dimensions.
class FilterParser {
public:
    /// Parse from argc/argv. Consumes matching --filter-* args.
    /// @param composite  output filter (rules are appended)
    /// @param argc       argument count
    /// @param argv       argument vector
    void parse(CompositeFilter& composite, int argc, char* argv[]);

    /// Parse a single dimension from a key/value pair.
    /// Key format: "filter-include-name", "filter-exclude-size", etc.
    bool parseOne(CompositeFilter& composite,
                  const std::string& key,
                  const std::string& value);

private:
    bool parseSizeValue(const std::string& s, SizeOp& op,
                         off_t& v1, off_t& v2);
    bool parseTimeValue(const std::string& s, TimeOp& op,
                         time_t& t1, time_t& t2);
};

#endif // BACKUP_FILTERPARSER_H
