#include "compress/RleCompressStrategy.h"
#include <iostream>
#include <cstring>

std::vector<uint8_t> RleCompressStrategy::compress(const std::vector<uint8_t>& input) {
    std::vector<uint8_t> result;
    if (input.empty()) {
        // 空输入: 只写原始大小 0
        for (int i = 0; i < 8; ++i) result.push_back(0);
        return result;
    }

    // 写入原始大小 (8B)
    uint64_t origSize = input.size();
    for (int i = 0; i < 8; ++i) {
        result.push_back(static_cast<uint8_t>((origSize >> (i * 8)) & 0xFF));
    }

    size_t pos = 0;
    while (pos < input.size()) {
        // 检测连续重复
        uint8_t byteVal = input[pos];
        size_t runLen = 1;
        while (pos + runLen < input.size() && input[pos + runLen] == byteVal && runLen < 128) {
            ++runLen;
        }

        // 检测不重复序列
        size_t literalStart = pos;
        size_t literalLen = 1;
        if (runLen < 3) {
            // 不够连续重复，尝试收集不重复字节
            while (pos + literalLen < input.size() && literalLen < 128) {
                uint8_t nextByte = input[pos + literalLen];
                // 检查接下来是否会重复
                size_t nextRun = 1;
                while (pos + literalLen + nextRun < input.size() &&
                       input[pos + literalLen + nextRun] == nextByte && nextRun < 3) {
                    ++nextRun;
                }
                if (nextRun >= 3 && literalLen <= 128) {
                    break;
                }
                ++literalLen;
                if (literalLen >= 128) break;
            }
            // 写入原始块
            // 控制字节 bit7=0, bit[6:0]=literalLen-1
            uint8_t ctrl = static_cast<uint8_t>((literalLen - 1) & 0x7F);  // bit7=0
            result.push_back(ctrl);
            for (size_t i = 0; i < literalLen; ++i) {
                result.push_back(input[literalStart + i]);
            }
            pos += literalLen;
        } else {
            // 写入重复块
            // 控制字节 bit7=1, bit[6:0]=runLen-1
            uint8_t ctrl = static_cast<uint8_t>(0x80 | ((runLen - 1) & 0x7F));
            result.push_back(ctrl);
            result.push_back(byteVal);
            pos += runLen;
        }
    }

    return result;
}

std::vector<uint8_t> RleCompressStrategy::decompress(const std::vector<uint8_t>& input) {
    if (input.size() < 8) {
        std::cerr << "Error: RLE data too small" << std::endl;
        return {};
    }

    // 读取原始大小
    uint64_t origSize = 0;
    for (int i = 0; i < 8; ++i) {
        origSize |= static_cast<uint64_t>(input[i]) << (i * 8);
    }

    std::vector<uint8_t> result;
    result.reserve(static_cast<size_t>(origSize));

    size_t pos = 8;
    while (pos < input.size() && result.size() < origSize) {
        uint8_t ctrl = input[pos++];

        if (ctrl & 0x80) {
            // 重复块
            size_t count = (ctrl & 0x7F) + 1;
            if (pos >= input.size()) break;
            uint8_t val = input[pos++];
            for (size_t i = 0; i < count && result.size() < origSize; ++i) {
                result.push_back(val);
            }
        } else {
            // 原始块
            size_t count = (ctrl & 0x7F) + 1;
            for (size_t i = 0; i < count && pos < input.size() && result.size() < origSize; ++i) {
                result.push_back(input[pos++]);
            }
        }
    }

    return result;
}
