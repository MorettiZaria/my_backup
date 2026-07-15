#include "gtest/gtest.h"
#include "pack/TarPackStrategy.h"
#include "pack/IndexPackStrategy.h"
#include "core/FileInfo.h"
#include <sys/stat.h>
#include <cstring>

// ===================== TarPackStrategy =====================

namespace {
    FileInfo makeRegularFile(const std::string& path, const std::vector<uint8_t>& content) {
        FileInfo fi;
        fi.relativePath = path;
        fi.fileType     = S_IFREG;
        fi.permissions  = 0644;
        fi.owner        = 1000;
        fi.group        = 1000;
        fi.fileSize     = static_cast<off_t>(content.size());
        fi.mtime        = 1000000;
        fi.atime        = 1000000;
        fi.ctime        = 1000000;
        fi.content      = content;
        return fi;
    }

    FileInfo makeDir(const std::string& path) {
        FileInfo fi;
        fi.relativePath = path;
        fi.fileType     = S_IFDIR;
        fi.permissions  = 0755;
        fi.owner        = 1000;
        fi.group        = 1000;
        fi.mtime        = 1000000;
        fi.atime        = 1000000;
        fi.ctime        = 1000000;
        return fi;
    }

    FileInfo makeSymlink(const std::string& path, const std::string& target) {
        FileInfo fi;
        fi.relativePath = path;
        fi.fileType     = S_IFLNK;
        fi.permissions  = 0777;
        fi.owner        = 1000;
        fi.group        = 1000;
        fi.symlinkTarget = target;
        fi.mtime        = 1000000;
        fi.atime        = 1000000;
        fi.ctime        = 1000000;
        return fi;
    }
}

TEST(TarPackStrategyTest, NameAndAlgoId) {
    TarPackStrategy tar;
    EXPECT_EQ(tar.name(), "tar");
    EXPECT_EQ(tar.algoId(), 1);
}

TEST(TarPackStrategyTest, PackEmpty) {
    TarPackStrategy tar;
    std::vector<uint8_t> result = tar.pack("/tmp", {});
    EXPECT_EQ(result.size(), 1024u); // 两个全零 512B 结束标记
}

TEST(TarPackStrategyTest, PackSingleFile) {
    TarPackStrategy tar;
    std::vector<FileInfo> files;
    files.push_back(makeRegularFile("hello.txt", {'H', 'e', 'l', 'l', 'o'}));

    auto packed = tar.pack("/tmp", files);
    // header(512) + data(5) + padding(507) + eof(1024) = 2048
    EXPECT_GE(packed.size(), 2048u);
}

TEST(TarPackStrategyTest, PackDirAndFile) {
    TarPackStrategy tar;
    std::vector<FileInfo> files;
    files.push_back(makeDir("mydir"));
    files.push_back(makeRegularFile("mydir/test.txt", {'A', 'B', 'C'}));

    auto packed = tar.pack("/tmp", files);
    EXPECT_GE(packed.size(), 2560u); // dir header(512) + file header(512) + data(3) + padding(509) + eof(1024) = 2560
}

TEST(TarPackStrategyTest, PackWithSymlink) {
    TarPackStrategy tar;
    std::vector<FileInfo> files;
    files.push_back(makeSymlink("mylink", "/usr/bin/python"));

    auto packed = tar.pack("/tmp", files);
    EXPECT_GE(packed.size(), 1536u);
}

TEST(TarPackStrategyTest, UnpackEmpty) {
    TarPackStrategy tar;
    std::vector<uint8_t> data(1024, 0); // 两个空块 = 结束标记
    std::vector<FileInfo> files;
    tar.unpack(data, "/tmp", files);
    EXPECT_TRUE(files.empty());
}

TEST(TarPackStrategyTest, UnpackSingleFile) {
    TarPackStrategy tar;
    std::vector<FileInfo> input;
    input.push_back(makeRegularFile("test.txt", {'X', 'Y', 'Z'}));
    auto packed = tar.pack("/tmp", input);

    std::vector<FileInfo> output;
    tar.unpack(packed, "/tmp/out", output);
    ASSERT_EQ(output.size(), 1u);
    EXPECT_EQ(output[0].relativePath, "test.txt");
    EXPECT_EQ(output[0].content, (std::vector<uint8_t>{'X', 'Y', 'Z'}));
    EXPECT_TRUE(output[0].isRegular());
}

