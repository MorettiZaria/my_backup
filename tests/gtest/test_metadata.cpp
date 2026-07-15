#include "gtest/gtest.h"
#include "metadata/MetadataSerializer.h"
#include "core/FileInfo.h"
#include <sys/stat.h>
#include <cstring>
#include <string>
#include <vector>

// ===================== MetadataSerializer Tests =====================

TEST(MetadataSerializerTest, SerializeEmptyList) {
    MetadataSerializer serializer;
    std::vector<FileInfo> empty;
    std::vector<uint8_t> data = serializer.serialize(empty);
    std::vector<FileInfo> result = serializer.deserialize(data);
    EXPECT_TRUE(result.empty());
}

TEST(MetadataSerializerTest, SerializeSingleFile) {
    MetadataSerializer serializer;
    FileInfo original;
    original.relativePath = "test.txt";
    original.fileType     = S_IFREG;
    original.permissions  = 0644;
    original.owner        = 1000;
    original.group        = 1000;
    original.fileSize     = 5;
    original.atime        = 1000000;
    original.mtime        = 2000000;
    original.ctime        = 3000000;
    original.content      = {'h', 'e', 'l', 'l', 'o'};

    std::vector<uint8_t> data = serializer.serialize({original});
    std::vector<FileInfo> result = serializer.deserialize(data);

    ASSERT_EQ(result.size(), 1u);
    const FileInfo& info = result[0];
    EXPECT_EQ(info.relativePath, "test.txt");
    EXPECT_EQ(info.fileType, S_IFREG);
    EXPECT_EQ(info.permissions, 0644u);
    EXPECT_EQ(info.owner, 1000u);
    EXPECT_EQ(info.group, 1000u);
    EXPECT_EQ(info.fileSize, 5);
    EXPECT_EQ(info.atime, 1000000);
    EXPECT_EQ(info.mtime, 2000000);
    EXPECT_EQ(info.ctime, 3000000);
}

TEST(MetadataSerializerTest, SerializeMultipleFiles) {
    MetadataSerializer serializer;
    FileInfo f1, f2, f3;
    f1.relativePath = "a.txt"; f1.fileType = S_IFREG; f1.permissions = 0644; f1.fileSize = 10;
    f2.relativePath = "b.txt"; f2.fileType = S_IFREG; f2.permissions = 0600; f2.fileSize = 20;
    f3.relativePath = "subdir"; f3.fileType = S_IFDIR; f3.permissions = 0755;

    std::vector<uint8_t> data = serializer.serialize({f1, f2, f3});
    std::vector<FileInfo> result = serializer.deserialize(data);

    ASSERT_EQ(result.size(), 3u);
    EXPECT_EQ(result[0].relativePath, "a.txt");
    EXPECT_EQ(result[0].fileType, S_IFREG);
    EXPECT_EQ(result[1].relativePath, "b.txt");
    EXPECT_EQ(result[1].permissions, 0600u);
    EXPECT_EQ(result[2].relativePath, "subdir");
    EXPECT_EQ(result[2].fileType, S_IFDIR);
}

TEST(MetadataSerializerTest, SerializeSpecialFileTypes) {
    MetadataSerializer serializer;
    FileInfo dirInfo, symInfo, fifoInfo;
    dirInfo.relativePath = "mydir";     dirInfo.fileType = S_IFDIR;  dirInfo.permissions = 0755;
    symInfo.relativePath = "mylink";    symInfo.fileType = S_IFLNK;  symInfo.permissions = 0777;
    symInfo.symlinkTarget = "/usr/bin";
    fifoInfo.relativePath = "mypipe";   fifoInfo.fileType = S_IFIFO; fifoInfo.permissions = 0644;

    std::vector<uint8_t> data = serializer.serialize({dirInfo, symInfo, fifoInfo});
    std::vector<FileInfo> result = serializer.deserialize(data);

    ASSERT_EQ(result.size(), 3u);
    EXPECT_EQ(result[0].fileType, S_IFDIR);
    EXPECT_EQ(result[1].fileType, S_IFLNK);
    EXPECT_EQ(result[1].symlinkTarget, "/usr/bin");
    EXPECT_EQ(result[2].fileType, S_IFIFO);
}

