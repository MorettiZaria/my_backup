#include "encrypt/VigenereEncryptStrategy.h"

std::vector<uint8_t> VigenereEncryptStrategy::encrypt(const std::vector<uint8_t>& input,
                                                       const std::string& password) {
    if (password.empty()) return input;

    std::vector<uint8_t> result;

    // 写入原始大小 (8B)
    uint64_t origSize = input.size();
    for (int i = 0; i < 8; ++i) {
        result.push_back(static_cast<uint8_t>((origSize >> (i * 8)) & 0xFF));
    }

    // 加密: result[i] = (input[i] + key[i % keyLen]) mod 256
    const auto& key = password;
    for (size_t i = 0; i < input.size(); ++i) {
        uint8_t kb = static_cast<uint8_t>(key[i % key.size()]);
        uint8_t encrypted = static_cast<uint8_t>((static_cast<uint16_t>(input[i]) + kb) & 0xFF);
        result.push_back(encrypted);
    }

    return result;
}

std::vector<uint8_t> VigenereEncryptStrategy::decrypt(const std::vector<uint8_t>& input,
                                                       const std::string& password) {
    if (input.size() < 8) return {};
    if (password.empty()) {
        return std::vector<uint8_t>(input.begin() + 8, input.end());
    }

    // 读取原始大小
    uint64_t origSize = 0;
    for (int i = 0; i < 8; ++i) {
        origSize |= static_cast<uint64_t>(input[i]) << (i * 8);
    }

    // 解密: result[i] = (cipher[i] - key[i % keyLen] + 256) mod 256
    const auto& key = password;
    std::vector<uint8_t> result;
    result.reserve(static_cast<size_t>(origSize));

    size_t dataStart = 8;
    size_t dataLen = input.size() - dataStart;
    for (size_t i = 0; i < dataLen; ++i) {
        uint8_t kb = static_cast<uint8_t>(key[i % key.size()]);
        uint8_t decrypted = static_cast<uint8_t>(
            (static_cast<uint16_t>(input[dataStart + i]) - kb + 256) & 0xFF);
        result.push_back(decrypted);
    }

    return result;
}
