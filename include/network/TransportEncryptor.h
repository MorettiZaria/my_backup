#ifndef BACKUP_TRANSPORTENCRYPTOR_H
#define BACKUP_TRANSPORTENCRYPTOR_H

#include <vector>
#include <string>
#include <cstdint>

/// 传输层加密器：基于 XOR LCG 流密码，每消息使用不同的 IV（序列号）
class TransportEncryptor {
public:
    TransportEncryptor();

    /// 从密码 + 服务器 salt 初始化会话密钥
    void initSession(const std::string& password,
                     const std::vector<uint8_t>& serverSalt);

    /// 加密载荷
    std::vector<uint8_t> encrypt(const std::vector<uint8_t>& plaintext,
                                  uint64_t seqNum);

    /// 解密载荷（与加密相同操作——XOR 对称性）
    std::vector<uint8_t> decrypt(const std::vector<uint8_t>& ciphertext,
                                  uint64_t seqNum);

    /// 是否已激活
    bool isActive() const;

private:
    bool active_ = false;
    std::vector<uint8_t> sessionKey_;   // 16 字节会话密钥
};

#endif // BACKUP_TRANSPORTENCRYPTOR_H
