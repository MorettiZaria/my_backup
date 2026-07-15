#include "gtest/gtest.h"
#include "core/BackupEngine.h"
#include "core/RestoreEngine.h"
#include "pack/TarPackStrategy.h"
#include "pack/IndexPackStrategy.h"
#include "pack/PackManager.h"
#include "compress/RleCompressStrategy.h"
#include "compress/HuffmanCompressStrategy.h"
#include "compress/CompressManager.h"
#include "encrypt/XorEncryptStrategy.h"
#include "encrypt/VigenereEncryptStrategy.h"
#include "encrypt/EncryptManager.h"
#include "core/FileInfo.h"
#include "metadata/MetadataSerializer.h"
#include <fstream>
#include <sys/stat.h>

namespace {
    // Helper: create temp directory and clean up
    std::string tmpDir() {
        std::string d = "/tmp/backup_gtest_" + std::to_string(getpid());
        mkdir(d.c_str(), 0755);
        return d;
    }

    void rmrf(const std::string& path) {
        // Simple cleanup
        std::string cmd = "rm -rf " + path + " 2>/dev/null";
        (void)system(cmd.c_str());
    }

    void writeFile(const std::string& path, const std::string& content) {
        // Ensure parent dir exists
        size_t pos = path.rfind('/');
        if (pos != std::string::npos) {
            mkdir(path.substr(0, pos).c_str(), 0755);
        }
        std::ofstream out(path, std::ios::binary);
        out.write(content.data(), static_cast<std::streamsize>(content.size()));
        out.close();
    }
}

// ===================== BackupEngine =====================

TEST(BackupEngineTest, RawBackupNoStrategies) {
    std::string src = tmpDir() + "/test_src";
    std::string dst = tmpDir() + "/out.bak";
    rmrf(src); rmrf(dst);
    writeFile(src + "/a.txt", "hello");
    writeFile(src + "/sub/b.txt", "world");

    BackupEngine engine;
    EXPECT_TRUE(engine.run(src, dst));

    // Verify output file exists
    std::ifstream check(dst);
    EXPECT_TRUE(check.good());

    rmrf(tmpDir());
}

TEST(BackupEngineTest, BackupWithPackTar) {
    std::string src = tmpDir() + "/test_src2";
    std::string dst = tmpDir() + "/out_tar.bak";
    rmrf(src); rmrf(dst);
    writeFile(src + "/x.txt", "content");

    TarPackStrategy tar;
    BackupEngine engine;
    engine.setPackStrategy(&tar);
    EXPECT_TRUE(engine.run(src, dst));

    std::ifstream check(dst);
    EXPECT_TRUE(check.good());

    rmrf(tmpDir());
}

TEST(BackupEngineTest, BackupWithPackIndex) {
    std::string src = tmpDir() + "/test_src3";
    std::string dst = tmpDir() + "/out_idx.bak";
    rmrf(src); rmrf(dst);
    writeFile(src + "/y.txt", "index test");

    IndexPackStrategy idx;
    BackupEngine engine;
    engine.setPackStrategy(&idx);
    EXPECT_TRUE(engine.run(src, dst));

    std::ifstream check(dst);
    EXPECT_TRUE(check.good());

    rmrf(tmpDir());
}

TEST(BackupEngineTest, BackupWithCompress) {
    std::string src = tmpDir() + "/test_src4";
    std::string dst = tmpDir() + "/out_cmp.bak";
    rmrf(src); rmrf(dst);
    writeFile(src + "/data.txt", std::string(500, 'A'));

    TarPackStrategy tar;
    RleCompressStrategy rle;
    BackupEngine engine;
    engine.setPackStrategy(&tar);
    engine.setCompressStrategy(&rle);
    EXPECT_TRUE(engine.run(src, dst));

    std::ifstream check(dst);
    EXPECT_TRUE(check.good());

    rmrf(tmpDir());
}

TEST(BackupEngineTest, BackupWithEncrypt) {
    std::string src = tmpDir() + "/test_src5";
    std::string dst = tmpDir() + "/out_enc.bak";
    rmrf(src); rmrf(dst);
    writeFile(src + "/secret.txt", "top secret data");

    TarPackStrategy tar;
    XorEncryptStrategy x;
    BackupEngine engine;
    engine.setPackStrategy(&tar);
    engine.setEncryptStrategy(&x);
    EXPECT_TRUE(engine.run(src, dst, "mypassword"));

    std::ifstream check(dst);
    EXPECT_TRUE(check.good());

    rmrf(tmpDir());
}

