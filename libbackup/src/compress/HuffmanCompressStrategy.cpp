#include "compress/HuffmanCompressStrategy.h"
#include <iostream>
#include <algorithm>
#include <cstring>

// ===== 频率统计 =====

std::array<uint32_t, 256> HuffmanCompressStrategy::countFrequencies(const std::vector<uint8_t>& data) {
    std::array<uint32_t, 256> freqs{};
    for (uint8_t b : data) {
        freqs[b]++;
    }
    // 确保每个字节至少出现 0 次（避免空频率导致树构建问题）
    // 给所有未出现的字节设置频率 0
    return freqs;
}

// ===== 构建哈夫曼树 =====

HuffmanNode* HuffmanCompressStrategy::buildTree(const std::array<uint32_t, 256>& freqs) {
    std::priority_queue<HuffmanNode*, std::vector<HuffmanNode*>, CompareNode> pq;

    for (int i = 0; i < 256; ++i) {
        if (freqs[i] > 0) {
            HuffmanNode* node = new HuffmanNode();
            node->byteValue = static_cast<uint8_t>(i);
            node->frequency = freqs[i];
            pq.push(node);
        }
    }

    // 如果所有字节频率都为 0（空输入），创建一个哨兵节点
    if (pq.empty()) {
        HuffmanNode* node = new HuffmanNode();
        node->byteValue = 0;
        node->frequency = 1;
        pq.push(node);
    }

    while (pq.size() > 1) {
        HuffmanNode* left = pq.top(); pq.pop();
        HuffmanNode* right = pq.top(); pq.pop();

        HuffmanNode* parent = new HuffmanNode();
        parent->frequency = left->frequency + right->frequency;
        parent->left  = left;
        parent->right = right;

        pq.push(parent);
    }

    return pq.empty() ? nullptr : pq.top();
}

// ===== 生成编码表 =====

void HuffmanCompressStrategy::generateCodes(HuffmanNode* node, uint64_t bits, uint8_t length,
                                            std::array<HuffmanCode, 256>& codes) {
    if (!node) return;

    if (node->isLeaf()) {
        codes[node->byteValue].bits   = bits;
        codes[node->byteValue].length = length;
        return;
    }

    // 左子树：追加 bit 0
    generateCodes(node->left, bits, length + 1, codes);
    // 右子树：追加 bit 1
    generateCodes(node->right, bits | (1ULL << length), length + 1, codes);
}

// ===== 释放树 =====

void HuffmanCompressStrategy::freeTree(HuffmanNode* node) {
    if (!node) return;
    freeTree(node->left);
    freeTree(node->right);
    delete node;
}

// ===== 重建树（解压用） =====

HuffmanNode* HuffmanCompressStrategy::rebuildTree(const std::array<uint32_t, 256>& freqs) {
    return buildTree(freqs);
}

// ===== compress =====

std::vector<uint8_t> HuffmanCompressStrategy::compress(const std::vector<uint8_t>& input) {
    std::vector<uint8_t> result;

    // 写入原始大小 (8B)
    uint64_t origSize = input.size();
    for (int i = 0; i < 8; ++i) {
        result.push_back(static_cast<uint8_t>((origSize >> (i * 8)) & 0xFF));
    }

    if (input.empty()) {
        // 空输入：频率表全零
        for (int i = 0; i < 256 * 4; ++i) result.push_back(0);
        // 压缩后位数 = 0
        for (int i = 0; i < 8; ++i) result.push_back(0);
        return result;
    }

    // 统计频率
    auto freqs = countFrequencies(input);

    // 写入频率表 (256 × 4B)
    for (int i = 0; i < 256; ++i) {
        uint32_t f = freqs[i];
        result.push_back(static_cast<uint8_t>(f & 0xFF));
        result.push_back(static_cast<uint8_t>((f >> 8) & 0xFF));
        result.push_back(static_cast<uint8_t>((f >> 16) & 0xFF));
        result.push_back(static_cast<uint8_t>((f >> 24) & 0xFF));
    }

    // 构建哈夫曼树并生成编码表
    HuffmanNode* root = buildTree(freqs);
    std::array<HuffmanCode, 256> codes{};
    generateCodes(root, 0, 0, codes);

    // 编码数据
    std::vector<uint8_t> bitBuffer;
    uint8_t currentByte = 0;
    int bitCount = 0;

    for (uint8_t b : input) {
        const auto& code = codes[b];
        for (int i = 0; i < code.length; ++i) {
            if (code.bits & (1ULL << i)) {
                currentByte |= (1 << bitCount);
            }
            ++bitCount;

            if (bitCount == 8) {
                bitBuffer.push_back(currentByte);
                currentByte = 0;
                bitCount = 0;
            }
        }
    }
    // 写入剩余位
    if (bitCount > 0) {
        bitBuffer.push_back(currentByte);
    }

    // 写入压缩后位数 (8B)
    uint64_t totalBits = 0;
    for (uint8_t b : input) {
        totalBits += codes[b].length;
    }
    for (int i = 0; i < 8; ++i) {
        result.push_back(static_cast<uint8_t>((totalBits >> (i * 8)) & 0xFF));
    }

    // 写入位数据
    result.insert(result.end(), bitBuffer.begin(), bitBuffer.end());

    freeTree(root);
    return result;
}

// ===== decompress =====

std::vector<uint8_t> HuffmanCompressStrategy::decompress(const std::vector<uint8_t>& input) {
    if (input.size() < 8 + 256*4 + 8) {
        std::cerr << "Error: Huffman data too small" << std::endl;
        return {};
    }

    size_t offset = 0;

    // 读取原始大小
    auto r64 = [&]() -> uint64_t {
        uint64_t v = 0;
        for (int i = 0; i < 8; ++i) v |= static_cast<uint64_t>(input[offset+i]) << (i*8);
        offset += 8;
        return v;
    };

    uint64_t origSize = r64();
    if (origSize == 0) return {};

    // 读取频率表
    std::array<uint32_t, 256> freqs{};
    for (int i = 0; i < 256; ++i) {
        freqs[i] = static_cast<uint32_t>(input[offset]) |
                   (static_cast<uint32_t>(input[offset+1]) << 8) |
                   (static_cast<uint32_t>(input[offset+2]) << 16) |
                   (static_cast<uint32_t>(input[offset+3]) << 24);
        offset += 4;
    }

    // 读取压缩后位数
    uint64_t totalBits = r64();

    // 重建哈夫曼树
    HuffmanNode* root = rebuildTree(freqs);
    if (!root) return {};

    // 按位流解码
    std::vector<uint8_t> result;
    result.reserve(static_cast<size_t>(origSize));

    const uint8_t* bitData = input.data() + offset;
    size_t bitIndex = 0;

    HuffmanNode* current = root;
    while (result.size() < origSize && bitIndex < totalBits) {
        uint8_t byte = bitData[bitIndex / 8];
        uint8_t bit = (byte >> (bitIndex % 8)) & 1;
        ++bitIndex;

        current = (bit == 0) ? current->left : current->right;

        if (!current) break;

        if (current->isLeaf()) {
            result.push_back(current->byteValue);
            current = root;
        }
    }

    freeTree(root);
    return result;
}
