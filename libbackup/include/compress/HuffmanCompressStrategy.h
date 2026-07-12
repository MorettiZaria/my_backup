#ifndef BACKUP_HUFFMANCOMPRESSSTRATEGY_H
#define BACKUP_HUFFMANCOMPRESSSTRATEGY_H

#include "ICompressStrategy.h"
#include <array>
#include <queue>
#include <cstdint>

/// 哈夫曼树节点
struct HuffmanNode {
    uint8_t  byteValue = 0;   // 叶节点存储的字节值
    uint32_t frequency = 0;   // 出现频率
    HuffmanNode* left  = nullptr;
    HuffmanNode* right = nullptr;

    bool isLeaf() const { return left == nullptr && right == nullptr; }
};

/// 编码表项：byte → 变长位串
struct HuffmanCode {
    uint64_t bits  = 0;   // 编码位（右对齐）
    uint8_t  length = 0;  // 有效位长度 (1–32)
};

/// 优先队列比较器（频率小的优先）
struct CompareNode {
    bool operator()(HuffmanNode* a, HuffmanNode* b) {
        return a->frequency > b->frequency;
    }
};

/// 算法2：哈夫曼编码
/// 输出格式: [原始大小:8B] + [频率表:256×4B] + [压缩后位数:8B] + [位数据:N B]
class HuffmanCompressStrategy : public ICompressStrategy {
public:
    std::vector<uint8_t> compress(const std::vector<uint8_t>& input) override;
    std::vector<uint8_t> decompress(const std::vector<uint8_t>& input) override;
    std::string name() const override { return "huffman"; }
    uint8_t algoId() const override { return 2; }

private:
    /// 统计 256 个字节值的出现频率
    std::array<uint32_t, 256> countFrequencies(const std::vector<uint8_t>& data);

    /// 从频率表构建哈夫曼树，返回根节点
    HuffmanNode* buildTree(const std::array<uint32_t, 256>& freqs);

    /// 从根节点 DFS 生成编码表
    void generateCodes(HuffmanNode* node, uint64_t bits, uint8_t length,
                       std::array<HuffmanCode, 256>& codes);

    /// 递归释放哈夫曼树
    void freeTree(HuffmanNode* node);

    /// 从频率表重建哈夫曼树（解压时使用）
    HuffmanNode* rebuildTree(const std::array<uint32_t, 256>& freqs);
};

#endif // BACKUP_HUFFMANCOMPRESSSTRATEGY_H
