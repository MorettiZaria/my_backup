#include "pack/IndexPackStrategy.h"
#include <iostream>
#include <sys/stat.h>

// ===== DirEntry serialization =====

std::vector<uint8_t> IndexPackStrategy::DirEntry::serialize() const {
    std::vector<uint8_t> buf;

    auto w16 = [&](uint16_t v) {
        buf.push_back(v & 0xFF);
        buf.push_back((v >> 8) & 0xFF);
    };
    auto w32 = [&](uint32_t v) {
        for (int i = 0; i < 4; ++i) buf.push_back((v >> (i*8)) & 0xFF);
    };
    auto w64 = [&](uint64_t v) {
        for (int i = 0; i < 8; ++i) buf.push_back((v >> (i*8)) & 0xFF);
    };

    w16(static_cast<uint16_t>(path.size()));
    buf.insert(buf.end(), path.begin(), path.end());
    w64(offset);
    w64(size);
    w16(fileType);
    w16(permissions);
    w32(uid);
    w32(gid);
    w64(mtime);
    w16(static_cast<uint16_t>(linkTarget.size()));
    buf.insert(buf.end(), linkTarget.begin(), linkTarget.end());

    return buf;
}

IndexPackStrategy::DirEntry IndexPackStrategy::DirEntry::deserialize(const uint8_t* data, size_t& offset) {
    DirEntry e;

    auto r16 = [&]() -> uint16_t {
        uint16_t v = data[offset] | (static_cast<uint16_t>(data[offset+1]) << 8);
        offset += 2; return v;
    };
    auto r32 = [&]() -> uint32_t {
        uint32_t v = 0;
        for (int i = 0; i < 4; ++i) v |= static_cast<uint32_t>(data[offset+i]) << (i*8);
        offset += 4; return v;
    };
    auto r64 = [&]() -> uint64_t {
        uint64_t v = 0;
        for (int i = 0; i < 8; ++i) v |= static_cast<uint64_t>(data[offset+i]) << (i*8);
        offset += 8; return v;
    };

    uint16_t pathLen = r16();
    e.path.assign(reinterpret_cast<const char*>(data + offset), pathLen);
    offset += pathLen;
    e.offset      = r64();
    e.size        = r64();
    e.fileType    = r16();
    e.permissions = r16();
    e.uid         = r32();
    e.gid         = r32();
    e.mtime       = r64();
    uint16_t linkLen = r16();
    e.linkTarget.assign(reinterpret_cast<const char*>(data + offset), linkLen);
    offset += linkLen;

    return e;
}

// ===== helpers =====

void IndexPackStrategy::writeUint16(std::vector<uint8_t>& buf, uint16_t val) {
    buf.push_back(val & 0xFF);
    buf.push_back((val >> 8) & 0xFF);
}
void IndexPackStrategy::writeUint32(std::vector<uint8_t>& buf, uint32_t val) {
    for (int i = 0; i < 4; ++i) buf.push_back((val >> (i*8)) & 0xFF);
}
void IndexPackStrategy::writeUint64(std::vector<uint8_t>& buf, uint64_t val) {
    for (int i = 0; i < 8; ++i) buf.push_back((val >> (i*8)) & 0xFF);
}
uint16_t IndexPackStrategy::readUint16(const uint8_t* data, size_t& offset) {
    uint16_t v = data[offset] | (static_cast<uint16_t>(data[offset+1]) << 8);
    offset += 2; return v;
}
uint32_t IndexPackStrategy::readUint32(const uint8_t* data, size_t& offset) {
    uint32_t v = 0;
    for (int i = 0; i < 4; ++i) v |= static_cast<uint32_t>(data[offset+i]) << (i*8);
    offset += 4; return v;
}
uint64_t IndexPackStrategy::readUint64(const uint8_t* data, size_t& offset) {
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) v |= static_cast<uint64_t>(data[offset+i]) << (i*8);
    offset += 8; return v;
}

// ===== pack =====

