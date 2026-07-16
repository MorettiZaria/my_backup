#include "gtest/gtest.h"
#include "filter/FilterRule.h"
#include "filter/CompositeFilter.h"
#include "filter/FilterParser.h"
#include "core/FileInfo.h"
#include <sys/stat.h>
#include <cstring>

// ===== Helper =====
namespace {
    FileInfo makeFile(const std::string& path, mode_t type = S_IFREG,
                      off_t size = 100, time_t mtime = 0,
                      uid_t owner = 1000, gid_t group = 1000) {
        FileInfo fi;
        fi.relativePath = path;
        fi.fileType     = type;
        fi.permissions  = 0644;
        fi.owner        = owner;
        fi.group        = group;
        fi.fileSize     = size;
        fi.mtime        = mtime;
        fi.atime        = mtime;
        fi.ctime        = mtime;
        return fi;
    }
}

// ===================== FilterRule =====================

TEST(FilterRuleTest, PathGlobMatch) {
    FilterRule rule(FilterDimension::PATH, FilterAction::INCLUDE, "src/*.cpp");
    EXPECT_TRUE(rule.matches(makeFile("src/main.cpp")));
    EXPECT_FALSE(rule.matches(makeFile("src/main.h")));
    EXPECT_FALSE(rule.matches(makeFile("test/main.cpp")));
}

TEST(FilterRuleTest, PathGlobRecursive) {
    // fnmatch with FNM_PATHNAME: * matches everything except /
    // Use src/* to match any path directly under src/
    FilterRule rule(FilterDimension::PATH, FilterAction::INCLUDE, "src/*");
    EXPECT_TRUE(rule.matches(makeFile("src/main.cpp")));
    EXPECT_FALSE(rule.matches(makeFile("src/sub/dir/file.cpp")));
    EXPECT_FALSE(rule.matches(makeFile("test/main.cpp")));
}

TEST(FilterRuleTest, NameGlobMatch) {
    FilterRule rule(FilterDimension::NAME, FilterAction::INCLUDE, "*.cpp");
    EXPECT_TRUE(rule.matches(makeFile("src/main.cpp")));
    EXPECT_TRUE(rule.matches(makeFile("main.cpp")));
    EXPECT_FALSE(rule.matches(makeFile("src/main.h")));
}

TEST(FilterRuleTest, NameGlobWildcard) {
    FilterRule rule(FilterDimension::NAME, FilterAction::INCLUDE, "report-*.pdf");
    EXPECT_TRUE(rule.matches(makeFile("docs/report-2024.pdf")));
    EXPECT_FALSE(rule.matches(makeFile("docs/report.txt")));
}

TEST(FilterRuleTest, TypeFilterReg) {
    FilterRule rule(FilterAction::INCLUDE, 'f');
    EXPECT_TRUE(rule.matches(makeFile("a.txt", S_IFREG)));
    EXPECT_FALSE(rule.matches(makeFile("a", S_IFDIR)));
    EXPECT_FALSE(rule.matches(makeFile("a", S_IFLNK)));
}

TEST(FilterRuleTest, TypeFilterDir) {
    FilterRule rule(FilterAction::INCLUDE, 'd');
    EXPECT_TRUE(rule.matches(makeFile("a", S_IFDIR)));
    EXPECT_FALSE(rule.matches(makeFile("a.txt", S_IFREG)));
}

TEST(FilterRuleTest, TypeFilterSymlink) {
    FilterRule rule(FilterAction::INCLUDE, 'l');
    EXPECT_TRUE(rule.matches(makeFile("link", S_IFLNK)));
}

TEST(FilterRuleTest, TypeFilterPipe) {
    FilterRule rule(FilterAction::INCLUDE, 'p');
    EXPECT_TRUE(rule.matches(makeFile("fifo", S_IFIFO)));
}

TEST(FilterRuleTest, TimeBefore) {
    FilterRule rule(FilterDimension::MTIME, FilterAction::INCLUDE,
                    TimeOp::BEFORE, 1000);
    EXPECT_TRUE(rule.matches(makeFile("a", S_IFREG, 100, 500)));
    EXPECT_FALSE(rule.matches(makeFile("b", S_IFREG, 100, 1500)));
}

TEST(FilterRuleTest, TimeAfter) {
    FilterRule rule(FilterDimension::MTIME, FilterAction::INCLUDE,
                    TimeOp::AFTER, 1000);
    EXPECT_TRUE(rule.matches(makeFile("a", S_IFREG, 100, 1500)));
    EXPECT_FALSE(rule.matches(makeFile("b", S_IFREG, 100, 500)));
}

