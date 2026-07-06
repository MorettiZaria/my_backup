#ifndef BACKUP_RLECOMPRESSSTRATEGY_H
#define BACKUP_RLECOMPRESSSTRATEGY_H

#include "ICompressStrategy.h"

/// 算法1：行程编码 (Run-Length Encoding)
/// 编码格式: [原始大小:8B] + [压缩数据...]
/// 每个压缩单元 = [控制字节:1B] [数据:N B]
///   控制字节 bit7=1 → 重复块: bit[6:0]=重复次数(0-127), 后跟1字节重复值(实际重复1-128次)
///   控制字节 bit7=0 → 原始块: bit[6:0]=原始字节数(0-127), 后跟N+1字节原始数据
class RleCompressStrategy : public ICompressStrategy {
public:
    std::vector<uint8_t> compress(const std::vector<uint8_t>& input) override;
    std::vector<uint8_t> decompress(const std::vector<uint8_t>& input) override;
    std::string name() const override { return "rle"; }
    uint8_t algoId() const override { return 1; }
};

#endif // BACKUP_RLECOMPRESSSTRATEGY_H