TEST(BackupEngineTest, BackupFullPipeline) {
    std::string src = tmpDir() + "/test_src6";
    std::string dst = tmpDir() + "/out_full.bak";
    rmrf(src); rmrf(dst);
    writeFile(src + "/sub/f.txt", std::string(300, 'B'));

    IndexPackStrategy idx;
    HuffmanCompressStrategy huff;
    VigenereEncryptStrategy vig;
    BackupEngine engine;
    engine.setPackStrategy(&idx);
    engine.setCompressStrategy(&huff);
    engine.setEncryptStrategy(&vig);
    EXPECT_TRUE(engine.run(src, dst, "password123"));

    std::ifstream check(dst);
    EXPECT_TRUE(check.good());

    rmrf(tmpDir());
}

TEST(BackupEngineTest, OutputFileCannotBeCreated) {
    BackupEngine engine;
    EXPECT_FALSE(engine.run("/nonexistent_src_dir_xyz", "/dev/null/invalid/file.bak"));
}

TEST(BackupEngineTest, BackupEmptyDir) {
    std::string src = tmpDir() + "/empty_src";
    std::string dst = tmpDir() + "/empty.bak";
    rmrf(src); rmrf(dst);
    mkdir(src.c_str(), 0755);

    BackupEngine engine;
    EXPECT_TRUE(engine.run(src, dst));

    std::ifstream check(dst);
    EXPECT_TRUE(check.good());

    rmrf(tmpDir());
}

// ===================== RestoreEngine =====================

TEST(RestoreEngineTest, RestoreFileNotFound) {
    RestoreEngine engine;
    EXPECT_FALSE(engine.run("/nonexistent_bak_file_12345.bak", "/tmp/out"));
}

TEST(RestoreEngineTest, RestoreInvalidMagic) {
    std::string bak = tmpDir() + "/bad.bak";
    rmrf(bak);
    {
        std::ofstream out(bak, std::ios::binary);
        uint32_t badMagic = 0xDEADBEEF;
        out.write(reinterpret_cast<const char*>(&badMagic), 4);
        // Write enough zeros so the RestoreEngine has something to read
        for (int i = 0; i < 100; ++i) {
            uint32_t z = 0;
            out.write(reinterpret_cast<const char*>(&z), 4);
        }
    }

    RestoreEngine engine;
    EXPECT_FALSE(engine.run(bak, tmpDir() + "/out_invalid"));

    rmrf(tmpDir());
}

TEST(RestoreEngineTest, RestoreRawFormat) {
    std::string src = tmpDir() + "/raw_src";
    std::string dst = tmpDir() + "/raw.bak";
    std::string out = tmpDir() + "/raw_out";
    rmrf(src); rmrf(dst); rmrf(out);

    writeFile(src + "/hello.txt", "world");

    BackupEngine backup;
    EXPECT_TRUE(backup.run(src, dst));

    RestoreEngine restore;
    EXPECT_TRUE(restore.run(dst, out));

    // Verify file was restored
    std::string restoredPath = out;
    struct stat st;
    EXPECT_EQ(stat(restoredPath.c_str(), &st), 0);

    rmrf(tmpDir());
}

TEST(RestoreEngineTest, RestoreTarFormat) {
    std::string src = tmpDir() + "/tar_src";
    std::string dst = tmpDir() + "/tar.bak";
    std::string out = tmpDir() + "/tar_out";
    rmrf(src); rmrf(dst); rmrf(out);

    writeFile(src + "/f.txt", "tar content");

    TarPackStrategy tar;
    BackupEngine backup;
    backup.setPackStrategy(&tar);
    EXPECT_TRUE(backup.run(src, dst));

    PackManager pmgr;
    pmgr.registerStrategy(std::make_unique<TarPackStrategy>());
    pmgr.registerStrategy(std::make_unique<IndexPackStrategy>());

    RestoreEngine restore;
    restore.setPackManager(&pmgr);
    EXPECT_TRUE(restore.run(dst, out));

    struct stat st;
    EXPECT_EQ(stat(out.c_str(), &st), 0);

    rmrf(tmpDir());
}

TEST(RestoreEngineTest, RestoreWithCompress) {
    std::string src = tmpDir() + "/cmp_src";
    std::string dst = tmpDir() + "/cmp.bak";
    std::string out = tmpDir() + "/cmp_out";
    rmrf(src); rmrf(dst); rmrf(out);

    writeFile(src + "/g.txt", std::string(500, 'C'));

    TarPackStrategy tar;
    RleCompressStrategy rle;
    BackupEngine backup;
    backup.setPackStrategy(&tar);
    backup.setCompressStrategy(&rle);
    EXPECT_TRUE(backup.run(src, dst));

    PackManager pmgr;
    pmgr.registerStrategy(std::make_unique<TarPackStrategy>());
    CompressManager cmgr;
    cmgr.registerStrategy(std::make_unique<RleCompressStrategy>());
    cmgr.registerStrategy(std::make_unique<HuffmanCompressStrategy>());

    RestoreEngine restore;
    restore.setPackManager(&pmgr);
    restore.setCompressManager(&cmgr);
    EXPECT_TRUE(restore.run(dst, out));

    rmrf(tmpDir());
}

