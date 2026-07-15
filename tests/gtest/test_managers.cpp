#include "gtest/gtest.h"
#include "compress/CompressManager.h"
#include "compress/RleCompressStrategy.h"
#include "compress/HuffmanCompressStrategy.h"
#include "encrypt/EncryptManager.h"
#include "encrypt/XorEncryptStrategy.h"
#include "encrypt/VigenereEncryptStrategy.h"
#include "pack/PackManager.h"
#include "pack/TarPackStrategy.h"
#include "pack/IndexPackStrategy.h"

#include <memory>

// ===================== CompressManager =====================

TEST(CompressManagerTest, RegisterAndSelectByName) {
    CompressManager mgr;
    mgr.registerStrategy(std::make_unique<RleCompressStrategy>());
    mgr.registerStrategy(std::make_unique<HuffmanCompressStrategy>());
    EXPECT_NE(mgr.select("rle"), nullptr);
    EXPECT_NE(mgr.select("huffman"), nullptr);
    EXPECT_EQ(mgr.select("nonexistent"), nullptr);
    EXPECT_EQ(mgr.select(""), nullptr);
}

TEST(CompressManagerTest, RegisterAndSelectById) {
    CompressManager mgr;
    mgr.registerStrategy(std::make_unique<RleCompressStrategy>());
    mgr.registerStrategy(std::make_unique<HuffmanCompressStrategy>());
    EXPECT_NE(mgr.selectById(1), nullptr);
    EXPECT_NE(mgr.selectById(2), nullptr);
    EXPECT_EQ(mgr.selectById(0), nullptr);
    EXPECT_EQ(mgr.selectById(99), nullptr);
}

TEST(CompressManagerTest, ListNames) {
    CompressManager mgr;
    EXPECT_TRUE(mgr.listNames().empty());
    mgr.registerStrategy(std::make_unique<RleCompressStrategy>());
    mgr.registerStrategy(std::make_unique<HuffmanCompressStrategy>());
    auto names = mgr.listNames();
    EXPECT_EQ(names.size(), 2u);
    EXPECT_EQ(names[0], "rle");
    EXPECT_EQ(names[1], "huffman");
}

TEST(CompressManagerTest, EmptyManager) {
    CompressManager mgr;
    EXPECT_EQ(mgr.select("rle"), nullptr);
    EXPECT_EQ(mgr.selectById(1), nullptr);
    EXPECT_TRUE(mgr.listNames().empty());
}

// ===================== EncryptManager =====================

TEST(EncryptManagerTest, RegisterAndSelectByName) {
    EncryptManager mgr;
    mgr.registerStrategy(std::make_unique<XorEncryptStrategy>());
    mgr.registerStrategy(std::make_unique<VigenereEncryptStrategy>());
    EXPECT_NE(mgr.select("xor"), nullptr);
    EXPECT_NE(mgr.select("vigenere"), nullptr);
    EXPECT_EQ(mgr.select("nonexistent"), nullptr);
    EXPECT_EQ(mgr.select(""), nullptr);
}

TEST(EncryptManagerTest, RegisterAndSelectById) {
    EncryptManager mgr;
    mgr.registerStrategy(std::make_unique<XorEncryptStrategy>());
    mgr.registerStrategy(std::make_unique<VigenereEncryptStrategy>());
    EXPECT_NE(mgr.selectById(1), nullptr);
    EXPECT_NE(mgr.selectById(2), nullptr);
    EXPECT_EQ(mgr.selectById(0), nullptr);
    EXPECT_EQ(mgr.selectById(99), nullptr);
}

TEST(EncryptManagerTest, ListNames) {
    EncryptManager mgr;
    EXPECT_TRUE(mgr.listNames().empty());
    mgr.registerStrategy(std::make_unique<XorEncryptStrategy>());
    mgr.registerStrategy(std::make_unique<VigenereEncryptStrategy>());
    auto names = mgr.listNames();
    EXPECT_EQ(names.size(), 2u);
    EXPECT_EQ(names[0], "xor");
    EXPECT_EQ(names[1], "vigenere");
}

TEST(EncryptManagerTest, EmptyManager) {
    EncryptManager mgr;
    EXPECT_EQ(mgr.select("xor"), nullptr);
    EXPECT_EQ(mgr.selectById(1), nullptr);
    EXPECT_TRUE(mgr.listNames().empty());
}

// ===================== PackManager =====================

TEST(PackManagerTest, RegisterAndSelectByName) {
    PackManager mgr;
    mgr.registerStrategy(std::make_unique<TarPackStrategy>());
    mgr.registerStrategy(std::make_unique<IndexPackStrategy>());
    EXPECT_NE(mgr.select("tar"), nullptr);
    EXPECT_NE(mgr.select("index"), nullptr);
    EXPECT_EQ(mgr.select("nonexistent"), nullptr);
    EXPECT_EQ(mgr.select(""), nullptr);
}

TEST(PackManagerTest, RegisterAndSelectById) {
    PackManager mgr;
    mgr.registerStrategy(std::make_unique<TarPackStrategy>());
    mgr.registerStrategy(std::make_unique<IndexPackStrategy>());
    EXPECT_NE(mgr.selectById(1), nullptr);
    EXPECT_NE(mgr.selectById(2), nullptr);
    EXPECT_EQ(mgr.selectById(0), nullptr);
    EXPECT_EQ(mgr.selectById(99), nullptr);
}

TEST(PackManagerTest, ListNames) {
    PackManager mgr;
    EXPECT_TRUE(mgr.listNames().empty());
    mgr.registerStrategy(std::make_unique<TarPackStrategy>());
    mgr.registerStrategy(std::make_unique<IndexPackStrategy>());
    auto names = mgr.listNames();
    EXPECT_EQ(names.size(), 2u);
    EXPECT_EQ(names[0], "tar");
    EXPECT_EQ(names[1], "index");
}

TEST(PackManagerTest, EmptyManager) {
    PackManager mgr;
    EXPECT_EQ(mgr.select("tar"), nullptr);
    EXPECT_EQ(mgr.selectById(1), nullptr);
    EXPECT_TRUE(mgr.listNames().empty());
}

TEST(PackManagerTest, SelectStrategiesWork) {
    PackManager mgr;
    mgr.registerStrategy(std::make_unique<TarPackStrategy>());
    auto* s = mgr.select("tar");
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->name(), "tar");
    EXPECT_EQ(s->algoId(), 1);
}
