#include "gtest/gtest.h"
#include "metadata/MetadataStore.h"
#include "core/FileInfo.h"
#include <sstream>
#include <sys/stat.h>

TEST(MetadataStoreTest, SaveAndLoadHeaderRoundTrip) {
    std::vector<FileInfo> files;
    FileInfo f1;
    f1.relativePath = "test.txt";
    f1.fileType     = S_IFREG;
    f1.permissions  = 0644;
    f1.fileSize     = 100;
    f1.mtime        = 1000000;
    files.push_back(f1);

    FileInfo f2;
    f2.relativePath = "subdir";
    f2.fileType     = S_IFDIR;
    f2.permissions  = 0755;
    files.push_back(f2);

    uint32_t flags = 0;
    uint8_t packAlgo = 1;
    uint8_t compressAlgo = 2;
    uint8_t encryptAlgo = 0;

    // Save to stringstream
    MetadataStore store;
    std::stringstream ss(std::ios::in | std::ios::out | std::ios::binary);
    store.saveHeader(ss, files, flags, packAlgo, compressAlgo, encryptAlgo);

    // Read back
    ss.seekg(0);
    auto hdr = store.loadHeader(ss);
    EXPECT_EQ(hdr.magic, BackupHeader::MAGIC);
    EXPECT_EQ(hdr.version, BackupHeader::VERSION);
    EXPECT_EQ(hdr.flags, 0u);
    EXPECT_EQ(hdr.packAlgo, 1u);
    EXPECT_EQ(hdr.compressAlgo, 2u);
    EXPECT_EQ(hdr.encryptAlgo, 0u);
    EXPECT_GT(hdr.metaSize, 0u);

    auto loaded = store.loadMetadata(ss, hdr.metaSize);
    ASSERT_EQ(loaded.size(), 2u);
    EXPECT_EQ(loaded[0].relativePath, "test.txt");
    EXPECT_TRUE(loaded[0].isRegular());
    EXPECT_EQ(loaded[1].relativePath, "subdir");
    EXPECT_TRUE(loaded[1].isDirectory());
}

TEST(MetadataStoreTest, SaveWithAllFlags) {
    std::vector<FileInfo> files;
    FileInfo f;
    f.relativePath = "data.bin";
    f.fileType     = S_IFREG;
    f.permissions  = 0600;
    files.push_back(f);

    MetadataStore store;
    std::stringstream ss(std::ios::in | std::ios::out | std::ios::binary);
    store.saveHeader(ss, files, 0x07, 2, 1, 2);

    ss.seekg(0);
    auto hdr = store.loadHeader(ss);
    EXPECT_EQ(hdr.flags, 0x07u);
    EXPECT_EQ(hdr.packAlgo, 2u);
    EXPECT_EQ(hdr.compressAlgo, 1u);
    EXPECT_EQ(hdr.encryptAlgo, 2u);
}

TEST(MetadataStoreTest, LoadInvalidMagic) {
    MetadataStore store;
    // Write bad magic
    std::stringstream ss(std::ios::in | std::ios::out | std::ios::binary);
    uint32_t badMagic = 0xDEADBEEF;
    ss.write(reinterpret_cast<const char*>(&badMagic), 4);
    // Write rest of header
    uint32_t ver = 1;
    ss.write(reinterpret_cast<const char*>(&ver), 4);

    ss.seekg(0);
    auto hdr = store.loadHeader(ss);
    EXPECT_NE(hdr.magic, BackupHeader::MAGIC);
}

TEST(MetadataStoreTest, LoadInvalidVersion) {
    MetadataStore store;
    std::stringstream ss(std::ios::in | std::ios::out | std::ios::binary);
    uint32_t magic = BackupHeader::MAGIC;
    uint32_t badVer = 999;
    ss.write(reinterpret_cast<const char*>(&magic), 4);
    ss.write(reinterpret_cast<const char*>(&badVer), 4);

    ss.seekg(0);
    auto hdr = store.loadHeader(ss);
    EXPECT_NE(hdr.version, BackupHeader::VERSION);
}

TEST(MetadataStoreTest, LoadMetadataEmpty) {
    MetadataStore store;
    std::stringstream ss(std::ios::in | std::ios::out | std::ios::binary);

    // Write header with metaSize = 4 (just the file count of 0)
    uint32_t magic = BackupHeader::MAGIC;
    uint32_t version = BackupHeader::VERSION;
    uint32_t flags = 0;
    uint8_t pa = 0, ca = 0, ea = 0;
    uint32_t metaSize = 4;  // just 4 bytes for file count = 0

    ss.write(reinterpret_cast<const char*>(&magic), 4);
    ss.write(reinterpret_cast<const char*>(&version), 4);
    ss.write(reinterpret_cast<const char*>(&flags), 4);
    ss.write(reinterpret_cast<const char*>(&pa), 1);
    ss.write(reinterpret_cast<const char*>(&ca), 1);
    ss.write(reinterpret_cast<const char*>(&ea), 1);
    ss.write(reinterpret_cast<const char*>(&metaSize), 4);

    // Write metadata: file count = 0
    uint32_t count = 0;
    ss.write(reinterpret_cast<const char*>(&count), 4);

    ss.seekg(0);
    auto hdr = store.loadHeader(ss);
    auto loaded = store.loadMetadata(ss, hdr.metaSize);
    EXPECT_TRUE(loaded.empty());
}

TEST(MetadataStoreTest, SaveEmptyFileList) {
    MetadataStore store;
    std::stringstream ss(std::ios::in | std::ios::out | std::ios::binary);
    store.saveHeader(ss, {}, 0, 0, 0, 0);

    ss.seekg(0);
    auto hdr = store.loadHeader(ss);
    EXPECT_EQ(hdr.magic, BackupHeader::MAGIC);
    auto loaded = store.loadMetadata(ss, hdr.metaSize);
    EXPECT_TRUE(loaded.empty());
}

TEST(MetadataStoreTest, MultipleFiles) {
    std::vector<FileInfo> files;
    for (int i = 0; i < 5; ++i) {
        FileInfo f;
        f.relativePath = "file_" + std::to_string(i) + ".dat";
        f.fileType     = S_IFREG;
        f.permissions  = static_cast<mode_t>(0600 + i);
        files.push_back(f);
    }

    MetadataStore store;
    std::stringstream ss(std::ios::in | std::ios::out | std::ios::binary);
    store.saveHeader(ss, files, 0, 0, 0, 0);
    ss.seekg(0);
    auto hdr = store.loadHeader(ss);
    auto loaded = store.loadMetadata(ss, hdr.metaSize);
    ASSERT_EQ(loaded.size(), 5u);
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(loaded[i].relativePath, "file_" + std::to_string(i) + ".dat");
    }
}