TEST(RestoreEngineTest, RestoreWithEncrypt) {
    std::string src = tmpDir() + "/enc_src";
    std::string dst = tmpDir() + "/enc.bak";
    std::string out = tmpDir() + "/enc_out";
    rmrf(src); rmrf(dst); rmrf(out);

    writeFile(src + "/secret.bin", "classified");

    TarPackStrategy tar;
    XorEncryptStrategy x;
    BackupEngine backup;
    backup.setPackStrategy(&tar);
    backup.setEncryptStrategy(&x);
    EXPECT_TRUE(backup.run(src, dst, "p@ssw0rd"));

    PackManager pmgr;
    pmgr.registerStrategy(std::make_unique<TarPackStrategy>());
    EncryptManager emgr;
    emgr.registerStrategy(std::make_unique<XorEncryptStrategy>());

    RestoreEngine restore;
    restore.setPackManager(&pmgr);
    restore.setEncryptManager(&emgr);
    EXPECT_TRUE(restore.run(dst, out, "p@ssw0rd"));

    rmrf(tmpDir());
}

TEST(RestoreEngineTest, RestoreFullPipeline) {
    std::string src = tmpDir() + "/full_src";
    std::string dst = tmpDir() + "/full.bak";
    std::string out = tmpDir() + "/full_out";
    rmrf(src); rmrf(dst); rmrf(out);

    writeFile(src + "/nested/sub/file.bin", std::string(200, 'D'));

    IndexPackStrategy idx;
    HuffmanCompressStrategy huff;
    VigenereEncryptStrategy vig;
    BackupEngine backup;
    backup.setPackStrategy(&idx);
    backup.setCompressStrategy(&huff);
    backup.setEncryptStrategy(&vig);
    EXPECT_TRUE(backup.run(src, dst, "fullpass"));

    PackManager pmgr;
    pmgr.registerStrategy(std::make_unique<TarPackStrategy>());
    pmgr.registerStrategy(std::make_unique<IndexPackStrategy>());
    CompressManager cmgr;
    cmgr.registerStrategy(std::make_unique<RleCompressStrategy>());
    cmgr.registerStrategy(std::make_unique<HuffmanCompressStrategy>());
    EncryptManager emgr;
    emgr.registerStrategy(std::make_unique<XorEncryptStrategy>());
    emgr.registerStrategy(std::make_unique<VigenereEncryptStrategy>());

    RestoreEngine restore;
    restore.setPackManager(&pmgr);
    restore.setCompressManager(&cmgr);
    restore.setEncryptManager(&emgr);
    EXPECT_TRUE(restore.run(dst, out, "fullpass"));

    rmrf(tmpDir());
}

TEST(RestoreEngineTest, PackedButNoManager) {
    std::string src = tmpDir() + "/nopmgr_src";
    std::string dst = tmpDir() + "/nopmgr.bak";
    std::string out = tmpDir() + "/nopmgr_out";
    rmrf(src); rmrf(dst); rmrf(out);

    writeFile(src + "/x.txt", "test");

    TarPackStrategy tar;
    BackupEngine backup;
    backup.setPackStrategy(&tar);
    EXPECT_TRUE(backup.run(src, dst));

    RestoreEngine restore; // no PackManager set
    EXPECT_FALSE(restore.run(dst, out));

    rmrf(tmpDir());
}

TEST(RestoreEngineTest, CompressedButNoManager) {
    std::string src = tmpDir() + "/nocmgr_src";
    std::string dst = tmpDir() + "/nocmgr.bak";
    std::string out = tmpDir() + "/nocmgr_out";
    rmrf(src); rmrf(dst); rmrf(out);

    writeFile(src + "/x.txt", "test");

    TarPackStrategy tar;
    RleCompressStrategy rle;
    BackupEngine backup;
    backup.setPackStrategy(&tar);
    backup.setCompressStrategy(&rle);
    EXPECT_TRUE(backup.run(src, dst));

    PackManager pmgr;
    pmgr.registerStrategy(std::make_unique<TarPackStrategy>());
    RestoreEngine restore;
    restore.setPackManager(&pmgr);
    // no CompressManager
    EXPECT_FALSE(restore.run(dst, out));

    rmrf(tmpDir());
}
