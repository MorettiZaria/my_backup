#include "gtest/gtest.h"
#include "encrypt/XorEncryptStrategy.h"
#include "encrypt/VigenereEncryptStrategy.h"

#include <vector>
#include <string>
#include <cstdint>
#include <random>

namespace {
std::vector<uint8_t> toBytes(const std::string& s) {
    return std::vector<uint8_t>(s.begin(), s.end());
}
std::vector<uint8_t> randomBytes(size_t count, unsigned seed = 42) {
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> dist(0, 255);
    std::vector<uint8_t> data(count);
    for (size_t i = 0; i < count; ++i)
        data[i] = static_cast<uint8_t>(dist(rng));
    return data;
}
} // namespace

// ===================== XorEncryptStrategy =====================

TEST(XorTest, EmptyEncryptDecrypt) {
    XorEncryptStrategy xorAlgo;
    std::vector<uint8_t> empty;
    auto encrypted = xorAlgo.encrypt(empty, "test");
    ASSERT_EQ(encrypted.size(), 8u);
    auto decrypted = xorAlgo.decrypt(encrypted, "test");
    EXPECT_TRUE(decrypted.empty());
}

TEST(XorTest, SimpleEncryptDecrypt) {
    XorEncryptStrategy xorAlgo;
    auto plain = toBytes("Hello World");
    auto encrypted = xorAlgo.encrypt(plain, "key123");
    ASSERT_EQ(encrypted.size(), 8u + plain.size());
    auto decrypted = xorAlgo.decrypt(encrypted, "key123");
    EXPECT_EQ(decrypted, plain);
}

TEST(XorTest, WrongPasswordDifferent) {
    XorEncryptStrategy xorAlgo;
    auto plain = toBytes("Hello World");
    auto encrypted = xorAlgo.encrypt(plain, "pass1");
    auto decrypted = xorAlgo.decrypt(encrypted, "pass2");
    EXPECT_NE(decrypted, plain);
}

TEST(XorTest, SamePasswordDifferentData) {
    XorEncryptStrategy xorAlgo;
    auto enc1 = xorAlgo.encrypt(toBytes("AAAA"), "samekey");
    auto enc2 = xorAlgo.encrypt(toBytes("BBBB"), "samekey");
    std::vector<uint8_t> c1(enc1.begin() + 8, enc1.end());
    std::vector<uint8_t> c2(enc2.begin() + 8, enc2.end());
    EXPECT_NE(c1, c2);
}

TEST(XorTest, DeterministicEncryption) {
    XorEncryptStrategy xorAlgo;
    auto plain = toBytes("SensitiveData123");
    auto enc1 = xorAlgo.encrypt(plain, "secret");
    auto enc2 = xorAlgo.encrypt(plain, "secret");
    EXPECT_EQ(enc1, enc2);
}

TEST(XorTest, BinaryDataEncryptDecrypt) {
    XorEncryptStrategy xorAlgo;
    auto original = randomBytes(1000, 123);
    auto encrypted = xorAlgo.encrypt(original, "binaryKey");
    ASSERT_EQ(encrypted.size(), 8u + original.size());
    auto decrypted = xorAlgo.decrypt(encrypted, "binaryKey");
    EXPECT_EQ(decrypted, original);
}

TEST(XorTest, NameAndAlgoId) {
    XorEncryptStrategy xorAlgo;
    EXPECT_EQ(xorAlgo.name(), "xor");
    EXPECT_EQ(xorAlgo.algoId(), 1);
}

TEST(XorTest, EncryptFormat) {
    XorEncryptStrategy xorAlgo;
    std::vector<uint8_t> plain(256);
    for (size_t i = 0; i < plain.size(); ++i)
        plain[i] = static_cast<uint8_t>(i);
    auto encrypted = xorAlgo.encrypt(plain, "fmtTest");
    uint64_t storedSize = 0;
    for (int i = 0; i < 8; ++i)
        storedSize |= static_cast<uint64_t>(encrypted[i]) << (i * 8);
    EXPECT_EQ(storedSize, 256u);
}

// ===================== VigenereEncryptStrategy =====================

TEST(VigenereTest, EmptyEncryptDecrypt) {
    VigenereEncryptStrategy vigAlgo;
    std::vector<uint8_t> empty;
    auto encrypted = vigAlgo.encrypt(empty, "test");
    ASSERT_EQ(encrypted.size(), 8u);
    auto decrypted = vigAlgo.decrypt(encrypted, "test");
    EXPECT_TRUE(decrypted.empty());
}

TEST(VigenereTest, SimpleEncryptDecrypt) {
    VigenereEncryptStrategy vigAlgo;
    auto plain = toBytes("Hello World");
    auto encrypted = vigAlgo.encrypt(plain, "key123");
    ASSERT_EQ(encrypted.size(), 8u + plain.size());
    auto decrypted = vigAlgo.decrypt(encrypted, "key123");
    EXPECT_EQ(decrypted, plain);
}

TEST(VigenereTest, WrongPasswordDifferent) {
    VigenereEncryptStrategy vigAlgo;
    auto plain = toBytes("Hello World");
    auto encrypted = vigAlgo.encrypt(plain, "passA");
    auto decrypted = vigAlgo.decrypt(encrypted, "passB");
    EXPECT_NE(decrypted, plain);
}

TEST(VigenereTest, LongPasswordEncryptDecrypt) {
    VigenereEncryptStrategy vigAlgo;
    std::string longPassword(50, 'x');
    for (size_t i = 0; i < longPassword.size(); ++i)
        longPassword[i] = static_cast<char>('A' + (i % 26));
    auto plain = toBytes("Data with a very long password test");
    auto encrypted = vigAlgo.encrypt(plain, longPassword);
    ASSERT_EQ(encrypted.size(), 8u + plain.size());
    auto decrypted = vigAlgo.decrypt(encrypted, longPassword);
    EXPECT_EQ(decrypted, plain);
}

TEST(VigenereTest, BinaryDataEncryptDecrypt) {
    VigenereEncryptStrategy vigAlgo;
    auto original = randomBytes(1000, 456);
    auto encrypted = vigAlgo.encrypt(original, "binaryVigKey");
    ASSERT_EQ(encrypted.size(), 8u + original.size());
    auto decrypted = vigAlgo.decrypt(encrypted, "binaryVigKey");
    EXPECT_EQ(decrypted, original);
}

TEST(VigenereTest, ShortPassword) {
    VigenereEncryptStrategy vigAlgo;
    auto plain = toBytes("Some text with a 1-char key");
    auto encrypted = vigAlgo.encrypt(plain, "A");
    ASSERT_EQ(encrypted.size(), 8u + plain.size());
    auto decrypted = vigAlgo.decrypt(encrypted, "A");
    EXPECT_EQ(decrypted, plain);
}

TEST(VigenereTest, NameAndAlgoId) {
    VigenereEncryptStrategy vigAlgo;
    EXPECT_EQ(vigAlgo.name(), "vigenere");
    EXPECT_EQ(vigAlgo.algoId(), 2);
}

TEST(VigenereTest, RepeatedPattern) {
    VigenereEncryptStrategy vigAlgo;
    std::vector<uint8_t> plain;
    for (int i = 0; i < 100; ++i) {
        plain.push_back('A');
        plain.push_back('B');
        plain.push_back('C');
    }
    auto encrypted = vigAlgo.encrypt(plain, "XYZ");
    ASSERT_EQ(encrypted.size(), 8u + plain.size());
    auto decrypted = vigAlgo.decrypt(encrypted, "XYZ");
    EXPECT_EQ(decrypted, plain);
}