TEST(TarPackStrategyTest, UnpackDirAndFile) {
    TarPackStrategy tar;
    std::vector<FileInfo> input;
    input.push_back(makeDir("dir"));
    input.push_back(makeRegularFile("dir/f.txt", {'1', '2', '3'}));

    auto packed = tar.pack("/tmp", input);
    std::vector<FileInfo> output;
    tar.unpack(packed, "/tmp/out", output);
    ASSERT_EQ(output.size(), 2u);
    EXPECT_EQ(output[0].relativePath, "dir");
    EXPECT_TRUE(output[0].isDirectory());
    EXPECT_EQ(output[1].relativePath, "dir/f.txt");
    EXPECT_TRUE(output[1].isRegular());
    EXPECT_EQ(output[1].content, (std::vector<uint8_t>{'1', '2', '3'}));
}

TEST(TarPackStrategyTest, UnpackSymlink) {
    TarPackStrategy tar;
    std::vector<FileInfo> input;
    input.push_back(makeSymlink("link", "/usr/bin/python"));

    auto packed = tar.pack("/tmp", input);
    std::vector<FileInfo> output;
    tar.unpack(packed, "/tmp/out", output);
    ASSERT_EQ(output.size(), 1u);
    EXPECT_EQ(output[0].relativePath, "link");
    EXPECT_TRUE(output[0].isSymlink());
    EXPECT_EQ(output[0].symlinkTarget, "/usr/bin/python");
}

TEST(TarPackStrategyTest, UnpackWithPadding) {
    TarPackStrategy tar;
    std::vector<FileInfo> input;
    input.push_back(makeRegularFile("a.txt", {'A'}));
    input.push_back(makeRegularFile("b.txt", {'B'}));

    auto packed = tar.pack("/tmp", input);
    std::vector<FileInfo> output;
    tar.unpack(packed, "/tmp/out", output);
    ASSERT_EQ(output.size(), 2u);
    EXPECT_EQ(output[0].content, (std::vector<uint8_t>{'A'}));
    EXPECT_EQ(output[1].content, (std::vector<uint8_t>{'B'}));
}

TEST(TarPackStrategyTest, RoundTripMultipleTypes) {
    TarPackStrategy tar;
    std::vector<FileInfo> input;
    input.push_back(makeDir("root"));
    input.push_back(makeSymlink("root/link", "target"));
    input.push_back(makeRegularFile("root/data.bin", {0x00, 0x01, 0x02, 0x03, 0xFF}));
    input.push_back(makeDir("root/sub"));

    auto packed = tar.pack("/tmp", input);
    std::vector<FileInfo> output;
    tar.unpack(packed, "/tmp/out", output);
    ASSERT_EQ(output.size(), 4u);
    EXPECT_EQ(output[0].relativePath, "root");
    EXPECT_TRUE(output[0].isDirectory());
    EXPECT_EQ(output[0].permissions, 0755u);
    EXPECT_EQ(output[1].relativePath, "root/link");
    EXPECT_TRUE(output[1].isSymlink());
    EXPECT_EQ(output[1].symlinkTarget, "target");
    EXPECT_EQ(output[2].relativePath, "root/data.bin");
    EXPECT_TRUE(output[2].isRegular());
    EXPECT_EQ(output[2].content, (std::vector<uint8_t>{0x00, 0x01, 0x02, 0x03, 0xFF}));
    EXPECT_EQ(output[3].relativePath, "root/sub");
    EXPECT_TRUE(output[3].isDirectory());
}

// ===================== IndexPackStrategy =====================

TEST(IndexPackStrategyTest, NameAndAlgoId) {
    IndexPackStrategy idx;
    EXPECT_EQ(idx.name(), "index");
    EXPECT_EQ(idx.algoId(), 2);
}

TEST(IndexPackStrategyTest, PackEmpty) {
    IndexPackStrategy idx;
    auto packed = idx.pack("/tmp", {});
    EXPECT_GE(packed.size(), 32u); // EOCD
}

TEST(IndexPackStrategyTest, PackSingleFile) {
    IndexPackStrategy idx;
    std::vector<FileInfo> files;
    files.push_back(makeRegularFile("test.bin", {0x01, 0x02, 0x03, 0x04}));

    auto packed = idx.pack("/tmp", files);
    EXPECT_GE(packed.size(), 36u);
}

TEST(IndexPackStrategyTest, PackDirAndFile) {
    IndexPackStrategy idx;
    std::vector<FileInfo> files;
    files.push_back(makeDir("ddir"));
    files.push_back(makeRegularFile("ddir/file.txt", {'H', 'i'}));

    auto packed = idx.pack("/tmp", files);
    EXPECT_GE(packed.size(), 64u);
}

TEST(IndexPackStrategyTest, UnpackEmpty) {
    IndexPackStrategy idx;
    auto packed = idx.pack("/tmp", {});
    std::vector<FileInfo> output;
    idx.unpack(packed, "/tmp/out", output);
    EXPECT_TRUE(output.empty());
}

