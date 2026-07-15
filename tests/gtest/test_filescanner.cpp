#include "gtest/gtest.h"
#include "core/FileScanner.h"
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>

namespace {
    std::string tmpDir() {
        std::string d = "/tmp/fs_gtest_" + std::to_string(getpid());
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
            mkdir(path.substr(0, pos).c_str(), 0755);
        }
        std::ofstream out(path, std::ios::binary);
        out.write(content.data(), static_cast<std::streamsize>(content.size()));
        out.close();
    }
}

TEST(FileScannerTest, ScanEmptyDirectory) {
    std::string tdir = tmpDir();
    FileScanner scanner;
    auto files = scanner.scan(tdir);
    EXPECT_TRUE(files.empty());
    rmrf(tdir);
}

TEST(FileScannerTest, ScanSingleFile) {
    std::string tdir = tmpDir();
    writeFile(tdir + "/hello.txt", "Hello World");
    FileScanner scanner;
    auto files = scanner.scan(tdir);
    ASSERT_GE(files.size(), 1u);
    bool found = false;
    for (const auto& f : files) {
        if (f.relativePath == "hello.txt") {
            found = true;
            EXPECT_TRUE(f.isRegular());
            EXPECT_EQ(f.content.size(), 11u);
            break;
        }
    }
    EXPECT_TRUE(found);
    rmrf(tdir);
}

TEST(FileScannerTest, ScanNestedDirectories) {
    std::string tdir = tmpDir();
    writeFile(tdir + "/a.txt", "a");
    writeFile(tdir + "/sub/b.txt", "b");
    writeFile(tdir + "/sub/deep/c.txt", "c");

    FileScanner scanner;
    auto files = scanner.scan(tdir);
    EXPECT_GE(files.size(), 5u); // 3 files + 2 dirs

    int dirs = 0, regulars = 0;
    for (const auto& f : files) {
        if (f.isDirectory()) dirs++;
        if (f.isRegular()) regulars++;
    }
    EXPECT_EQ(regulars, 3);
    EXPECT_GE(dirs, 2);
    rmrf(tdir);
}

TEST(FileScannerTest, FileInfoHasPermissions) {
    std::string tdir = tmpDir();
    std::string filePath = tdir + "/perm.txt";
    writeFile(filePath, "test");
    chmod(filePath.c_str(), 0644);

    FileScanner scanner;
    auto files = scanner.scan(tdir);
    for (const auto& f : files) {
        if (f.relativePath == "perm.txt" && f.isRegular()) {
            EXPECT_EQ(f.permissions & 0777, 0644u);
        }
    }
    rmrf(tdir);
}

TEST(FileScannerTest, FileInfoHasTimestamps) {
    std::string tdir = tmpDir();
    writeFile(tdir + "/time.txt", "timestamp");
    FileScanner scanner;
    auto files = scanner.scan(tdir);
    for (const auto& f : files) {
        if (f.relativePath == "time.txt") {
            EXPECT_GT(f.mtime, 0);
            EXPECT_GT(f.atime, 0);
            EXPECT_GT(f.ctime, 0);
        }
    }
    rmrf(tdir);
}

TEST(FileScannerTest, ScanSymlink) {
    std::string tdir = tmpDir();
    writeFile(tdir + "/target.txt", "target content");
    symlink((tdir + "/target.txt").c_str(), (tdir + "/link.txt").c_str());

    FileScanner scanner;
    auto files = scanner.scan(tdir);
    bool hasLink = false;
    for (const auto& f : files) {
        if (f.relativePath == "link.txt") {
            hasLink = true;
            EXPECT_TRUE(f.isSymlink());
            EXPECT_EQ(f.symlinkTarget, tdir + "/target.txt");
        }
    }
    EXPECT_TRUE(hasLink);
    rmrf(tdir);
}

TEST(FileScannerTest, ScanFifo) {
    std::string tdir = tmpDir();
    std::string fifoPath = tdir + "/mypipe";
    mkfifo(fifoPath.c_str(), 0644);

    FileScanner scanner;
    auto files = scanner.scan(tdir);
    bool hasFifo = false;
    for (const auto& f : files) {
        if (f.relativePath == "mypipe") {
            hasFifo = true;
            EXPECT_TRUE(f.isFifo());
        }
    }
    EXPECT_TRUE(hasFifo);
    rmrf(tdir);
}

TEST(FileScannerTest, ScanNonexistentDir) {
    FileScanner scanner;
    auto files = scanner.scan("/nonexistent_directory_xyz_12345");
    EXPECT_TRUE(files.empty());
}

TEST(FileScannerTest, EmptyFileContent) {
    std::string tdir = tmpDir();
    writeFile(tdir + "/empty.bin", "");
    FileScanner scanner;
    auto files = scanner.scan(tdir);
    for (const auto& f : files) {
        if (f.relativePath == "empty.bin") {
            EXPECT_TRUE(f.content.empty());
            EXPECT_EQ(f.fileSize, 0);
        }
    }
    rmrf(tdir);
}

TEST(FileScannerTest, FileSizes) {
    std::string tdir = tmpDir();
    writeFile(tdir + "/small.txt", "hi");
    writeFile(tdir + "/medium.txt", std::string(1000, 'X'));

    FileScanner scanner;
    auto files = scanner.scan(tdir);
    for (const auto& f : files) {
        if (f.relativePath == "small.txt") EXPECT_EQ(f.fileSize, 2);
        if (f.relativePath == "medium.txt") EXPECT_EQ(f.fileSize, 1000);
    }
    rmrf(tdir);
}