std::vector<uint8_t> IndexPackStrategy::pack(const std::string& /*baseDir*/,
                                             const std::vector<FileInfo>& files) {
    std::vector<uint8_t> dataArea;
    std::vector<DirEntry> entries;

    uint64_t currentOffset = 0;

    for (const auto& info : files) {
        DirEntry e;
        e.path        = info.relativePath;
        e.fileType    = static_cast<uint16_t>(info.fileType);
        e.permissions = static_cast<uint16_t>(info.permissions);
        e.uid         = static_cast<uint32_t>(info.owner);
        e.gid         = static_cast<uint32_t>(info.group);
        e.mtime       = static_cast<uint64_t>(info.mtime);
        e.linkTarget  = info.symlinkTarget;
        e.offset      = currentOffset;

        if (info.isRegular()) {
            e.size = info.content.size();
            dataArea.insert(dataArea.end(), info.content.begin(), info.content.end());
        } else {
            e.size = 0;
        }

        currentOffset += e.size;
        entries.push_back(e);
    }

    // 构造中央目录
    std::vector<uint8_t> centralDir;
    for (const auto& e : entries) {
        std::vector<uint8_t> eb = e.serialize();
        centralDir.insert(centralDir.end(), eb.begin(), eb.end());
    }

    uint64_t centralDirOffset = dataArea.size();
    uint64_t centralDirSize   = centralDir.size();

    // 组装最终输出：数据区 + 中央目录 + EOCD
    std::vector<uint8_t> result;
    result.insert(result.end(), dataArea.begin(), dataArea.end());
    result.insert(result.end(), centralDir.begin(), centralDir.end());

    // EOCD (32B)
    writeUint32(result, EOCD_SIGNATURE);
    writeUint32(result, static_cast<uint32_t>(entries.size()));
    writeUint64(result, centralDirOffset);
    writeUint64(result, centralDirSize);
    // padding to 32B
    for (int i = 0; i < 8; ++i) result.push_back(0);

    return result;
}

// ===== unpack =====

void IndexPackStrategy::unpack(const std::vector<uint8_t>& data,
                               const std::string& /*destDir*/,
                               std::vector<FileInfo>& outFiles) {
    if (data.size() < 32) {
        std::cerr << "Error: index pack data too small" << std::endl;
        return;
    }

    // 读取 EOCD（末尾 32B）
    size_t eocdOffset = data.size() - 32;
    size_t off = eocdOffset;

    uint32_t signature = readUint32(data.data(), off);
    if (signature != EOCD_SIGNATURE) {
        std::cerr << "Error: invalid EOCD signature in index pack" << std::endl;
        return;
    }

    uint32_t entryCount      = readUint32(data.data(), off);
    uint64_t centralDirStart = readUint64(data.data(), off);
    /*uint64_t centralDirSize  =*/ readUint64(data.data(), off);

    // 解析中央目录
    size_t cdOff = static_cast<size_t>(centralDirStart);
    std::vector<DirEntry> entries;

    for (uint32_t i = 0; i < entryCount; ++i) {
        entries.push_back(DirEntry::deserialize(data.data(), cdOff));
    }

    // 按偏移读取文件数据
    for (const auto& e : entries) {
        FileInfo info;
        info.relativePath = e.path;
        info.fileType     = static_cast<mode_t>(e.fileType);
        info.permissions  = static_cast<mode_t>(e.permissions);
        info.owner        = static_cast<uid_t>(e.uid);
        info.group        = static_cast<gid_t>(e.gid);
        info.fileSize     = static_cast<off_t>(e.size);
        info.mtime        = static_cast<time_t>(e.mtime);
        info.atime        = info.mtime;
        info.ctime        = info.mtime;
        info.symlinkTarget = e.linkTarget;
        info.deviceId      = 0;

        if (info.isRegular() && e.size > 0) {
            size_t dataStart = static_cast<size_t>(e.offset);
            info.content.assign(data.begin() + static_cast<ptrdiff_t>(dataStart),
                                data.begin() + static_cast<ptrdiff_t>(dataStart + e.size));
        }

        outFiles.push_back(std::move(info));
    }
}