TEST(FilterRuleTest, TimeBetween) {
    FilterRule rule(FilterDimension::MTIME, FilterAction::INCLUDE,
                    TimeOp::BETWEEN, 1000, 2000);
    EXPECT_TRUE(rule.matches(makeFile("a", S_IFREG, 100, 1500)));
    EXPECT_FALSE(rule.matches(makeFile("b", S_IFREG, 100, 500)));
    EXPECT_FALSE(rule.matches(makeFile("c", S_IFREG, 100, 2500)));
}

TEST(FilterRuleTest, TimeAtime) {
    FilterRule rule(FilterDimension::ATIME, FilterAction::INCLUDE,
                    TimeOp::AFTER, 1000);
    // Uses atime field, which is set equal to mtime in our helper
    EXPECT_TRUE(rule.matches(makeFile("a", S_IFREG, 100, 1500)));
}

TEST(FilterRuleTest, SizeGT) {
    FilterRule rule(FilterAction::INCLUDE, SizeOp::GT, 1000);
    EXPECT_TRUE(rule.matches(makeFile("big", S_IFREG, 2000)));
    EXPECT_FALSE(rule.matches(makeFile("small", S_IFREG, 500)));
}

TEST(FilterRuleTest, SizeLT) {
    FilterRule rule(FilterAction::INCLUDE, SizeOp::LT, 1000);
    EXPECT_TRUE(rule.matches(makeFile("small", S_IFREG, 500)));
    EXPECT_FALSE(rule.matches(makeFile("big", S_IFREG, 2000)));
}

TEST(FilterRuleTest, SizeRange) {
    FilterRule rule(FilterAction::INCLUDE, SizeOp::RANGE, 100, 200);
    EXPECT_TRUE(rule.matches(makeFile("a", S_IFREG, 150)));
    EXPECT_FALSE(rule.matches(makeFile("b", S_IFREG, 50)));
    EXPECT_FALSE(rule.matches(makeFile("c", S_IFREG, 300)));
}

TEST(FilterRuleTest, OwnerByUid) {
    FilterRule rule(FilterDimension::OWNER, FilterAction::INCLUDE,
                    static_cast<uid_t>(1000));
    EXPECT_TRUE(rule.matches(makeFile("a", S_IFREG, 100, 0, 1000)));
    EXPECT_FALSE(rule.matches(makeFile("b", S_IFREG, 100, 0, 2000)));
}

TEST(FilterRuleTest, GroupByGid) {
    FilterRule rule(FilterDimension::GROUP, FilterAction::INCLUDE,
                    static_cast<uid_t>(1000));
    EXPECT_TRUE(rule.matches(makeFile("a", S_IFREG, 100, 0, 1000, 1000)));
    EXPECT_FALSE(rule.matches(makeFile("b", S_IFREG, 100, 0, 1000, 2000)));
}

// ===================== CompositeFilter =====================

TEST(CompositeFilterTest, NoRulesEverythingPasses) {
    CompositeFilter filter;
    EXPECT_TRUE(filter.isEmpty());
    EXPECT_TRUE(filter.matches(makeFile("anything")));
}

TEST(CompositeFilterTest, SingleIncludeRule) {
    CompositeFilter filter;
    filter.addIncludeName("*.cpp");
    EXPECT_TRUE(filter.matches(makeFile("src/a.cpp")));
    EXPECT_FALSE(filter.matches(makeFile("src/a.h")));
}

TEST(CompositeFilterTest, SameDimensionOR) {
    CompositeFilter filter;
    filter.addIncludeName("*.cpp");
    filter.addIncludeName("*.h");
    // OR: either .cpp or .h passes
    EXPECT_TRUE(filter.matches(makeFile("src/a.cpp")));
    EXPECT_TRUE(filter.matches(makeFile("src/a.h")));
    EXPECT_FALSE(filter.matches(makeFile("src/a.txt")));
}

TEST(CompositeFilterTest, DifferentDimensionAND) {
    CompositeFilter filter;
    filter.addIncludeName("*.cpp");
    filter.addIncludeSize(SizeOp::GT, 1000);
    EXPECT_TRUE(filter.matches(makeFile("big.cpp", S_IFREG, 2000)));
    EXPECT_FALSE(filter.matches(makeFile("small.cpp", S_IFREG, 500)));   // name matches but size fails
    EXPECT_FALSE(filter.matches(makeFile("big.txt", S_IFREG, 2000)));    // size matches but name fails
}

TEST(CompositeFilterTest, ExcludeTakesPriority) {
    CompositeFilter filter;
    filter.addIncludeName("*.cpp");
    filter.addExcludePath("*/test/*");
    EXPECT_TRUE(filter.matches(makeFile("src/a.cpp")));
    EXPECT_FALSE(filter.matches(makeFile("src/test/a.cpp")));  // excluded by path
}

