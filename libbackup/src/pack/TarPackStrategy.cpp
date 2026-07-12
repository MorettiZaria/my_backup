#include "pack/TarPackStrategy.h"
#include <iostream>
#include <fstream>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

// ===== pack =====

void TarPackStrategy::writeOctal(char* buf, size_t len, uint64_t val) {
    // 写入八进制文本，剩余位置填 '\0'
    std::memset(buf, '0', len);
    if (len > 0) {
        buf[len - 1] = '\0';
    }
    // 从后向前写入八进制数字
    int pos = static_cast<int>(len) - 2;
    if (pos < 0) return;
    if (val == 0) {
        buf[pos] = '0';
        return;
    }
    while (val > 0 && pos >= 0) {
        buf[pos] = '0' + (val & 0x7);
        val >>= 3;
        --pos;
    }
}

uint64_t TarPackStrategy::readOctal(const char* buf, size_t len) {
    uint64_t val = 0;
    for (size_t i = 0; i < len && buf[i] != '\0'; ++i) {
        if (buf[i] >= '0' && buf[i] <= '7') {
            val = (val << 3) | static_cast<uint64_t>(buf[i] - '0');
        }
    }
    return val;
}

std::vector<uint8_t> TarPackStrategy::buildHeader(const FileInfo& info) {
    std::vector<uint8_t> header(HEADER_SIZE, 0);
    char* buf = reinterpret_cast<char*>(header.data());

    // name [100]
    std::strncpy(buf, info.relativePath.c_str(), 100);
    // mode [8]
    writeOctal(buf + 100, 8, static_cast<uint64_t>(info.permissions));
    // uid [8]
    writeOctal(buf + 108, 8, static_cast<uint64_t>(info.owner));
    // gid [8]
    writeOctal(buf + 116, 8, static_cast<uint64_t>(info.group));
    // size [12]
    uint64_t dataSize = info.isRegular() ? info.content.size() : 0;
    writeOctal(buf + 124, 12, dataSize);
    // mtime [12]
    writeOctal(buf + 136, 12, static_cast<uint64_t>(info.mtime));
    // type flag [1] (offset 156)
    char typeFlag = '0';  // 普通文件
    if (info.isDirectory()) {
        typeFlag = '5';
    } else if (info.isSymlink()) {
        typeFlag = '2';
    }
    buf[156] = typeFlag;
    // link target [100] (offset 157)
    if (info.isSymlink()) {
        std::strncpy(buf + 157, info.symlinkTarget.c_str(), 100);
    }

    return header;
}

FileInfo TarPackStrategy::parseHeader(const uint8_t* data) {
    const char* buf = reinterpret_cast<const char*>(data);
    FileInfo info;

    info.relativePath = std::string(buf, ::strnlen(buf, 100));
    info.permissions  = static_cast<mode_t>(readOctal(buf + 100, 8));
    info.owner        = static_cast<uid_t>(readOctal(buf + 108, 8));
    info.group        = static_cast<gid_t>(readOctal(buf + 116, 8));
    info.fileSize     = static_cast<off_t>(readOctal(buf + 124, 12));
    info.mtime        = static_cast<time_t>(readOctal(buf + 136, 12));
    info.atime        = info.mtime;  // tar 格式不单独存 atime
    info.ctime        = info.mtime;

    char typeFlag = buf[156];
    switch (typeFlag) {
    case '0': case '\0':
        info.fileType = S_IFREG; break;
    case '5':
        info.fileType = S_IFDIR; break;
    case '2':
        info.fileType = S_IFLNK;
        info.symlinkTarget = std::string(buf + 157, ::strnlen(buf + 157, 100));
        break;
    default:
        info.fileType = S_IFREG; break;
    }

    return info;
}

std::vector<uint8_t> TarPackStrategy::pack(const std::string& /*baseDir*/,
                                           const std::vector<FileInfo>& files) {
    std::vector<uint8_t> result;

    for (const auto& info : files) {
        // 构造 header
        std::vector<uint8_t> header = buildHeader(info);
        result.insert(result.end(), header.begin(), header.end());

        // 写入文件数据（仅普通文件）
        if (info.isRegular() && !info.content.empty()) {
            result.insert(result.end(), info.content.begin(), info.content.end());

            // 对齐到 512 字节边界
            size_t padding = BLOCK_SIZE - (info.content.size() % BLOCK_SIZE);
            if (padding < BLOCK_SIZE) {
                result.insert(result.end(), padding, 0);
            }
        }
    }

    // 结束标记：两个全零 512B 块
    result.insert(result.end(), BLOCK_SIZE * 2, 0);

    return result;
}

// ===== unpack =====

void TarPackStrategy::unpack(const std::vector<uint8_t>& data,
                             const std::string& /*destDir*/,
                             std::vector<FileInfo>& outFiles) {
    size_t offset = 0;

    while (offset + HEADER_SIZE <= data.size()) {
        // 读取 header
        const uint8_t* headerPtr = data.data() + offset;

        // 检查是否为全零块（结束标记）
        bool allZero = true;
        for (size_t i = 0; i < HEADER_SIZE; ++i) {
            if (headerPtr[i] != 0) {
                allZero = false;
                break;
            }
        }
        if (allZero) {
            // 验证下一个 512B 也为全零
            if (offset + HEADER_SIZE * 2 <= data.size()) {
                bool nextZero = true;
                for (size_t i = HEADER_SIZE; i < HEADER_SIZE * 2; ++i) {
                    if (data[offset + i] != 0) {
                        nextZero = false;
                        break;
                    }
                }
                if (nextZero) break;
            }
        }

        // 解析 header
        FileInfo info = parseHeader(headerPtr);
        offset += HEADER_SIZE;

        // 读取文件数据
        uint64_t dataSize = static_cast<uint64_t>(info.fileSize);
        if (info.isRegular() && dataSize > 0) {
            if (offset + dataSize <= data.size()) {
                info.content.assign(data.begin() + static_cast<ptrdiff_t>(offset),
                                    data.begin() + static_cast<ptrdiff_t>(offset + dataSize));
            }
        }

        outFiles.push_back(info);

        // 跳过数据区 + 对齐
        size_t paddedSize = ((dataSize + BLOCK_SIZE - 1) / BLOCK_SIZE) * BLOCK_SIZE;
        offset += paddedSize;
    }
}
