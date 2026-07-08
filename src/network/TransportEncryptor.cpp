#include "network/TransportEncryptor.h"
#include <cstring>

// LCG 与 XOR 加密中使用的相同参数
namespace {
    struct LCGRand {
        uint32_t state;
        explicit LCGRand(uint32_t seed) : state(seed) {}
        uint8_t next() {
            state = 1103515245u * state + 12345u;
            return static_cast<uint8_t>(state & 0xFF);
        }
    };

    uint32_t seedFromString(const std::string& s) {
        uint32_t seed = 0;
        for (char c : s) {
            seed = seed * 31 + static_cast<uint8_t>(c);
        }
        return seed;
    }
}

TransportEncryptor::TransportEncryptor() {}

void TransportEncryptor::initSession(const std::string& password,
                                      const std::vector<uint8_t>& serverSalt) {
    // 派生 16 字节会话密钥
    uint32_t seed = seedFromString(password);

    // 混入服务器 salt
    for (size_t i = 0; i < serverSalt.size(); ++i) {
        seed = seed * 31 + serverSalt[i];
    }

    LCGRand lcg(seed);
    // 消耗前 16 次
    for (int i = 0; i < 16; ++i) lcg.next();

    sessionKey_.resize(16);
    for (int i = 0; i < 16; ++i) {
        for (int j = 0; j < 3; ++j) lcg.next();
        sessionKey_[i] = lcg.next();
    }

    active_ = true;
}

bool TransportEncryptor::isActive() const {
    return active_;
}

std::vector<uint8_t> TransportEncryptor::encrypt(const std::vector<uint8_t>& plaintext,
                                                   uint64_t seqNum) {
    if (!active_) return plaintext;

    // 从 sessionKey + seqNum 派生本次消息的 LCG 种子
    uint32_t msgSeed = 0;
    for (size_t i = 0; i < sessionKey_.size(); ++i) {
        msgSeed = msgSeed * 31 + sessionKey_[i];
    }
    msgSeed = msgSeed ^ static_cast<uint32_t>(seqNum & 0xFFFFFFFF)
                      ^ static_cast<uint32_t>(seqNum >> 32);

    LCGRand lcg(msgSeed);
    for (int i = 0; i < 8; ++i) lcg.next();  // 消耗

    std::vector<uint8_t> result(plaintext.size());
    for (size_t i = 0; i < plaintext.size(); ++i) {
        result[i] = plaintext[i] ^ lcg.next();
    }
    return result;
}

std::vector<uint8_t> TransportEncryptor::decrypt(const std::vector<uint8_t>& ciphertext,
                                                   uint64_t seqNum) {
    // XOR 加解密完全相同
    return encrypt(ciphertext, seqNum);
}
