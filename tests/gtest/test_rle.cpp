#include "gtest/gtest.h"
#include "compress/RleCompressStrategy.h"

#include <vector>
#include <cstdint>
#include <random>

TEST(RleTest, EmptyCompressDecompress) {
    RleCompressStrategy rle;
    std::vector<uint8_t> input;
    std::vector<uint8_t> compressed = rle.compress(input);
    std::vector<uint8_t> decompressed = rle.decompress(compressed);
    EXPECT_TRUE(decompressed.empty());
    EXPECT_EQ(compressed.size(), 8u);
}

TEST(RleTest, SingleByteCompressDecompress) {
    RleCompressStrategy rle;
    std::vector<uint8_t> input = {0x42};
    std::vector<uint8_t> compressed = rle.compress(input);
    std::vector<uint8_t> decompressed = rle.decompress(compressed);
    ASSERT_EQ(decompressed.size(), input.size());
    EXPECT_EQ(decompressed[0], 0x42);
}

TEST(RleTest, RepeatedBytesCompressDecompress) {
    RleCompressStrategy rle;
    std::vector<uint8_t> input(100, 'A');
    std::vector<uint8_t> compressed = rle.compress(input);
    std::vector<uint8_t> decompressed = rle.decompress(compressed);
    ASSERT_EQ(decompressed.size(), input.size());
    for (size_t i = 0; i < input.size(); ++i) {
        EXPECT_EQ(decompressed[i], input[i]) << "Mismatch at index " << i;
    }
    EXPECT_LT(compressed.size(), input.size());
}

TEST(RleTest, RandomDataCompressDecompress) {
    RleCompressStrategy rle;
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(0, 255);
    std::vector<uint8_t> input(1000);
    for (size_t i = 0; i < input.size(); ++i) {
        input[i] = static_cast<uint8_t>(dist(rng));
    }
    std::vector<uint8_t> compressed = rle.compress(input);
    std::vector<uint8_t> decompressed = rle.decompress(compressed);
    ASSERT_EQ(decompressed.size(), input.size());
    EXPECT_EQ(decompressed, input);
}

TEST(RleTest, LargeRepeatedBlock) {
    RleCompressStrategy rle;
    std::vector<uint8_t> input(10000, 'Z');
    std::vector<uint8_t> compressed = rle.compress(input);
    std::vector<uint8_t> decompressed = rle.decompress(compressed);
    ASSERT_EQ(decompressed.size(), input.size());
    EXPECT_EQ(decompressed, input);
    EXPECT_LT(compressed.size(), input.size());
}

TEST(RleTest, AlternatingPattern) {
    RleCompressStrategy rle;
    std::vector<uint8_t> input;
    for (int i = 0; i < 50; ++i) {
        input.push_back('A');
        input.push_back('B');
    }
    std::vector<uint8_t> compressed = rle.compress(input);
    std::vector<uint8_t> decompressed = rle.decompress(compressed);
    EXPECT_EQ(decompressed, input);
}

TEST(RleTest, SingleRunNoCompression) {
    RleCompressStrategy rle;
    std::vector<uint8_t> input(256);
    for (int i = 0; i < 256; ++i) {
        input[static_cast<size_t>(i)] = static_cast<uint8_t>(i);
    }
    std::vector<uint8_t> compressed = rle.compress(input);
    std::vector<uint8_t> decompressed = rle.decompress(compressed);
    EXPECT_EQ(decompressed, input);
}

TEST(RleTest, DecompressInvalidInput) {
    RleCompressStrategy rle;
    std::vector<uint8_t> tooShort(4, 0);
    std::vector<uint8_t> result = rle.decompress(tooShort);
    EXPECT_TRUE(result.empty());
}

TEST(RleTest, NameAndAlgoId) {
    RleCompressStrategy rle;
    EXPECT_EQ(rle.name(), "rle");
    EXPECT_EQ(rle.algoId(), 1);
}