TEST(CompositeFilterTest, ExcludeOverridesInclude) {
    CompositeFilter filter;
    filter.addIncludePath("src/**");
    filter.addExcludeName("*.tmp");
    EXPECT_TRUE(filter.matches(makeFile("src/a.cpp")));
    EXPECT_FALSE(filter.matches(makeFile("src/a.tmp")));
}

TEST(CompositeFilterTest, OnlyExcludeNoInclude) {
    CompositeFilter filter;
    filter.addExcludeName("*.tmp");
    // No include rules → everything passes unless excluded
    EXPECT_TRUE(filter.matches(makeFile("src/a.cpp")));
    EXPECT_FALSE(filter.matches(makeFile("src/a.tmp")));
}

TEST(CompositeFilterTest, CopyConstructor) {
    CompositeFilter filter;
    filter.addIncludeName("*.cpp");
    CompositeFilter copy(filter);
    EXPECT_TRUE(copy.matches(makeFile("a.cpp")));
    EXPECT_FALSE(copy.matches(makeFile("a.h")));
}

// ===================== FilterParser =====================

TEST(FilterParserTest, ParseIncludeName) {
    FilterParser parser;
    CompositeFilter filter;
    EXPECT_TRUE(parser.parseOne(filter, "include-name", "*.cpp"));
    EXPECT_FALSE(filter.isEmpty());
    EXPECT_TRUE(filter.matches(makeFile("a.cpp")));
    EXPECT_FALSE(filter.matches(makeFile("a.h")));
}

TEST(FilterParserTest, ParseExcludePath) {
    FilterParser parser;
    CompositeFilter filter;
    EXPECT_TRUE(parser.parseOne(filter, "exclude-path", "*/test/*"));
    EXPECT_TRUE(filter.matches(makeFile("src/a.cpp")));
    EXPECT_FALSE(filter.matches(makeFile("src/test/a.cpp")));
}

TEST(FilterParserTest, ParseIncludeType) {
    FilterParser parser;
    CompositeFilter filter;
    EXPECT_TRUE(parser.parseOne(filter, "include-type", "d"));
    EXPECT_TRUE(filter.matches(makeFile("a", S_IFDIR)));
    EXPECT_FALSE(filter.matches(makeFile("a.txt", S_IFREG)));
}

TEST(FilterParserTest, ParseIncludeSizeGT) {
    FilterParser parser;
    CompositeFilter filter;
    EXPECT_TRUE(parser.parseOne(filter, "include-size", "+1M"));
    // +1M = > 1048576
    EXPECT_TRUE(filter.matches(makeFile("big", S_IFREG, 2000000)));
    EXPECT_FALSE(filter.matches(makeFile("small", S_IFREG, 500)));
}

TEST(FilterParserTest, ParseIncludeSizeRange) {
    FilterParser parser;
    CompositeFilter filter;
    EXPECT_TRUE(parser.parseOne(filter, "include-size", "100:1000"));
    EXPECT_TRUE(filter.matches(makeFile("a", S_IFREG, 500)));
    EXPECT_FALSE(filter.matches(makeFile("b", S_IFREG, 50)));
    EXPECT_FALSE(filter.matches(makeFile("c", S_IFREG, 2000)));
}

TEST(FilterParserTest, ParseMtimeAfter) {
    FilterParser parser;
    CompositeFilter filter;
    EXPECT_TRUE(parser.parseOne(filter, "include-mtime", "after:2024-06-01"));
    // After June 1 2024 = timestamp > 1717200000 (approx)
    EXPECT_TRUE(filter.matches(makeFile("a", S_IFREG, 100, 1720000000)));
    EXPECT_FALSE(filter.matches(makeFile("b", S_IFREG, 100, 1700000000)));
}

TEST(FilterParserTest, ParseMtimeBetween) {
    FilterParser parser;
    CompositeFilter filter;
    EXPECT_TRUE(parser.parseOne(filter, "include-mtime", "between:2024-01-01,2024-12-31"));
    // Mid-2024
    EXPECT_TRUE(filter.matches(makeFile("a", S_IFREG, 100, 1717200000)));
    // Early 2023
    EXPECT_FALSE(filter.matches(makeFile("b", S_IFREG, 100, 1672531200)));
}

TEST(FilterParserTest, ParseOwnerByUid) {
    FilterParser parser;
    CompositeFilter filter;
    EXPECT_TRUE(parser.parseOne(filter, "include-owner", "1000"));
    EXPECT_TRUE(filter.matches(makeFile("a", S_IFREG, 100, 0, 1000)));
    EXPECT_FALSE(filter.matches(makeFile("b", S_IFREG, 100, 0, 2000)));
}

TEST(FilterParserTest, EmptyFilter) {
    CompositeFilter filter;
    EXPECT_TRUE(filter.isEmpty());
    EXPECT_TRUE(filter.matches(makeFile("anything")));
}