TEST(MetadataSerializerTest, SerializeDeviceFiles) {
    MetadataSerializer serializer;
    FileInfo blk, chr;
    blk.relativePath = "blockdev";  blk.fileType = S_IFBLK;  blk.deviceId = 0x801;
    chr.relativePath = "chardev";   chr.fileType = S_IFCHR;  chr.deviceId = 0x100;

    std::vector<uint8_t> data = serializer.serialize({blk, chr});
    std::vector<FileInfo> result = serializer.deserialize(data);

    ASSERT_EQ(result.size(), 2u);
    EXPECT_EQ(result[0].fileType, S_IFBLK);
    EXPECT_EQ(result[0].deviceId, static_cast<dev_t>(0x801));
    EXPECT_EQ(result[1].fileType, S_IFCHR);
    EXPECT_EQ(result[1].deviceId, static_cast<dev_t>(0x100));
}

TEST(MetadataSerializerTest, SerializeRoundTripIntegrity) {
    MetadataSerializer serializer;
    std::vector<FileInfo> input;
    for (int i = 0; i < 5; ++i) {
        FileInfo fi;
        fi.relativePath = "file_" + std::to_string(i) + ".dat";
        fi.fileType     = S_IFREG;
        fi.permissions  = static_cast<mode_t>(0600 + i);
        fi.owner        = static_cast<uid_t>(1000 + i);
        fi.group        = static_cast<gid_t>(1000 + i);
        fi.fileSize     = static_cast<off_t>(i * 100);
        fi.atime        = static_cast<time_t>(i * 1000);
        fi.mtime        = static_cast<time_t>(i * 2000);
        fi.ctime        = static_cast<time_t>(i * 3000);
        input.push_back(fi);
    }

    std::vector<uint8_t> data = serializer.serialize(input);
    std::vector<FileInfo> result = serializer.deserialize(data);

    ASSERT_EQ(result.size(), input.size());
    for (size_t i = 0; i < input.size(); ++i) {
        EXPECT_EQ(result[i].relativePath, input[i].relativePath);
        EXPECT_EQ(result[i].fileType, input[i].fileType);
        EXPECT_EQ(result[i].permissions, input[i].permissions);
        EXPECT_EQ(result[i].owner, input[i].owner);
        EXPECT_EQ(result[i].group, input[i].group);
        EXPECT_EQ(result[i].fileSize, input[i].fileSize);
        EXPECT_EQ(result[i].atime, input[i].atime);
        EXPECT_EQ(result[i].mtime, input[i].mtime);
        EXPECT_EQ(result[i].ctime, input[i].ctime);
    }
}

TEST(MetadataSerializerTest, SerializeLargeContent) {
    MetadataSerializer serializer;
    FileInfo fi;
    fi.relativePath = "bigfile.bin";
    fi.fileType     = S_IFREG;
    fi.permissions  = 0644;
    fi.fileSize     = 10000;
    fi.content      = std::vector<uint8_t>(10000, 0xAB);

    std::vector<uint8_t> data = serializer.serialize({fi});
    std::vector<FileInfo> result = serializer.deserialize(data);

    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0].relativePath, "bigfile.bin");
    EXPECT_EQ(result[0].fileSize, 10000);
}

TEST(MetadataSerializerTest, EmptyPath) {
    MetadataSerializer serializer;
    FileInfo fi;
    fi.relativePath = "";
    fi.fileType     = S_IFREG;
    fi.permissions  = 0644;

    std::vector<uint8_t> data = serializer.serialize({fi});
    std::vector<FileInfo> result = serializer.deserialize(data);

    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0].relativePath, "");
}

TEST(MetadataSerializerTest, LongPath) {
    MetadataSerializer serializer;
    FileInfo fi;
    fi.relativePath = std::string(500, 'x');
    fi.fileType     = S_IFREG;
    fi.permissions  = 0644;

    std::vector<uint8_t> data = serializer.serialize({fi});
    std::vector<FileInfo> result = serializer.deserialize(data);

    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0].relativePath, fi.relativePath);
    EXPECT_EQ(result[0].relativePath.size(), 500u);
}

// ===================== FileInfo Tests =====================

TEST(FileInfoTest, DefaultConstructor) {
    FileInfo fi;
    EXPECT_TRUE(fi.relativePath.empty());
    EXPECT_EQ(fi.fileType, 0);
    EXPECT_EQ(fi.permissions, 0u);
    EXPECT_EQ(fi.owner, 0u);
    EXPECT_EQ(fi.group, 0u);
    EXPECT_EQ(fi.fileSize, 0);
    EXPECT_EQ(fi.atime, 0);
    EXPECT_EQ(fi.mtime, 0);
    EXPECT_EQ(fi.ctime, 0);
    EXPECT_TRUE(fi.symlinkTarget.empty());
    EXPECT_EQ(fi.deviceId, 0);
    EXPECT_TRUE(fi.content.empty());
}