TEST(IndexPackStrategyTest, UnpackSingleFile) {
    IndexPackStrategy idx;
    std::vector<FileInfo> input;
    input.push_back(makeRegularFile("data.dat", {0xAA, 0xBB, 0xCC}));

    auto packed = idx.pack("/tmp", input);
    std::vector<FileInfo> output;
    idx.unpack(packed, "/tmp/out", output);
    ASSERT_EQ(output.size(), 1u);
    EXPECT_EQ(output[0].relativePath, "data.dat");
    EXPECT_TRUE(output[0].isRegular());
    EXPECT_EQ(output[0].content, (std::vector<uint8_t>{0xAA, 0xBB, 0xCC}));
}

TEST(IndexPackStrategyTest, UnpackDirAndFile) {
    IndexPackStrategy idx;
    std::vector<FileInfo> input;
    input.push_back(makeDir("pkg"));
    input.push_back(makeRegularFile("pkg/a.txt", {'A', 'B'}));

    auto packed = idx.pack("/tmp", input);
    std::vector<FileInfo> output;
    idx.unpack(packed, "/tmp/out", output);
    ASSERT_EQ(output.size(), 2u);
    EXPECT_EQ(output[0].relativePath, "pkg");
    EXPECT_TRUE(output[0].isDirectory());
    EXPECT_EQ(output[1].relativePath, "pkg/a.txt");
    EXPECT_TRUE(output[1].isRegular());
    EXPECT_EQ(output[1].content, (std::vector<uint8_t>{'A', 'B'}));
}

TEST(IndexPackStrategyTest, UnpackWithSymlink) {
    IndexPackStrategy idx;
    std::vector<FileInfo> input;
    input.push_back(makeSymlink("sLink", "/target/path"));

    auto packed = idx.pack("/tmp", input);
    std::vector<FileInfo> output;
    idx.unpack(packed, "/tmp/out", output);
    ASSERT_EQ(output.size(), 1u);
    EXPECT_EQ(output[0].relativePath, "sLink");
    EXPECT_TRUE(output[0].isSymlink());
    EXPECT_EQ(output[0].symlinkTarget, "/target/path");
}

TEST(IndexPackStrategyTest, UnpackTooSmall) {
    IndexPackStrategy idx;
    std::vector<uint8_t> small(10, 0);
    std::vector<FileInfo> output;
    idx.unpack(small, "/tmp", output);
    EXPECT_TRUE(output.empty());
}

TEST(IndexPackStrategyTest, RoundTripMultipleFiles) {
    IndexPackStrategy idx;
    std::vector<FileInfo> input;
    input.push_back(makeDir("work"));
    input.push_back(makeSymlink("work/config", "/etc/app.cfg"));
    input.push_back(makeRegularFile("work/log.txt", {'L', 'O', 'G'}));

    auto packed = idx.pack("/tmp", input);
    std::vector<FileInfo> output;
    idx.unpack(packed, "/tmp/out", output);
    ASSERT_EQ(output.size(), 3u);
    EXPECT_EQ(output[0].relativePath, "work");
    EXPECT_TRUE(output[0].isDirectory());
    EXPECT_EQ(output[1].relativePath, "work/config");
    EXPECT_TRUE(output[1].isSymlink());
    EXPECT_EQ(output[1].symlinkTarget, "/etc/app.cfg");
    EXPECT_EQ(output[2].relativePath, "work/log.txt");
    EXPECT_TRUE(output[2].isRegular());
    EXPECT_EQ(output[2].content, (std::vector<uint8_t>{'L', 'O', 'G'}));
}

TEST(IndexPackStrategyTest, MetadataFieldsPreserved) {
    IndexPackStrategy idx;
    FileInfo orig;
    orig.relativePath = "meta.txt";
    orig.fileType     = S_IFREG;
    orig.permissions  = 0600;
    orig.owner        = 1001;
    orig.group        = 1002;
    orig.mtime        = 9999999;
    orig.atime        = 8888888;
    orig.ctime        = 7777777;
    orig.content      = {0x01, 0x02};

    auto packed = idx.pack("/tmp", {orig});
    std::vector<FileInfo> output;
    idx.unpack(packed, "/tmp/out", output);
    ASSERT_EQ(output.size(), 1u);
    EXPECT_EQ(output[0].permissions, 0600u);
    EXPECT_EQ(output[0].owner, 1001u);
    EXPECT_EQ(output[0].group, 1002u);
    EXPECT_EQ(output[0].mtime, 9999999);
}
