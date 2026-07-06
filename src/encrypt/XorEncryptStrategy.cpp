#include "encrypt/XorEncryptStrategy.h"

XorEncryptStrategy::LCG::LCG(uint32_t seed) : state(seed) {}

uint8_t XorEncryptStrategy::LCG::next() {
    // LCG: X_{n+1} = (1103515245 * X_n + 12345) mod 2^31
    state = 1103515245u * state + 12345u;
    // 返回 state 的低 8 位作为密钥字节
    return static_cast<uint8_t>(state & 0xFF);
}

uint32_t XorEncryptStrategy::seedFromPassword(const std::string& password) {
    uint32_t seed = 0;
    for (char c : password) {
        seed = seed * 31 + static_cast<uint8_t>(c);
    }
    return seed;
}

std::vector<uint8_t> XorEncryptStrategy::crypt(const std::vector<uint8_t>& input,
                                                const std::string& password) {
    uint32_t seed = seedFromPassword(password);
    LCG lcg(seed);

    // 先消耗几个字节以减少弱密码的规律性
    for (int i = 0; i < 16; ++i) lcg.next();

    std::vector<uint8_t> result(input.size());
    for (size_t i = 0; i < input.size(); ++i) {
        result[i] = input[i] ^ lcg.next();
    }
    return result;
}

std::vector<uint8_t> XorEncryptStrategy::encrypt(const std::vector<uint8_t>& input,
                                                  const std::string& password) {
    std::vector<uint8_t> result;

    // 写入原始大小 (8B) — 不加密
    uint64_t origSize = input.size();
    for (int i = 0; i < 8; ++i) {
        result.push_back(static_cast<uint8_t>((origSize >> (i * 8)) & 0xFF));
    }

    // 加密数据
    std::vector<uint8_t> cipher = crypt(input, password);
    result.insert(result.end(), cipher.begin(), cipher.end());

    return result;
}

std::vector<uint8_t> XorEncryptStrategy::decrypt(const std::vector<uint8_t>& input,
                                                  const std::string& password) {
    if (input.size() < 8) return {};

    // 读取原始大小
    uint64_t origSize = 0;
    for (int i = 0; i < 8; ++i) {
        origSize |= static_cast<uint64_t>(input[i]) << (i * 8);
    }

    // 提取密文部分
    std::vector<uint8_t> cipher(input.begin() + 8, input.end());

    // 解密（XOR 同一操作）
    return crypt(cipher, password);
}