TEST(FileInfoTest, FromStatRegularFile) {
    struct stat st{};
    st.st_mode  = S_IFREG | 0644;
    st.st_size  = 100;
    st.st_uid   = 1000;
    st.st_gid   = 1000;
    st.st_atime = 111111;
    st.st_mtime = 222222;
    st.st_ctime = 333333;
    st.st_dev   = 2049;
    st.st_ino   = 12345;

    FileInfo fi;
    fi.fromStat(st, "doc/readme.txt");

    EXPECT_EQ(fi.relativePath, "doc/readme.txt");
    EXPECT_EQ(fi.fileType & S_IFMT, static_cast<mode_t>(S_IFREG));
#if !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
    EXPECT_EQ(fi.permissions, 0644u);
#pragma GCC diagnostic pop
#else
    EXPECT_EQ(fi.permissions & 0777, 0644u);
#endif
    EXPECT_EQ(fi.fileSize, 100);
}

TEST(FileInfoTest, FromStatDirectory) {
    struct stat st{};
    st.st_mode  = S_IFDIR | 0755;
    st.st_size  = 4096;
    st.st_uid   = 0;
    st.st_gid   = 0;

    FileInfo fi;
    fi.fromStat(st, "subdir/");

    EXPECT_EQ(fi.fileType & S_IFMT, static_cast<mode_t>(S_IFDIR));
    EXPECT_TRUE(fi.isDirectory());
    EXPECT_FALSE(fi.isRegular());
    EXPECT_FALSE(fi.isSymlink());
}

TEST(FileInfoTest, FromStatSymlink) {
    struct stat st{};
    st.st_mode = S_IFLNK | 0777;

    FileInfo fi;
    fi.fromStat(st, "link");

    EXPECT_EQ(fi.fileType & S_IFMT, static_cast<mode_t>(S_IFLNK));
    EXPECT_TRUE(fi.isSymlink());
    EXPECT_FALSE(fi.isRegular());
}

TEST(FileInfoTest, FromStatFifo) {
    struct stat st{};
    st.st_mode = S_IFIFO | 0644;

    FileInfo fi;
    fi.fromStat(st, "pipe");

    EXPECT_EQ(fi.fileType & S_IFMT, static_cast<mode_t>(S_IFIFO));
    EXPECT_FALSE(fi.isDirectory());
    EXPECT_FALSE(fi.isRegular());
}

TEST(FileInfoTest, SerializeDeserializeRoundTrip) {
    FileInfo original;
    original.relativePath  = "data/test.bin";
    original.fileType      = S_IFREG;
    original.permissions   = 0644;
    original.owner         = 1000;
    original.group         = 1000;
    original.fileSize      = 500;
    original.atime         = 1000000;
    original.mtime         = 2000000;
    original.ctime         = 3000000;
    original.symlinkTarget = "";
    original.deviceId      = 0;

    std::vector<uint8_t> data = original.serialize();
    size_t offset = 0;
    FileInfo restored = FileInfo::deserialize(data.data(), offset);

    EXPECT_EQ(restored.relativePath, original.relativePath);
    EXPECT_EQ(restored.fileType, original.fileType);
    EXPECT_EQ(restored.permissions, original.permissions);
    EXPECT_EQ(restored.owner, original.owner);
    EXPECT_EQ(restored.group, original.group);
    EXPECT_EQ(restored.fileSize, original.fileSize);
    EXPECT_EQ(restored.atime, original.atime);
    EXPECT_EQ(restored.mtime, original.mtime);
    EXPECT_EQ(restored.ctime, original.ctime);
}

TEST(FileInfoTest, IsRegularIsDirectoryIsSymlink) {
    {
        FileInfo fi;
        fi.fileType = S_IFREG;
        EXPECT_TRUE(fi.isRegular());
        EXPECT_FALSE(fi.isDirectory());
        EXPECT_FALSE(fi.isSymlink());
    }
    {
        FileInfo fi;
        fi.fileType = S_IFDIR;
        EXPECT_FALSE(fi.isRegular());
        EXPECT_TRUE(fi.isDirectory());
        EXPECT_FALSE(fi.isSymlink());
    }
    {
        FileInfo fi;
        fi.fileType = S_IFLNK;
        EXPECT_FALSE(fi.isRegular());
        EXPECT_FALSE(fi.isDirectory());
        EXPECT_TRUE(fi.isSymlink());
    }
}
