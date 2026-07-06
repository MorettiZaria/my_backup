#ifndef BACKUP_VIGENEREENCRYPTSTRATEGY_H
#define BACKUP_VIGENEREENCRYPTSTRATEGY_H

#include "IEncryptStrategy.h"

/// 算法2：维吉尼亚密码（字节级多表置换）
/// encrypt: result[i] = (input[i] + key[i % keyLen]) mod 256
/// decrypt: result[i] = (input[i] - key[i % keyLen] + 256) mod 256
/// 格式: [原始大小:8B] + [密文:N B]
class VigenereEncryptStrategy : public IEncryptStrategy {
public:
    std::vector<uint8_t> encrypt(const std::vector<uint8_t>& input,
                                 const std::string& password) override;
    std::vector<uint8_t> decrypt(const std::vector<uint8_t>& input,
                                 const std::string& password) override;
    std::string name() const override { return "vigenere"; }
    uint8_t algoId() const override { return 2; }
};

#endif // BACKUP_VIGENEREENCRYPTSTRATEGY_H
