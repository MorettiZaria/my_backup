#ifndef BACKUP_IENCRYPTSTRATEGY_H
#define BACKUP_IENCRYPTSTRATEGY_H

#include <vector>
#include <string>
#include <cstdint>

/// 加密策略抽象接口
class IEncryptStrategy {
public:
    virtual ~IEncryptStrategy() = default;

    virtual std::vector<uint8_t> encrypt(const std::vector<uint8_t>& input,
                                         const std::string& password) = 0;
    virtual std::vector<uint8_t> decrypt(const std::vector<uint8_t>& input,
                                         const std::string& password) = 0;
    virtual std::string name() const = 0;
    virtual uint8_t algoId() const = 0;
};

#endif // BACKUP_IENCRYPTSTRATEGY_H
