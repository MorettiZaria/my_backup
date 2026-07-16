#include "gtest/gtest.h"
#include "filter/FilterRule.h"
#include "filter/CompositeFilter.h"
#include "filter/FilterParser.h"
#include "core/FileInfo.h"
#include "core/FileScanner.h"
#include <sys/stat.h>
#include <cstring>
#include <fstream>
#include <unistd.h>

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

    std::string tmpDir() {
        std::string d = "/tmp/flt_gtest_" + std::to_string(getpid());
        mkdir(d.c_str(), 0755);
        return d;
    }

    void rmrf(const std::string& path) {
        std::string cmd = "rm -rf " + path + " 2>/dev/null";
        (void)system(cmd.c_str());
    }

    void writeFile(const std::string& path, const std::string& content) {
        size_t pos = path.rfind('/');
        if (pos != std::string::npos) {
            std::string dir = path.substr(0, pos);
            std::string cmd = "mkdir -p " + dir;
            (void)system(cmd.c_str());
        }
        std::ofstream out(path, std::ios::binary);
        out.write(content.data(), static_cast<std::streamsize>(content.size()));
        out.close();
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

// ===================== FilterRule: additional dimensions =====================

TEST(FilterRuleTest, TypeFilterBlock) {
    FilterRule rule(FilterAction::INCLUDE, 'b');
    EXPECT_TRUE(rule.matches(makeFile("blk", S_IFBLK)));
    EXPECT_FALSE(rule.matches(makeFile("f", S_IFREG)));
}

TEST(FilterRuleTest, TypeFilterChar) {
    FilterRule rule(FilterAction::INCLUDE, 'c');
    EXPECT_TRUE(rule.matches(makeFile("chr", S_IFCHR)));
    EXPECT_FALSE(rule.matches(makeFile("f", S_IFREG)));
}

TEST(FilterRuleTest, TypeFilterSock) {
    FilterRule rule(FilterAction::INCLUDE, 's');
    EXPECT_TRUE(rule.matches(makeFile("sock", S_IFSOCK)));
    EXPECT_FALSE(rule.matches(makeFile("f", S_IFREG)));
}

TEST(FilterRuleTest, TypeFilterInvalidCode) {
    FilterRule rule(FilterAction::INCLUDE, 'x');
    // typeMask_ == 0 → never matches anything positive
    EXPECT_FALSE(rule.matches(makeFile("a", S_IFREG)));
    EXPECT_FALSE(rule.matches(makeFile("a", S_IFDIR)));
}

TEST(FilterRuleTest, TimeCtime) {
    FilterRule rule(FilterDimension::CTIME, FilterAction::INCLUDE,
                    TimeOp::AFTER, 1000);
    EXPECT_TRUE(rule.matches(makeFile("a", S_IFREG, 100, 1500)));
    EXPECT_FALSE(rule.matches(makeFile("b", S_IFREG, 100, 500)));
}

TEST(FilterRuleTest, TimeBoundaryBetween) {
    // BETWEEN is inclusive on both ends
    FilterRule rule(FilterDimension::MTIME, FilterAction::INCLUDE,
                    TimeOp::BETWEEN, 1000, 2000);
    EXPECT_TRUE(rule.matches(makeFile("a", S_IFREG, 100, 1000)));  // lower bound
    EXPECT_TRUE(rule.matches(makeFile("b", S_IFREG, 100, 2000)));  // upper bound
}

TEST(FilterRuleTest, SizeEQ) {
    FilterRule rule(FilterAction::INCLUDE, SizeOp::EQ, 500, 0);
    EXPECT_TRUE(rule.matches(makeFile("a", S_IFREG, 500)));
    EXPECT_FALSE(rule.matches(makeFile("b", S_IFREG, 501)));
    EXPECT_FALSE(rule.matches(makeFile("c", S_IFREG, 499)));
}

TEST(FilterRuleTest, SizeGE) {
    FilterRule rule(FilterAction::INCLUDE, SizeOp::GE, 500, 0);
    EXPECT_TRUE(rule.matches(makeFile("a", S_IFREG, 500)));
    EXPECT_TRUE(rule.matches(makeFile("b", S_IFREG, 1000)));
    EXPECT_FALSE(rule.matches(makeFile("c", S_IFREG, 499)));
}

TEST(FilterRuleTest, SizeLE) {
    FilterRule rule(FilterAction::INCLUDE, SizeOp::LE, 500, 0);
    EXPECT_TRUE(rule.matches(makeFile("a", S_IFREG, 500)));
    EXPECT_TRUE(rule.matches(makeFile("b", S_IFREG, 100)));
    EXPECT_FALSE(rule.matches(makeFile("c", S_IFREG, 501)));
}

TEST(FilterRuleTest, CloneDeepCopy) {
    FilterRule original(FilterDimension::NAME, FilterAction::EXCLUDE, "*.tmp");
    auto copy = original.clone();
    EXPECT_EQ(copy->dimension(), FilterDimension::NAME);
    EXPECT_EQ(copy->action(), FilterAction::EXCLUDE);
    EXPECT_TRUE(copy->matches(makeFile("src/a.tmp")));
    EXPECT_FALSE(copy->matches(makeFile("src/a.cpp")));
    // Original still works
    EXPECT_FALSE(original.matches(makeFile("src/a.cpp")));
}

TEST(FilterRuleTest, OwnerByNameExists) {
    // getpwnam("root") should exist on macOS/Linux
    FilterRule rule(FilterDimension::OWNER, FilterAction::INCLUDE,
                    std::string("root"), ByName{});
    uid_t rootUid = 0;  // root is typically uid 0
    EXPECT_TRUE(rule.matches(makeFile("a", S_IFREG, 100, 0, rootUid)));
    EXPECT_FALSE(rule.matches(makeFile("b", S_IFREG, 100, 0, 1000)));
}

TEST(FilterRuleTest, OwnerByNameNotExists) {
    FilterRule rule(FilterDimension::OWNER, FilterAction::INCLUDE,
                    std::string("__nonexistent_user_xyz__"), ByName{});
    // getpwnam fails → owner_ = -1, never matches any file
    EXPECT_FALSE(rule.matches(makeFile("a", S_IFREG, 100, 0, 1000)));
    EXPECT_FALSE(rule.matches(makeFile("a", S_IFREG, 100, 0, 0)));
}

TEST(FilterRuleTest, GroupByNameExists) {
    // getgrnam("wheel") or "staff" should exist on macOS
    FilterRule rule(FilterDimension::GROUP, FilterAction::INCLUDE,
                    std::string("wheel"), ByName{});
    // We just verify it doesn't crash; exact gid depends on system
    // At minimum, non-existent user's files shouldn't match
    EXPECT_FALSE(rule.matches(makeFile("a", S_IFREG, 100, 0, 1000, 99999)));
}

TEST(FilterRuleTest, GroupByNameNotExists) {
    FilterRule rule(FilterDimension::GROUP, FilterAction::INCLUDE,
                    std::string("__nonexistent_group_xyz__"), ByName{});
    // getgrnam fails → group_ = -1, never matches
    EXPECT_FALSE(rule.matches(makeFile("a", S_IFREG, 100, 0, 1000, 1000)));
}

// ===================== CompositeFilter: additional =====================

TEST(CompositeFilterTest, AssignmentOperator) {
    CompositeFilter filter;
    filter.addIncludeName("*.cpp");
    filter.addExcludePath("*/test/*");

    CompositeFilter copy;
    copy = filter;
    EXPECT_TRUE(copy.matches(makeFile("src/a.cpp")));
    EXPECT_FALSE(copy.matches(makeFile("src/test/a.cpp")));
}

TEST(CompositeFilterTest, ClearResetsToEmpty) {
    CompositeFilter filter;
    filter.addIncludeName("*.cpp");
    EXPECT_FALSE(filter.isEmpty());
    filter.clear();
    EXPECT_TRUE(filter.isEmpty());
    EXPECT_TRUE(filter.matches(makeFile("anything.txt")));
}

TEST(CompositeFilterTest, MoveConstructor) {
    CompositeFilter filter;
    filter.addIncludeName("*.cpp");
    CompositeFilter moved(std::move(filter));
    EXPECT_TRUE(moved.matches(makeFile("a.cpp")));
}

TEST(CompositeFilterTest, AtimeFilterWorks) {
    CompositeFilter filter;
    filter.addIncludeAtime(TimeOp::AFTER, 1000);
    EXPECT_TRUE(filter.matches(makeFile("recent", S_IFREG, 100, 1500)));
    EXPECT_FALSE(filter.matches(makeFile("old", S_IFREG, 100, 500)));
}

TEST(CompositeFilterTest, CtimeFilterWorks) {
    CompositeFilter filter;
    filter.addIncludeCtime(TimeOp::BEFORE, 1000);
    EXPECT_TRUE(filter.matches(makeFile("old", S_IFREG, 100, 500)));
    EXPECT_FALSE(filter.matches(makeFile("recent", S_IFREG, 100, 1500)));
}

TEST(CompositeFilterTest, SizeGEWorks) {
    CompositeFilter filter;
    filter.addIncludeSize(SizeOp::GE, 1000);
    EXPECT_TRUE(filter.matches(makeFile("big", S_IFREG, 1000)));
    EXPECT_TRUE(filter.matches(makeFile("bigger", S_IFREG, 2000)));
    EXPECT_FALSE(filter.matches(makeFile("small", S_IFREG, 500)));
}

TEST(CompositeFilterTest, SizeLEWorks) {
    CompositeFilter filter;
    filter.addIncludeSize(SizeOp::LE, 1000);
    EXPECT_TRUE(filter.matches(makeFile("small", S_IFREG, 500)));
    EXPECT_TRUE(filter.matches(makeFile("exact", S_IFREG, 1000)));
    EXPECT_FALSE(filter.matches(makeFile("big", S_IFREG, 2000)));
}

TEST(CompositeFilterTest, OwnerByUid) {
    CompositeFilter filter;
    filter.addIncludeOwner(static_cast<uid_t>(1000));
    EXPECT_TRUE(filter.matches(makeFile("a", S_IFREG, 100, 0, 1000)));
    EXPECT_FALSE(filter.matches(makeFile("b", S_IFREG, 100, 0, 2000)));
}

TEST(CompositeFilterTest, GroupByGid) {
    CompositeFilter filter;
    filter.addIncludeGroup(static_cast<gid_t>(1000));
    EXPECT_TRUE(filter.matches(makeFile("a", S_IFREG, 100, 0, 1000, 1000)));
    EXPECT_FALSE(filter.matches(makeFile("b", S_IFREG, 100, 0, 1000, 2000)));
}

TEST(CompositeFilterTest, ThreeDimensionAND) {
    CompositeFilter filter;
    filter.addIncludeName("*.cpp");
    filter.addIncludeSize(SizeOp::GE, 100);
    filter.addIncludePath("src/*");
    EXPECT_TRUE(filter.matches(makeFile("src/a.cpp", S_IFREG, 200)));
    EXPECT_FALSE(filter.matches(makeFile("src/a.h", S_IFREG, 200)));    // name fails
    EXPECT_FALSE(filter.matches(makeFile("src/a.cpp", S_IFREG, 10)));   // size fails
    EXPECT_FALSE(filter.matches(makeFile("lib/a.cpp", S_IFREG, 200)));  // path fails
}

TEST(CompositeFilterTest, MultipleExcludeDimensions) {
    CompositeFilter filter;
    filter.addExcludeName("*.tmp");
    filter.addExcludeSize(SizeOp::GT, 10000);
    filter.addExcludePath("*/build/*");
    // File that hits exclude in name
    EXPECT_FALSE(filter.matches(makeFile("src/a.tmp")));
    // File that hits exclude in size
    EXPECT_FALSE(filter.matches(makeFile("src/big.bin", S_IFREG, 20000)));
    // File that hits exclude in path
    EXPECT_FALSE(filter.matches(makeFile("src/build/output.o")));
    // Clean file passes
    EXPECT_TRUE(filter.matches(makeFile("src/clean.cpp")));
}

TEST(CompositeFilterTest, SameDimensionORMultipleRules) {
    CompositeFilter filter;
    filter.addIncludePath("src/*");
    filter.addIncludePath("lib/*");
    filter.addIncludePath("include/*");
    // OR: any path match passes
    EXPECT_TRUE(filter.matches(makeFile("src/a.cpp")));
    EXPECT_TRUE(filter.matches(makeFile("lib/b.cpp")));
    EXPECT_TRUE(filter.matches(makeFile("include/c.h")));
    EXPECT_FALSE(filter.matches(makeFile("test/d.cpp")));
}

// ===================== FilterParser: additional =====================

TEST(FilterParserTest, ParseSizeLE) {
    FilterParser parser;
    CompositeFilter filter;
    EXPECT_TRUE(parser.parseOne(filter, "include-size", "<=500"));
    EXPECT_TRUE(filter.matches(makeFile("small", S_IFREG, 500)));
    EXPECT_TRUE(filter.matches(makeFile("tiny", S_IFREG, 100)));
    EXPECT_FALSE(filter.matches(makeFile("big", S_IFREG, 501)));
}

TEST(FilterParserTest, ParseSizeGE) {
    FilterParser parser;
    CompositeFilter filter;
    EXPECT_TRUE(parser.parseOne(filter, "include-size", ">=1K"));
    EXPECT_TRUE(filter.matches(makeFile("big", S_IFREG, 1024)));
    EXPECT_TRUE(filter.matches(makeFile("bigger", S_IFREG, 2048)));
    EXPECT_FALSE(filter.matches(makeFile("small", S_IFREG, 500)));
}

TEST(FilterParserTest, ParseSizeExact) {
    FilterParser parser;
    CompositeFilter filter;
    EXPECT_TRUE(parser.parseOne(filter, "include-size", "=100"));
    EXPECT_TRUE(filter.matches(makeFile("a", S_IFREG, 100)));
    EXPECT_FALSE(filter.matches(makeFile("b", S_IFREG, 101)));
}

TEST(FilterParserTest, ParseSizeNoPrefix) {
    FilterParser parser;
    CompositeFilter filter;
    EXPECT_TRUE(parser.parseOne(filter, "include-size", "100"));
    EXPECT_TRUE(filter.matches(makeFile("a", S_IFREG, 100)));
    EXPECT_FALSE(filter.matches(makeFile("b", S_IFREG, 200)));
}

TEST(FilterParserTest, ParseSizeKMG) {
    FilterParser parser;

    {
        CompositeFilter filter;
        EXPECT_TRUE(parser.parseOne(filter, "include-size", ">1K"));
        EXPECT_TRUE(filter.matches(makeFile("big", S_IFREG, 2000)));
        EXPECT_FALSE(filter.matches(makeFile("small", S_IFREG, 500)));
    }
    {
        CompositeFilter filter;
        EXPECT_TRUE(parser.parseOne(filter, "include-size", ">1M"));
        EXPECT_TRUE(filter.matches(makeFile("big", S_IFREG, 2000000)));
        EXPECT_FALSE(filter.matches(makeFile("small", S_IFREG, 500)));
    }
    {
        CompositeFilter filter;
        EXPECT_TRUE(parser.parseOne(filter, "include-size", "<1G"));
        EXPECT_TRUE(filter.matches(makeFile("small", S_IFREG, 500)));
    }
}

TEST(FilterParserTest, ParseTimeCtime) {
    FilterParser parser;
    CompositeFilter filter;
    EXPECT_TRUE(parser.parseOne(filter, "include-ctime", "after:2024-06-01"));
    EXPECT_TRUE(filter.matches(makeFile("a", S_IFREG, 100, 1720000000)));
    EXPECT_FALSE(filter.matches(makeFile("b", S_IFREG, 100, 1700000000)));
}

TEST(FilterParserTest, ParseTimeAtime) {
    FilterParser parser;
    CompositeFilter filter;
    EXPECT_TRUE(parser.parseOne(filter, "include-atime", "before:2025-01-01"));
    EXPECT_TRUE(filter.matches(makeFile("a", S_IFREG, 100, 1700000000)));
    EXPECT_FALSE(filter.matches(makeFile("b", S_IFREG, 100, 1735689600 + 1000000)));
}

TEST(FilterParserTest, ParseGroupById) {
    FilterParser parser;
    CompositeFilter filter;
    EXPECT_TRUE(parser.parseOne(filter, "include-group", "1000"));
    EXPECT_TRUE(filter.matches(makeFile("a", S_IFREG, 100, 0, 1000, 1000)));
    EXPECT_FALSE(filter.matches(makeFile("b", S_IFREG, 100, 0, 1000, 2000)));
}

TEST(FilterParserTest, ParseGroupByName) {
    FilterParser parser;
    CompositeFilter filter;
    EXPECT_TRUE(parser.parseOne(filter, "exclude-group", "wheel"));
    // At minimum doesn't crash; exact match depends on system
    EXPECT_FALSE(filter.isEmpty());
}

TEST(FilterParserTest, ParseOwnerByName) {
    FilterParser parser;
    CompositeFilter filter;
    EXPECT_TRUE(parser.parseOne(filter, "include-owner", "root"));
    EXPECT_FALSE(filter.isEmpty());
    // root has uid 0
    EXPECT_TRUE(filter.matches(makeFile("a", S_IFREG, 100, 0, 0)));
    EXPECT_FALSE(filter.matches(makeFile("b", S_IFREG, 100, 0, 1000)));
}

TEST(FilterParserTest, ParseFromArgv) {
    FilterParser parser;
    CompositeFilter filter;

    // Test parseOne interop (full parse() requires char* argv[],
    // but parseOne covers all the parsing logic dimension-by-dimension)
    parser.parseOne(filter, "include-name", "*.cpp");
    parser.parseOne(filter, "exclude-path", "*/test/*");

    EXPECT_TRUE(filter.matches(makeFile("src/a.cpp")));
    EXPECT_FALSE(filter.matches(makeFile("src/test/a.cpp")));
}

TEST(FilterParserTest, UnknownDimensionIgnored) {
    FilterParser parser;
    CompositeFilter filter;
    EXPECT_FALSE(parser.parseOne(filter, "include-unknown", "value"));
    EXPECT_TRUE(filter.isEmpty());
}

TEST(FilterParserTest, EmptyValueRejected) {
    FilterParser parser;
    CompositeFilter filter;
    EXPECT_FALSE(parser.parseOne(filter, "include-name", ""));
    EXPECT_TRUE(filter.isEmpty());
}

TEST(FilterParserTest, InvalidTimeFormat) {
    FilterParser parser;
    CompositeFilter filter;
    // missing colon
    EXPECT_FALSE(parser.parseOne(filter, "include-mtime", "after2024-01-01"));
    EXPECT_TRUE(filter.isEmpty());
}

TEST(FilterParserTest, InvalidTimeOp) {
    FilterParser parser;
    CompositeFilter filter;
    EXPECT_FALSE(parser.parseOne(filter, "include-mtime", "unknown:2024-01-01"));
    EXPECT_TRUE(filter.isEmpty());
}

TEST(FilterParserTest, TimeBetweenMissingComma) {
    FilterParser parser;
    CompositeFilter filter;
    EXPECT_FALSE(parser.parseOne(filter, "include-mtime", "between:2024-01-012024-12-31"));
    EXPECT_TRUE(filter.isEmpty());
}

TEST(FilterParserTest, IncludeExcludeTypeOther) {
    FilterParser parser;
    CompositeFilter filter;
    EXPECT_TRUE(parser.parseOne(filter, "exclude-type", "l"));
    EXPECT_FALSE(filter.matches(makeFile("link", S_IFLNK)));
    EXPECT_TRUE(filter.matches(makeFile("file", S_IFREG)));
}

// ===================== FileScanner with Filter =====================

TEST(FileScannerFilterTest, ScanWithNameFilter) {
    std::string tdir = tmpDir();
    writeFile(tdir + "/keep.cpp", "cpp content");
    writeFile(tdir + "/skip.h", "header content");
    writeFile(tdir + "/skip.tmp", "temp");

    CompositeFilter filter;
    filter.addIncludeName("*.cpp");

    FileScanner scanner;
    scanner.setFilter(&filter);
    auto files = scanner.scan(tdir);

    bool hasKeep = false, hasSkip = false;
    for (const auto& f : files) {
        if (f.relativePath == "keep.cpp") hasKeep = true;
        if (f.relativePath == "skip.h") hasSkip = true;
    }
    EXPECT_TRUE(hasKeep);
    EXPECT_FALSE(hasSkip);
    rmrf(tdir);
}

TEST(FileScannerFilterTest, ScanWithExcludeName) {
    std::string tdir = tmpDir();
    writeFile(tdir + "/keep.txt", "keep");
    writeFile(tdir + "/junk.tmp", "junk");
    writeFile(tdir + "/junk.log", "log");

    CompositeFilter filter;
    filter.addExcludeName("*.tmp");
    filter.addExcludeName("*.log");

    FileScanner scanner;
    scanner.setFilter(&filter);
    auto files = scanner.scan(tdir);

    int count = 0;
    for (const auto& f : files) {
        if (f.isRegular()) count++;
    }
    EXPECT_EQ(count, 1);
    rmrf(tdir);
}

TEST(FileScannerFilterTest, ScanWithFilterRespectsNestedDirs) {
    std::string tdir = tmpDir();
    writeFile(tdir + "/main.cpp", "main");
    writeFile(tdir + "/sub/deep.cpp", "deep");
    writeFile(tdir + "/sub/skip.h", "skip");
    writeFile(tdir + "/sub/deep/skip.txt", "skip");

    CompositeFilter filter;
    filter.addIncludeName("*.cpp");  // only .cpp files

    FileScanner scanner;
    scanner.setFilter(&filter);
    auto files = scanner.scan(tdir);

    bool hasMain = false, hasDeep = false;
    bool hasSkipH = false, hasSkipTxt = false;
    for (const auto& f : files) {
        if (f.relativePath == "main.cpp") hasMain = true;
        if (f.relativePath == "sub/deep.cpp") hasDeep = true;
        if (f.relativePath == "sub/skip.h") hasSkipH = true;
        if (f.relativePath == "sub/deep/skip.txt") hasSkipTxt = true;
    }
    EXPECT_TRUE(hasMain);
    EXPECT_TRUE(hasDeep);
    EXPECT_FALSE(hasSkipH);
    EXPECT_FALSE(hasSkipTxt);
    rmrf(tdir);
}

TEST(FileScannerFilterTest, ScanWithPathExclude) {
    std::string tdir = tmpDir();
    writeFile(tdir + "/src/main.cpp", "main");
    writeFile(tdir + "/src/build/cache.o", "cache");
    writeFile(tdir + "/src/build/temp.o", "temp");
    writeFile(tdir + "/lib/util.cpp", "util");

    CompositeFilter filter;
    filter.addExcludePath("src/build/*");

    FileScanner scanner;
    scanner.setFilter(&filter);
    auto files = scanner.scan(tdir);

    bool hasMain = false, hasCache = false, hasUtil = false;
    for (const auto& f : files) {
        if (f.relativePath == "src/main.cpp") hasMain = true;
        if (f.relativePath == "src/build/cache.o") hasCache = true;
        if (f.relativePath == "lib/util.cpp") hasUtil = true;
    }
    EXPECT_TRUE(hasMain);
    EXPECT_FALSE(hasCache);
    EXPECT_TRUE(hasUtil);
    rmrf(tdir);
}

TEST(FileScannerFilterTest, ScanWithSizeFilter) {
    std::string tdir = tmpDir();
    writeFile(tdir + "/small.txt", "hi");
    std::string bigContent(10000, 'X');
    writeFile(tdir + "/big.txt", bigContent);

    CompositeFilter filter;
    filter.addExcludeSize(SizeOp::GT, 5000);

    FileScanner scanner;
    scanner.setFilter(&filter);
    auto files = scanner.scan(tdir);

    bool hasSmall = false, hasBig = false;
    for (const auto& f : files) {
        if (f.relativePath == "small.txt") hasSmall = true;
        if (f.relativePath == "big.txt") hasBig = true;
    }
    EXPECT_TRUE(hasSmall);
    EXPECT_FALSE(hasBig);
    rmrf(tdir);
}

TEST(FileScannerFilterTest, ScanNullFilterScansAll) {
    std::string tdir = tmpDir();
    writeFile(tdir + "/a.txt", "a");
    writeFile(tdir + "/b.tmp", "b");

    FileScanner scanner;
    // No filter set → scan everything
    auto files = scanner.scan(tdir);
    int count = 0;
    for (const auto& f : files) {
        if (f.isRegular()) count++;
    }
    EXPECT_EQ(count, 2);
    rmrf(tdir);
}

TEST(FileScannerFilterTest, ScanWithTypeFilter) {
    std::string tdir = tmpDir();
    writeFile(tdir + "/file.txt", "content");
    mkdir((tdir + "/mydir").c_str(), 0755);

    CompositeFilter filter;
    filter.addIncludeType('f');  // only regular files

    FileScanner scanner;
    scanner.setFilter(&filter);
    auto files = scanner.scan(tdir);

    bool hasFile = false, hasDir = false;
    for (const auto& f : files) {
        if (f.relativePath == "file.txt") hasFile = true;
        if (f.relativePath == "mydir") hasDir = true;
    }
    EXPECT_TRUE(hasFile);
    EXPECT_FALSE(hasDir);  // directory filtered out
    rmrf(tdir);
}
