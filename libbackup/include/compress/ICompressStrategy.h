#ifndef BACKUP_ICOMPRESSSTRATEGY_H
#define BACKUP_ICOMPRESSSTRATEGY_H

#include <vector>
#include <string>
#include <cstdint>

/// 压缩策略抽象接口
class ICompressStrategy {
public:
    virtual ~ICompressStrategy() = default;

    virtual std::vector<uint8_t> compress(const std::vector<uint8_t>& input) = 0;
    virtual std::vector<uint8_t> decompress(const std::vector<uint8_t>& input) = 0;
    virtual std::string name() const = 0;
    virtual uint8_t algoId() const = 0;
};

#endif // BACKUP_ICOMPRESSSTRATEGY_H
