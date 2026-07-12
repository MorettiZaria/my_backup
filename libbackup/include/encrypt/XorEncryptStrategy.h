#ifndef BACKUP_XORENCRYPTSTRATEGY_H
#define BACKUP_XORENCRYPTSTRATEGY_H

#include "IEncryptStrategy.h"

/// 算法1：XOR 流加密
/// 从密码生成 LCG 伪随机密钥流，逐字节异或。加密解密同一操作。
/// 格式: [原始大小:8B] + [密文:N B]
class XorEncryptStrategy : public IEncryptStrategy {
public:
    std::vector<uint8_t> encrypt(const std::vector<uint8_t>& input,
                                 const std::string& password) override;
    std::vector<uint8_t> decrypt(const std::vector<uint8_t>& input,
                                 const std::string& password) override;
    std::string name() const override { return "xor"; }
    uint8_t algoId() const override { return 1; }

private:
    /// 加解密核心（encrypt 和 decrypt 是同一操作）
    std::vector<uint8_t> crypt(const std::vector<uint8_t>& input,
                               const std::string& password);

    /// 从密码生成 32-bit 种子
    uint32_t seedFromPassword(const std::string& password);

    /// LCG 伪随机数生成器
    struct LCG {
        uint32_t state;
        explicit LCG(uint32_t seed);
        uint8_t next();  // 返回下一个密钥字节
    };
};

#endif // BACKUP_XORENCRYPTSTRATEGY_H
